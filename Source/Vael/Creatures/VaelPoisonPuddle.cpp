// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelPoisonPuddle.h"
#include "Combat/VaelCombatStatics.h"
#include "Combat/VaelCharacterBase.h"
#include "Components/StaticMeshComponent.h"
#include "Creatures/VaelCreatureData.h"
#include "Creatures/VaelPollenBurst.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/VaelCharacter.h"
#include "World/VaelRegion.h"

namespace
{
	/** Seconds between two poison damage steps */
	constexpr float PoisonHurtInterval = 0.5f;

	/** Size of the engine plane the blobs are drawn on, and where the material puts the rim (share of it from the middle) */
	constexpr float PoisonPlaneSize = 100.0f;
	constexpr float PoisonRimShare = 0.42f;

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

void AVaelPoisonPuddle::AddPoison(const FVector& GroundLocation, float Amount)
{
	const UVaelPfuhlnatterData* Data = PoisonData();
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

	UStaticMeshComponent* Blob = NewObject<UStaticMeshComponent>(this);
	Blob->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")));
	Blob->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Blob->SetCastShadow(false);
	Blob->SetupAttachment(RootComponent);
	Blob->RegisterComponent();
	if (Material == nullptr)
	{
		if (UMaterialInterface* Base = Data->PuddleMaterial.LoadSynchronous())
		{
			Material = UMaterialInstanceDynamic::Create(Base, this);
		}
	}
	if (Material != nullptr)
	{
		Blob->SetMaterial(0, Material);
	}
	// Each blob a little higher than the one before, so overlapping blobs don't flicker
	Blob->SetWorldLocation(GroundLocation + FVector(0.0f, 0.0f, 2.0f + 0.15f * Blobs.Num()));
	Blob->SetWorldScale3D(FVector(0.01f));
	Blobs.Add(Blob);
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
		const FLinearColor Color = bCorrupted ? Data->CorruptPoisonColor : Data->PoisonColor;
		AVaelPollenBurst::Spawn(GetWorld(), Ground + FVector(0.0f, 0.0f, 25.0f), 80.0f, Color, Color * 0.5f, 0.6f);

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
		ShownColor = Color;
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
			Blobs[Index]->DestroyComponent();
			Blobs.RemoveAt(Index);
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
	for (int32 Index = 0; Index < Blobs.Num(); ++Index)
	{
		const float Wanted = BlobPoison[Index] < PoisonMinBlob ? 0.0f : Data->BlobRadius * FMath::Sqrt(BlobPoison[Index]);
		BlobShownRadius[Index] = FMath::FInterpTo(BlobShownRadius[Index], Wanted, DeltaSeconds, 3.0f);
		Blobs[Index]->SetWorldScale3D(FVector(FMath::Max(BlobShownRadius[Index], 1.0f) / (PoisonPlaneSize * PoisonRimShare), FMath::Max(BlobShownRadius[Index], 1.0f) / (PoisonPlaneSize * PoisonRimShare), 1.0f));
	}
	if (Material != nullptr)
	{
		const FLinearColor Wanted = State == EVaelPoisonState::Frozen ? Data->FrozenPoisonColor : (bCorrupted ? Data->CorruptPoisonColor : Data->PoisonColor);
		ShownColor = FMath::CInterpTo(ShownColor, Wanted, DeltaSeconds, 3.0f);
		Material->SetVectorParameterValue(TEXT("Farbe"), ShownColor);
		Material->SetScalarParameterValue(TEXT("Leuchten"), State == EVaelPoisonState::Frozen ? 0.05f : (State == EVaelPoisonState::Cloud ? 1.2f : 0.3f));
		Material->SetScalarParameterValue(TEXT("Rauheit"), State == EVaelPoisonState::Frozen ? 0.45f : 0.04f);
	}
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
