// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelElementComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Combat/VaelAttributeSet.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
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

	Queue.Add(Element);
	OnQueueChanged.Broadcast();

	return true;
}

void UVaelElementComponent::ClearQueue()
{
	if (!Queue.IsEmpty())
	{
		Queue.Reset();
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
	const UVaelGrimoireSubsystem* Grimoire = GetGrimoire();
	if (AbilitySystem == nullptr || Grimoire == nullptr)
	{
		return FinishCast(EVaelCastResult::Blocked, nullptr);
	}

	const UVaelFormula* Formula = Grimoire->FindFormula(Queue);
	if (Formula == nullptr)
	{
		return FinishCast(EVaelCastResult::NoFormula, nullptr);
	}

	const FGameplayAbilitySpecHandle* AbilityHandle = FormulaAbilities.Find(Formula);
	if (AbilityHandle == nullptr || !Grimoire->IsFormulaKnown(Formula))
	{
		return FinishCast(EVaelCastResult::UnknownFormula, Formula);
	}

	// Elements drawn from the environment will lower the cost and raise the power once the environment system exists
	PendingCast.ManaCost = UVaelMagicSettings::Get()->GetManaCost(Queue.Num(), 0);
	PendingCast.Power = 1.0f;

	if (AbilitySystem->GetNumericAttribute(UVaelAttributeSet::GetManaAttribute()) < PendingCast.ManaCost)
	{
		return FinishCast(EVaelCastResult::NotEnoughMana, Formula);
	}

	const bool bActivated = AbilitySystem->TryActivateAbility(*AbilityHandle);

	return FinishCast(bActivated ? EVaelCastResult::Success : EVaelCastResult::Blocked, Formula);
}

EVaelCastResult UVaelElementComponent::FinishCast(EVaelCastResult Result, const UVaelFormula* Formula)
{
	UE_LOG(LogVael, Verbose, TEXT("'%s' casts %d elements: %s (%s)"), *GetNameSafe(GetOwner()), Queue.Num(),
		*UEnum::GetValueAsString(Result), Formula != nullptr ? *Formula->DisplayName.ToString() : TEXT("no formula"));

	PendingCast = FVaelPendingCast();

	Queue.Reset();
	OnQueueChanged.Broadcast();
	OnCastFinished.Broadcast(Result, Formula);

	return Result;
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
