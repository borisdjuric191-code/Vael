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
#include "Magic/VaelGameplayTags.h"
#include "Magic/VaelGrimoireSubsystem.h"
#include "Magic/VaelMagicSettings.h"
#include "Vael.h"
#include "World/VaelRegion.h"
#include "World/VaelWorldSettings.h"

UVaelElementComponent::UVaelElementComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	QuickSlots.Init(nullptr, NumQuickSlots);
	QuickSlotReadyTimes.Init(0.0f, NumQuickSlots);
	QuickSlotCooldowns.Init(0.0f, NumQuickSlots);
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

		// Quick slots start with the known combinations, in the order of the grimoire
		for (const UVaelFormula* Formula : Grimoire->GetAllFormulas())
		{
			if (Grimoire->IsFormulaKnown(Formula))
			{
				FillEmptyQuickSlot(Formula);
			}
		}

		FormulaLearnedHandle = Grimoire->OnFormulaLearned.AddUObject(this, &UVaelElementComponent::OnFormulaLearned);
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

	// While a spell is channeled the cast is ignored and the queue waits for later
	if (AbilitySystem->HasMatchingGameplayTag(VaelTags::State_Channeling))
	{
		return EVaelCastResult::Blocked;
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
			// The echo reveals where the formula can be found
			Grimoire->AddEcho(Formula);
			return FinishCast(EVaelCastResult::UnknownFormula, Formula);
		}

		if (FMath::FRand() >= MagicSettings->DiscoveryChance)
		{
			UnstableDischarge();
			return FinishCast(EVaelCastResult::UnstableDischarge, Formula);
		}

		// Learning grants the ability to every mage, this one included
		Grimoire->LearnFormula(Formula, NSLOCTEXT("VaelMagic", "LearnedByExperiment", "Durch Experimentieren entdeckt."));
	}

	const int32 NumEnvironmentElements = QueueFromEnvironment.FilterByPredicate([](bool bFromEnvironment) { return bFromEnvironment; }).Num();

	return FinishCast(ActivateFormula(Formula, Queue.Num(), NumEnvironmentElements, false), Formula);
}

EVaelCastResult UVaelElementComponent::ActivateFormula(UVaelFormula* Formula, int32 NumElements, int32 NumEnvironmentElements, bool bFromQuickSlot)
{
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystem();
	const FGameplayAbilitySpecHandle* AbilityHandle = FormulaAbilities.Find(Formula);
	if (AbilitySystem == nullptr || AbilityHandle == nullptr)
	{
		return EVaelCastResult::Blocked;
	}

	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();

	// Elements drawn from the environment make the formula cheaper and stronger.
	// Casters touched by the Mark get the smaller bonus: the pure path is the stronger one here.
	const bool bPure = AbilitySystem->GetNumericAttribute(UVaelAttributeSet::GetCorruptionAttribute()) < 1.0f;
	PendingCast.ManaCost = MagicSettings->GetManaCost(NumElements, NumEnvironmentElements);
	PendingCast.Power = 1.0f + NumEnvironmentElements * (bPure ? MagicSettings->EnvironmentPowerBonusPure : MagicSettings->EnvironmentPowerBonusCorrupted);

	// Quick slots trade cost and power for speed
	if (bFromQuickSlot)
	{
		PendingCast.ManaCost = FMath::RoundToFloat(PendingCast.ManaCost * MagicSettings->QuickManaCostMultiplier);
		PendingCast.Power *= MagicSettings->QuickPowerMultiplier;
	}

	// The weather favors some elements and works against others
	const UVaelWorldSettings* WorldSettings = UVaelWorldSettings::Get();
	const AVaelRegion* Region = AVaelRegion::GetRegionAt(GetWorld(), GetOwner()->GetActorLocation());
	const EVaelWeather Weather = Region != nullptr ? Region->GetWeather() : EVaelWeather::Clear;
	const bool bAttuned = AbilitySystem->HasMatchingGameplayTag(VaelTags::Gear_WeatherAttunement);

	for (const FVaelWeatherSpellModifier& Modifier : WorldSettings->SpellModifiers)
	{
		if (Modifier.Weather != Weather || Modifier.Element != Formula->DamageElement)
		{
			continue;
		}

		// Attuned gear doubles what the weather gives, never what it takes
		const float CostMultiplier = bAttuned && Modifier.ManaCostMultiplier < 1.0f ? 1.0f - 2.0f * (1.0f - Modifier.ManaCostMultiplier) : Modifier.ManaCostMultiplier;
		const float PowerMultiplier = bAttuned && Modifier.PowerMultiplier > 1.0f ? 1.0f + 2.0f * (Modifier.PowerMultiplier - 1.0f) : Modifier.PowerMultiplier;

		PendingCast.ManaCost = FMath::Max(FMath::RoundToFloat(PendingCast.ManaCost * FMath::Max(CostMultiplier, 0.0f)), MagicSettings->MinManaCost);
		PendingCast.Power *= PowerMultiplier;
	}

	if (AbilitySystem->GetNumericAttribute(UVaelAttributeSet::GetManaAttribute()) < PendingCast.ManaCost)
	{
		return EVaelCastResult::NotEnoughMana;
	}

	// Wetness has to be known before the formula runs
	const bool bBacklash = Formula->bLightning && AbilitySystem->HasMatchingGameplayTag(VaelTags::Status_Wet) && !AbilitySystem->HasMatchingGameplayTag(VaelTags::Gear_WeatherWard);
	const float BacklashDamage = Formula->Damage * PendingCast.Power * WorldSettings->WetLightningBacklashShare;

	if (!AbilitySystem->TryActivateAbility(*AbilityHandle))
	{
		return EVaelCastResult::Blocked;
	}

	// Lightning cast with wet hands runs through the caster too
	if (bBacklash && BacklashDamage > 0.0f)
	{
		UVaelCombatStatics::DealDamage(GetOwner(), GetOwner(), BacklashDamage, EVaelElement::Air);
		OnLightningBacklash.Broadcast(BacklashDamage);
	}

	// Mark corrupts the caster and the region around them
	const int32 NumMarkElements = Formula->Elements.FilterByPredicate([](EVaelElement Element) { return Element == EVaelElement::Mark; }).Num();
	if (NumMarkElements > 0)
	{
		const float Corruption = AbilitySystem->GetNumericAttribute(UVaelAttributeSet::GetCorruptionAttribute());
		AbilitySystem->SetNumericAttributeBase(UVaelAttributeSet::GetCorruptionAttribute(), FMath::Min(Corruption + NumMarkElements * WorldSettings->PlayerCorruptionPerMarkElement, 100.0f));

		if (AVaelRegion* CastRegion = AVaelRegion::GetRegionAt(GetWorld(), GetOwner()->GetActorLocation()))
		{
			CastRegion->AddCorruption(NumMarkElements * WorldSettings->RegionCorruptionPerMarkElement);
		}
	}

	return EVaelCastResult::Success;
}

EVaelCastResult UVaelElementComponent::CastQuickSlot(int32 SlotIndex)
{
	UVaelFormula* Formula = GetQuickSlotFormula(SlotIndex);
	UVaelGrimoireSubsystem* Grimoire = GetGrimoire();

	// Quick slots wait until the channeled spell has ended
	const UAbilitySystemComponent* AbilitySystem = GetAbilitySystem();
	if (AbilitySystem != nullptr && AbilitySystem->HasMatchingGameplayTag(VaelTags::State_Channeling))
	{
		return EVaelCastResult::Blocked;
	}

	EVaelCastResult Result = EVaelCastResult::EmptyQueue;

	if (Formula != nullptr && Grimoire != nullptr && Grimoire->IsFormulaKnown(Formula))
	{
		if (GetWorld()->GetTimeSeconds() < QuickSlotReadyTimes[SlotIndex])
		{
			Result = EVaelCastResult::OnCooldown;
		}
		else
		{
			// Whether an element comes from the environment is decided right now
			int32 NumEnvironmentElements = 0;
			for (const EVaelElement Element : Formula->Elements)
			{
				NumEnvironmentElements += UVaelEnvironmentStatics::IsElementInEnvironment(GetOwner(), Element) ? 1 : 0;
			}

			Result = ActivateFormula(Formula, Formula->Elements.Num(), NumEnvironmentElements, true);

			if (Result == EVaelCastResult::Success)
			{
				QuickSlotCooldowns[SlotIndex] = Formula->QuickCooldown;
				QuickSlotReadyTimes[SlotIndex] = GetWorld()->GetTimeSeconds() + Formula->QuickCooldown;
			}
		}
	}

	UE_LOG(LogVael, Verbose, TEXT("'%s' casts quick slot %d: %s (%s), cost %.0f mana, power %.2f"), *GetNameSafe(GetOwner()), SlotIndex,
		*UEnum::GetValueAsString(Result), Formula != nullptr ? *Formula->DisplayName.ToString() : TEXT("empty"), PendingCast.ManaCost, PendingCast.Power);

	PendingCast = FVaelPendingCast();
	OnCastFinished.Broadcast(Result, Formula);

	return Result;
}

void UVaelElementComponent::AssignQuickSlot(int32 SlotIndex, UVaelFormula* Formula)
{
	if (!QuickSlots.IsValidIndex(SlotIndex))
	{
		return;
	}

	// A formula sits on one slot at most
	if (Formula != nullptr)
	{
		const int32 PreviousSlot = QuickSlots.Find(Formula);
		if (PreviousSlot != INDEX_NONE)
		{
			QuickSlots[PreviousSlot] = nullptr;
		}
	}

	QuickSlots[SlotIndex] = Formula;
}

UVaelFormula* UVaelElementComponent::GetQuickSlotFormula(int32 SlotIndex) const
{
	return QuickSlots.IsValidIndex(SlotIndex) ? QuickSlots[SlotIndex].Get() : nullptr;
}

float UVaelElementComponent::GetQuickSlotCooldownFraction(int32 SlotIndex) const
{
	if (!QuickSlotReadyTimes.IsValidIndex(SlotIndex) || QuickSlotCooldowns[SlotIndex] <= 0.0f)
	{
		return 0.0f;
	}

	const float Remaining = QuickSlotReadyTimes[SlotIndex] - GetWorld()->GetTimeSeconds();
	return FMath::Clamp(Remaining / QuickSlotCooldowns[SlotIndex], 0.0f, 1.0f);
}

void UVaelElementComponent::OnFormulaLearned(const UVaelFormula* Formula, const FText& Reason)
{
	GrantFormula(Formula);
	FillEmptyQuickSlot(Formula);
}

void UVaelElementComponent::FillEmptyQuickSlot(const UVaelFormula* Formula)
{
	// Single elements are on the face buttons already
	if (Formula == nullptr || Formula->Elements.Num() < 2 || QuickSlots.Contains(Formula))
	{
		return;
	}

	const int32 EmptySlot = QuickSlots.Find(nullptr);
	if (EmptySlot != INDEX_NONE)
	{
		QuickSlots[EmptySlot] = const_cast<UVaelFormula*>(Formula);
	}
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
