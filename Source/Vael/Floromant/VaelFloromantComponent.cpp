// Copyright Epic Games, Inc. All Rights Reserved.

#include "Floromant/VaelFloromantComponent.h"
#include "AbilitySystemComponent.h"
#include "Combat/VaelAttributeSet.h"
#include "Combat/VaelCombatStatics.h"
#include "Creatures/VaelCreature.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Floromant/VaelFloromantSettings.h"
#include "Floromant/VaelPlant.h"
#include "Floromant/VaelPlantArmComponent.h"
#include "Floromant/VaelPlantArmData.h"
#include "Floromant/VaelPlantData.h"
#include "Floromant/VaelSeed.h"
#include "Magic/VaelDebrisBurst.h"
#include "Nature/VaelGarden.h"
#include "Nature/VaelHarvestable.h"
#include "Player/VaelCharacter.h"
#include "UI/VaelCombatTextSubsystem.h"
#include "World/VaelGround.h"
#include "World/VaelRegion.h"
#include "World/VaelSafeZone.h"
#include "Vael.h"
#include "VaelAssets.h"

namespace
{
	/** Shortest throw, so a seed never lands at the feet */
	constexpr float MinThrow = 120.0f;

	/** Half angle of the cone the thorn whip hits, in degrees */
	constexpr float WhipHalfAngle = 22.0f;

	/** Pink and turquoise of the Floromant */
	const FLinearColor FloromantPink(0.82f, 0.4f, 0.6f);
	const FLinearColor FloromantTurquoise(0.31f, 0.7f, 0.66f);
}

UVaelFloromantComponent::UVaelFloromantComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

AVaelCharacter* UVaelFloromantComponent::GetCharacter() const
{
	return Cast<AVaelCharacter>(GetOwner());
}

UVaelPlantData* UVaelFloromantComponent::FindPlantData(FName PlantId)
{
	if (PlantId.IsNone())
	{
		return nullptr;
	}
	const FString Folder = UVaelFloromantSettings::Get()->PlantDataFolder;
	const FString Name = FString(TEXT("DA_Pflanze_")) + PlantId.ToString();
	return Cast<UVaelPlantData>(VaelAssets::LoadOptional(FSoftObjectPath(FString::Printf(TEXT("%s/%s.%s"), *Folder, *Name, *Name))));
}

bool UVaelFloromantComponent::GrowsShadowForms() const
{
	const AVaelCharacter* Character = GetCharacter();
	return Character != nullptr && Character->GetPath() == EVaelPath::Shadow;
}

// ---------------------------------------------------------------- Seed belt

int32 UVaelFloromantComponent::GetBeltSize() const
{
	return UVaelFloromantSettings::Get()->SeedBelt.Num();
}

void UVaelFloromantComponent::SelectBelt(int32 Index)
{
	if (Index >= 0 && Index < GetBeltSize())
	{
		BeltSelection = Index;
	}
}

UVaelPlantData* UVaelFloromantComponent::GetBeltPlant(int32 Index) const
{
	const TArray<TSoftObjectPtr<UVaelPlantData>>& Belt = UVaelFloromantSettings::Get()->SeedBelt;
	return Belt.IsValidIndex(Index) ? VaelAssets::LoadOptional(Belt[Index]) : nullptr;
}

FText UVaelFloromantComponent::GetBeltName(int32 Index) const
{
	const UVaelPlantData* Plant = GetBeltPlant(Index);
	return Plant != nullptr ? Plant->GetName(GrowsShadowForms()) : FText::GetEmpty();
}

FLinearColor UVaelFloromantComponent::GetBeltColor(int32 Index) const
{
	const UVaelPlantData* Plant = GetBeltPlant(Index);
	return Plant == nullptr ? FLinearColor::Gray : GrowsShadowForms() ? Plant->ShadowColor : Plant->Color;
}

// ---------------------------------------------------------------- Helpers

bool UVaelFloromantComponent::Use(EVaelHUDAbility Ability, float Cost)
{
	AVaelCharacter* Character = GetCharacter();
	if (Character == nullptr || Character->IsDowned() || GetCooldownLeft(Ability) > 0.0f)
	{
		return false;
	}

	// Nothing grows in the camp: it is a safe zone, like for the spells of the mage
	if (AVaelSafeZone::IsSafe(GetWorld(), Character->GetActorLocation()))
	{
		return false;
	}

	if (Character->GetMana() < Cost)
	{
		UVaelCombatTextSubsystem::PostPickup(Character, NSLOCTEXT("VaelFloromant", "NoSaft", "Zu wenig Saft"), FLinearColor(FColor(158, 143, 125)));
		ReadyTimes.Add(Ability, GetWorld()->GetTimeSeconds() + 0.4f);
		return false;
	}

	Character->GetAbilitySystemComponent()->SetNumericAttributeBase(UVaelAttributeSet::GetManaAttribute(), Character->GetMana() - Cost);
	ReadyTimes.Add(Ability, GetWorld()->GetTimeSeconds() + GetCooldownDuration(Ability));
	return true;
}

FVector UVaelFloromantComponent::GroundTarget(const FVector& AimLocation, float Range) const
{
	const AVaelCharacter* Character = GetCharacter();
	const FVector Feet = Character->GetActorLocation() - FVector(0.0f, 0.0f, Character->GetSimpleCollisionHalfHeight());
	FVector Offset = (AimLocation - Feet) * FVector(1.0f, 1.0f, 0.0f);
	if (Offset.Size() < MinThrow)
	{
		Offset = Character->GetActorForwardVector() * MinThrow;
	}
	FVector Target = Feet + Offset.GetClampedToMaxSize(Range);

	FHitResult GroundHit;
	if (VaelGround::TraceGround(GetWorld(), Target + FVector(0.0f, 0.0f, 300.0f), Target - FVector(0.0f, 0.0f, 600.0f), GroundHit, Character))
	{
		Target = GroundHit.Location;
	}
	return Target;
}

float UVaelFloromantComponent::GetCooldownLeft(EVaelHUDAbility Ability) const
{
	const float* Ready = ReadyTimes.Find(Ability);
	return Ready != nullptr ? FMath::Max(0.0f, *Ready - GetWorld()->GetTimeSeconds()) : 0.0f;
}

float UVaelFloromantComponent::GetCooldownDuration(EVaelHUDAbility Ability) const
{
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	switch (Ability)
	{
	case EVaelHUDAbility::Sow:			return Settings->ThrowCooldown;
	case EVaelHUDAbility::GardenCall:	return Settings->CallCooldown;
	case EVaelHUDAbility::RootGrip:		return Settings->GripCooldown;
	case EVaelHUDAbility::RootLeap:		return Settings->LeapCooldown;
	case EVaelHUDAbility::Overgrowth:	return Settings->OvergrowthCooldown;
	case EVaelHUDAbility::ThornWhip:	return Settings->WhipCooldown;
	default:							return 0.0f;
	}
}

// ---------------------------------------------------------------- Abilities

bool UVaelFloromantComponent::Sow(const FVector& AimLocation)
{
	UVaelPlantData* Plant = GetBeltPlant(BeltSelection);
	AVaelCharacter* Character = GetCharacter();
	if (Plant == nullptr || Character == nullptr || !Use(EVaelHUDAbility::Sow, Plant->SowCost))
	{
		return false;
	}

	const FVector Target = GroundTarget(AimLocation, UVaelFloromantSettings::Get()->ThrowRange);
	const FVector Hand = Character->GetActorLocation() + Character->GetActorForwardVector() * 30.0f + FVector(0.0f, 0.0f, 40.0f);
	return AVaelSeed::Throw(Character, Plant, GrowsShadowForms(), Hand, Target) != nullptr;
}

AVaelPlant* UVaelFloromantComponent::PlantSeed(UVaelPlantData* Plant, bool bShadow, const FVector& GroundLocation, float Yaw)
{
	AVaelCharacter* Character = GetCharacter();
	if (Character == nullptr || Plant == nullptr)
	{
		return nullptr;
	}

	// The oldest plants wither once there would be too many
	Plants.RemoveAll([](const TWeakObjectPtr<AVaelPlant>& Grown) { return !Grown.IsValid() || Grown->IsWithering(); });
	while (Plants.Num() >= UVaelFloromantSettings::Get()->MaxPlants)
	{
		Plants[0]->Wither();
		Plants.RemoveAt(0);
	}

	AVaelPlant* Grown = AVaelPlant::GrowPlant(Plant, Character, GroundLocation, Yaw, bShadow);
	if (Grown != nullptr)
	{
		Plants.Add(Grown);
	}
	return Grown;
}

int32 UVaelFloromantComponent::CallPlants(float Radius, float Duration, bool bGiant, bool bPaySaft)
{
	AVaelCharacter* Character = GetCharacter();
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	const EVaelPath Path = Character->GetPath();
	const FVector Center = Character->GetActorLocation();

	// Who answers: light only answers light, shadow answers shadow and spoils light, twilight and the unsworn answer both
	struct FCandidate
	{
		AVaelPlant* Own = nullptr;
		UVaelPlantData* Data = nullptr;
		FVector Ground = FVector::ZeroVector;
		bool bShadow = false;
		float Distance = 0.0f;
	};
	TArray<FCandidate> Candidates;

	Plants.RemoveAll([](const TWeakObjectPtr<AVaelPlant>& Grown) { return !Grown.IsValid() || Grown->IsWithering(); });
	for (const TWeakObjectPtr<AVaelPlant>& Grown : Plants)
	{
		const float Distance = FVector::Dist2D(Grown->GetActorLocation(), Center);
		if (Distance < Radius)
		{
			Candidates.Add({ Grown.Get(), Grown->GetData(), Grown->GetActorLocation(), Grown->IsShadowForm(), Distance });
		}
	}

	// Plants growing in the world: sown in a garden, or wild like the Glutdistel
	for (TActorIterator<AVaelSownPlant> It(GetWorld()); It; ++It)
	{
		const float Distance = FVector::Dist2D(It->GetActorLocation(), Center);
		UVaelPlantData* Data = Distance < Radius && It->IsGrown() ? FindPlantData(It->GetPlantId()) : nullptr;
		if (Data != nullptr)
		{
			Candidates.Add({ nullptr, Data, It->GetActorLocation(), It->IsShadowForm(), Distance });
		}
	}
	for (TActorIterator<AVaelHarvestable> It(GetWorld()); It; ++It)
	{
		const float Distance = FVector::Dist2D(It->GetActorLocation(), Center);
		UVaelPlantData* Data = Distance < Radius ? FindPlantData(It->GetCompendiumId()) : nullptr;
		if (Data != nullptr)
		{
			Candidates.Add({ nullptr, Data, It->GetActorLocation(), false, Distance });
		}
	}

	Candidates.Sort([](const FCandidate& A, const FCandidate& B) { return A.Distance < B.Distance; });

	int32 NumActing = 0;
	int32 NumSpoiled = 0;
	for (const FCandidate& Candidate : Candidates)
	{
		bool bActAsShadow = Candidate.bShadow;
		if (Path == EVaelPath::Light && Candidate.bShadow)
		{
			continue;
		}
		if (Path == EVaelPath::Shadow && !Candidate.bShadow)
		{
			bActAsShadow = true;
			++NumSpoiled;
		}

		// Each plant takes its Saft; when it runs out the farther plants stay still
		if (bPaySaft)
		{
			if (Character->GetMana() < Candidate.Data->CallCost)
			{
				break;
			}
			Character->GetAbilitySystemComponent()->SetNumericAttributeBase(UVaelAttributeSet::GetManaAttribute(), Character->GetMana() - Candidate.Data->CallCost);
		}

		if (Candidate.Own != nullptr)
		{
			Candidate.Own->Call(Duration, bActAsShadow);
			if (bGiant)
			{
				Candidate.Own->MakeGiant(Duration);
			}
		}
		else
		{
			FVector Ground = Candidate.Ground;
			FHitResult GroundHit;
			if (VaelGround::TraceGround(GetWorld(), Ground + FVector(0.0f, 0.0f, 200.0f), Ground - FVector(0.0f, 0.0f, 400.0f), GroundHit, Character))
			{
				Ground = GroundHit.Location;
			}
			AVaelPlant::ActForWildPlant(Candidate.Data, Character, Ground, bActAsShadow, Duration, bGiant);
		}
		++NumActing;
	}

	// Spoiling light forms seeps corruption into the region
	if (NumSpoiled > 0)
	{
		if (AVaelRegion* Region = AVaelRegion::GetRegionAt(GetWorld(), Center))
		{
			Region->AddCorruption(NumSpoiled * Settings->SpoilCorruption);
		}
	}

	UE_LOG(LogVael, Log, TEXT("'%s' calls the garden: %d plants act%s"), *GetNameSafe(Character), NumActing, NumSpoiled > 0 ? *FString::Printf(TEXT(", %d spoiled"), NumSpoiled) : TEXT(""));
	return NumActing;
}

int32 UVaelFloromantComponent::CallGarden()
{
	if (!Use(EVaelHUDAbility::GardenCall, 0.0f))
	{
		return 0;
	}

	// A ring of pink and turquoise light runs out over the ground
	AVaelCharacter* Character = GetCharacter();
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	for (const FLinearColor& Color : { FloromantPink, FloromantTurquoise })
	{
		FVaelDebris Ring;
		Ring.Count = 18;
		Ring.Spread = 40.0f;
		Ring.Speed = Settings->CallRadius * 1.2f;
		Ring.Lift = 40.0f;
		Ring.Gravity = -60.0f;
		Ring.Drag = 1.5f;
		Ring.Lifetime = 0.9f;
		Ring.MinSize = 4.0f;
		Ring.MaxSize = 8.0f;
		Ring.Color = Color;
		Ring.Glow = 3.0f;
		Ring.bSoft = true;
		Ring.bLandOnGround = false;
		AVaelDebrisBurst::Spawn(Character, Character->GetActorLocation() - FVector(0.0f, 0.0f, 60.0f), Ring);
	}

	return CallPlants(Settings->CallRadius, Settings->CallDuration, false, true);
}

int32 UVaelFloromantComponent::Overgrow()
{
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	if (!Use(EVaelHUDAbility::Overgrowth, Settings->OvergrowthCost))
	{
		return 0;
	}
	return CallPlants(Settings->OvergrowthRadius, Settings->OvergrowthDuration, true, false);
}

bool UVaelFloromantComponent::RootGrip(const FVector& AimLocation)
{
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	if (!Use(EVaelHUDAbility::RootGrip, Settings->GripCost))
	{
		return false;
	}
	return AVaelRootGrip::Spawn(GetCharacter(), GroundTarget(AimLocation, Settings->ThrowRange)) != nullptr;
}

bool UVaelFloromantComponent::RootLeap(const FVector& AimLocation)
{
	AVaelCharacter* Character = GetCharacter();
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	if (Character == nullptr)
	{
		return false;
	}

	// The own plant nearest to where the player aims, within reach
	AVaelPlant* Goal = nullptr;
	float Best = UE_BIG_NUMBER;
	for (const TWeakObjectPtr<AVaelPlant>& Grown : Plants)
	{
		if (Grown.IsValid() && !Grown->IsWithering() && FVector::Dist2D(Grown->GetActorLocation(), Character->GetActorLocation()) < Settings->LeapRange)
		{
			const float Distance = FVector::Dist2D(Grown->GetActorLocation(), AimLocation);
			if (Distance < Best)
			{
				Best = Distance;
				Goal = Grown.Get();
			}
		}
	}
	if (Goal == nullptr || !Use(EVaelHUDAbility::RootLeap, Settings->LeapCost))
	{
		return false;
	}

	// Into the ground here, out of the ground by the plant, on the side facing back to where the leap came from
	const FVector From = Character->GetActorLocation();
	const FVector Back = (From - Goal->GetActorLocation()).GetSafeNormal2D();
	const FVector To = Goal->GetActorLocation() + Back * 90.0f + FVector(0.0f, 0.0f, Character->GetSimpleCollisionHalfHeight());

	FVaelDebris Earth;
	Earth.Count = 14;
	Earth.Spread = 30.0f;
	Earth.Speed = 220.0f;
	Earth.Lift = 380.0f;
	Earth.Lifetime = 0.7f;
	Earth.Color = FLinearColor(0.11f, 0.075f, 0.05f);
	AVaelDebrisBurst::Spawn(Character, From - FVector(0.0f, 0.0f, Character->GetSimpleCollisionHalfHeight()), Earth);

	Character->TeleportTo(To, (-Back).Rotation());
	Character->SetInvulnerableFor(0.4f);
	AVaelDebrisBurst::Spawn(Character, To - FVector(0.0f, 0.0f, Character->GetSimpleCollisionHalfHeight()), Earth);
	return true;
}

bool UVaelFloromantComponent::ThornWhip(const FVector& AimLocation)
{
	AVaelCharacter* Character = GetCharacter();
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	if (Character == nullptr || !Use(EVaelHUDAbility::ThornWhip, 0.0f))
	{
		return false;
	}

	// Lashes the way the player aims; everyone in the narrow cone is hit, the vine reaches the nearest
	FVector Direction = (AimLocation - Character->GetActorLocation()).GetSafeNormal2D();
	Direction = Direction.IsNearlyZero() ? Character->GetActorForwardVector() : Direction;
	const FVector Hand = Character->GetActorLocation() + Direction * 30.0f + FVector(0.0f, 0.0f, 30.0f);
	FVector Tip = Hand + Direction * Settings->WhipRange;
	float Nearest = Settings->WhipRange;

	FVaelSpellHit Hit;
	Hit.Damage = Settings->WhipDamage;
	Hit.Element = EVaelElement::Earth;
	Hit.Knockback = 150.0f;

	for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
	{
		const FVector Offset = (It->GetActorLocation() - Character->GetActorLocation()) * FVector(1.0f, 1.0f, 0.0f);
		const float Distance = Offset.Size();
		const float Angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Offset.GetSafeNormal(), Direction)));
		if (!It->IsDead() && !It->IsOnPlayerSide() && Distance < Settings->WhipRange + It->GetSimpleCollisionRadius() && Angle < WhipHalfAngle)
		{
			UVaelCombatStatics::ApplySpellHit(Character, *It, Hit, Offset);
			if (Distance < Nearest)
			{
				Nearest = Distance;
				Tip = It->GetActorLocation();
			}
		}
	}

	AVaelThornWhip::Lash(Character, Hand, Tip);
	return true;
}

int32 UVaelFloromantComponent::GetNumPlants() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AVaelPlant>& Plant : Plants)
	{
		Count += Plant.IsValid() && !Plant->IsWithering() ? 1 : 0;
	}
	return Count;
}

void UVaelFloromantComponent::WitherAll()
{
	ClearHands();

	for (const TWeakObjectPtr<AVaelPlant>& Plant : Plants)
	{
		if (Plant.IsValid())
		{
			Plant->Wither();
		}
	}
	Plants.Reset();
}

// ---------------------------------------------------------------- Plant arms

int32 UVaelFloromantComponent::GetHandBeltSize() const
{
	return UVaelFloromantSettings::Get()->HandBelt.Num();
}

UVaelPlantArmData* UVaelFloromantComponent::GetHandBeltPlant(int32 Index) const
{
	const TArray<TSoftObjectPtr<UVaelPlantArmData>>& Belt = UVaelFloromantSettings::Get()->HandBelt;
	return Belt.IsValidIndex(Index) ? VaelAssets::LoadOptional(Belt[Index]) : nullptr;
}

void UVaelFloromantComponent::PageHandBelt(int32 Step)
{
	const int32 Size = GetHandBeltSize();
	if (Size == 0)
	{
		return;
	}
	HandBeltSelection = (HandBeltSelection + Step % Size + Size) % Size;

	// Shows which plant is picked now
	if (AVaelCharacter* Character = GetCharacter())
	{
		if (const UVaelPlantArmData* Plant = GetHandBeltPlant(HandBeltSelection))
		{
			UVaelCombatTextSubsystem::PostPickup(Character, Plant->GetDisplayName(GrowsShadowForms()), Plant->GetColor(GrowsShadowForms()));
		}
	}
}

void UVaelFloromantComponent::PutOnHand(bool bLeft)
{
	UVaelPlantArmData* Plant = GetHandBeltPlant(HandBeltSelection);
	AVaelCharacter* Character = GetCharacter();
	if (Plant == nullptr || Character == nullptr || Character->IsDowned())
	{
		return;
	}
	GetHand(bLeft)->SetPlant(Plant);
}

UVaelPlantArmComponent* UVaelFloromantComponent::GetHand(bool bLeft)
{
	TObjectPtr<UVaelPlantArmComponent>& Hand = bLeft ? LeftHand : RightHand;
	if (Hand == nullptr && GetOwner() != nullptr)
	{
		Hand = NewObject<UVaelPlantArmComponent>(GetOwner(), bLeft ? TEXT("PlantArmLeft") : TEXT("PlantArmRight"));
		Hand->SetHand(bLeft);
		Hand->RegisterComponent();
	}
	return Hand;
}

void UVaelFloromantComponent::SetHandFiring(bool bLeft, bool bFiring)
{
	// A bare hand has nothing to fire; it doesn't need to exist for that
	UVaelPlantArmComponent* Hand = bLeft ? LeftHand : RightHand;
	if (Hand != nullptr)
	{
		Hand->SetFiring(bFiring);
	}
}

bool UVaelFloromantComponent::IsFiring() const
{
	return (LeftHand != nullptr && LeftHand->IsFiring()) || (RightHand != nullptr && RightHand->IsFiring());
}

float UVaelFloromantComponent::GetStance(bool bLeft) const
{
	const UVaelPlantArmComponent* Hand = bLeft ? LeftHand : RightHand;
	return Hand != nullptr ? Hand->GetStance() : 0.0f;
}

float UVaelFloromantComponent::GetMoveSpeedScale() const
{
	const float Stance = FMath::Max(GetStance(true), GetStance(false));
	return FMath::Lerp(1.0f, UVaelFloromantSettings::Get()->FiringMoveSpeedScale, Stance);
}

void UVaelFloromantComponent::ClearHands()
{
	for (UVaelPlantArmComponent* Hand : { LeftHand.Get(), RightHand.Get() })
	{
		if (Hand != nullptr)
		{
			Hand->Clear();
		}
	}
}

void UVaelFloromantComponent::ShowNotBuilt(const FText& Name) const
{
	if (AVaelCharacter* Character = GetCharacter())
	{
		UVaelCombatTextSubsystem::PostPickup(Character, FText::Format(NSLOCTEXT("VaelFloromant", "NotBuilt", "{0} (kommt noch)"), Name), FLinearColor(FColor(158, 143, 125)));
	}
	UE_LOG(LogVael, Log, TEXT("'%s': %s is not built yet"), *GetNameSafe(GetOwner()), *Name.ToString());
}

void UVaelFloromantComponent::RootTower(bool bLeft)
{
	ShowNotBuilt(bLeft ? NSLOCTEXT("VaelFloromant", "TowerLeft", "Turm links") : NSLOCTEXT("VaelFloromant", "TowerRight", "Turm rechts"));
}

void UVaelFloromantComponent::CrossHands(bool bCharged)
{
	ShowNotBuilt(bCharged ? NSLOCTEXT("VaelFloromant", "CrossCharged", "Kreuzen, aufgeladen") : NSLOCTEXT("VaelFloromant", "Cross", "Kreuzschuss"));
}

void UVaelFloromantComponent::ReloadHands()
{
	ShowNotBuilt(NSLOCTEXT("VaelFloromant", "Reload", "Keimpelz nachladen"));
}

void UVaelFloromantComponent::OpenHerbarium()
{
	ShowNotBuilt(NSLOCTEXT("VaelFloromant", "Herbarium", "Herbarium"));
}
