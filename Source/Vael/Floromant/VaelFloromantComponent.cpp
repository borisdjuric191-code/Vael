// Copyright Epic Games, Inc. All Rights Reserved.

#include "Floromant/VaelFloromantComponent.h"
#include "AbilitySystemComponent.h"
#include "Combat/VaelAttributeSet.h"
#include "Engine/World.h"
#include "Floromant/VaelPlant.h"
#include "Floromant/VaelSeed.h"
#include "Player/VaelCharacter.h"
#include "UI/VaelCombatTextSubsystem.h"
#include "World/VaelGround.h"
#include "World/VaelSafeZone.h"
#include "Vael.h"

UVaelFloromantComponent::UVaelFloromantComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

AVaelCharacter* UVaelFloromantComponent::GetCharacter() const
{
	return Cast<AVaelCharacter>(GetOwner());
}

void UVaelFloromantComponent::SelectSeed(EVaelSeedKind Kind)
{
	SelectedSeed = Kind;
}

bool UVaelFloromantComponent::ThrowSeed(const FVector& AimLocation)
{
	AVaelCharacter* Character = GetCharacter();
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	const float Now = GetWorld()->GetTimeSeconds();
	if (Character == nullptr || Character->IsDowned() || Now < NextThrowTime)
	{
		return false;
	}

	// Nothing grows in the camp: it is a safe zone, like for the spells of the mage
	if (AVaelSafeZone::IsSafe(GetWorld(), Character->GetActorLocation()))
	{
		return false;
	}

	const float Cost = Settings->GetPlant(SelectedSeed).SaftCost;
	if (Character->GetMana() < Cost)
	{
		UVaelCombatTextSubsystem::PostPickup(Character, NSLOCTEXT("VaelFloromant", "NoSaft", "Zu wenig Saft"), FLinearColor(FColor(158, 143, 125)));
		NextThrowTime = Now + 0.4f;
		return false;
	}

	// Where it lands: the aim point, no farther than the throw range, on the ground below
	const FVector Feet = Character->GetActorLocation() - FVector(0.0f, 0.0f, Character->GetSimpleCollisionHalfHeight());
	FVector Offset = (AimLocation - Feet) * FVector(1.0f, 1.0f, 0.0f);
	if (Offset.Size() < 120.0f)
	{
		Offset = Character->GetActorForwardVector() * 120.0f;
	}
	FVector Target = Feet + Offset.GetClampedToMaxSize(Settings->ThrowRange);

	FHitResult GroundHit;
	if (VaelGround::TraceGround(GetWorld(), Target + FVector(0.0f, 0.0f, 300.0f), Target - FVector(0.0f, 0.0f, 600.0f), GroundHit, Character))
	{
		Target = GroundHit.Location;
	}

	UAbilitySystemComponent* AbilitySystem = Character->GetAbilitySystemComponent();
	AbilitySystem->SetNumericAttributeBase(UVaelAttributeSet::GetManaAttribute(), Character->GetMana() - Cost);
	NextThrowTime = Now + Settings->ThrowCooldown;

	const FVector Hand = Character->GetActorLocation() + Character->GetActorForwardVector() * 30.0f + FVector(0.0f, 0.0f, 40.0f);
	return AVaelSeed::Throw(Character, SelectedSeed, Hand, Target) != nullptr;
}

void UVaelFloromantComponent::PlantSeed(EVaelSeedKind Kind, const FVector& GroundLocation, float Yaw)
{
	AVaelCharacter* Character = GetCharacter();
	if (Character == nullptr)
	{
		return;
	}

	// The oldest plants wither once there would be too many
	Plants.RemoveAll([](const TWeakObjectPtr<AVaelPlant>& Plant) { return !Plant.IsValid() || Plant->IsWithering(); });
	while (Plants.Num() >= UVaelFloromantSettings::Get()->MaxPlants)
	{
		Plants[0]->Wither();
		Plants.RemoveAt(0);
	}

	if (AVaelPlant* Plant = AVaelPlant::GrowPlant(Kind, Character, GroundLocation, Yaw))
	{
		Plants.Add(Plant);
	}
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
	for (const TWeakObjectPtr<AVaelPlant>& Plant : Plants)
	{
		if (Plant.IsValid())
		{
			Plant->Wither();
		}
	}
	Plants.Reset();
}

FText UVaelFloromantComponent::GetBeltName(int32 Index) const
{
	return UVaelFloromantSettings::Get()->GetPlant(static_cast<EVaelSeedKind>(Index)).DisplayName;
}

FLinearColor UVaelFloromantComponent::GetBeltColor(int32 Index) const
{
	return UVaelFloromantSettings::Get()->GetPlant(static_cast<EVaelSeedKind>(Index)).Color;
}

float UVaelFloromantComponent::GetCooldownLeft(EVaelHUDAbility Ability) const
{
	return Ability == EVaelHUDAbility::Sow ? FMath::Max(0.0f, NextThrowTime - GetWorld()->GetTimeSeconds()) : 0.0f;
}

float UVaelFloromantComponent::GetCooldownDuration(EVaelHUDAbility Ability) const
{
	return Ability == EVaelHUDAbility::Sow ? UVaelFloromantSettings::Get()->ThrowCooldown : 0.0f;
}
