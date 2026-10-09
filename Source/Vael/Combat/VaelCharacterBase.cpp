// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VaelCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "Combat/VaelAttributeSet.h"
#include "Combat/VaelGameplayEffects.h"
#include "GameFramework/CharacterMovementComponent.h"
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

	AbilitySystemComponent->RegisterGameplayTagEvent(VaelTags::Status_Frozen, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &AVaelCharacterBase::OnFrozenChanged);
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UVaelAttributeSet::GetHealthAttribute()).AddUObject(this, &AVaelCharacterBase::HandleHealthChanged);
}

void AVaelCharacterBase::HandleHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	OnHealthChanged(ChangeData.OldValue, ChangeData.NewValue);
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

void AVaelCharacterBase::ApplyPull(const FVector& Location, float Speed, float DeltaSeconds)
{
	const FVector ToLocation = (Location - GetActorLocation()) * FVector(1.0f, 1.0f, 0.0f);
	const float Step = FMath::Min(Speed * DeltaSeconds, ToLocation.Size());

	if (Step > KINDA_SMALL_NUMBER)
	{
		// Swept, so walls and other characters still stop the pull
		AddActorWorldOffset(ToLocation.GetSafeNormal() * Step, true);
	}
}

FText AVaelCharacterBase::GetStatusText() const
{
	TArray<FString> Names;

	if (AbilitySystemComponent->HasMatchingGameplayTag(VaelTags::Status_Wet))
	{
		Names.Add(TEXT("nass"));
	}
	if (AbilitySystemComponent->HasMatchingGameplayTag(VaelTags::Status_Burning))
	{
		Names.Add(TEXT("brennt"));
	}
	if (AbilitySystemComponent->HasMatchingGameplayTag(VaelTags::Status_Frozen))
	{
		Names.Add(TEXT("gefroren"));
	}
	if (AbilitySystemComponent->HasMatchingGameplayTag(VaelTags::Status_Slowed))
	{
		Names.Add(TEXT("verlangsamt"));
	}
	if (AbilitySystemComponent->HasMatchingGameplayTag(VaelTags::Status_Marked))
	{
		Names.Add(TEXT("gezeichnet"));
	}
	if (AbilitySystemComponent->HasMatchingGameplayTag(VaelTags::Status_Feared))
	{
		Names.Add(TEXT("in Furcht"));
	}
	if (AbilitySystemComponent->HasMatchingGameplayTag(VaelTags::Status_Fevered))
	{
		Names.Add(TEXT("im Fieber"));
	}

	return FText::FromString(FString::Join(Names, TEXT(", ")));
}

void AVaelCharacterBase::OnFrozenChanged(const FGameplayTag Tag, int32 NewCount)
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();

	if (NewCount > 0)
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}
	else if (Movement->MovementMode == MOVE_None && !IsDefeated())
	{
		// Walking, or flying for creatures of the air
		Movement->SetDefaultMovementMode();
	}
}

float AVaelCharacterBase::GetCorruption() const
{
	return AttributeSet->GetCorruption();
}
