// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelWaterPuddle.h"
#include "Combat/VaelCombatStatics.h"
#include "Combat/VaelCharacterBase.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Magic/VaelElementSource.h"
#include "Magic/VaelMagicSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInstance.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Vael.h"
#include "VaelAssets.h"
#include "World/VaelGround.h"
#include "World/VaelRegion.h"

namespace
{
	/** Size of the engine plane */
	constexpr float PlaneMeshSize = 100.0f;

	/** The surface of a puddle floats this many cm over the ground */
	constexpr float SurfaceLift = 1.0f;

	/** Texels along each side of the outline mask */
	constexpr int32 MaskResolution = 64;

	/** Seconds between two paintings of the outline at most */
	constexpr float RepaintInterval = 0.05f;

	/** A blob pulls water up to this many of its radii away; two blobs flow together once their edges come this close */
	constexpr float FieldReach = 1.6f;

	/** What one blob pulls at its own edge */
	constexpr float EdgeField = (1.0f - 1.0f / (FieldReach * FieldReach)) * (1.0f - 1.0f / (FieldReach * FieldReach));

	/** How far down water looks for ground to land on */
	constexpr float GroundSearchDistance = 500.0f;

	/** Seconds between two wetting steps */
	constexpr float WetStepInterval = 0.5f;

	/** A puddle with less water than this is gone */
	constexpr float DriedOutWater = 0.05f;

	/** Most blobs in one puddle; more water makes the nearest blob bigger */
	constexpr int32 MaxBlobs = 16;

	/** The soft edge of the water material fades within this many cm of the ground, so a flat blob doesn't vanish into it */
	constexpr float PuddleContactFade = 0.3f;

	/** A puddle barely wobbles */
	constexpr float PuddleWobble = 0.4f;

	/** Look of water on the ground: little glow, some of its deep color, a crisp edge */
	constexpr float PuddleMaxEmissive = 0.2f;
	constexpr float PuddleMinMurkiness = 0.35f;
	constexpr float PuddleEdgeSoftness = 0.04f;

	/** How fast a blob grows or shrinks towards the size of its water */
	constexpr float BlobGrowSpeed = 5.0f;

	/** Water runs on to a spot this much lower, in cm, so it gathers at the foot of rocks and walls and not on top */
	constexpr float RunOffDrop = 12.0f;

	/** Distance of every step water runs downhill, in cm */
	constexpr float RunOffStep = 45.0f;

	/** Most steps water runs before it stays */
	constexpr int32 MaxRunOffSteps = 8;

	/** Obstacles are looked for in a flat slab this high above the ground, so the ground itself and pebbles don't count */
	constexpr float ObstacleSlabBottom = 12.0f;
	constexpr float ObstacleSlabTop = 60.0f;

	/** Smallest blob, in cm; water that finds no room for it stays in the blob it came from */
	constexpr float MinBlobRadius = 12.0f;

	/** Water spills over to free spots at most this many times in a row */
	constexpr int32 MaxSpillDepth = 3;

	/** Share of the water that falls straight down; the rest lands around it as drops */
	constexpr float CenterShare = 0.4f;

	/** True if something stands in the way of a blob of the given radius lying at a ground location */
	bool IsBlocked(const UWorld* World, const FVector& Ground, float Radius)
	{
		// A box a little smaller than the blob, so a wall that only touches the edge doesn't count
		const float HalfHeight = (ObstacleSlabTop - ObstacleSlabBottom) * 0.5f;
		const FCollisionShape Slab = FCollisionShape::MakeBox(FVector(Radius * 0.8f, Radius * 0.8f, HalfHeight));
		const FVector Center = Ground + FVector(0.0f, 0.0f, ObstacleSlabBottom + HalfHeight);

		return World->OverlapAnyTestByObjectType(Center, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic), Slab);
	}

	/** Largest radius up to the wanted one a blob has room for at a ground location */
	float FindFreeRadius(const UWorld* World, const FVector& Ground, float Wanted)
	{
		for (float Radius = Wanted; Radius >= MinBlobRadius; Radius *= 0.8f)
		{
			if (!IsBlocked(World, Ground, Radius))
			{
				return Radius;
			}
		}

		return 0.0f;
	}

	/** Ground near a point, at most the given height above or below it; false if there is none */
	bool FindGroundNear(const UWorld* World, const FVector& Point, float MaxRise, float MaxFall, FVector& OutGround)
	{
		FHitResult Hit;
		if (!VaelGround::TraceGround(World, Point + FVector(0.0f, 0.0f, MaxRise), Point - FVector(0.0f, 0.0f, MaxFall), Hit))
		{
			return false;
		}

		OutGround = Hit.ImpactPoint;
		return true;
	}

	/** Where water falling from a location ends up: on the ground below it, then downhill off rocks, walls and ledges */
	bool FindLanding(const UWorld* World, const FVector& Location, FVector& OutGround)
	{
		FHitResult Hit;
		if (!VaelGround::TraceGround(World, Location, Location - FVector(0.0f, 0.0f, GroundSearchDistance), Hit))
		{
			return false;
		}

		FVector Spot = Hit.ImpactPoint;
		for (int32 Step = 0; Step < MaxRunOffSteps; ++Step)
		{
			// Look around for the lowest ground; water runs there if it is clearly lower
			FVector Lowest = Spot;
			const float StartAngle = FMath::FRandRange(0.0f, UE_TWO_PI);
			for (int32 Direction = 0; Direction < 8; ++Direction)
			{
				const float Angle = StartAngle + Direction * UE_TWO_PI / 8.0f;
				FVector Neighbour;
				if (FindGroundNear(World, Spot + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * RunOffStep, 5.0f, GroundSearchDistance, Neighbour) && Neighbour.Z < Lowest.Z)
				{
					Lowest = Neighbour;
				}
			}

			if (Lowest.Z > Spot.Z - RunOffDrop)
			{
				break;
			}

			Spot = Lowest;
		}

		OutGround = Spot;
		return true;
	}
}

AVaelWaterPuddle::AVaelWaterPuddle()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// The engine plane is 1 m across and faces up
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	Surface = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Surface"));
	Surface->SetupAttachment(RootComponent);
	Surface->SetStaticMesh(PlaneMesh.Object);
	Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Surface->SetCastShadow(false);
	Surface->SetUsingAbsoluteLocation(true);
	Surface->SetUsingAbsoluteRotation(true);
	Surface->SetUsingAbsoluteScale(true);
	Surface->SetVisibility(false);
}

void AVaelWaterPuddle::CreateSurfaceMaterial()
{
	if (Source == nullptr || Source->Material == nullptr)
	{
		return;
	}

	// The puddle material with the look of the source's water; without it the source's own material over the whole plane
	UMaterialInterface* PuddleMaterial = VaelAssets::LoadOptional(UVaelMagicSettings::Get()->PuddleMaterial);
	if (PuddleMaterial != nullptr)
	{
		Material = UMaterialInstanceDynamic::Create(PuddleMaterial, this);
		if (UMaterialInstance* SourceInstance = Cast<UMaterialInstance>(Source->Material))
		{
			Material->CopyParameterOverrides(SourceInstance);
		}
	}
	else
	{
		UE_LOG(LogVael, Warning, TEXT("Puddle material %s is missing, puddles show as squares"), *UVaelMagicSettings::Get()->PuddleMaterial.ToString());
		Material = UMaterialInstanceDynamic::Create(Source->Material, this);
	}

	Material->SetScalarParameterValue(TEXT("ContactFade"), PuddleContactFade);
	Material->SetScalarParameterValue(TEXT("WobbleStrength"), PuddleWobble);

	// Water on the ground hardly glows and shows more of its deep color than a ball of it in the air
	float Emissive = 0.0f;
	float Murkiness = 0.0f;
	Source->Material->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("EmissiveStrength")), Emissive);
	Source->Material->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("Murkiness")), Murkiness);
	Material->SetScalarParameterValue(TEXT("EmissiveStrength"), FMath::Min(Emissive, PuddleMaxEmissive));
	Material->SetScalarParameterValue(TEXT("Murkiness"), FMath::Max(Murkiness, PuddleMinMurkiness));
	Material->SetScalarParameterValue(TEXT("EdgeSoftness"), PuddleEdgeSoftness);
	Surface->SetMaterial(0, Material);

	Mask = UTexture2D::CreateTransient(MaskResolution, MaskResolution, PF_B8G8R8A8);
	if (Mask != nullptr)
	{
		Mask->SRGB = false;
		Mask->Filter = TF_Bilinear;
		Mask->AddressX = TA_Clamp;
		Mask->AddressY = TA_Clamp;
		Mask->UpdateResource();
		Material->SetTextureParameterValue(TEXT("PuddleMask"), Mask);
	}
}

void AVaelWaterPuddle::PourSplash(UWorld* World, const FVector& Location, float Amount, UVaelElementSource* InSource, APawn* InInstigator)
{
	if (World == nullptr || Amount <= 0.0f)
	{
		return;
	}

	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	const TWeakObjectPtr<UVaelElementSource> WeakSource = InSource;
	const TWeakObjectPtr<APawn> WeakInstigator = InInstigator;

	auto PourLater = [World, WeakSource, WeakInstigator](const FVector& DropLocation, float DropAmount, float Delay)
	{
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(World, [World, DropLocation, DropAmount, WeakSource, WeakInstigator]()
		{
			AddSplash(World, DropLocation, DropAmount, WeakSource.Get(), WeakInstigator.Get());
		}), FMath::Max(Delay, 0.01f), false);
	};

	// The body of the water comes down first, the drops thrown out further land after it
	PourLater(Location, Amount * CenterShare, MagicSettings->PuddleFirstDropDelay);

	const int32 NumDrops = FMath::Clamp(FMath::RoundToInt(2.0f + Amount * 1.5f), 3, 8);
	const float Spread = GetBlobRadius(Amount) * 1.3f + 30.0f;
	const float StartAngle = FMath::FRandRange(0.0f, UE_TWO_PI);

	for (int32 Index = 0; Index < NumDrops; ++Index)
	{
		const float Angle = StartAngle + (Index + FMath::FRandRange(-0.3f, 0.3f)) * UE_TWO_PI / NumDrops;
		const FVector Offset = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Spread * FMath::FRandRange(0.5f, 1.0f);
		PourLater(Location + Offset, Amount * (1.0f - CenterShare) / NumDrops, FMath::FRandRange(MagicSettings->PuddleFirstDropDelay, MagicSettings->PuddleLastDropDelay));
	}
}

AVaelWaterPuddle* AVaelWaterPuddle::AddSplash(UWorld* World, const FVector& Location, float Amount, UVaelElementSource* InSource, APawn* InInstigator)
{
	if (World == nullptr || Amount <= 0.0f)
	{
		return nullptr;
	}

	FVector GroundLocation;
	if (!FindLanding(World, Location, GroundLocation))
	{
		return nullptr;
	}

	const float MergeDistance = UVaelMagicSettings::Get()->PuddleMergeDistance;

	// Water landing on or beside puddles of the same water and side joins them; puddles it connects become one
	AVaelWaterPuddle* Puddle = nullptr;
	TArray<AVaelWaterPuddle*> Joined;
	for (TActorIterator<AVaelWaterPuddle> It(World); It; ++It)
	{
		if (!It->IsActorBeingDestroyed() && It->Accepts(InSource, InInstigator) && It->Covers(GroundLocation, MergeDistance))
		{
			Joined.Add(*It);
		}
	}

	if (Joined.Num() > 0)
	{
		Puddle = Joined[0];
		for (int32 Index = 1; Index < Joined.Num(); ++Index)
		{
			Puddle->Absorb(Joined[Index]);
		}
	}
	else
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Instigator = InInstigator;
		SpawnParameters.Owner = InInstigator;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		Puddle = World->SpawnActor<AVaelWaterPuddle>(AVaelWaterPuddle::StaticClass(), FTransform(GroundLocation), SpawnParameters);
		if (Puddle == nullptr)
		{
			return nullptr;
		}

		Puddle->Source = InSource;
		Puddle->CreateSurfaceMaterial();
	}

	Puddle->AddWater(GroundLocation, Amount);
	return Puddle;
}

bool AVaelWaterPuddle::IsDeepWaterAt(const UWorld* World, const FVector& Location)
{
	if (World == nullptr)
	{
		return false;
	}

	for (TActorIterator<AVaelWaterPuddle> It(World); It; ++It)
	{
		if (It->IsDeepEnough() && It->Covers(Location))
		{
			return true;
		}
	}

	return false;
}

float AVaelWaterPuddle::GetWater() const
{
	float Water = 0.0f;
	for (const float Amount : BlobWater)
	{
		Water += Amount;
	}

	return Water;
}

bool AVaelWaterPuddle::IsDeepEnough() const
{
	return GetWater() >= UVaelMagicSettings::Get()->PuddleWaterToCount;
}

bool AVaelWaterPuddle::Covers(const FVector& Location, float ExtraDistance) const
{
	for (int32 Index = 0; Index < BlobCenters.Num(); ++Index)
	{
		if (FVector::DistSquared2D(Location, BlobCenters[Index]) <= FMath::Square(GetBlobRadius(BlobWater[Index]) + ExtraDistance))
		{
			return true;
		}
	}

	return false;
}

bool AVaelWaterPuddle::Accepts(const UVaelElementSource* InSource, const APawn* InInstigator) const
{
	if (Source != InSource)
	{
		return false;
	}

	return GetInstigator() == InInstigator || (GetInstigator() != nullptr && InInstigator != nullptr && !UVaelCombatStatics::CanDamage(GetInstigator(), InInstigator));
}

void AVaelWaterPuddle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();

	// Rain keeps puddles, a drought dries them faster
	float DryPerSecond = MagicSettings->PuddleDryPerSecond;
	if (const AVaelRegion* Region = AVaelRegion::GetRegionAt(GetWorld(), GetActorLocation()))
	{
		DryPerSecond *= Region->GetWeather() == EVaelWeather::Rain ? 0.0f : Region->GetWeather() == EVaelWeather::Drought ? MagicSettings->PuddleDroughtDryMultiplier : 1.0f;
	}

	// Every blob shrinks by the same share, so the puddle keeps its shape while it dries; the blobs follow their water smoothly
	const float Water = GetWater();
	const float Keep = Water > 0.0f ? FMath::Max(Water - DryPerSecond * DeltaSeconds, 0.0f) / Water : 0.0f;
	for (int32 Index = 0; Index < BlobCenters.Num(); ++Index)
	{
		BlobWater[Index] *= Keep;

		const float Shown = FMath::FInterpTo(BlobShownRadius[Index], GetBlobRadius(BlobWater[Index]), DeltaSeconds, BlobGrowSpeed);
		bSurfaceDirty |= FMath::Abs(Shown - BlobShownRadius[Index]) > KINDA_SMALL_NUMBER;
		BlobShownRadius[Index] = Shown;
	}

	// The outline is painted a few times a second at most, growing and drying are slow enough for that
	RepaintTime += DeltaSeconds;
	if (bSurfaceDirty && RepaintTime >= RepaintInterval)
	{
		RefreshSurface();
	}

	if (Water * Keep < DriedOutWater)
	{
		UE_LOG(LogVael, Verbose, TEXT("Puddle %s has dried out"), *GetName());
		Destroy();
		return;
	}

	WetStepTime += DeltaSeconds;
	if (WetStepTime < WetStepInterval || !IsDeepEnough())
	{
		return;
	}

	WetStepTime = 0.0f;

	// Whoever stands in deep enough water gets wet; a puddle of a caster only wets the caster's enemies
	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		if (!Covers(It->GetActorLocation()))
		{
			continue;
		}

		if (GetInstigator() == nullptr || UVaelCombatStatics::CanDamage(GetInstigator(), *It))
		{
			UVaelCombatStatics::ApplyStatus(GetInstigator(), *It, EVaelStatus::Wet, MagicSettings->PuddleWetDuration);
		}
	}
}

void AVaelWaterPuddle::Absorb(AVaelWaterPuddle* Other)
{
	if (Other == nullptr || Other == this)
	{
		return;
	}

	for (int32 Index = 0; Index < Other->BlobCenters.Num(); ++Index)
	{
		if (BlobCenters.Num() < MaxBlobs)
		{
			AddBlob(Other->BlobCenters[Index], Other->BlobWater[Index], Other->BlobShownRadius[Index]);
		}
		else
		{
			AddWater(Other->BlobCenters[Index], Other->BlobWater[Index], MaxSpillDepth);
		}
	}

	UE_LOG(LogVael, Verbose, TEXT("Puddle %s flows into %s"), *Other->GetName(), *GetName());

	Other->BlobCenters.Reset();
	Other->BlobWater.Reset();
	Other->BlobShownRadius.Reset();
	Other->Destroy();

	MergeBlobs();
}

void AVaelWaterPuddle::AddWater(const FVector& GroundLocation, float Amount, int32 SpillDepth)
{
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	Amount = FMath::Min(Amount, FMath::Max(MagicSettings->PuddleMaxWater - GetWater(), 0.0f));
	if (Amount <= 0.0f)
	{
		return;
	}

	// Water that lands on or beside a blob makes it bigger, as if it had landed in its middle; otherwise it starts a blob of its own
	int32 Nearest = INDEX_NONE;
	float NearestGap = TNumericLimits<float>::Max();
	for (int32 Index = 0; Index < BlobCenters.Num(); ++Index)
	{
		const float Gap = FVector::Dist2D(GroundLocation, BlobCenters[Index]) - GetBlobRadius(BlobWater[Index]);
		if (Gap < NearestGap)
		{
			Nearest = Index;
			NearestGap = Gap;
		}
	}

	// Water spilled around an obstacle must not run back into the blob it came from
	const float JoinDistance = SpillDepth > 0 ? 0.0f : MagicSettings->PuddleMergeDistance;
	const bool bJoinsBlob = Nearest != INDEX_NONE && (NearestGap <= JoinDistance || BlobCenters.Num() >= MaxBlobs);
	if (bJoinsBlob)
	{
		// The blob drifts a little towards where the water came in, but keeps its height on the ground
		const FVector Center = BlobCenters[Nearest];
		const float Share = Amount / (BlobWater[Nearest] + Amount);
		BlobCenters[Nearest] = (FVector(FMath::Lerp(FVector2D(Center), FVector2D(GroundLocation), Share * 0.5f), Center.Z));
		BlobWater[Nearest] += Amount;
	}
	else
	{
		Nearest = AddBlob(GroundLocation, Amount, 0.0f);
	}

	FitBlob(Nearest, SpillDepth);
	MergeBlobs();
	bSurfaceDirty = true;

	UE_LOG(LogVael, Verbose, TEXT("Puddle %s holds %.2f water in %d blobs%s"), *GetName(), GetWater(), BlobCenters.Num(), IsDeepEnough() ? TEXT(", deep enough to count") : TEXT(""));
}

int32 AVaelWaterPuddle::AddBlob(const FVector& GroundLocation, float Water, float ShownRadius)
{
	BlobCenters.Add(GroundLocation);
	BlobWater.Add(Water);
	BlobShownRadius.Add(ShownRadius);
	bSurfaceDirty = true;
	return BlobCenters.Num() - 1;
}

void AVaelWaterPuddle::RemoveBlob(int32 Index)
{
	BlobCenters.RemoveAt(Index);
	BlobWater.RemoveAt(Index);
	BlobShownRadius.RemoveAt(Index);
	bSurfaceDirty = true;
}

void AVaelWaterPuddle::RefreshSurface()
{
	bSurfaceDirty = false;
	RepaintTime = 0.0f;

	if (BlobCenters.Num() == 0 || Material == nullptr)
	{
		Surface->SetVisibility(false);
		return;
	}

	// A square over all blobs with room for the water that joins them; the surface lies at the height of most of the water
	FBox2D Bounds(ForceInit);
	float Height = 0.0f;
	float Weight = 0.0f;
	for (int32 Index = 0; Index < BlobCenters.Num(); ++Index)
	{
		const FVector2D Center(BlobCenters[Index]);
		const float Reach = BlobShownRadius[Index] * FieldReach + 10.0f;
		Bounds += Center - FVector2D(Reach);
		Bounds += Center + FVector2D(Reach);
		Height += BlobCenters[Index].Z * (BlobWater[Index] + 0.01f);
		Weight += BlobWater[Index] + 0.01f;
	}

	const FVector2D Middle = Bounds.GetCenter();
	const float Size = FMath::Max(Bounds.GetSize().GetMax(), 1.0f);
	const FVector2D Origin = Middle - FVector2D(Size * 0.5f);

	Surface->SetWorldLocationAndRotation(FVector(Middle, Height / Weight + SurfaceLift), FRotator::ZeroRotator);
	Surface->SetWorldScale3D(FVector(Size / PlaneMeshSize, Size / PlaneMeshSize, 1.0f));
	Surface->SetVisibility(true);

	Material->SetVectorParameterValue(TEXT("MaskOrigin"), FLinearColor(Origin.X, Origin.Y, 0.0f, 0.0f));
	Material->SetScalarParameterValue(TEXT("MaskSize"), Size);

	if (Mask == nullptr || Mask->GetResource() == nullptr)
	{
		return;
	}

	// Every blob pulls the water towards it, the closer the stronger and not beyond its reach; where the pulls add up to a lone blob's edge or more there is water.
	// So blobs close to each other flow together into one puddle with a soft, round outline, like drops of mercury.
	const float TexelSize = Size / MaskResolution;
	uint8* Pixels = new uint8[MaskResolution * MaskResolution * 4];
	for (int32 Row = 0; Row < MaskResolution; ++Row)
	{
		for (int32 Column = 0; Column < MaskResolution; ++Column)
		{
			const FVector2D Point = Origin + FVector2D(Column + 0.5f, Row + 0.5f) * TexelSize;

			float Field = 0.0f;
			for (int32 Index = 0; Index < BlobCenters.Num(); ++Index)
			{
				const float Reach = BlobShownRadius[Index] * FieldReach;
				const float Share = FVector2D::DistSquared(Point, FVector2D(BlobCenters[Index])) / FMath::Max(Reach * Reach, 1.0f);
				Field += FMath::Square(FMath::Max(1.0f - Share, 0.0f));
			}

			// The edge of a lone blob is written as one half, which the material takes as the edge
			const uint8 Value = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Field / (2.0f * EdgeField) * 255.0f), 0, 255));
			uint8* Pixel = Pixels + (Row * MaskResolution + Column) * 4;
			Pixel[0] = Value;
			Pixel[1] = Value;
			Pixel[2] = Value;
			Pixel[3] = 255;
		}
	}

	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, MaskResolution, MaskResolution);
	Mask->UpdateTextureRegions(0, 1, Region, MaskResolution * 4, 4, Pixels, [](uint8* Data, const FUpdateTextureRegion2D* Regions)
	{
		delete[] Data;
		delete Regions;
	});
}

void AVaelWaterPuddle::FitBlob(int32 Index, int32 SpillDepth)
{
	const UWorld* World = GetWorld();
	const FVector Center = BlobCenters[Index];
	const float Wanted = GetBlobRadius(BlobWater[Index]);
	const float Free = FindFreeRadius(World, Center, Wanted);
	if (Free >= Wanted * 0.95f)
	{
		return;
	}

	// Water that has no room here runs around the obstacle to the side where there is room
	const float Kept = FMath::Max(Free, MinBlobRadius);
	const float Excess = BlobWater[Index] - GetBlobWater(Kept);
	if (Excess <= 0.0f || SpillDepth >= MaxSpillDepth)
	{
		return;
	}

	const float SpillRadius = FMath::Max(GetBlobRadius(Excess), MinBlobRadius);
	const float StartAngle = FMath::FRandRange(0.0f, UE_TWO_PI);
	for (int32 Direction = 0; Direction < 12; ++Direction)
	{
		const float Angle = StartAngle + Direction * UE_TWO_PI / 12.0f;
		const FVector Wanted2D = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * (Kept + SpillRadius * 0.7f);

		// Only onto ground about as high as the puddle, never up onto the obstacle
		FVector Spot;
		if (!FindGroundNear(World, Wanted2D, RunOffDrop, 100.0f, Spot) || Spot.Z > Center.Z + RunOffDrop || IsBlocked(World, Spot, SpillRadius * 0.6f))
		{
			continue;
		}

		BlobWater[Index] -= Excess;
		AddWater(Spot, Excess, SpillDepth + 1);
		return;
	}
}

void AVaelWaterPuddle::MergeBlobs()
{
	const UWorld* World = GetWorld();

	bool bMerged = true;
	while (bMerged)
	{
		bMerged = false;
		for (int32 First = 0; First < BlobCenters.Num() && !bMerged; ++First)
		{
			for (int32 Second = First + 1; Second < BlobCenters.Num() && !bMerged; ++Second)
			{
				const FVector FirstCenter = BlobCenters[First];
				const FVector SecondCenter = BlobCenters[Second];
				const float Overlap = GetBlobRadius(BlobWater[First]) + GetBlobRadius(BlobWater[Second]) - FVector::Dist2D(FirstCenter, SecondCenter);
				if (Overlap < (GetBlobRadius(BlobWater[First]) + GetBlobRadius(BlobWater[Second])) * 0.3f)
				{
					continue;
				}

				// Join in the middle of their water, but only where the joined blob has room; around an obstacle they stay two
				const float Water = BlobWater[First] + BlobWater[Second];
				const float Share = BlobWater[Second] / Water;
				const FVector Joined = FVector(FMath::Lerp(FVector2D(FirstCenter), FVector2D(SecondCenter), Share), FMath::Lerp(FirstCenter.Z, SecondCenter.Z, Share));
				if (FindFreeRadius(World, Joined, GetBlobRadius(Water)) < GetBlobRadius(Water) * 0.95f)
				{
					continue;
				}

				BlobCenters[First] = Joined;
				BlobWater[First] = Water;
				BlobShownRadius[First] = FMath::Max(BlobShownRadius[First], BlobShownRadius[Second]);
				RemoveBlob(Second);
				bMerged = true;
			}
		}
	}
}

float AVaelWaterPuddle::GetBlobRadius(float Water)
{
	// The puddle covers ground in step with its water
	return UVaelMagicSettings::Get()->PuddleRadiusPerWater * FMath::Sqrt(FMath::Max(Water, 0.0f));
}

float AVaelWaterPuddle::GetBlobWater(float Radius)
{
	return FMath::Square(Radius / UVaelMagicSettings::Get()->PuddleRadiusPerWater);
}
