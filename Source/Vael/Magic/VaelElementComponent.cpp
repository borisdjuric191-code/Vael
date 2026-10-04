// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelElementComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Combat/VaelAttributeSet.h"
#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelCombatStatics.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Magic/VaelEnvironmentStatics.h"
#include "Magic/VaelFormula.h"
#include "Magic/VaelFormulaAbility.h"
#include "Magic/VaelGrimoireSubsystem.h"
#include "Magic/VaelMagicSettings.h"
#include "Vael.h"

UVaelElementComponent::UVaelElementComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVaelElementComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UVaelGrimoireSubsystem* Grimoire = GetGrimoire())
	{
		for (const UVaelFormula* Formula : Grimoire->GetKnownFormulas())
		{
			GrantFormula(Formula);
		}

		FormulaLearnedHandle = Grimoire->OnFormulaLearned.AddUObject(this, &UVaelElementComponent::GrantFormula);
	}
}

void UVaelElementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UVaelGrimoireSubsystem* Grimoire = GetGrimoire())
	{
		Grimoire->OnFormulaLearned.Remove(FormulaLearnedHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void UVaelElementComponent::GrantFormula(const UVaelFormula* Formula)
{
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystem();
	if (AbilitySystem == nullptr || Formula == nullptr || Formula->AbilityClass == nullptr || FormulaAbilities.Contains(Formula))
	{
		return;
	}

	// The formula asset travels with the ability as its source object
	const FGameplayAbilitySpec AbilitySpec(Formula->AbilityClass, 1, INDEX_NONE, const_cast<UVaelFormula*>(Formula));
	FormulaAbilities.Add(Formula, AbilitySystem->GiveAbility(AbilitySpec));
}

bool UVaelElementComponent::AddElement(EVaelElement Element)
{
	if (Queue.Num() >= GetNumSlots())
	{
		return false;
	}

	// Where the element comes from is decided at the moment it is queued, not when the formula is cast
	Queue.Add(Element);
	QueueFromEnvironment.Add(UVaelEnvironmentStatics::IsElementInEnvironment(GetOwner(), Element));
	OnQueueChanged.Broadcast();

	return true;
}

void UVaelElementComponent::ClearQueue()
{
	if (!Queue.IsEmpty())
	{
		Queue.Reset();
		QueueFromEnvironment.Reset();
		OnQueueChanged.Broadcast();
	}
}

EVaelCastResult UVaelElementComponent::CastQueue()
{
	if (Queue.IsEmpty())
	{
		return EVaelCastResult::EmptyQueue;
	}

	UAbilitySystemComponent* AbilitySystem = GetAbilitySystem();
	UVaelGrimoireSubsystem* Grimoire = GetGrimoire();
	if (AbilitySystem == nullptr || Grimoire == nullptr)
	{
		return FinishCast(EVaelCastResult::Blocked, nullptr);
	}

	UVaelFormula* Formula = Grimoire->FindFormula(Queue);
	if (Formula == nullptr)
	{
		return FinishCast(EVaelCastResult::NoFormula, nullptr);
	}

	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();

	if (!Grimoire->IsFormulaKnown(Formula))
	{
		// Free formulas can be discovered by trying them out, everything else has to be found in the world
		if (Formula->Source != EVaelFormulaSource::Free)
		{
			return FinishCast(EVaelCastResult::UnknownFormula, Formula);
		}

		if (FMath::FRand() >= MagicSettings->DiscoveryChance)
		{
			UnstableDischarge();
			return FinishCast(EVaelCastResult::UnstableDischarge, Formula);
		}

		// Learning grants the ability to every mage, this one included
		Grimoire->LearnFormula(Formula);
	}

	const FGameplayAbilitySpecHandle* AbilityHandle = FormulaAbilities.Find(Formula);
	if (AbilityHandle == nullptr)
	{
		return FinishCast(EVaelCastResult::Blocked, Formula);
	}

	// Elements drawn from the environment make the formula cheaper and stronger.
	// Casters corrupted by Mark get a smaller bonus; until corruption exists everybody counts as pure.
	const int32 NumEnvironmentElements = QueueFromEnvironment.FilterByPredicate([](bool bFromEnvironment) { return bFromEnvironment; }).Num();

	PendingCast.ManaCost = MagicSettings->GetManaCost(Queue.Num(), NumEnvironmentElements);
	PendingCast.Power = 1.0f + NumEnvironmentElements * MagicSettings->EnvironmentPowerBonusPure;

	if (AbilitySystem->GetNumericAttribute(UVaelAttributeSet::GetManaAttribute()) < PendingCast.ManaCost)
	{
		return FinishCast(EVaelCastResult::NotEnoughMana, Formula);
	}

	const bool bActivated = AbilitySystem->TryActivateAbility(*AbilityHandle);

	return FinishCast(bActivated ? EVaelCastResult::Success : EVaelCastResult::Blocked, Formula);
}

EVaelCastResult UVaelElementComponent::FinishCast(EVaelCastResult Result, const UVaelFormula* Formula)
{
	UE_LOG(LogVael, Verbose, TEXT("'%s' casts %d elements: %s (%s), cost %.0f mana, power %.2f"), *GetNameSafe(GetOwner()), Queue.Num(),
		*UEnum::GetValueAsString(Result), Formula != nullptr ? *Formula->DisplayName.ToString() : TEXT("no formula"), PendingCast.ManaCost, PendingCast.Power);

	PendingCast = FVaelPendingCast();

	Queue.Reset();
	QueueFromEnvironment.Reset();
	OnQueueChanged.Broadcast();
	OnCastFinished.Broadcast(Result, Formula);

	return Result;
}

void UVaelElementComponent::UnstableDischarge()
{
	AActor* Caster = GetOwner();
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();

	FVaelSpellHit Hit;
	Hit.Damage = MagicSettings->UnstableDamage;
	Hit.Element = EVaelElement::Air;
	Hit.Knockback = MagicSettings->UnstableKnockback;

	const FVector Origin = Caster->GetActorLocation();
	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		const FVector ToTarget = It->GetActorLocation() - Origin;
		if (ToTarget.SizeSquared2D() <= FMath::Square(MagicSettings->UnstableRadius))
		{
			UVaelCombatStatics::ApplySpellHit(Caster, *It, Hit, ToTarget);
		}
	}

	UVaelCombatStatics::DealDamage(Caster, Caster, MagicSettings->UnstableSelfDamage, EVaelElement::Air);
}

UAbilitySystemComponent* UVaelElementComponent::GetAbilitySystem() const
{
	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
}

UVaelGrimoireSubsystem* UVaelElementComponent::GetGrimoire() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;

	return GameInstance != nullptr ? GameInstance->GetSubsystem<UVaelGrimoireSubsystem>() : nullptr;
}
