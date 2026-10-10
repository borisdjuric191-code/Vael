// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/VaelInventory.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Combat/VaelAttributeSet.h"
#include "Combat/VaelGameplayEffects.h"
#include "Items/VaelItemData.h"
#include "Items/VaelItemSettings.h"
#include "Magic/VaelGameplayTags.h"
#include "Vael.h"

namespace
{
	/** Shown for an empty place */
	const FVaelItem NoItem;
}

UVaelInventory::UVaelInventory()
{
	Equipped.SetNum(static_cast<int32>(EVaelEquipSlot::Count));
}

void UVaelInventory::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAbilitySystemComponent* AbilitySystem = GetAbilitySystem())
	{
		AbilitySystem->RemoveLooseGameplayTags(AppliedTags);
		AbilitySystem->RemoveActiveGameplayEffect(ManaRegenHandle);
	}

	Super::EndPlay(EndPlayReason);
}

bool UVaelInventory::AddItem(const FVaelItem& Item)
{
	if (!Item.IsValid())
	{
		return false;
	}

	// A free place of the right kind takes it right away
	TArray<EVaelEquipSlot> EquipSlots;
	VaelItems::GetEquipSlots(Item.Slot, EquipSlots);

	for (const EVaelEquipSlot EquipSlot : EquipSlots)
	{
		FVaelItem& Worn = Equipped[static_cast<int32>(EquipSlot)];
		if (!Worn.IsValid())
		{
			Worn = Item;
			ApplyEquipment();
			OnInventoryChanged.Broadcast();
			return true;
		}
	}

	if (IsBackpackFull())
	{
		return false;
	}

	Backpack.Add(Item);
	OnInventoryChanged.Broadcast();
	return true;
}

bool UVaelInventory::EquipFromBackpack(int32 BackpackIndex)
{
	if (!Backpack.IsValidIndex(BackpackIndex))
	{
		return false;
	}

	const FVaelItem Item = Backpack[BackpackIndex];

	// A free place of the right kind first, otherwise the first one; rings swap with the first ring
	TArray<EVaelEquipSlot> EquipSlots;
	VaelItems::GetEquipSlots(Item.Slot, EquipSlots);
	if (EquipSlots.IsEmpty())
	{
		return false;
	}

	EVaelEquipSlot Target = EquipSlots[0];
	for (const EVaelEquipSlot EquipSlot : EquipSlots)
	{
		if (!Equipped[static_cast<int32>(EquipSlot)].IsValid())
		{
			Target = EquipSlot;
			break;
		}
	}

	FVaelItem& Worn = Equipped[static_cast<int32>(Target)];
	if (Worn.IsValid())
	{
		Backpack[BackpackIndex] = Worn;
	}
	else
	{
		Backpack.RemoveAt(BackpackIndex);
	}

	Worn = Item;
	ApplyEquipment();
	OnInventoryChanged.Broadcast();
	return true;
}

bool UVaelInventory::Unequip(EVaelEquipSlot EquipSlot)
{
	FVaelItem& Worn = Equipped[static_cast<int32>(EquipSlot)];
	if (!Worn.IsValid() || IsBackpackFull())
	{
		return false;
	}

	Backpack.Add(Worn);
	Worn = FVaelItem();
	ApplyEquipment();
	OnInventoryChanged.Broadcast();
	return true;
}

const FVaelItem& UVaelInventory::GetEquipped(EVaelEquipSlot EquipSlot) const
{
	const int32 Index = static_cast<int32>(EquipSlot);
	return Equipped.IsValidIndex(Index) ? Equipped[Index] : NoItem;
}

bool UVaelInventory::IsBackpackFull() const
{
	return Backpack.Num() >= UVaelItemSettings::Get()->BackpackSize;
}

void UVaelInventory::AddCoins(int32 Amount)
{
	if (Amount > 0)
	{
		GuildCoins += Amount;
		OnInventoryChanged.Broadcast();
	}
}

bool UVaelInventory::SpendCoins(int32 Amount)
{
	if (Amount < 0 || GuildCoins < Amount)
	{
		return false;
	}

	GuildCoins -= Amount;
	OnInventoryChanged.Broadcast();
	return true;
}

float UVaelInventory::GetStatTotal(EVaelItemStat Stat) const
{
	float Total = 0.0f;

	const auto AddStats = [&Total, Stat](const TArray<FVaelItemStatValue>& Stats)
	{
		for (const FVaelItemStatValue& StatValue : Stats)
		{
			Total += StatValue.Stat == Stat ? StatValue.Value : 0.0f;
		}
	};

	for (const FVaelItem& Item : Equipped)
	{
		if (Item.IsValid())
		{
			AddStats(Item.Stats);

			if (const FVaelLegendaryAbility* Ability = Item.GetLegendaryAbility())
			{
				AddStats(Ability->BonusStats);
			}
		}
	}

	return Total;
}

void UVaelInventory::ApplyEquipment()
{
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystem();
	if (AbilitySystem == nullptr)
	{
		return;
	}

	// Maximum health and mana: take away the old bonus, add the new one, never more than the new maximum
	const auto ApplyMaximum = [AbilitySystem](const FGameplayAttribute& MaxAttribute, const FGameplayAttribute& CurrentAttribute, float& Applied, float Bonus)
	{
		const float NewMax = AbilitySystem->GetNumericAttributeBase(MaxAttribute) - Applied + Bonus;
		AbilitySystem->SetNumericAttributeBase(MaxAttribute, NewMax);
		Applied = Bonus;

		if (AbilitySystem->GetNumericAttribute(CurrentAttribute) > NewMax)
		{
			AbilitySystem->SetNumericAttributeBase(CurrentAttribute, NewMax);
		}
	};

	ApplyMaximum(UVaelAttributeSet::GetMaxHealthAttribute(), UVaelAttributeSet::GetHealthAttribute(), AppliedMaxHealth, GetStatTotal(EVaelItemStat::MaxHealth));
	ApplyMaximum(UVaelAttributeSet::GetMaxManaAttribute(), UVaelAttributeSet::GetManaAttribute(), AppliedMaxMana, GetStatTotal(EVaelItemStat::MaxMana));

	// Mana per second runs as a second regeneration next to the normal one
	AbilitySystem->RemoveActiveGameplayEffect(ManaRegenHandle);
	ManaRegenHandle.Invalidate();

	const float ManaRegen = GetStatTotal(EVaelItemStat::ManaRegen);
	if (ManaRegen > 0.0f)
	{
		const FGameplayEffectSpecHandle RegenSpec = AbilitySystem->MakeOutgoingSpec(UVaelGE_ManaRegen::StaticClass(), 1.0f, AbilitySystem->MakeEffectContext());
		if (RegenSpec.IsValid())
		{
			RegenSpec.Data->SetSetByCallerMagnitude(VaelTags::SetByCaller_Mana, ManaRegen * UVaelGE_ManaRegen::StepInterval);
			ManaRegenHandle = AbilitySystem->ApplyGameplayEffectSpecToSelf(*RegenSpec.Data);
		}
	}

	// Tags of the special abilities of legendary items
	FGameplayTagContainer Tags;
	for (const FVaelItem& Item : Equipped)
	{
		if (const FVaelLegendaryAbility* Ability = Item.IsValid() ? Item.GetLegendaryAbility() : nullptr)
		{
			Tags.AppendTags(Ability->GrantedTags);
		}
	}

	AbilitySystem->RemoveLooseGameplayTags(AppliedTags);
	AbilitySystem->AddLooseGameplayTags(Tags);
	AppliedTags = Tags;

	UE_LOG(LogVael, Verbose, TEXT("'%s' wears +%.0f health, +%.0f mana, +%.1f mana per second"), *GetNameSafe(GetOwner()), AppliedMaxHealth, AppliedMaxMana, ManaRegen);
}

UAbilitySystemComponent* UVaelInventory::GetAbilitySystem() const
{
	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
}
