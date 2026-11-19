// Copyright Epic Games, Inc. All Rights Reserved.

#include "Floromant/VaelFloromantComponent.h"
#include "World/VaelBalance.h"
#include "Combat/VaelHitFeedbackSubsystem.h"
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
#include "Floromant/VaelCrossBarrelComponent.h"
#include "Magic/VaelEnvironmentStatics.h"
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
#include "Magic/VaelSpellEffects.h"
#include "Nature/VaelGarden.h"
#include "Nature/VaelHarvestable.h"
#include "Player/VaelCharacter.h"
#include "Save/VaelSaveGame.h"
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
	WheelFlipAge += DeltaTime;
	UpdateSeed(DeltaTime);

	// Once the path is chosen it decides the form of each hand, also of the plants laid on before
	for (const bool bLeftHand : { true, false })
	{
		if (UVaelPlantArmComponent* Hand = bLeftHand ? LeftHand.Get() : RightHand.Get())
		{
			Hand->SetShadowForm(GetHandForm(bLeftHand, Hand->IsShadowForm()));
		}
	}

	const AVaelCharacter* Character = GetCharacter();
	if (Character != nullptr && Character->IsFloromant() && !Character->IsDowned())
	{
		AddKeimpelz(UVaelFloromantSettings::Get()->KeimpelzGrowPerSecond * DeltaTime);
	}

	TickFlight(DeltaTime);
	TickWetRoots(DeltaTime);
	TickCross(DeltaTime);
	UpdateCrossKnot(DeltaTime);
	TickShield(DeltaTime);
	TickNid(DeltaTime);


	// Every frame while she is rooted, charging a cross or has plants on her arms (their roots follow the bones)
	SetComponentTickInterval(bCrossCharging || CrossShowLeft > 0.0f || IsRooted() || FindHand(true) != nullptr || FindHand(false) != nullptr ? 0.0f : 0.2f);
}

void UVaelFloromantComponent::ToggleRoot()
{
	const AVaelCharacter* Character = GetCharacter();
	if (Character == nullptr || !Character->IsFloromant() || Character->IsDowned() || Character->IsDodging())
	{
		return;
	}
	// Up high there is no ground to root in: she comes down first
	bRiseWanted = false;
	bRootWanted = !bRootWanted;
	UE_LOG(LogVael, Log, TEXT("'%s' %s"), *GetNameSafe(Character), bRootWanted ? TEXT("takes root") : TEXT("lets go of the ground"));
}

void UVaelFloromantComponent::TearOut()
{
	if (!bRootWanted && RootGrowth <= 0.0f)
	{
		return;
	}
	// Torn out at once by the dodge: a puff of earth, and she staggers on slower for a moment
	bRootWanted = false;
	RootGrowth = 0.0f;
	TornTimeLeft = UVaelFloromantSettings::Get()->TearOutSeconds;
	if (AVaelCharacter* Character = GetCharacter())
	{
		FVaelDebris Earth;
		Earth.Count = 10;
		Earth.Spread = 25.0f;
		Earth.Speed = 160.0f;
		Earth.Lift = 220.0f;
		Earth.Lifetime = 0.6f;
		Earth.Color = FLinearColor(0.11f, 0.075f, 0.05f);
		AVaelDebrisBurst::Spawn(Character, GetFeet(), Earth);
		Character->SetManaRegen(UVaelFloromantSettings::Get()->SaftPerSecond);
	}
}

void UVaelFloromantComponent::TickWetRoots(float DeltaTime)
{
	AVaelCharacter* Character = GetCharacter();
	if (Character == nullptr || !Character->IsFloromant() || RootGrowth < 1.0f)
	{
		WetShots = 0.0f;
		return;
	}

	// Rooted where it rains or by water, the earth is wet and feeds her (Boris 2026-10-27)
	const AVaelRegion* Region = AVaelRegion::GetRegionAt(GetWorld(), Character->GetActorLocation());
	const bool bRain = (Region != nullptr && (Region->GetWeather() == EVaelWeather::Rain || Region->GetWeather() == EVaelWeather::Storm))
		|| AVaelLocalWeather::IsRainingAt(GetWorld(), Character->GetActorLocation());
	if (!bRain && !UVaelEnvironmentStatics::IsElementInEnvironment(Character, EVaelElement::Water))
	{
		WetShots = 0.0f;
		return;
	}

	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	AddKeimpelz(Settings->KeimpelzGrowPerSecond * (Settings->RootedWetKeimpelzScale - 1.0f) * DeltaTime);
	WetShots += Settings->RootedWetShotsPerSecond * DeltaTime;
	while (WetShots >= 1.0f)
	{
		WetShots -= 1.0f;
		for (const bool bLeft : { true, false })
		{
			if (UVaelPlantArmComponent* Hand = FindHand(bLeft))
			{
				Hand->RefillBar(1);
			}
		}
	}

	// Drops of water rise along her roots, so it shows that the ground feeds her
	WetPuffCountdown -= DeltaTime;
	if (WetPuffCountdown <= 0.0f)
	{
		WetPuffCountdown = 0.8f;
		FVaelDebris Drops;
		Drops.Count = 8;
		Drops.Spread = 35.0f;
		Drops.Speed = 30.0f;
		Drops.Lift = 140.0f;
		Drops.Gravity = -60.0f;
		Drops.Lifetime = 0.9f;
		Drops.MinSize = 1.5f;
		Drops.MaxSize = 3.0f;
		Drops.Color = FloromantTurquoise;
		Drops.Glow = 3.0f;
		Drops.bLandOnGround = false;
		AVaelDebrisBurst::Spawn(Character, GetFeet(), Drops);
	}
}

void UVaelFloromantComponent::SwapHands()
{
	UVaelPlantArmComponent* Left = FindHand(true);
	UVaelPlantArmComponent* Right = FindHand(false);
	const AVaelCharacter* Character = GetCharacter();
	if (Left == nullptr || Right == nullptr || Character == nullptr || !Character->IsFloromant() || bCrossCharging)
	{
		return;
	}
	UVaelPlantArmData* LeftPlant = Left->GetPlant();
	UVaelPlantArmData* RightPlant = Right->GetPlant();
	if (LeftPlant == RightPlant)
	{
		return;
	}
	// Each plant keeps its form when it changes sides, unless the path decides the form of each hand
	const bool bLeftShadow = Left->IsShadowForm();
	const bool bRightShadow = Right->IsShadowForm();
	Left->SetShadowForm(GetHandForm(true, bRightShadow));
	Right->SetShadowForm(GetHandForm(false, bLeftShadow));
	Left->SetPlant(RightPlant);
	Right->SetPlant(LeftPlant);
}

void UVaelFloromantComponent::StartCross()
{
	AVaelCharacter* Character = GetCharacter();
	UVaelPlantArmComponent* Left = FindHand(true);
	UVaelPlantArmComponent* Right = FindHand(false);
	// Say why it can not cross, instead of doing nothing (Boris 2026-11-06)
	if (Character != nullptr && Character->IsFloromant() && !bCrossCharging && Left != nullptr && Right != nullptr)
	{
		const auto Tell = [Character](const FText& Text) { UVaelCombatTextSubsystem::PostPickup(Character, Text, FLinearColor(FColor(158, 143, 125))); };
		if (Left->IsRegrowing() || Right->IsRegrowing())
		{
			Tell(NSLOCTEXT("VaelFloromant", "CrossRegrow", "Die Pflanzen wachsen noch nach"));
			return;
		}
		if (GetCooldownLeft(EVaelHUDAbility::Placeholder) > 0.0f)
		{
			Tell(NSLOCTEXT("VaelFloromant", "CrossCooldown", "Die Hände sind noch nicht so weit"));
			return;
		}
		if (AVaelSafeZone::IsSafe(GetWorld(), Character->GetActorLocation()))
		{
			Tell(NSLOCTEXT("VaelFloromant", "CrossCamp", "Im Lager wächst nichts"));
			return;
		}
	}
	if (Character == nullptr || !Character->IsFloromant() || Character->IsDowned() || bCrossCharging || Left == nullptr || Right == nullptr
		|| Left->GetPlant() == nullptr || Right->GetPlant() == nullptr || Left->IsRegrowing() || Right->IsRegrowing())
	{
		return;
	}
	const EVaelSymbiosis Kind = VaelSymbiosis::Find(Left->GetPlant(), Right->GetPlant());
	if (Kind == EVaelSymbiosis::None)
	{
		UVaelCombatTextSubsystem::PostPickup(Character, NSLOCTEXT("VaelFloromant", "NoPair", "Diese beiden wachsen nicht zusammen"), FLinearColor(FColor(158, 143, 125)));
		return;
	}

	// Each hand in shadow form pays its share of the cross in Nid
	PayNid((Left->IsShadowForm() ? Left->GetPlant()->NidPerCross : 0.0f) + (Right->IsShadowForm() ? Right->GetPlant()->NidPerCross : 0.0f));

	// The hands stop firing and grow towards each other; the knot between them swells while it charges
	bCrossCharging = true;
	bCrossHeld = true;
	CrossChargeAge = 0.0f;
	Left->SetCrossing(true);
	Right->SetCrossing(true);
	if (CrossKnot == nullptr)
	{
		CrossKnot = NewObject<UVaelSymbiosisKnotComponent>(GetOwner(), TEXT("CrossKnot"));
		CrossKnot->SetupAttachment(GetOwner()->GetRootComponent());
		CrossKnot->RegisterComponent();
	}
	CrossKnot->Setup(Left->GetPlant(), Right->GetPlant(), VaelSymbiosis::GetColor(Kind), Left->IsShadowForm() && Right->IsShadowForm());
	CrossShowLeft = 0.0f;
	SetComponentTickInterval(0.0f);

	// Two shooting plants: the arms grow into the Wurzelrohr instead of the knot (the Rußrosen keep their heart)
	bBarrelCross = Kind != EVaelSymbiosis::Rosenherz && VaelSymbiosis::IsShotPair(Left->GetPlant(), Right->GetPlant());
	if (bBarrelCross)
	{
		CrossKnot->Clear();
		if (CrossBarrel == nullptr)
		{
			CrossBarrel = NewObject<UVaelCrossBarrelComponent>(GetOwner(), TEXT("CrossBarrel"));
			CrossBarrel->SetupAttachment(GetOwner()->GetRootComponent());
			CrossBarrel->RegisterComponent();
		}
		CrossBarrel->Begin(Left->GetPlant(), Right->GetPlant(), VaelSymbiosis::GetColor(Kind), Left->IsShadowForm() && Right->IsShadowForm());
	}
}

void UVaelFloromantComponent::MergeHandsIntoBarrel(bool bMerged)
{
	for (const bool bLeftHand : { true, false })
	{
		if (UVaelPlantArmComponent* Hand = FindHand(bLeftHand))
		{
			Hand->SetMergedIntoBarrel(bMerged);
		}
	}
	if (FootRoots != nullptr)
	{
		FootRoots->SetArmsHidden(bMerged);
	}
}

void UVaelFloromantComponent::ReleaseCross()
{
	bCrossHeld = false;
}

void UVaelFloromantComponent::TickCross(float DeltaTime)
{
	if (!bCrossCharging)
	{
		return;
	}
	AVaelCharacter* Character = GetCharacter();
	UVaelPlantArmComponent* Left = FindHand(true);
	UVaelPlantArmComponent* Right = FindHand(false);
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	const auto Stop = [&]()
	{
		bCrossCharging = false;
		for (UVaelPlantArmComponent* Hand : { Left, Right })
		{
			if (Hand != nullptr)
			{
				Hand->SetCrossing(false);
			}
		}
	};
	if (Character == nullptr || Left == nullptr || Right == nullptr || Character->IsDowned() || Character->IsDodging() || Left->GetPlant() == nullptr || Right->GetPlant() == nullptr)
	{
		Stop();
		if (CrossKnot != nullptr)
		{
			CrossKnot->Clear();
		}
		if (CrossBarrel != nullptr)
		{
			CrossBarrel->Clear();
		}
		bBarrelCross = false;
		CrossPose = 0.0f;
		MergeHandsIntoBarrel(false);
		return;
	}

	CrossChargeAge += DeltaTime;
	const float Charge = FMath::Clamp((CrossChargeAge - Settings->CrossChargeTime) / FMath::Max(Settings->CrossFullChargeTime - Settings->CrossChargeTime, 0.05f), 0.0f, 1.0f);

	// The vines grow from both hands into the knot over the short charge; held on, it swells and glows brighter
	FVector Aim = UVaelFormulaAbility::GetAimDirection(Character);
	Aim.Z = 0.0f;
	CrossAim = Aim.IsNearlyZero() ? Character->GetActorForwardVector() : Aim.GetSafeNormal();
	if (bBarrelCross && CrossBarrel != nullptr)
	{
		// The hands come together first, then the barrel shoots out of the wrists
		CrossPose = FMath::Min(1.0f, CrossChargeAge / 0.18f);
		// The hands' own plants go into the barrel the moment its strands have wound down the forearms
		MergeHandsIntoBarrel(CrossChargeAge > 0.22f);
		CrossBarrel->Update(Left->GetElbowLocation(), Left->GetHandLocation(), Right->GetElbowLocation(), Right->GetHandLocation(), CrossAim,
			FMath::Clamp((CrossChargeAge - 0.08f) / Settings->CrossChargeTime, 0.0f, 1.0f), Charge, DeltaTime);
	}
	else if (CrossKnot != nullptr)
	{
		const FVector Knot = (Left->GetHandLocation() + Right->GetHandLocation()) * 0.5f + CrossAim * 40.0f;
		CrossKnot->SetCharge(Charge);
		CrossKnot->Update(Left->GetHandLocation(), Right->GetHandLocation(), Knot, FMath::Min(1.0f, CrossChargeAge / Settings->CrossChargeTime));
	}

	// Let go and charged at least the short time: the pair acts; held on it stays at full charge while she aims
	if (!bCrossHeld && CrossChargeAge >= Settings->CrossChargeTime)
	{
		Stop();
		CrossHands(Charge);
	}
}

float UVaelFloromantComponent::GetShieldStance(bool bLeft) const
{
	const UVaelPlantArmComponent* Hand = FindHand(bLeft);
	return Hand != nullptr ? Hand->GetShieldStance() : 0.0f;
}

bool UVaelFloromantComponent::IsMortarHand(bool bLeft) const
{
	const UVaelPlantArmComponent* Hand = FindHand(bLeft);
	return Hand != nullptr && Hand->IsMortar();
}

float UVaelFloromantComponent::GetMortarDraw(bool bLeft) const
{
	const UVaelPlantArmComponent* Hand = FindHand(bLeft);
	return Hand != nullptr ? Hand->GetMortarDraw() : 0.0f;
}

float UVaelFloromantComponent::GetWhipStroke(bool bLeft, float& OutDuration, float& OutStartSide) const
{
	const UVaelPlantArmComponent* Hand = FindHand(bLeft);
	OutDuration = 0.34f;
	OutStartSide = 1.0f;
	return Hand != nullptr ? Hand->GetWhipStroke(OutDuration, OutStartSide) : -1.0f;
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
		Dome = AVaelGlimmDome::Raise(Character, Character, Shield, EVaelSymbiosis::None, Shield->ShieldRadius, Raised->IsShadowForm());
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
		bRootWanted = false;
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

		// Square roots her feet and lets them go (Boris 2026-10-27, a choice now); the old way roots her by itself while she
		// fires on the spot. Rooted she stays where she is; the dodge dives through the roots or tears them out
		if (Settings->bAutoRoot)
		{
			const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
			const bool bWantsToMove = Movement != nullptr && Movement->GetCurrentAcceleration().SizeSquared() > 1.0f;
			const bool bStill = Stance >= 0.9f && !bWantsToMove && !Character->IsDodging() && Character->GetVelocity().Size2D() < 40.0f;
			StillFiringTime = bStill ? StillFiringTime + DeltaTime : 0.0f;
			bRootWanted = bRootWanted ? !(Stance < 0.5f || bWantsToMove || Character->IsDodging()) : StillFiringTime >= Settings->RootDelay;
		}
		if (bRootWanted)
		{
			UprootTimeLeft = -1.0f;
			RootGrowth = FMath::Min(1.0f, FMath::Max(RootGrowth, KINDA_SMALL_NUMBER) + DeltaTime / Settings->RootGrowTime);
		}
		else if (RootGrowth > 0.0f)
		{
			RootGrowth = FMath::Max(0.0f, RootGrowth - DeltaTime / FMath::Max(Settings->UprootTime, 0.05f));
			StillFiringTime = 0.0f;
		}
		TornTimeLeft = FMath::Max(0.0f, TornTimeLeft - DeltaTime);

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

	HoldRootYaw(DeltaTime);
	UpdateFootRoots();
}

void UVaelFloromantComponent::UpdateFootRoots()
{
	AVaelCharacter* Character = GetCharacter();
	if (FootRoots == nullptr)
	{
		if (GetOwner() == nullptr)
		{
			return;
		}
		FootRoots = NewObject<UVaelFootRootsComponent>(GetOwner(), TEXT("FootRoots"));
		FootRoots->SetupAttachment(GetOwner()->GetRootComponent());
		FootRoots->RegisterComponent();
	}

	USkeletalMeshComponent* Body = Character != nullptr ? Character->GetMesh() : nullptr;
	const float DeltaTime = GetWorld() != nullptr ? GetWorld()->GetDeltaSeconds() : 0.0f;
	const bool bFiring = IsRooted() && FMath::Max(GetStance(true), GetStance(false)) > 0.5f;
	FootRoots->SetGrowth(Body, RootGrowth, bFiring, DeltaTime);

	// The same roots on the arms, grown with the plant of each hand (not where the arm itself became vines, the Rußrose)
	float ArmGrowth[2] = { 0.0f, 0.0f };
	bool bArmFiring[2] = { false, false };
	for (int32 Arm = 0; Arm < 2; ++Arm)
	{
		const UVaelPlantArmComponent* Hand = FindHand(Arm == 0);
		if (Hand != nullptr && Hand->GetPlant() != nullptr && !Hand->IsArmHidden())
		{
			ArmGrowth[Arm] = Hand->GetGrowth();
			bArmFiring[Arm] = Hand->GetStance() > 0.5f;
		}
	}
	FootRoots->SetArms(Body, ArmGrowth, bArmFiring, DeltaTime);
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

	// Turmruf (Boris 2026-10-27): her towers grow into their giant form; rooted the call reaches them through the roots and they
	// fire at once as it arrives
	AVaelCharacter* Character = GetCharacter();
	int32 NumTowers = 0;
	for (TActorIterator<AVaelPlantTower> It(GetWorld()); It; ++It)
	{
		if (!It->IsStanding() || It->GetGardener() != Character || FVector::Dist2D(It->GetActorLocation(), Character->GetActorLocation()) > Radius)
		{
			continue;
		}
		++NumTowers;
		if (!bRootNet)
		{
			It->CallUp(Settings->TowerCallSeconds);
			continue;
		}
		TWeakObjectPtr<AVaelPlantTower> Tower = *It;
		const float Power = Settings->RootWaveTowerPower;
		const float Seconds = Settings->TowerCallSeconds;
		AVaelRootRunner::Run(Character, GetFeet(), It->GetActorLocation(), Settings->RootWaveSpeed, FloromantTurquoise, [Tower, Power, Seconds]()
		{
			if (AVaelPlantTower* Reached = Tower.Get())
			{
				Reached->CallUp(Seconds);
				Reached->FireNow(Power);
			}
		});
	}
	UE_LOG(LogVael, Log, TEXT("'%s' calls her towers%s: %d plants, %d towers"), *GetNameSafe(Character), bRootNet ? TEXT(" through the root net") : TEXT(""), NumActing, NumTowers);
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

bool UVaelFloromantComponent::RootDive(const FVector& AimLocation)
{
	AVaelCharacter* Character = GetCharacter();
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	if (Character == nullptr || !IsRooted())
	{
		return false;
	}

	// Her standing tower within reach nearest to where she aims (Boris 2026-10-27: towers are her anchors now)
	AVaelPlantTower* Goal = nullptr;
	float Best = UE_BIG_NUMBER;
	for (const TWeakObjectPtr<AVaelPlantTower>& Standing : Towers)
	{
		if (Standing.IsValid() && Standing->IsStanding() && FVector::Dist2D(Standing->GetActorLocation(), Character->GetActorLocation()) < Settings->LeapRange)
		{
			const float Distance = FVector::Dist2D(Standing->GetActorLocation(), AimLocation);
			if (Distance < Best)
			{
				Best = Distance;
				Goal = Standing.Get();
			}
		}
	}
	if (Goal == nullptr || !Use(EVaelHUDAbility::RootLeap, Settings->LeapCost))
	{
		return false;
	}

	// Into the ground here, along a root under the earth, out of the ground beside the tower on the side she came from; her roots
	// grow again where she comes up, so she stays rooted
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
	AVaelDebrisBurst::Spawn(Character, GetFeet(), Earth);
	AVaelRootRunner::Run(Character, GetFeet(), Goal->GetActorLocation(), Settings->RootWaveSpeed * 2.0f, FloromantTurquoise, []() {});

	Character->TeleportTo(To, (-Back).Rotation());
	Character->SetInvulnerableFor(0.4f);
	RootGrowth = FMath::Min(RootGrowth, 0.3f);
	AVaelDebrisBurst::Spawn(Character, To - FVector(0.0f, 0.0f, Character->GetSimpleCollisionHalfHeight()), Earth);
	UE_LOG(LogVael, Log, TEXT("'%s' dives through the roots to '%s'"), *GetNameSafe(Character), *GetNameSafe(Goal));
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
	EnsureWheel();
	return WheelSlots.Num();
}

UVaelPlantArmData* UVaelFloromantComponent::GetHandBeltPlant(int32 Index) const
{
	EnsureWheel();
	return WheelSlots.IsValidIndex(Index) ? WheelSlots[Index].Get() : nullptr;
}

int32 UVaelFloromantComponent::GetCatalogSize() const
{
	return UVaelFloromantSettings::Get()->HandBelt.Num();
}

UVaelPlantArmData* UVaelFloromantComponent::GetCatalogPlant(int32 Index) const
{
	const TArray<TSoftObjectPtr<UVaelPlantArmData>>& Belt = UVaelFloromantSettings::Get()->HandBelt;
	return Belt.IsValidIndex(Index) ? VaelAssets::LoadOptional(Belt[Index]) : nullptr;
}

void UVaelFloromantComponent::EnsureWheel() const
{
	const int32 Places = FMath::Clamp(UVaelFloromantSettings::Get()->HandWheelSlots, 1, 8);
	if (WheelSlots.Num() == Places)
	{
		return;
	}
	WheelSlots.SetNum(Places);
	WheelShadowSides.SetNum(Places);
	// Empty places are filled with the plants of the repertoire she has not set yet, in its order
	int32 Next = 0;
	for (TObjectPtr<UVaelPlantArmData>& Place : WheelSlots)
	{
		while (Place == nullptr && Next < GetCatalogSize())
		{
			UVaelPlantArmData* Candidate = GetCatalogPlant(Next++);
			if (Candidate != nullptr && !WheelSlots.Contains(Candidate))
			{
				Place = Candidate;
			}
		}
	}
}

void UVaelFloromantComponent::SetWheelPlant(int32 Slot, UVaelPlantArmData* Plant)
{
	EnsureWheel();
	if (!WheelSlots.IsValidIndex(Slot))
	{
		return;
	}
	// A plant sits in one place only: where it sat before is left to the one it replaces
	const int32 Before = WheelSlots.IndexOfByKey(Plant);
	if (Before != INDEX_NONE)
	{
		WheelSlots[Before] = WheelSlots[Slot];
	}
	WheelSlots[Slot] = Plant;
}

bool UVaelFloromantComponent::PickPlant(const UVaelPlantArmData* Plant)
{
	EnsureWheel();
	if (Plant == nullptr)
	{
		return false;
	}
	int32 Slot = WheelSlots.IndexOfByKey(Plant);
	if (Slot == INDEX_NONE)
	{
		Slot = HandBeltSelection;
		SetWheelPlant(Slot, const_cast<UVaelPlantArmData*>(Plant));
	}
	HandBeltSelection = Slot;
	return true;
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
	Hand->SetShadowForm(GetHandForm(bLeft, IsWheelShadow(HandBeltSelection)));
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
	// Charging the crossed hands, or staggering on after the roots were torn out, she is slower
	const float Burdened = (bCrossCharging ? Settings->CrossMoveSpeedScale : 1.0f) * (TornTimeLeft > 0.0f ? Settings->TearOutMoveSpeedScale : 1.0f);
	return FMath::Lerp(1.0f, Settings->FiringMoveSpeedScale, Stance) * FMath::Lerp(1.0f, Settings->RiseMoveSpeedScale, Rise) * Burdened;
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

void UVaelFloromantComponent::RootTower(bool bLeft, const FVector* At, bool bPaid)
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
	if (!bPaid && !Use(Ability, Settings->TowerSaftCost))
	{
		return;
	}

	// Tapped it goes into the ground just ahead; aimed (held) or sown it goes where the mark was
	const FVector Ahead = Character->GetActorForwardVector().GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Ahead) * (bLeft ? -35.0f : 35.0f);
	const FVector Ground = At != nullptr ? *At : GroundTarget(Character->GetActorLocation() + Ahead * Settings->TowerPlantDistance + Side, Settings->TowerPlantDistance + 60.0f);
	AVaelPlantTower* Tower = AVaelPlantTower::PlantTower(Character, Plant, Ground, Hand != nullptr && Hand->IsShadowForm());
	if (Tower == nullptr)
	{
		return;
	}
	Hand->PlantAway(Settings->TowerRegrowTime);
	if (Hand->IsShadowForm())
	{
		PayNid(Plant->NidPerTower);
	}

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

FVector UVaelFloromantComponent::GetTowerAimPoint(float AimSeconds) const
{
	const AVaelCharacter* Character = GetCharacter();
	if (Character == nullptr)
	{
		return FVector::ZeroVector;
	}
	// The mark wanders out where she looks, the longer it is held
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	const float Reach = FMath::Lerp(Settings->TowerPlantDistance + 80.0f, Settings->TowerAimRange, FMath::SmoothStep(0.0f, 1.2f, AimSeconds));
	const FVector Ahead = Character->GetActorForwardVector().GetSafeNormal2D();
	return GroundTarget(Character->GetActorLocation() + Ahead * Reach, Reach + 60.0f);
}

FVector UVaelFloromantComponent::KeepTowerAim(const FVector& Point) const
{
	const AVaelCharacter* Character = GetCharacter();
	if (Character == nullptr)
	{
		return Point;
	}
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	const FVector Here = Character->GetActorLocation();
	FVector Away = Point - Here;
	Away.Z = 0.0f;
	const float Distance = Away.Size();
	const float Near = Settings->TowerPlantDistance + 40.0f;
	FVector Way = Away.IsNearlyZero() ? Character->GetActorForwardVector().GetSafeNormal2D() : Away / Distance;
	const float Reach = FMath::Clamp(Distance, Near, Settings->TowerAimRange);
	return GroundTarget(Here + Way * Reach, Reach + 60.0f);
}

void UVaelFloromantComponent::ShowTowerAim(int32 Hands, const FVector& At)
{
	AVaelCharacter* Character = GetCharacter();
	if (Character == nullptr)
	{
		return;
	}
	if (AimMark == nullptr && Hands != 0)
	{
		UMaterialInterface* Core = nullptr;
		UMaterialInterface* Glow = nullptr;
		VaelEffects::LoadLookMaterials(Core, Glow);
		if (Glow == nullptr)
		{
			return;
		}
		AimMark = VaelEffects::AddLookShape(Character, Glow, FLinearColor::White, 0.0f);
		AimMark->SetUsingAbsoluteLocation(true);
		AimMark->SetUsingAbsoluteRotation(true);
		AimMark->SetUsingAbsoluteScale(true);
	}
	if (AimMark == nullptr)
	{
		return;
	}
	if (Hands == 0)
	{
		AimMark->SetVisibility(false);
		AimHands = 0;
		return;
	}

	// The mark glows in the colour of the plant, both hands in the colour of their pair (named once when the pair is aimed)
	UVaelPlantArmComponent* Left = FindHand(true);
	UVaelPlantArmComponent* Right = FindHand(false);
	FLinearColor Colour(0.6f, 0.9f, 0.4f);
	if (Hands == 3 && Left != nullptr && Right != nullptr)
	{
		const EVaelSymbiosis Kind = VaelSymbiosis::Find(Left->GetPlant(), Right->GetPlant());
		if (Kind != EVaelSymbiosis::None)
		{
			Colour = VaelSymbiosis::GetColor(Kind);
			if (AimHands != 3)
			{
				const bool bDragon = VaelSymbiosis::HasHeritage(Kind) && IsHeritageAwake(Kind);
				UVaelCombatTextSubsystem::PostPickup(Character, bDragon ? VaelSymbiosis::GetHeritageName(Kind) : VaelSymbiosis::GetName(Kind), Colour);
			}
		}
		else
		{
			Colour = FLinearColor(0.4f, 0.35f, 0.3f);
		}
	}
	else if (UVaelPlantArmComponent* Hand = Hands == 1 ? Left : Right)
	{
		Colour = VaelSymbiosis::GetPlantColor(Hand->GetPlant(), Hand->IsShadowForm());
	}
	AimHands = Hands;
	const float Time = GetWorld()->GetTimeSeconds();
	const float Size = (Hands == 3 ? 1.4f : 1.0f) * (1.0f + 0.08f * FMath::Sin(Time * 6.0f));
	AimMark->SetVisibility(true);
	AimMark->SetWorldLocationAndRotation(At + FVector(0.0f, 0.0f, 3.0f), FRotator::ZeroRotator);
	AimMark->SetWorldScale3D(FVector(Size * 0.7f, Size * 0.7f, 0.03f));
	if (UMaterialInstanceDynamic* Dynamic = Cast<UMaterialInstanceDynamic>(AimMark->GetMaterial(0)))
	{
		Dynamic->SetVectorParameterValue(VaelEffects::LookColorParameter, Colour);
	}
	VaelEffects::SetLookGlow(AimMark, 0.7f + 0.3f * FMath::Sin(Time * 6.0f));
}

void UVaelFloromantComponent::SowSymbiosis(const FVector& At)
{
	AVaelCharacter* Character = GetCharacter();
	UVaelPlantArmComponent* Left = FindHand(true);
	UVaelPlantArmComponent* Right = FindHand(false);
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	if (Character == nullptr || Left == nullptr || Right == nullptr || Left->GetPlant() == nullptr || Right->GetPlant() == nullptr || SeedTime >= 0.0f)
	{
		return;
	}
	if (Left->IsRegrowing() || Right->IsRegrowing())
	{
		UVaelCombatTextSubsystem::PostPickup(Character, NSLOCTEXT("VaelFloromant", "Regrowing", "Der Arm wächst noch nach"), FLinearColor(FColor(158, 143, 125)));
		return;
	}
	const EVaelSymbiosis Kind = VaelSymbiosis::Find(Left->GetPlant(), Right->GetPlant());
	if (Kind == EVaelSymbiosis::None)
	{
		UVaelCombatTextSubsystem::PostPickup(Character, NSLOCTEXT("VaelFloromant", "NoPair", "Diese beiden wachsen nicht zusammen"), FLinearColor(FColor(158, 143, 125)));
		return;
	}
	if (!Use(EVaelHUDAbility::HandLeft, Settings->TowerSaftCost) || !Use(EVaelHUDAbility::HandRight, Settings->TowerSaftCost))
	{
		return;
	}

	// The seed leaves her hands
	if (SeedShape == nullptr)
	{
		UMaterialInterface* Core = nullptr;
		UMaterialInterface* Glow = nullptr;
		VaelEffects::LoadLookMaterials(Core, Glow);
		if (Glow != nullptr)
		{
			SeedShape = VaelEffects::AddLookShape(Character, Glow, FLinearColor::White, 0.0f);
			SeedShape->SetUsingAbsoluteLocation(true);
			SeedShape->SetUsingAbsoluteRotation(true);
			SeedShape->SetUsingAbsoluteScale(true);
		}
	}
	if (SeedShape != nullptr)
	{
		if (UMaterialInstanceDynamic* Dynamic = Cast<UMaterialInstanceDynamic>(SeedShape->GetMaterial(0)))
		{
			Dynamic->SetVectorParameterValue(VaelEffects::LookColorParameter, VaelSymbiosis::GetColor(Kind));
		}
	}
	SeedFrom = Character->GetActorLocation() + Character->GetActorForwardVector() * 30.0f + FVector(0.0f, 0.0f, 50.0f);
	SeedTo = At;
	SeedTime = 0.0f;
}

void UVaelFloromantComponent::UpdateSeed(float DeltaSeconds)
{
	if (SeedTime < 0.0f)
	{
		return;
	}
	constexpr float Flight = 0.6f;
	constexpr float Burrow = 0.35f;
	const float Was = SeedTime;
	SeedTime += DeltaSeconds;
	AVaelCharacter* Character = GetCharacter();
	if (Character == nullptr)
	{
		SeedTime = -1.0f;
		return;
	}
	auto Earth = [Character](const FVector& At, int32 Count, float Speed, float Lift)
	{
		FVaelDebris Clods;
		Clods.Count = Count;
		Clods.Spread = 12.0f;
		Clods.Speed = Speed;
		Clods.Lift = Lift;
		Clods.Lifetime = 0.9f;
		Clods.MinSize = 2.0f;
		Clods.MaxSize = 6.0f;
		Clods.Color = FLinearColor(0.14f, 0.1f, 0.07f);
		AVaelDebrisBurst::Spawn(Character, At, Clods);
	};

	if (SeedShape != nullptr)
	{
		if (SeedTime < Flight)
		{
			// An arc through the air
			const float Along = SeedTime / Flight;
			const FVector Place = FMath::Lerp(SeedFrom, SeedTo, Along) + FVector(0.0f, 0.0f, 160.0f * 4.0f * Along * (1.0f - Along));
			SeedShape->SetVisibility(true);
			SeedShape->SetWorldLocation(Place);
			SeedShape->SetWorldScale3D(FVector(0.12f));
			VaelEffects::SetLookGlow(SeedShape, 5.0f);
		}
		else
		{
			// It bores itself into the ground, sinking and dimming
			const float Down = FMath::Clamp((SeedTime - Flight) / Burrow, 0.0f, 1.0f);
			SeedShape->SetWorldLocation(SeedTo - FVector(0.0f, 0.0f, 14.0f * Down));
			SeedShape->SetWorldScale3D(FVector(FMath::Max(0.12f * (1.0f - 0.7f * Down), 0.001f)));
			VaelEffects::SetLookGlow(SeedShape, 5.0f * (1.0f - Down) + 1.0f);
			SeedShape->SetVisibility(Down < 1.0f);
		}
	}
	// Where it strikes the ground earth flies
	if (Was < Flight && SeedTime >= Flight)
	{
		Earth(SeedTo, 18, 200.0f, 230.0f);
		UVaelHitFeedbackSubsystem::Shake(Character, 0.12f);
	}
	if (SeedTime >= Flight + Burrow)
	{
		// The pair bursts out of the earth: both towers rise together, so they fuse at once
		SeedTime = -1.0f;
		const FVector Ahead = (SeedTo - Character->GetActorLocation()).GetSafeNormal2D();
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Ahead.IsNearlyZero() ? FVector::ForwardVector : Ahead) * 30.0f;
		const FVector LeftAt = SeedTo - Side;
		const FVector RightAt = SeedTo + Side;
		RootTower(true, &LeftAt, true);
		RootTower(false, &RightAt, true);
		Earth(SeedTo, 30, 320.0f, 380.0f);
	}
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

void UVaelFloromantComponent::CrossHands(float Charge)
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
	if (!Use(EVaelHUDAbility::Placeholder, FMath::Lerp(Settings->CrossSaftCost, Settings->CrossChargedSaftCost, Charge)))
	{
		return;
	}
	Left->RefillBar(-Settings->CrossBarShots);
	Right->RefillBar(-Settings->CrossBarShots);

	FVector Aim = UVaelFormulaAbility::GetAimDirection(Character);
	Aim.Z = 0.0f;
	Aim = Aim.IsNearlyZero() ? Character->GetActorForwardVector() : Aim.GetSafeNormal();
	const bool bShadow = Left->IsShadowForm() && Right->IsShadowForm();
	// Out of the mouth of the Wurzelrohr, or out of the knot between the hands
	const bool bFromBarrel = bBarrelCross && CrossBarrel != nullptr;
	const FVector Knot = bFromBarrel ? CrossBarrel->GetMuzzle() : (Left->GetHandLocation() + Right->GetHandLocation()) * 0.5f + Aim * 45.0f;
	VaelSymbiosis::Fire(Character, Kind, Left->GetPlant(), Right->GetPlant(), Knot, Aim, GetHandPower(), Charge, bShadow);
	LearnPair(Kind);

	// The moment the arms grow into each other is felt: a burst of both plants at the knot and a light jolt of the camera
	VaelSymbiosis::MuzzlePuff(Character, Left->GetPlant(), Knot, bShadow, 2.6f + Charge);
	VaelSymbiosis::MuzzlePuff(Character, Right->GetPlant(), Knot, bShadow, 2.6f + Charge);
	UVaelHitFeedbackSubsystem::Shake(Character, (bFromBarrel ? 0.2f : 0.12f) + 0.1f * Charge);

	// The Wurzelrohr kicks back, flares its crown and unwinds back into the arms
	if (bFromBarrel)
	{
		CrossBarrel->Fire();
		CrossShowLeft = Settings->CrossShowTime;
		CrossAim = Aim;
		UVaelCombatTextSubsystem::PostPickup(Character, VaelSymbiosis::GetName(Kind), VaelSymbiosis::GetColor(Kind));
		return;
	}

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
	const bool bShadow = Left->IsShadowForm() && Right->IsShadowForm();
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
	// The Wurzelrohr after its shot: it holds a moment, then unwinds back into the arms and the hands part
	if (bBarrelCross && !bCrossCharging)
	{
		const UVaelPlantArmComponent* BarrelLeft = FindHand(true);
		const UVaelPlantArmComponent* BarrelRight = FindHand(false);
		CrossShowLeft -= DeltaTime;
		if (CrossShowLeft <= 0.0f || CrossBarrel == nullptr || BarrelLeft == nullptr || BarrelRight == nullptr)
		{
			if (CrossBarrel != nullptr)
			{
				CrossBarrel->Clear();
			}
			bBarrelCross = false;
			CrossPose = 0.0f;
			CrossShowLeft = 0.0f;
			MergeHandsIntoBarrel(false);
			return;
		}
		const float Unwind = FMath::Min(1.0f, CrossShowLeft / 0.35f);
		// The fingers come back out of the arms as the braid unwinds
		MergeHandsIntoBarrel(Unwind > 0.4f);
		CrossPose = Unwind;
		CrossBarrel->Update(BarrelLeft->GetElbowLocation(), BarrelLeft->GetHandLocation(), BarrelRight->GetElbowLocation(), BarrelRight->GetHandLocation(), CrossAim, Unwind, 0.0f, DeltaTime);
		return;
	}
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

// ---------------------------------------------------------------- Medallions of the plant wheel

bool UVaelFloromantComponent::GetHandForm(bool bLeft, bool bChosen) const
{
	const AVaelCharacter* Character = GetCharacter();
	switch (Character != nullptr ? Character->GetPath() : EVaelPath::None)
	{
	case EVaelPath::Light:		return false;
	case EVaelPath::Shadow:		return true;
	case EVaelPath::Twilight:	return !bLeft;
	default:					return bChosen;
	}
}

bool UVaelFloromantComponent::IsWheelShadow(int32 Slot) const
{
	EnsureWheel();
	const UVaelPlantArmData* Plant = GetHandBeltPlant(Slot);
	if (Plant == nullptr || Plant->IsLightOnly())
	{
		return false;
	}
	const AVaelCharacter* Character = GetCharacter();
	const EVaelPath Path = Character != nullptr ? Character->GetPath() : EVaelPath::None;
	return Path == EVaelPath::Shadow || (Path == EVaelPath::None && WheelShadowSides.IsValidIndex(Slot) && WheelShadowSides[Slot]);
}

bool UVaelFloromantComponent::CanFlipWheel(int32 Slot) const
{
	const AVaelCharacter* Character = GetCharacter();
	const UVaelPlantArmData* Plant = GetHandBeltPlant(Slot);
	return Character != nullptr && Character->GetPath() == EVaelPath::None && Plant != nullptr && !Plant->IsLightOnly();
}

void UVaelFloromantComponent::FlipWheel()
{
	AVaelCharacter* Character = GetCharacter();
	const UVaelPlantArmData* Plant = GetHandBeltPlant(HandBeltSelection);
	if (Character == nullptr || Plant == nullptr)
	{
		return;
	}
	if (!CanFlipWheel(HandBeltSelection))
	{
		// After the vow the path has decided; the Mondtraufe has only its light
		const FText Why = Plant->IsLightOnly() ? NSLOCTEXT("VaelFloromant", "LightOnly", "Sie kennt nur das Licht")
			: NSLOCTEXT("VaelFloromant", "PathChosen", "Der Weg ist gewählt");
		UVaelCombatTextSubsystem::PostPickup(Character, Why, FLinearColor(FColor(158, 143, 125)));
		return;
	}
	WheelShadowSides[HandBeltSelection] = !WheelShadowSides[HandBeltSelection];
	const bool bShadow = WheelShadowSides[HandBeltSelection];
	UVaelCombatTextSubsystem::PostPickup(Character, Plant->GetDisplayName(bShadow), Plant->GetColor(bShadow));
	WheelFlipAge = 0.0f;
}

void UVaelFloromantComponent::TickNid(float DeltaTime)
{
	const AVaelCharacter* Character = GetCharacter();
	if (Character == nullptr || !Character->IsFloromant() || Character->IsDowned())
	{
		return;
	}
	float Points = 0.0f;
	for (const UVaelPlantArmComponent* Hand : { LeftHand.Get(), RightHand.Get() })
	{
		const UVaelPlantArmData* Plant = Hand != nullptr ? Hand->GetPlant() : nullptr;
		if (Plant != nullptr && !Plant->IsLightOnly() && Hand->IsShadowForm() && Hand->IsFiring())
		{
			Points += Plant->NidPerSecond * GetGeneScale(Plant, EVaelGeneEffect::NidPrice) * DeltaTime;
		}
	}
	if (Points > 0.0f)
	{
		PayNid(Points);
	}
}

void UVaelFloromantComponent::PayNid(float Points)
{
	AVaelCharacter* Character = GetCharacter();
	UAbilitySystemComponent* AbilitySystem = Character != nullptr ? Character->GetAbilitySystemComponent() : nullptr;
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	const float Price = Points * Settings->NidPriceScale;
	if (AbilitySystem == nullptr || Price <= 0.0f)
	{
		return;
	}
	UVaelBalanceSubsystem::ChangePlayer(Character, Price, NSLOCTEXT("VaelFloromant", "ShadowFormReason", "Schattenform"));
	if (AVaelRegion* Region = AVaelRegion::GetRegionAt(GetWorld(), Character->GetActorLocation()))
	{
		Region->AddCorruption(Price * Settings->NidRegionShare);
	}
}

int32 UVaelFloromantComponent::GetGene(const UVaelPlantArmData* Plant, int32 Tier) const
{
	const int32* Packed = Plant != nullptr && Tier >= 0 && Tier < 8 ? Genes.Find(Plant->GetFName()) : nullptr;
	return Packed != nullptr ? ((*Packed >> (Tier * 4)) & 0xF) - 1 : -1;
}

bool UVaelFloromantComponent::SetGene(const UVaelPlantArmData* Plant, int32 Tier, int32 Choice)
{
	if (Plant == nullptr || !IsGeneTierOpen(Tier) || !Plant->GetGeneTiers().IsValidIndex(Tier)
		|| (Choice >= 0 && !Plant->GetGeneTiers()[Tier].Options.IsValidIndex(Choice)))
	{
		return false;
	}
	int32& Packed = Genes.FindOrAdd(Plant->GetFName());
	Packed = (Packed & ~(0xF << (Tier * 4))) | (FMath::Clamp(Choice + 1, 0, 15) << (Tier * 4));
	UE_LOG(LogVael, Log, TEXT("'%s' takes Erbanlage %d of tier %d of %s"), *GetNameSafe(GetOwner()), Choice, Tier + 1, *Plant->GetName());
	return true;
}

bool UVaelFloromantComponent::IsGeneTierOpen(int32 Tier) const
{
	const TArray<int32>& Levels = UVaelFloromantSettings::Get()->GeneTierLevels;
	const AVaelCharacter* Character = GetCharacter();
	return Levels.IsValidIndex(Tier) && Character != nullptr && Character->GetLevel() >= Levels[Tier];
}

bool UVaelFloromantComponent::IsHeritageLevelReached() const
{
	const AVaelCharacter* Character = GetCharacter();
	return Character != nullptr && Character->GetLevel() >= UVaelFloromantSettings::Get()->HeritageLevel;
}

bool UVaelFloromantComponent::SetHeritageAwake(EVaelSymbiosis Kind, bool bAwake, bool bForce)
{
	if (!VaelSymbiosis::HasHeritage(Kind))
	{
		return false;
	}
	if (!bAwake)
	{
		AwakePairs.Remove(static_cast<uint8>(Kind));
		return true;
	}
	if (!bForce && (!IsPairKnown(Kind) || !IsHeritageLevelReached()))
	{
		return false;
	}
	LearnPair(Kind);
	AwakePairs.Add(static_cast<uint8>(Kind));
	UE_LOG(LogVael, Log, TEXT("'%s' lets the dragon heritage of %s bloom"), *GetNameSafe(GetOwner()), *VaelSymbiosis::GetName(Kind).ToString());
	return true;
}

void UVaelFloromantComponent::GetPlateCrosses(const UVaelPlantArmData* Plant, TArray<TPair<EVaelSymbiosis, const UVaelPlantArmData*>>& Out) const
{
	Out.Reset();
	if (Plant == nullptr)
	{
		return;
	}
	// With itself first (Glutbusch is two Glutdisteln), then with the others of her repertoire
	const EVaelSymbiosis Self = VaelSymbiosis::Find(Plant, Plant);
	if (Self != EVaelSymbiosis::None)
	{
		Out.Add({ Self, Plant });
	}
	for (int32 Index = 0; Index < GetCatalogSize(); ++Index)
	{
		const UVaelPlantArmData* Other = GetCatalogPlant(Index);
		const EVaelSymbiosis Kind = Other != nullptr && Other != Plant ? VaelSymbiosis::Find(Plant, Other) : EVaelSymbiosis::None;
		if (Kind != EVaelSymbiosis::None)
		{
			Out.Add({ Kind, Other });
		}
	}
	auto Rank = [this](EVaelSymbiosis Kind) { return (IsPairKnown(Kind) ? 2 : 0) + (VaelSymbiosis::HasHeritage(Kind) ? 1 : 0); };
	Out.StableSort([&Rank](const TPair<EVaelSymbiosis, const UVaelPlantArmData*>& A, const TPair<EVaelSymbiosis, const UVaelPlantArmData*>& B)
	{
		return Rank(A.Key) > Rank(B.Key);
	});
}

float UVaelFloromantComponent::GetGeneScale(const UVaelPlantArmData* Plant, EVaelGeneEffect Effect) const
{
	if (Plant == nullptr)
	{
		return 1.0f;
	}
	float Sum = 0.0f;
	const TArray<FVaelGeneTier>& Tiers = Plant->GetGeneTiers();
	for (int32 Tier = 0; Tier < Tiers.Num(); ++Tier)
	{
		const int32 Choice = GetGene(Plant, Tier);
		if (Tiers[Tier].Options.IsValidIndex(Choice) && Tiers[Tier].Options[Choice].Effect == Effect)
		{
			Sum += Tiers[Tier].Options[Choice].Amount;
		}
	}
	switch (Effect)
	{
	case EVaelGeneEffect::Damage:		return 1.0f + Sum;
	case EVaelGeneEffect::FireSpeed:	return 1.0f / (1.0f + Sum);
	default:							return FMath::Max(0.0f, 1.0f - Sum);
	}
}

float UVaelFloromantComponent::GetPlantPower(const UVaelPlantArmData* Plant) const
{
	return GetHandPower() * GetGeneScale(Plant, EVaelGeneEffect::Damage);
}

void UVaelFloromantComponent::WriteSave(FVaelSavedPlayer& Saved) const
{
	EnsureWheel();
	Saved.WheelPlants.Reset();
	for (const TObjectPtr<UVaelPlantArmData>& Plant : WheelSlots)
	{
		Saved.WheelPlants.Add(FSoftObjectPath(Plant.Get()));
	}
	Saved.WheelShadowSides = WheelShadowSides;
	Saved.KnownPairs = KnownPairs.Array();
	Saved.AwakePairs = AwakePairs.Array();
	Saved.Genes = Genes;
}

void UVaelFloromantComponent::ReadSave(const FVaelSavedPlayer& Saved)
{
	EnsureWheel();
	for (int32 Slot = 0; Slot < Saved.WheelPlants.Num() && Slot < WheelSlots.Num(); ++Slot)
	{
		if (UVaelPlantArmData* Plant = Cast<UVaelPlantArmData>(Saved.WheelPlants[Slot].TryLoad()))
		{
			SetWheelPlant(Slot, Plant);
		}
	}
	for (int32 Slot = 0; Slot < Saved.WheelShadowSides.Num() && Slot < WheelShadowSides.Num(); ++Slot)
	{
		WheelShadowSides[Slot] = Saved.WheelShadowSides[Slot];
	}
	KnownPairs = TSet<uint8>(Saved.KnownPairs);
	AwakePairs = TSet<uint8>(Saved.AwakePairs);
	Genes = Saved.Genes;
}

void UVaelFloromantComponent::HoldRootYaw(float DeltaTime)
{
	AVaelCharacter* Character = GetCharacter();
	UCharacterMovementComponent* Movement = Character != nullptr ? Character->GetCharacterMovement() : nullptr;
	if (Movement == nullptr)
	{
		return;
	}
	if (IsRooted() && Character->IsFloromant() && !Character->IsDodging())
	{
		// Her feet stay as they took root: the body no longer turns with the aim, only the upper body twists towards it
		if (!bRootYawHeld)
		{
			RootYaw = Character->GetActorRotation().Yaw;
			RootTwist = 0.0f;
			bRootYawHeld = true;
		}
		Movement->bUseControllerDesiredRotation = false;
		Character->SetActorRotation(FRotator(0.0f, RootYaw, 0.0f));
		const FVector Aim = UVaelFormulaAbility::GetAimDirection(Character).GetSafeNormal2D();
		const float MaxTwist = UVaelFloromantSettings::Get()->MaxRootTwist;
		const float Wanted = Aim.IsNearlyZero() ? RootTwist : FMath::Clamp(FRotator::NormalizeAxis(Aim.Rotation().Yaw - RootYaw), -MaxTwist, MaxTwist);
		RootTwist = FMath::FInterpTo(RootTwist, Wanted, DeltaTime, 9.0f);
	}
	else if (bRootYawHeld)
	{
		bRootYawHeld = false;
		RootTwist = 0.0f;
		if (!Character->IsDodging())
		{
			Movement->bUseControllerDesiredRotation = true;
		}
	}
}

FVector UVaelFloromantComponent::ClampAimToRoots(const FVector& Aim) const
{
	if (!IsRooted() || !bRootYawHeld || Aim.IsNearlyZero())
	{
		return Aim;
	}
	const float MaxTwist = UVaelFloromantSettings::Get()->MaxRootTwist;
	const float Turn = FRotator::NormalizeAxis(Aim.Rotation().Yaw - RootYaw);
	const float Clamped = FMath::Clamp(Turn, -MaxTwist, MaxTwist);
	return FMath::IsNearlyEqual(Turn, Clamped, 0.01f) ? Aim : FRotator(0.0f, Clamped - Turn, 0.0f).RotateVector(Aim);
}
