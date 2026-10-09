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
#include "Items/VaelInventory.h"
#include "Magic/VaelEnvironmentStatics.h"
#include "Magic/VaelFormula.h"
#include "Magic/VaelFormulaAbility.h"
#include "Magic/VaelGameplayTags.h"
#include "Magic/VaelGrimoireSubsystem.h"
#include "Magic/VaelMagicSettings.h"
#include "UI/VaelNoticeSubsystem.h"
#include "Story/VaelQuestSubsystem.h"
#include "Vael.h"
#include "World/VaelRegion.h"
#include "World/VaelWorldSettings.h"

namespace
{
	/** Percent more damage worn gear gives formulas of the element */
	float GetGearElementBonus(const UVaelInventory& Inventory, EVaelElement Element)
	{
		switch (Element)
		{
		case EVaelElement::Fire:	return Inventory.GetStatTotal(EVaelItemStat::FireDamage);
		case EVaelElement::Water:	return Inventory.GetStatTotal(EVaelItemStat::WaterDamage);
		case EVaelElement::Earth:	return Inventory.GetStatTotal(EVaelItemStat::EarthDamage);
		case EVaelElement::Air:		return Inventory.GetStatTotal(EVaelItemStat::AirDamage);
		default:					return 0.0f;
		}
	}
}

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

	// The fifth element can't be touched before the Mark awakens (Act III)
	const UVaelGrimoireSubsystem* Grimoire = GetGrimoire();
	if (Element == EVaelElement::Mark && (Grimoire == nullptr || !Grimoire->IsMarkAwakened()))
	{
		UVaelNoticeSubsystem::Post(this, NSLOCTEXT("VaelMagic", "MarkStirs", "Etwas in dir regt sich"),
			NSLOCTEXT("VaelMagic", "MarkAsleep", "Das Mark schläft noch in dir."), UVaelMagicSettings::Get()->GetElementColor(EVaelElement::Mark), 2.5f);
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

	// Worn gear can make the environment give more
	const UVaelInventory* Inventory = GetOwner()->FindComponentByClass<UVaelInventory>();
	const float GearEnvironmentFactor = 1.0f + (Inventory != nullptr ? Inventory->GetStatTotal(EVaelItemStat::EnvironmentPower) / 100.0f : 0.0f);
	PendingCast.Power = 1.0f + NumEnvironmentElements * (bPure ? MagicSettings->EnvironmentPowerBonusPure : MagicSettings->EnvironmentPowerBonusCorrupted) * GearEnvironmentFactor;

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

	// Worn gear strengthens formulas of its elements, and lightning
	if (Inventory != nullptr)
	{
		const float LightningBonus = Formula->bLightning ? Inventory->GetStatTotal(EVaelItemStat::LightningDamage) : 0.0f;
		PendingCast.Power *= 1.0f + (GetGearElementBonus(*Inventory, Formula->DamageElement) + LightningBonus) / 100.0f;
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
			CastRegion->AddScar(Formula->RegionScar);
		}

		// Deep in the Mark every use of it hurts, and it starts to whisper
		const float NewCorruption = AbilitySystem->GetNumericAttribute(UVaelAttributeSet::GetCorruptionAttribute());
		if (NewCorruption >= WorldSettings->CorruptionPainThreshold)
		{
			UVaelCombatStatics::DealDamage(GetOwner(), GetOwner(), NumMarkElements * WorldSettings->CorruptionPainPerMarkElement, EVaelElement::Mark);
		}

		if (NewCorruption >= WorldSettings->CorruptionWhisperThreshold && FMath::FRand() < WorldSettings->CorruptionWhisperChance)
		{
			UVaelNoticeSubsystem::Post(this, NSLOCTEXT("VaelMagic", "MarkWhisper", "…tiefer…"), NSLOCTEXT("VaelMagic", "MarkWhisperDetail", "Das Mark flüstert in deinem Kopf."),
				FLinearColor(FColor(185, 139, 232)), 3.0f);
		}
	}

	// Some formulas are paid with blood; they never kill the caster
	if (Formula->HealthCost > 0.0f)
	{
		const float Health = AbilitySystem->GetNumericAttribute(UVaelAttributeSet::GetHealthAttribute());
		AbilitySystem->SetNumericAttributeBase(UVaelAttributeSet::GetHealthAttribute(), FMath::Max(1.0f, Health - Formula->HealthCost));
	}

	if (UVaelQuestSubsystem* Quests = UVaelQuestSubsystem::Get(this))
	{
		Quests->NotifyCast(Formula->GetFName());
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
		if (Grimoire->IsBlockedByMark(Formula))
		{
			Result = EVaelCastResult::MarkAsleep;
		}
		else if (GetWorld()->GetTimeSeconds() < QuickSlotReadyTimes[SlotIndex])
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
				// Worn gear can shorten the cooldown
				const UVaelInventory* Inventory = GetOwner()->FindComponentByClass<UVaelInventory>();
				const float CooldownReduction = Inventory != nullptr ? FMath::Clamp(Inventory->GetStatTotal(EVaelItemStat::QuickCooldown) / 100.0f, 0.0f, 0.9f) : 0.0f;
				const float Cooldown = Formula->QuickCooldown * (1.0f - CooldownReduction);

				QuickSlotCooldowns[SlotIndex] = Cooldown;
				QuickSlotReadyTimes[SlotIndex] = GetWorld()->GetTimeSeconds() + Cooldown;
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
	// Single elements are on the face buttons already; Mark formulas wait until the Mark awakens
	const UVaelGrimoireSubsystem* Grimoire = GetGrimoire();
	if (Formula == nullptr || Formula->Elements.Num() < 2 || QuickSlots.Contains(Formula) || (Grimoire != nullptr && Grimoire->IsBlockedByMark(Formula)))
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
