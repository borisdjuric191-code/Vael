// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VaelCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "Combat/VaelAttributeSet.h"
#include "Combat/VaelGameplayEffects.h"
#include "Magic/VaelGameplayTags.h"

AVaelCharacterBase::AVaelCharacterBase()
{
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));

	// The ability system finds attribute sets that are subobjects of its owner on its own
	AttributeSet = CreateDefaultSubobject<UVaelAttributeSet>(TEXT("AttributeSet"));
}

void AVaelCharacterBase::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	AttributeSet->InitMaxHealth(StartingHealth);
	AttributeSet->InitHealth(StartingHealth);
	AttributeSet->InitMaxMana(StartingMana);
	AttributeSet->InitMana(StartingMana);
}

void AVaelCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	if (ManaRegenPerSecond > 0.0f)
	{
		const FGameplayEffectSpecHandle RegenSpec = AbilitySystemComponent->MakeOutgoingSpec(UVaelGE_ManaRegen::StaticClass(), 1.0f, AbilitySystemComponent->MakeEffectContext());
		if (RegenSpec.IsValid())
		{
			RegenSpec.Data->SetSetByCallerMagnitude(VaelTags::SetByCaller_Mana, ManaRegenPerSecond * UVaelGE_ManaRegen::StepInterval);
			AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*RegenSpec.Data);
		}
	}
}

UAbilitySystemComponent* AVaelCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

float AVaelCharacterBase::GetHealth() const
{
	return AttributeSet->GetHealth();
}

float AVaelCharacterBase::GetMaxHealth() const
{
	return AttributeSet->GetMaxHealth();
}

float AVaelCharacterBase::GetMana() const
{
	return AttributeSet->GetMana();
}

float AVaelCharacterBase::GetMaxMana() const
{
	return AttributeSet->GetMaxMana();
}

void AVaelCharacterBase::ApplyKnockback(const FVector& Direction, float Speed)
{
	const FVector GroundDirection = Direction.GetSafeNormal2D();
	if (Speed > 0.0f && !GroundDirection.IsNearlyZero())
	{
		LaunchCharacter(GroundDirection * Speed, true, false);
	}
}
