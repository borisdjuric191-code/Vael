// Copyright Epic Games, Inc. All Rights Reserved.

#include "Floromant/VaelFloromantComponent.h"
#include "AbilitySystemComponent.h"
#include "Combat/VaelAttributeSet.h"
#include "Combat/VaelCombatStatics.h"
#include "Creatures/VaelCreature.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Floromant/VaelFloromantSettings.h"
#include "Floromant/VaelFootRootsComponent.h"
#include "Floromant/VaelGlimmDome.h"
#include "Floromant/VaelRoseHeart.h"
#include "Floromant/VaelPlant.h"
#include "Floromant/VaelPlantTower.h"
#include "Floromant/VaelRootRunner.h"
#include "Floromant/VaelSymbiosis.h"
#include "Floromant/VaelSymbiosisKnotComponent.h"
#include "Magic/VaelFormulaAbility.h"
#include "TimerManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Magic/VaelGameplayTags.h"
#include "World/VaelLocalWeather.h"
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
	// Only the Keimpelz pelt grows back over time
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.2f;
}

void UVaelFloromantComponent::BeginPlay()
{
	Super::BeginPlay();
	Keimpelz = UVaelFloromantSettings::Get()->KeimpelzMax;
}

void UVaelFloromantComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const AVaelCharacter* Character = GetCharacter();
	if (Character != nullptr && Character->IsFloromant() && !Character->IsDowned())
	{
		AddKeimpelz(UVaelFloromantSettings::Get()->KeimpelzGrowPerSecond * DeltaTime);
	}

	TickFlight(DeltaTime);
	UpdateCrossKnot(DeltaTime);
	TickShield(DeltaTime);
}

float UVaelFloromantComponent::GetShieldStance(bool bLeft) const
{
	const UVaelPlantArmComponent* Hand = FindHand(bLeft);
	return Hand != nullptr ? Hand->GetShieldStance() : 0.0f;
}

void UVaelFloromantComponent::TickShield(float DeltaTime)
{
	AVaelCharacter* Character = GetCharacter();
	UVaelPlantArmComponent* Raised = nullptr;
	bool bOtherFires = false;
	for (const bool bLeft : { true, false })
	{
		UVaelPlantArmComponent* Hand = FindHand(bLeft);
		if (Hand != nullptr && Hand->GetPlant() != nullptr && Hand->GetPlant()->bShield && Hand->GetShieldStance() > 0.6f)
		{
			Raised = Hand;
			bOtherFires = GetStance(!bLeft) > 0.5f;
		}
	}

	// The dome stands while the hand is up and the Saft lasts; nothing grows in the camp
	const bool bCanHold = Raised != nullptr && Character != nullptr && Character->IsFloromant() && !Character->IsDowned()
		&& !AVaelSafeZone::IsSafe(GetWorld(), Character->GetActorLocation());
	const UVaelPlantArmData* Shield = Raised != nullptr ? Raised->GetPlant() : nullptr;
	const float Cost = Shield != nullptr ? Shield->ShieldSaftPerSecond * DeltaTime : 0.0f;
	if (!bCanHold || Character->GetMana() < Cost)
	{
		if (AVaelGlimmDome* Dome = ShieldDome.Get())
		{
			Dome->Lower();
		}
		ShieldDome = nullptr;
		return;
	}
	Character->GetAbilitySystemComponent()->ApplyModToAttribute(UVaelAttributeSet::GetManaAttribute(), EGameplayModOp::Additive, -Cost);

	AVaelGlimmDome* Dome = ShieldDome.Get();
	if (Dome == nullptr)
	{
		Dome = AVaelGlimmDome::Raise(Character, Character, Shield, EVaelSymbiosis::None, Shield->ShieldRadius, Character->GetPath() == EVaelPath::Shadow);
		ShieldDome = Dome;
	}

	// Anchored (Boris 2026-10-26): while the other hand fires, or rooted, the dome is bigger and holds and burns better
	if (Dome != nullptr)
	{
		const float Anchored = bOtherFires || IsRooted() ? 1.0f : 0.0f;
		Dome->SetAnchored(Anchored);
		Dome->SetTargetRadius(Shield->ShieldRadius * FMath::Lerp(1.0f, Shield->AnchoredRadiusScale, Anchored));
	}
}

// ---------------------------------------------------------------- Flight

void UVaelFloromantComponent::ToggleRise()
{
	const AVaelCharacter* Character = GetCharacter();
	if (Character == nullptr || !Character->IsFloromant() || Character->IsDowned())
	{
		return;
	}
	bRiseWanted = !bRiseWanted;
}

void UVaelFloromantComponent::SetRiseHeld(bool bHeld)
{
	const AVaelCharacter* Character = GetCharacter();
	bRiseWanted = bHeld && Character != nullptr && Character->IsFloromant() && !Character->IsDowned();
}

float UVaelFloromantComponent::GetHandPower() const
{
	return FMath::Lerp(1.0f, UVaelFloromantSettings::Get()->RootedHandPower, RootGrowth);
}

void UVaelFloromantComponent::TickFlight(float DeltaTime)
{
	AVaelCharacter* Character = GetCharacter();
	if (Character == nullptr)
	{
		return;
	}

	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	const bool bWasRooted = IsRooted();

	if (!Character->IsFloromant() || Character->IsDowned())
	{
		bRiseWanted = false;
		Rise = 0.0f;
		StillFiringTime = 0.0f;
		RootGrowth = 0.0f;
		UprootTimeLeft = -1.0f;
		WingWetness = 0.0f;
	}
	else
	{
		const float Stance = FMath::Max(GetStance(true), GetStance(false));
		UAbilitySystemComponent* AbilitySystem = Character->GetAbilitySystemComponent();

		// Firing brings her down: fire power is on the ground. Up high costs Saft; without it she sinks back
		if (Stance > 0.3f)
		{
			bRiseWanted = false;
		}
		if (bRiseWanted)
		{
			const float Cost = Settings->RiseSaftPerSecond * DeltaTime;
			if (Character->GetMana() < Cost)
			{
				bRiseWanted = false;
			}
			else if (AbilitySystem != nullptr && Cost > 0.0f)
			{
				AbilitySystem->ApplyModToAttribute(UVaelAttributeSet::GetManaAttribute(), EGameplayModOp::Additive, -Cost);
			}
		}
		Rise = FMath::FInterpConstantTo(Rise, bRiseWanted ? 1.0f : 0.0f, DeltaTime, 1.0f / Settings->RiseTime);

		// Standing on the spot while a hand fires, her feet take root; moving, darting or ceasing fire pulls them out again
		const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
		const bool bWantsToMove = Movement != nullptr && Movement->GetCurrentAcceleration().SizeSquared() > 1.0f;
		const bool bLetGo = Stance < 0.5f || bWantsToMove || Character->IsDodging();
		if (RootGrowth <= 0.0f)
		{
			const bool bStill = Stance >= 0.9f && !bLetGo && Character->GetVelocity().Size2D() < 40.0f;
			StillFiringTime = bStill ? StillFiringTime + DeltaTime : 0.0f;
			if (StillFiringTime >= Settings->RootDelay)
			{
				RootGrowth = KINDA_SMALL_NUMBER;
				UprootTimeLeft = -1.0f;
			}
		}
		else
		{
			if (bLetGo && UprootTimeLeft < 0.0f)
			{
				UprootTimeLeft = Settings->UprootTime;
			}
			if (UprootTimeLeft >= 0.0f)
			{
				UprootTimeLeft -= DeltaTime;
				RootGrowth = FMath::Max(KINDA_SMALL_NUMBER, RootGrowth - DeltaTime / FMath::Max(Settings->UprootTime, 0.05f));
				if (UprootTimeLeft <= 0.0f)
				{
					RootGrowth = 0.0f;
					StillFiringTime = 0.0f;
					UprootTimeLeft = -1.0f;
				}
			}
			else
			{
				RootGrowth = FMath::Min(1.0f, RootGrowth + DeltaTime / Settings->RootGrowTime);
			}
		}

		// Rain soaks the wings, and so does water a spell left on her; they dry slowly
		const AVaelRegion* Region = AVaelRegion::GetRegionAt(GetWorld(), Character->GetActorLocation());
		const bool bRain = (Region != nullptr && (Region->GetWeather() == EVaelWeather::Rain || Region->GetWeather() == EVaelWeather::Storm))
			|| AVaelLocalWeather::IsRainingAt(GetWorld(), Character->GetActorLocation());
		const bool bWet = bRain || (AbilitySystem != nullptr && AbilitySystem->HasMatchingGameplayTag(VaelTags::Status_Wet));
		WingWetness = FMath::FInterpConstantTo(WingWetness, bWet ? 1.0f : 0.0f, DeltaTime, 1.0f / (bWet ? Settings->WingSoakTime : Settings->WingDryTime));
	}

	// Rooted, the earth gives her Saft
	if (bWasRooted != IsRooted() && Character->IsFloromant())
	{
		Character->SetManaRegen(Settings->SaftPerSecond + (IsRooted() ? Settings->RootedSaftPerSecond : 0.0f));
	}

	UpdateFootRoots();
}

void UVaelFloromantComponent::UpdateFootRoots()
{
	if (FootRoots == nullptr)
	{
		if (!IsRooted() || GetOwner() == nullptr)
		{
			return;
		}
		FootRoots = NewObject<UVaelFootRootsComponent>(GetOwner(), TEXT("FootRoots"));
		FootRoots->SetupAttachment(GetOwner()->GetRootComponent());
		FootRoots->RegisterComponent();
	}

	AVaelCharacter* Character = GetCharacter();
	FootRoots->SetGrowth(Character != nullptr ? Character->GetMesh() : nullptr, RootGrowth);
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
	case EVaelHUDAbility::HandLeft:
	case EVaelHUDAbility::HandRight:	return Settings->TowerRegrowTime;
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

int32 UVaelFloromantComponent::CallPlants(float Radius, float Duration, bool bGiant, bool bPaySaft, bool bRootWave)
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

		FVector Ground = Candidate.Ground;
		FHitResult GroundHit;
		if (VaelGround::TraceGround(GetWorld(), Ground + FVector(0.0f, 0.0f, 200.0f), Ground - FVector(0.0f, 0.0f, 400.0f), GroundHit, Character))
		{
			Ground = GroundHit.Location;
		}

		TWeakObjectPtr<AVaelPlant> Own = Candidate.Own;
		TWeakObjectPtr<AVaelCharacter> WeakCharacter = Character;
		UVaelPlantData* Data = Candidate.Data;
		const bool bOwn = Candidate.Own != nullptr;
		auto Act = [Own, WeakCharacter, Data, bOwn, Ground, bActAsShadow, Duration, bGiant]()
		{
			if (bOwn)
			{
				if (AVaelPlant* Plant = Own.Get())
				{
					Plant->Call(Duration, bActAsShadow);
					if (bGiant)
					{
						Plant->MakeGiant(Duration);
					}
				}
			}
			else if (AVaelCharacter* Caller = WeakCharacter.Get())
			{
				AVaelPlant::ActForWildPlant(Data, Caller, Ground, bActAsShadow, Duration, bGiant);
			}
		};

		// Through the root net the call runs under the ground from her feet, and the plant acts when the root reaches it
		if (bRootWave)
		{
			AVaelRootRunner::Run(Character, GetFeet(), Ground, Settings->RootWaveSpeed, bActAsShadow ? Data->ShadowColor : Data->Color, Act);
		}
		else
		{
			Act();
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
	// Rooted, the call runs through the root net: farther, plant by plant as the roots reach them, and the towers fire at once
	const bool bRootNet = IsRooted();
	const float Radius = Settings->OvergrowthRadius * (bRootNet ? Settings->RootNetReach : 1.0f);
	const int32 NumActing = CallPlants(Radius, Settings->OvergrowthDuration, true, false, bRootNet);
	if (!bRootNet)
	{
		return NumActing;
	}

	AVaelCharacter* Character = GetCharacter();
	int32 NumTowers = 0;
	for (TActorIterator<AVaelPlantTower> It(GetWorld()); It; ++It)
	{
		if (!It->IsStanding() || It->GetGardener() != Character || FVector::Dist2D(It->GetActorLocation(), Character->GetActorLocation()) > Radius)
		{
			continue;
		}
		TWeakObjectPtr<AVaelPlantTower> Tower = *It;
		const float Power = Settings->RootWaveTowerPower;
		AVaelRootRunner::Run(Character, GetFeet(), It->GetActorLocation(), Settings->RootWaveSpeed, FloromantTurquoise, [Tower, Power]()
		{
			if (AVaelPlantTower* Reached = Tower.Get())
			{
				Reached->FireNow(Power);
			}
		});
		++NumTowers;
	}
	UE_LOG(LogVael, Log, TEXT("'%s' calls through the root net: %d plants, %d towers"), *GetNameSafe(Character), NumActing, NumTowers);
	return NumActing + NumTowers;
}

FVector UVaelFloromantComponent::GetFeet() const
{
	const AVaelCharacter* Character = GetCharacter();
	return Character != nullptr ? Character->GetActorLocation() - FVector(0.0f, 0.0f, Character->GetSimpleCollisionHalfHeight()) : FVector::ZeroVector;
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
	// An arm still growing back after a tower takes the new plant once it is back
	UVaelPlantArmComponent* Hand = GetHand(bLeft);
	if (Hand->IsRegrowing())
	{
		Hand->SetRegrowPlant(Plant);
		return;
	}
	Hand->SetPlant(Plant);
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
	// Rooted she stands anchored; up high she flies a little faster
	if (IsRooted())
	{
		return 0.0f;
	}
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	const float Stance = FMath::Max(GetStance(true), GetStance(false));
	return FMath::Lerp(1.0f, Settings->FiringMoveSpeedScale, Stance) * FMath::Lerp(1.0f, Settings->RiseMoveSpeedScale, Rise);
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
	// She sticks the plant of the hand into the ground ahead; it grows into a tower, the arm grows back meanwhile
	AVaelCharacter* Character = GetCharacter();
	UVaelPlantArmComponent* Hand = FindHand(bLeft);
	UVaelPlantArmData* Plant = Hand != nullptr && !Hand->IsRegrowing() ? Hand->GetPlant() : nullptr;
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	if (Character == nullptr || Plant == nullptr)
	{
		if (Character != nullptr && Hand != nullptr && Hand->IsRegrowing())
		{
			UVaelCombatTextSubsystem::PostPickup(Character, NSLOCTEXT("VaelFloromant", "Regrowing", "Der Arm wächst noch nach"), FLinearColor(FColor(158, 143, 125)));
		}
		return;
	}
	const EVaelHUDAbility Ability = bLeft ? EVaelHUDAbility::HandLeft : EVaelHUDAbility::HandRight;
	if (!Use(Ability, Settings->TowerSaftCost))
	{
		return;
	}

	const FVector Ahead = Character->GetActorForwardVector().GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Ahead) * (bLeft ? -35.0f : 35.0f);
	const FVector Ground = GroundTarget(Character->GetActorLocation() + Ahead * Settings->TowerPlantDistance + Side, Settings->TowerPlantDistance + 60.0f);
	AVaelPlantTower* Tower = AVaelPlantTower::PlantTower(Character, Plant, Ground);
	if (Tower == nullptr)
	{
		return;
	}
	Hand->PlantAway(Settings->TowerRegrowTime);

	// Only so many stand at once: the oldest withers
	Towers.RemoveAll([](const TWeakObjectPtr<AVaelPlantTower>& Standing) { return !Standing.IsValid() || !Standing->IsStanding(); });
	Towers.Add(Tower);
	while (Towers.Num() > Settings->MaxTowers)
	{
		Towers[0]->Wither();
		Towers.RemoveAt(0);
	}
	UE_LOG(LogVael, Log, TEXT("'%s' plants a tower of %s"), *GetNameSafe(Character), *GetNameSafe(Plant));
}

int32 UVaelFloromantComponent::GetNumTowers() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AVaelPlantTower>& Standing : Towers)
	{
		Count += Standing.IsValid() && Standing->IsStanding() ? 1 : 0;
	}
	return Count;
}

void UVaelFloromantComponent::CrossHands(bool bCharged)
{
	// The arms of both firing hands grow into each other: the pair acts at the player, out of a knot between the hands
	AVaelCharacter* Character = GetCharacter();
	UVaelPlantArmComponent* Left = FindHand(true);
	UVaelPlantArmComponent* Right = FindHand(false);
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	if (Character == nullptr || Left == nullptr || Right == nullptr || Left->GetPlant() == nullptr || Right->GetPlant() == nullptr
		|| Left->IsRegrowing() || Right->IsRegrowing())
	{
		return;
	}
	const EVaelSymbiosis Kind = VaelSymbiosis::Find(Left->GetPlant(), Right->GetPlant());
	if (Kind == EVaelSymbiosis::None)
	{
		UVaelCombatTextSubsystem::PostPickup(Character, NSLOCTEXT("VaelFloromant", "NoPair", "Diese beiden wachsen nicht zusammen"), FLinearColor(FColor(158, 143, 125)));
		return;
	}

	// Two Rußrosen (Boris 2026-10-26): both roses wander from the shoulders, grow into one before her and shoot their heart
	if (Kind == EVaelSymbiosis::Rosenherz)
	{
		ShootRoseHeart(Left, Right);
		return;
	}
	if (Left->GetBarShots() < Settings->CrossBarShots || Right->GetBarShots() < Settings->CrossBarShots)
	{
		UVaelCombatTextSubsystem::PostPickup(Character, NSLOCTEXT("VaelFloromant", "BarsEmpty", "Pflanzenleisten zu leer"), FLinearColor(FColor(158, 143, 125)));
		return;
	}
	if (!Use(EVaelHUDAbility::Placeholder, bCharged ? Settings->CrossChargedSaftCost : Settings->CrossSaftCost))
	{
		return;
	}
	Left->RefillBar(-Settings->CrossBarShots);
	Right->RefillBar(-Settings->CrossBarShots);

	FVector Aim = UVaelFormulaAbility::GetAimDirection(Character);
	Aim.Z = 0.0f;
	Aim = Aim.IsNearlyZero() ? Character->GetActorForwardVector() : Aim.GetSafeNormal();
	const bool bShadow = Character->GetPath() == EVaelPath::Shadow;
	const FVector Knot = (Left->GetHandLocation() + Right->GetHandLocation()) * 0.5f + Aim * 45.0f;
	VaelSymbiosis::Fire(Character, Kind, Left->GetPlant(), Right->GetPlant(), Knot, Aim, GetHandPower(), bCharged ? 1.0f : 0.0f, bShadow);

	// The knot stays a moment between the hands
	if (CrossKnot == nullptr)
	{
		CrossKnot = NewObject<UVaelSymbiosisKnotComponent>(GetOwner(), TEXT("CrossKnot"));
		CrossKnot->SetupAttachment(GetOwner()->GetRootComponent());
		CrossKnot->RegisterComponent();
	}
	CrossKnot->Setup(Left->GetPlant(), Right->GetPlant(), VaelSymbiosis::GetColor(Kind), bShadow);
	CrossKnot->Flash();
	CrossShowLeft = Settings->CrossShowTime;
	CrossAim = Aim;
	UVaelCombatTextSubsystem::PostPickup(Character, VaelSymbiosis::GetName(Kind), VaelSymbiosis::GetColor(Kind));
}

void UVaelFloromantComponent::ShootRoseHeart(UVaelPlantArmComponent* Left, UVaelPlantArmComponent* Right)
{
	AVaelCharacter* Character = GetCharacter();
	const UVaelPlantArmData* Rose = Left->GetPlant();
	if (!Left->IsRoseReady() || !Right->IsRoseReady())
	{
		UVaelCombatTextSubsystem::PostPickup(Character, NSLOCTEXT("VaelFloromant", "RosesGrowing", "Die Rosen wachsen noch nach"), FLinearColor(FColor(158, 143, 125)));
		return;
	}
	if (!Use(EVaelHUDAbility::Placeholder, Rose->HeartSaftCost))
	{
		return;
	}

	FVector Aim = UVaelFormulaAbility::GetAimDirection(Character);
	Aim.Z = 0.0f;
	Aim = Aim.IsNearlyZero() ? Character->GetActorForwardVector() : Aim.GetSafeNormal();
	const FVector Merge = Character->GetActorLocation() + Aim * 80.0f + FVector(0.0f, 0.0f, 50.0f);
	Left->SendRoseTo(Merge);
	Right->SendRoseTo(Merge);

	// Once the roses have met, the heart flies: to a fallen ally first, else to the one hurt most in front; alone back to her
	AVaelCharacter* Ally = VaelRoseCare::FindHurtAlly(Character, Character->GetActorLocation(), 1800.0f, false, true, Aim);
	TWeakObjectPtr<AVaelCharacter> WeakCharacter = Character;
	TWeakObjectPtr<AVaelCharacter> WeakAlly = Ally;
	TWeakObjectPtr<const UVaelPlantArmData> WeakRose = Rose;
	const bool bShadow = Character->GetPath() == EVaelPath::Shadow;
	FTimerHandle Timer;
	Character->GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([WeakCharacter, WeakAlly, WeakRose, Merge, Aim, bShadow]()
	{
		AVaelCharacter* Gardener = WeakCharacter.Get();
		const UVaelPlantArmData* Data = WeakRose.Get();
		if (Gardener == nullptr || Data == nullptr)
		{
			return;
		}
		AVaelRoseHeart::Shoot(Gardener, Merge, WeakAlly.Get(), Aim, Data->HeartHealShare, Data->HeartReviveShare, Data->HeartSpeed,
			bShadow ? Data->BloodColor : Data->HeartColor, VaelAssets::LoadOptional(Data->ShoulderMesh), Data->ShoulderRoseSize * 1.6f);
	}), 0.35f, false);
	UVaelCombatTextSubsystem::PostPickup(Character, VaelSymbiosis::GetName(EVaelSymbiosis::Rosenherz), VaelSymbiosis::GetColor(EVaelSymbiosis::Rosenherz));
}

void UVaelFloromantComponent::UpdateCrossKnot(float DeltaTime)
{
	if (CrossKnot == nullptr || CrossShowLeft <= 0.0f)
	{
		return;
	}
	CrossShowLeft -= DeltaTime;
	const UVaelPlantArmComponent* Left = FindHand(true);
	const UVaelPlantArmComponent* Right = FindHand(false);
	if (CrossShowLeft <= 0.0f || Left == nullptr || Right == nullptr)
	{
		CrossKnot->Clear();
		CrossShowLeft = 0.0f;
		return;
	}

	// The vines shoot together in the first moment and draw back at the end
	const float Total = UVaelFloromantSettings::Get()->CrossShowTime;
	const float Age = Total - CrossShowLeft;
	const float Grow = FMath::Min(1.0f, Age / 0.15f) * FMath::Min(1.0f, CrossShowLeft / 0.25f);
	const FVector Knot = (Left->GetHandLocation() + Right->GetHandLocation()) * 0.5f + CrossAim * 40.0f;
	CrossKnot->Update(Left->GetHandLocation(), Right->GetHandLocation(), Knot, Grow);
}

float UVaelFloromantComponent::GetKeimpelzFraction() const
{
	return FMath::Clamp(Keimpelz / FMath::Max(UVaelFloromantSettings::Get()->KeimpelzMax, 1.0f), 0.0f, 1.0f);
}

void UVaelFloromantComponent::AddKeimpelz(float Amount)
{
	Keimpelz = FMath::Clamp(Keimpelz + Amount, 0.0f, UVaelFloromantSettings::Get()->KeimpelzMax);
}

int32 UVaelFloromantComponent::ReloadHands()
{
	AVaelCharacter* Character = GetCharacter();
	if (Character == nullptr || Character->IsDowned())
	{
		return 0;
	}

	// What each hand lacks, as far as the pelt reaches; the hand that is emptier first
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	TArray<UVaelPlantArmComponent*> Hands;
	for (UVaelPlantArmComponent* Hand : { LeftHand.Get(), RightHand.Get() })
	{
		if (Hand != nullptr && Hand->GetMissingShots() > 0 && !Hand->IsReloading())
		{
			Hands.Add(Hand);
		}
	}
	Hands.Sort([](const UVaelPlantArmComponent& A, const UVaelPlantArmComponent& B) { return A.GetMissingShots() > B.GetMissingShots(); });

	int32 Reloaded = 0;
	for (UVaelPlantArmComponent* Hand : Hands)
	{
		const int32 Affordable = Settings->KeimpelzPerShot > 0.0f ? FMath::FloorToInt32(Keimpelz / Settings->KeimpelzPerShot) : Hand->GetMissingShots();
		const int32 Shots = FMath::Min(Hand->GetMissingShots(), Affordable);
		if (Shots <= 0)
		{
			break;
		}
		Keimpelz -= Shots * Settings->KeimpelzPerShot;
		Hand->StartReload(Shots, Settings->ReloadTime);
		Reloaded += Shots;
	}

	if (Reloaded == 0 && !Hands.IsEmpty())
	{
		UVaelCombatTextSubsystem::PostPickup(Character, NSLOCTEXT("VaelFloromant", "NoKeimpelz", "Kein Keimpelz"), FLinearColor(FColor(158, 143, 125)));
	}
	return Reloaded;
}

void UVaelFloromantComponent::OpenHerbarium()
{
	ShowNotBuilt(NSLOCTEXT("VaelFloromant", "Herbarium", "Herbarium"));
}
