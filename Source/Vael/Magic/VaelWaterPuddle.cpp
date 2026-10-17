// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelWaterPuddle.h"
#include "Combat/VaelCombatStatics.h"
#include "Combat/VaelCharacterBase.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Magic/VaelElementSource.h"
#include "Magic/VaelMagicSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Vael.h"
#include "World/VaelGround.h"
#include "World/VaelRegion.h"

namespace
{
	/** Radius of the engine sphere */
	constexpr float SphereMeshRadius = 50.0f;

	/** Height of a blob in cm; half of it sinks into the ground */
	constexpr float BlobHeight = 4.0f;

	/** How far down water looks for ground to land on */
	constexpr float GroundSearchDistance = 500.0f;

	/** Seconds between two wetting steps */
	constexpr float WetStepInterval = 0.5f;

	/** A puddle with less water than this is gone */
	constexpr float DriedOutWater = 0.05f;

	/** Most blobs in one puddle; more water makes the nearest blob bigger */
	constexpr int32 MaxBlobs = 16;

	/** The soft edge of the water material fades within this many cm of the ground, so a flat blob doesn't vanish into it */
	constexpr float PuddleContactFade = 1.5f;

	/** A puddle barely wobbles */
	constexpr float PuddleWobble = 0.4f;

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

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	BlobMesh = SphereMesh.Object;
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
		if (InSource != nullptr && InSource->Material != nullptr)
		{
			Puddle->Material = UMaterialInstanceDynamic::Create(InSource->Material, Puddle);
			Puddle->Material->SetScalarParameterValue(TEXT("ContactFade"), PuddleContactFade);
			Puddle->Material->SetScalarParameterValue(TEXT("WobbleStrength"), PuddleWobble);
		}
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
	for (int32 Index = 0; Index < Blobs.Num(); ++Index)
	{
		if (FVector::DistSquared2D(Location, Blobs[Index]->GetComponentLocation()) <= FMath::Square(GetBlobRadius(BlobWater[Index]) + ExtraDistance))
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
	for (int32 Index = 0; Index < Blobs.Num(); ++Index)
	{
		BlobWater[Index] *= Keep;
		BlobShownRadius[Index] = FMath::FInterpTo(BlobShownRadius[Index], GetBlobRadius(BlobWater[Index]), DeltaSeconds, BlobGrowSpeed);

		// Slightly oval, so neighbouring blobs read as one puddle and not as coins
		const float Radius = FMath::Max(BlobShownRadius[Index], 0.1f);
		Blobs[Index]->SetWorldScale3D(FVector(Radius * 1.15f, Radius * 0.9f, BlobHeight * 0.5f) / SphereMeshRadius);
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

	for (int32 Index = 0; Index < Other->Blobs.Num(); ++Index)
	{
		if (Blobs.Num() < MaxBlobs)
		{
			AddBlob(Other->Blobs[Index]->GetComponentLocation(), Other->BlobWater[Index], Other->BlobShownRadius[Index]);
		}
		else
		{
			AddWater(Other->Blobs[Index]->GetComponentLocation(), Other->BlobWater[Index], MaxSpillDepth);
		}
	}

	UE_LOG(LogVael, Verbose, TEXT("Puddle %s flows into %s"), *Other->GetName(), *GetName());

	Other->Blobs.Reset();
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
	for (int32 Index = 0; Index < Blobs.Num(); ++Index)
	{
		const float Gap = FVector::Dist2D(GroundLocation, Blobs[Index]->GetComponentLocation()) - GetBlobRadius(BlobWater[Index]);
		if (Gap < NearestGap)
		{
			Nearest = Index;
			NearestGap = Gap;
		}
	}

	// Water spilled around an obstacle must not run back into the blob it came from
	const float JoinDistance = SpillDepth > 0 ? 0.0f : MagicSettings->PuddleMergeDistance;
	const bool bJoinsBlob = Nearest != INDEX_NONE && (NearestGap <= JoinDistance || Blobs.Num() >= MaxBlobs);
	if (bJoinsBlob)
	{
		// The blob drifts a little towards where the water came in, but keeps its height on the ground
		const FVector Center = Blobs[Nearest]->GetComponentLocation();
		const float Share = Amount / (BlobWater[Nearest] + Amount);
		Blobs[Nearest]->SetWorldLocation(FVector(FMath::Lerp(FVector2D(Center), FVector2D(GroundLocation), Share * 0.5f), Center.Z));
		BlobWater[Nearest] += Amount;
	}
	else
	{
		Nearest = AddBlob(GroundLocation, Amount, 0.0f);
	}

	FitBlob(Nearest, SpillDepth);
	MergeBlobs();

	UE_LOG(LogVael, Verbose, TEXT("Puddle %s holds %.2f water in %d blobs%s"), *GetName(), GetWater(), Blobs.Num(), IsDeepEnough() ? TEXT(", deep enough to count") : TEXT(""));
}

int32 AVaelWaterPuddle::AddBlob(const FVector& GroundLocation, float Water, float ShownRadius)
{
	UStaticMeshComponent* Blob = NewObject<UStaticMeshComponent>(this);
	Blob->SetStaticMesh(BlobMesh);
	Blob->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Blob->SetCastShadow(false);
	Blob->SetupAttachment(RootComponent);
	Blob->RegisterComponent();
	Blob->SetWorldLocationAndRotation(GroundLocation, FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f));
	Blob->SetWorldScale3D(FVector(FMath::Max(ShownRadius, 0.1f) / SphereMeshRadius));

	if (Material != nullptr)
	{
		Blob->SetMaterial(0, Material);
	}

	Blobs.Add(Blob);
	BlobWater.Add(Water);
	BlobShownRadius.Add(ShownRadius);
	return Blobs.Num() - 1;
}

void AVaelWaterPuddle::RemoveBlob(int32 Index)
{
	Blobs[Index]->DestroyComponent();
	Blobs.RemoveAt(Index);
	BlobWater.RemoveAt(Index);
	BlobShownRadius.RemoveAt(Index);
}

void AVaelWaterPuddle::FitBlob(int32 Index, int32 SpillDepth)
{
	const UWorld* World = GetWorld();
	const FVector Center = Blobs[Index]->GetComponentLocation();
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
		for (int32 First = 0; First < Blobs.Num() && !bMerged; ++First)
		{
			for (int32 Second = First + 1; Second < Blobs.Num() && !bMerged; ++Second)
			{
				const FVector FirstCenter = Blobs[First]->GetComponentLocation();
				const FVector SecondCenter = Blobs[Second]->GetComponentLocation();
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

				Blobs[First]->SetWorldLocation(Joined);
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
