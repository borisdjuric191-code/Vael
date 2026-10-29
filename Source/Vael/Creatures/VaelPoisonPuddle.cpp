// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelPoisonPuddle.h"
#include "Combat/VaelCombatStatics.h"
#include "Combat/VaelCharacterBase.h"
#include "Components/StaticMeshComponent.h"
#include "Creatures/VaelCreatureData.h"
#include "Creatures/VaelPollenBurst.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Magic/VaelCloudCards.h"
#include "Magic/VaelElementSource.h"
#include "Magic/VaelMagicSettings.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/VaelCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "VaelAssets.h"
#include "World/VaelRegion.h"

namespace
{
	/** Seconds between two poison damage steps */
	constexpr float PoisonHurtInterval = 0.5f;

	/** Size of the engine plane, texels along the outline mask, seconds between two paintings of it */
	constexpr float PoisonPlaneSize = 100.0f;
	constexpr int32 PoisonMaskResolution = 64;
	constexpr float PoisonRepaintInterval = 0.05f;

	/** As the water puddles: a blob pulls the poison up to this many of its radii away, so close blobs flow together */
	constexpr float PoisonFieldReach = 1.6f;
	constexpr float PoisonEdgeField = (1.0f - 1.0f / (PoisonFieldReach * PoisonFieldReach)) * (1.0f - 1.0f / (PoisonFieldReach * PoisonFieldReach));

	/** The look of water on the ground (the values of the water puddles) */
	constexpr float PoisonContactFade = 0.3f;
	constexpr float PoisonWobble = 0.4f;
	constexpr float PoisonMaxEmissive = 0.25f;
	constexpr float PoisonMinMurkiness = 0.45f;
	constexpr float PoisonEdgeSoftness = 0.04f;

	/** A blob with less poison than this is gone */
	constexpr float PoisonMinBlob = 0.04f;

	const UVaelPfuhlnatterData* PoisonData()
	{
		return GetDefault<UVaelPfuhlnatterData>();
	}
}

AVaelPoisonPuddle::AVaelPoisonPuddle()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

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

	Vapour = CreateDefaultSubobject<UVaelCloudCardsComponent>(TEXT("Vapour"));
	Vapour->SetupAttachment(RootComponent);
	Vapour->SetUsingAbsoluteLocation(true);
}

AVaelPoisonPuddle* AVaelPoisonPuddle::Spit(UWorld* World, const FVector& Location, float Delay, AActor* InInstigator)
{
	if (World == nullptr)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVaelPoisonPuddle* Puddle = World->SpawnActor<AVaelPoisonPuddle>(Location, FRotator::ZeroRotator, Params);
	if (Puddle != nullptr)
	{
		Puddle->SpitBy = InInstigator;
		Puddle->LandAt = Location;
		Puddle->FallDelay = Delay;
		Puddle->State = EVaelPoisonState::Falling;
	}
	return Puddle;
}

bool AVaelPoisonPuddle::FindGround(const UWorld* World, const FVector& Location, FVector& OutGround)
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(VaelPoisonGround), false);
	// Only the ground itself: a trace on the channel would stop on the capsule of the player it was spat at
	if (World->LineTraceSingleByObjectType(Hit, Location + FVector(0.0f, 0.0f, 200.0f), Location - FVector(0.0f, 0.0f, 800.0f), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		OutGround = Hit.ImpactPoint;
		return true;
	}
	return false;
}

void AVaelPoisonPuddle::SetupLook()
{
	const UVaelPfuhlnatterData* Data = PoisonData();
	const UVaelElementSource* Source = Data->PoisonSource.LoadSynchronous();
	UMaterialInterface* PuddleMaterial = VaelAssets::LoadOptional(UVaelMagicSettings::Get()->PuddleMaterial);
	if (Source == nullptr || Source->Material == nullptr || PuddleMaterial == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Poison puddle: the poison water or the puddle material is missing"));
		return;
	}

	// The water puddles' material in the colours of the poison water; on the ground it hardly glows and shows its depth
	Material = UMaterialInstanceDynamic::Create(PuddleMaterial, this);
	if (UMaterialInstance* SourceInstance = Cast<UMaterialInstance>(Source->Material))
	{
		Material->CopyParameterOverrides(SourceInstance);
	}
	float Emissive = 0.0f;
	float Murkiness = 0.0f;
	Source->Material->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("EmissiveStrength")), Emissive);
	Source->Material->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("Murkiness")), Murkiness);
	Source->Material->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("ShallowColor")), ShallowColor);
	Source->Material->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("DeepColor")), DeepColor);
	Material->SetScalarParameterValue(TEXT("ContactFade"), PoisonContactFade);
	Material->SetScalarParameterValue(TEXT("WobbleStrength"), PoisonWobble);
	Material->SetScalarParameterValue(TEXT("EmissiveStrength"), FMath::Min(Emissive, PoisonMaxEmissive));
	Material->SetScalarParameterValue(TEXT("Murkiness"), FMath::Max(Murkiness, PoisonMinMurkiness));
	Material->SetScalarParameterValue(TEXT("EdgeSoftness"), PoisonEdgeSoftness);
	if (bCorrupted)
	{
		ShallowColor = Data->CorruptPoisonColor;
		DeepColor = Data->CorruptPoisonDeepColor;
	}
	ShownColor = ShallowColor;
	Surface->SetMaterial(0, Material);

	Mask = UTexture2D::CreateTransient(PoisonMaskResolution, PoisonMaskResolution, PF_B8G8R8A8);
	if (Mask != nullptr)
	{
		Mask->SRGB = false;
		Mask->Filter = TF_Bilinear;
		Mask->AddressX = TA_Clamp;
		Mask->AddressY = TA_Clamp;
		Mask->UpdateResource();
		Material->SetTextureParameterValue(TEXT("PuddleMask"), Mask);
	}

	// Vapour: soft cloud cards in the poison's colour
	if (UMaterialInterface* CloudMaterial = VaelAssets::LoadOptional(UVaelMagicSettings::Get()->CloudCardMaterial))
	{
		UMaterialInstanceDynamic* VapourMaterial = UMaterialInstanceDynamic::Create(CloudMaterial, this);
		VapourMaterial->SetVectorParameterValue(TEXT("Farbe"), bCorrupted ? Data->CorruptPoisonColor : Data->PoisonColor);
		Vapour->Init(VapourMaterial);
	}
}

void AVaelPoisonPuddle::AddPoison(const FVector& GroundLocation, float Amount)
{
	const UVaelPfuhlnatterData* Data = PoisonData();
	if (Material == nullptr)
	{
		SetupLook();
	}
	// Onto the blob it lands on, or as a new one
	for (int32 Index = 0; Index < BlobCenters.Num(); ++Index)
	{
		if (FVector::Dist2D(BlobCenters[Index], GroundLocation) < Data->BlobRadius * FMath::Sqrt(BlobPoison[Index]) * 0.5f)
		{
			BlobPoison[Index] += Amount;
			return;
		}
	}
	BlobCenters.Add(GroundLocation);
	BlobPoison.Add(Amount);
	BlobShownRadius.Add(0.0f);
	// A new splash throws a few small drops round it
	const int32 Drops = FMath::RandRange(2, 4);
	for (int32 Drop = 0; Drop < Drops; ++Drop)
	{
		const FVector Out = FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f).Vector() * Data->BlobRadius * FMath::FRandRange(0.7f, 1.15f);
		BlobCenters.Add(GroundLocation + Out);
		BlobPoison.Add(Amount * FMath::FRandRange(0.06f, 0.14f));
		BlobShownRadius.Add(0.0f);
	}
}

void AVaelPoisonPuddle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const UVaelPfuhlnatterData* Data = PoisonData();
	StateTime += DeltaSeconds;
	Age += DeltaSeconds;

	switch (State)
	{
	case EVaelPoisonState::Falling:
	{
		FallDelay -= DeltaSeconds;
		if (FallDelay > 0.0f)
		{
			return;
		}
		FVector Ground;
		if (!FindGround(GetWorld(), LandAt, Ground))
		{
			Destroy();
			return;
		}
		// Splashes whoever it hits
		FVaelSpellHit Hit;
		Hit.Damage = Data->SplashDamage;
		Hit.Element = EVaelElement::Water;
		for (TActorIterator<AVaelCharacter> It(GetWorld()); It; ++It)
		{
			if (FVector::Dist2D(It->GetActorLocation(), Ground) <= Data->SplashRadius)
			{
				UVaelCombatStatics::ApplyNatureHit(*It, Hit, FVector::ZeroVector);
			}
		}
		const AVaelRegion* Region = AVaelRegion::GetRegionAt(GetWorld(), Ground);
		bCorrupted = Region != nullptr && Region->GetCorruption() >= Data->CorruptPoisonCorruption;

		// Joins a puddle lying there
		for (TActorIterator<AVaelPoisonPuddle> It(GetWorld()); It; ++It)
		{
			if (*It != this && It->State == EVaelPoisonState::Liquid && It->Covers(Ground, 60.0f))
			{
				It->AddPoison(Ground, 1.0f);
				Destroy();
				return;
			}
		}
		SetActorLocation(Ground);
		AddPoison(Ground, 1.0f);
		State = EVaelPoisonState::Liquid;
		StateTime = 0.0f;
		break;
	}

	case EVaelPoisonState::Liquid:
	{
		for (float& Poison : BlobPoison)
		{
			Poison -= Data->SeepPerSecond * DeltaSeconds;
		}
		HurtCountdown -= DeltaSeconds;
		if (HurtCountdown <= 0.0f)
		{
			HurtCountdown = PoisonHurtInterval;
			HurtThoseInside(Data->PuddleDamagePerSecond * PoisonHurtInterval, 0.0f, false);
		}
		break;
	}

	case EVaelPoisonState::Frozen:
		if (StateTime >= Data->FreezeTime)
		{
			State = EVaelPoisonState::Liquid;
			StateTime = 0.0f;
		}
		break;

	case EVaelPoisonState::Cloud:
	{
		for (float& Poison : BlobPoison)
		{
			Poison = FMath::FInterpTo(Poison, 0.0f, DeltaSeconds, 2.0f);
		}
		HurtCountdown -= DeltaSeconds;
		if (HurtCountdown <= 0.0f)
		{
			HurtCountdown = PoisonHurtInterval;
			HurtThoseInside(Data->CloudDamagePerSecond * PoisonHurtInterval, GetRadius() * (Data->CloudScale - 1.0f), true);
		}
		if (StateTime >= Data->CloudTime)
		{
			Destroy();
			return;
		}
		break;
	}
	}

	// Seeped away blobs go; no blob left, no puddle
	for (int32 Index = BlobPoison.Num() - 1; Index >= 0; --Index)
	{
		if (BlobPoison[Index] < PoisonMinBlob && State != EVaelPoisonState::Cloud && BlobShownRadius[Index] < 5.0f)
		{
			BlobCenters.RemoveAt(Index);
			BlobPoison.RemoveAt(Index);
			BlobShownRadius.RemoveAt(Index);
		}
	}
	if (BlobCenters.IsEmpty() && State != EVaelPoisonState::Cloud)
	{
		Destroy();
		return;
	}
	RefreshLook(DeltaSeconds);
}

void AVaelPoisonPuddle::RefreshLook(float DeltaSeconds)
{
	const UVaelPfuhlnatterData* Data = PoisonData();
	for (int32 Index = 0; Index < BlobCenters.Num(); ++Index)
	{
		const float Wanted = BlobPoison[Index] < PoisonMinBlob ? 0.0f : Data->BlobRadius * FMath::Sqrt(BlobPoison[Index]);
		BlobShownRadius[Index] = FMath::FInterpTo(BlobShownRadius[Index], Wanted, DeltaSeconds, 3.0f);
	}
	if (Material != nullptr)
	{
		// Frozen it turns to pale ice, still and dull; thawed it gets its poison colours back
		const bool bFrozen = State == EVaelPoisonState::Frozen;
		ShownColor = FMath::CInterpTo(ShownColor, bFrozen ? Data->FrozenPoisonColor : ShallowColor, DeltaSeconds, 3.0f);
		Material->SetVectorParameterValue(TEXT("ShallowColor"), ShownColor);
		Material->SetVectorParameterValue(TEXT("DeepColor"), bFrozen ? Data->FrozenPoisonColor * 0.4f : DeepColor);
		Material->SetScalarParameterValue(TEXT("WobbleStrength"), bFrozen ? 0.0f : PoisonWobble);
	}
	RepaintTime += DeltaSeconds;
	if (RepaintTime >= PoisonRepaintInterval)
	{
		RefreshSurface();
	}
	RefreshVapour(DeltaSeconds);
}

void AVaelPoisonPuddle::RefreshSurface()
{
	RepaintTime = 0.0f;
	if (BlobCenters.IsEmpty() || Material == nullptr)
	{
		Surface->SetVisibility(false);
		return;
	}

	// A square over all blobs with room for the poison that joins them, as the water puddles do
	FBox2D Bounds(ForceInit);
	float Height = 0.0f;
	for (int32 Index = 0; Index < BlobCenters.Num(); ++Index)
	{
		const FVector2D Center(BlobCenters[Index]);
		const float Reach = BlobShownRadius[Index] * PoisonFieldReach + 10.0f;
		Bounds += Center - FVector2D(Reach);
		Bounds += Center + FVector2D(Reach);
		Height += BlobCenters[Index].Z;
	}
	const FVector2D Middle = Bounds.GetCenter();
	const float Size = FMath::Max(Bounds.GetSize().GetMax(), 1.0f);
	const FVector2D Origin = Middle - FVector2D(Size * 0.5f);
	Surface->SetWorldLocationAndRotation(FVector(Middle, Height / BlobCenters.Num() + 1.5f), FRotator::ZeroRotator);
	Surface->SetWorldScale3D(FVector(Size / PoisonPlaneSize, Size / PoisonPlaneSize, 1.0f));
	Surface->SetVisibility(true);
	Material->SetVectorParameterValue(TEXT("MaskOrigin"), FLinearColor(Origin.X, Origin.Y, 0.0f, 0.0f));
	Material->SetScalarParameterValue(TEXT("MaskSize"), Size);

	if (Mask == nullptr || Mask->GetResource() == nullptr)
	{
		return;
	}
	const float TexelSize = Size / PoisonMaskResolution;
	uint8* Pixels = new uint8[PoisonMaskResolution * PoisonMaskResolution * 4];
	for (int32 Row = 0; Row < PoisonMaskResolution; ++Row)
	{
		for (int32 Column = 0; Column < PoisonMaskResolution; ++Column)
		{
			const FVector2D Point = Origin + FVector2D(Column + 0.5f, Row + 0.5f) * TexelSize;
			float Field = 0.0f;
			for (int32 Index = 0; Index < BlobCenters.Num(); ++Index)
			{
				// Splashed, not poured: each blob's edge bulges and dents round it in its own pattern
				const FVector2D Offset = Point - FVector2D(BlobCenters[Index]);
				const float Angle = FMath::Atan2(Offset.Y, Offset.X);
				const float Seed = FMath::Frac(BlobCenters[Index].X * 0.0137f + BlobCenters[Index].Y * 0.0291f) * 2.0f * PI;
				const float Ragged = 1.0f + 0.2f * FMath::Sin(3.0f * Angle + Seed) + 0.11f * FMath::Sin(5.0f * Angle + Seed * 2.3f) + 0.06f * FMath::Sin(9.0f * Angle + Seed * 3.7f);
				const float Reach = BlobShownRadius[Index] * PoisonFieldReach * Ragged;
				const float Share = Offset.SizeSquared() / FMath::Max(Reach * Reach, 1.0f);
				Field += FMath::Square(FMath::Max(1.0f - Share, 0.0f));
			}
			// The edge of a lone blob is written as one half, which the material takes as the edge
			const uint8 Value = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Field / (2.0f * PoisonEdgeField) * 255.0f), 0, 255));
			uint8* Pixel = Pixels + (Row * PoisonMaskResolution + Column) * 4;
			Pixel[0] = Value;
			Pixel[1] = Value;
			Pixel[2] = Value;
			Pixel[3] = 255;
		}
	}
	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, PoisonMaskResolution, PoisonMaskResolution);
	Mask->UpdateTextureRegions(0, 1, Region, PoisonMaskResolution * 4, 4, Pixels, [](uint8* Data, const FUpdateTextureRegion2D* Regions)
	{
		delete[] Data;
		delete Regions;
	});
}

void AVaelPoisonPuddle::RefreshVapour(float DeltaSeconds)
{
	const UVaelPfuhlnatterData* Data = PoisonData();
	if (Vapour == nullptr || BlobCenters.IsEmpty())
	{
		return;
	}
	// As many wisps as the puddle is big; none from frozen poison, the cloud takes over when it burns
	const float Radius = GetRadius();
	const int32 Wanted = State == EVaelPoisonState::Liquid
		? FMath::Clamp(FMath::RoundToInt(PI * FMath::Square(Radius / 100.0f) * Data->VapourPerSquareMetre), 1, Data->VapourMax) : 0;
	const FVector Middle = GetMiddle();
	while (WispCards.Num() < Wanted)
	{
		WispCards.Add(Vapour->AddCard(Middle, 1.0f, 0.0f));
		WispStarts.Add(Middle);
		WispAges.Add(FMath::FRand());
		WispSpeeds.Add(FMath::FRandRange(0.18f, 0.32f));
	}
	for (int32 Wisp = 0; Wisp < WispCards.Num(); ++Wisp)
	{
		WispAges[Wisp] += WispSpeeds[Wisp] * DeltaSeconds;
		if (WispAges[Wisp] >= 1.0f)
		{
			// Starts again somewhere over the poison
			WispAges[Wisp] = 0.0f;
			const int32 Blob = FMath::RandRange(0, BlobCenters.Num() - 1);
			const FVector2D Offset = FMath::RandPointInCircle(BlobShownRadius[Blob] * 0.8f);
			WispStarts[Wisp] = BlobCenters[Blob] + FVector(Offset, 5.0f);
		}
		const float Up = WispAges[Wisp];
		const bool bShown = Wisp < Wanted;
		// Rises, swells, fades in and out; thin, never a cloud
		const float Solid = bShown ? 0.16f * FMath::Sin(Up * PI) : 0.0f;
		const FVector Place = WispStarts[Wisp] + FVector(FMath::Sin(Up * 5.0f + Wisp) * 12.0f, FMath::Cos(Up * 4.0f + Wisp) * 12.0f, Up * Data->VapourHeight);
		Vapour->SetCard(WispCards[Wisp], Place - Vapour->GetComponentLocation(), 45.0f + 70.0f * Up, Solid);
	}
	Vapour->Commit();
}

void AVaelPoisonPuddle::HurtThoseInside(float Damage, float ExtraDistance, bool bEveryone)
{
	FVaelSpellHit Hit;
	Hit.Damage = Damage;
	Hit.Element = EVaelElement::Water;
	TArray<AVaelCharacterBase*> Victims;
	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		const bool bCounts = bEveryone || Cast<AVaelCharacter>(*It) != nullptr;
		if (bCounts && !It->IsDefeated() && FMath::Abs(It->GetActorLocation().Z - GetActorLocation().Z) < 300.0f && Covers(It->GetActorLocation(), ExtraDistance))
		{
			Victims.Add(*It);
		}
	}
	for (AVaelCharacterBase* Victim : Victims)
	{
		UVaelCombatStatics::ApplyNatureHit(Victim, Hit, FVector::ZeroVector);
	}
}

bool AVaelPoisonPuddle::Covers(const FVector& Location, float ExtraDistance) const
{
	for (int32 Index = 0; Index < BlobCenters.Num(); ++Index)
	{
		if (FVector::Dist2D(BlobCenters[Index], Location) <= FMath::Max(BlobShownRadius[Index], PoisonData()->BlobRadius * FMath::Sqrt(FMath::Max(BlobPoison[Index], 0.0f)) * 0.8f) + ExtraDistance)
		{
			return true;
		}
	}
	return false;
}

FVector AVaelPoisonPuddle::GetMiddle() const
{
	if (BlobCenters.IsEmpty())
	{
		return GetActorLocation();
	}
	FVector Sum = FVector::ZeroVector;
	for (const FVector& Center : BlobCenters)
	{
		Sum += Center;
	}
	return Sum / BlobCenters.Num();
}

float AVaelPoisonPuddle::GetRadius() const
{
	const FVector Middle = GetMiddle();
	float Radius = 0.0f;
	for (int32 Index = 0; Index < BlobCenters.Num(); ++Index)
	{
		Radius = FMath::Max(Radius, FVector::Dist2D(Middle, BlobCenters[Index]) + PoisonData()->BlobRadius * FMath::Sqrt(FMath::Max(BlobPoison[Index], 0.0f)));
	}
	return Radius;
}

float AVaelPoisonPuddle::Drain(float Amount)
{
	if (State != EVaelPoisonState::Liquid)
	{
		return 0.0f;
	}
	float Got = 0.0f;
	for (float& Poison : BlobPoison)
	{
		const float Take = FMath::Min(Poison, (Amount - Got));
		Poison -= Take;
		Got += Take;
		if (Got >= Amount)
		{
			break;
		}
	}
	return Got;
}

void AVaelPoisonPuddle::Freeze()
{
	if (State == EVaelPoisonState::Liquid || State == EVaelPoisonState::Frozen)
	{
		State = EVaelPoisonState::Frozen;
		StateTime = 0.0f;
	}
}

void AVaelPoisonPuddle::Ignite()
{
	if (State == EVaelPoisonState::Frozen)
	{
		// Fire on ice only thaws it
		State = EVaelPoisonState::Liquid;
		StateTime = 0.0f;
		return;
	}
	if (State != EVaelPoisonState::Liquid)
	{
		return;
	}
	const UVaelPfuhlnatterData* Data = PoisonData();
	State = EVaelPoisonState::Cloud;
	StateTime = 0.0f;
	HurtCountdown = 0.0f;
	const FLinearColor Color = bCorrupted ? Data->CorruptPoisonColor : Data->PoisonColor;
	AVaelPollenBurst::Spawn(GetWorld(), GetMiddle() + FVector(0.0f, 0.0f, 60.0f), GetRadius() * Data->CloudScale, Color, Color * 0.4f + FLinearColor(0.1f, 0.1f, 0.0f), Data->CloudTime);
}

void AVaelPoisonPuddle::Wash(const FVector& Location, float Radius)
{
	if (State != EVaelPoisonState::Liquid)
	{
		return;
	}
	for (float& Poison : BlobPoison)
	{
		Poison *= PoisonData()->WashShare;
	}
}

void AVaelPoisonPuddle::NotifySpellImpact(UWorld* World, const FVector& Location, float Radius, const FVaelSpellHit& Hit)
{
	if (World == nullptr)
	{
		return;
	}
	TArray<AVaelPoisonPuddle*> Touched;
	for (TActorIterator<AVaelPoisonPuddle> It(World); It; ++It)
	{
		if (It->State != EVaelPoisonState::Falling && It->State != EVaelPoisonState::Cloud && It->Covers(Location, Radius))
		{
			Touched.Add(*It);
		}
	}
	for (AVaelPoisonPuddle* Puddle : Touched)
	{
		if (Hit.Status == EVaelStatus::Frozen)
		{
			Puddle->Freeze();
		}
		else if (Hit.Element == EVaelElement::Fire)
		{
			Puddle->Ignite();
		}
		else if (Hit.Element == EVaelElement::Water)
		{
			Puddle->Wash(Location, Radius);
		}
	}
}

AVaelPoisonPuddle* AVaelPoisonPuddle::FindNearest(const UWorld* World, const FVector& Location, float MaxDistance)
{
	AVaelPoisonPuddle* Best = nullptr;
	float BestDistance = MaxDistance;
	for (TActorIterator<AVaelPoisonPuddle> It(World); It; ++It)
	{
		const float Distance = FVector::Dist2D(It->GetMiddle(), Location);
		if (It->State == EVaelPoisonState::Liquid && Distance <= BestDistance)
		{
			Best = *It;
			BestDistance = Distance;
		}
	}
	return Best;
}

int32 AVaelPoisonPuddle::CountPuddles(const UWorld* World)
{
	int32 Count = 0;
	for (TActorIterator<AVaelPoisonPuddle> It(World); It; ++It)
	{
		Count += It->State != EVaelPoisonState::Falling ? 1 : 0;
	}
	return Count;
}
