// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/VaelItemTypes.h"
#include "Items/VaelItemData.h"

#define LOCTEXT_NAMESPACE "VaelItems"

const FVaelLegendaryAbility* FVaelItem::GetLegendaryAbility() const
{
	const bool bHasAbility = Source != nullptr && (Rarity == EVaelRarity::Legendary || Rarity == EVaelRarity::Unique);
	return bHasAbility ? &Source->LegendaryAbility : nullptr;
}

void VaelItems::GetEquipSlots(EVaelItemSlot Slot, TArray<EVaelEquipSlot>& OutEquipSlots)
{
	OutEquipSlots.Reset();

	switch (Slot)
	{
	case EVaelItemSlot::Weapon:	OutEquipSlots.Add(EVaelEquipSlot::Weapon); break;
	case EVaelItemSlot::Offhand:	OutEquipSlots.Add(EVaelEquipSlot::Offhand); break;
	case EVaelItemSlot::Head:	OutEquipSlots.Add(EVaelEquipSlot::Head); break;
	case EVaelItemSlot::Chest:	OutEquipSlots.Add(EVaelEquipSlot::Chest); break;
	case EVaelItemSlot::Hands:	OutEquipSlots.Add(EVaelEquipSlot::Hands); break;
	case EVaelItemSlot::Feet:	OutEquipSlots.Add(EVaelEquipSlot::Feet); break;
	case EVaelItemSlot::Belt:	OutEquipSlots.Add(EVaelEquipSlot::Belt); break;
	case EVaelItemSlot::Amulet:	OutEquipSlots.Add(EVaelEquipSlot::Amulet); break;
	case EVaelItemSlot::Ring:	OutEquipSlots.Add(EVaelEquipSlot::Ring1); OutEquipSlots.Add(EVaelEquipSlot::Ring2); break;
	}
}

FLinearColor VaelItems::GetRarityColor(EVaelRarity Rarity)
{
	switch (Rarity)
	{
	case EVaelRarity::Magic:		return FLinearColor(FColor(0x6a, 0x9c, 0xf2));
	case EVaelRarity::Rare:			return FLinearColor(FColor(0xf0, 0xd0, 0x5a));
	case EVaelRarity::Legendary:	return FLinearColor(FColor(0xe8, 0x69, 0x2c));
	case EVaelRarity::Unique:		return FLinearColor(FColor(0xc9, 0xa2, 0x5f));
	default:						return FLinearColor(FColor(0xea, 0xdc, 0xc4));
	}
}

FText VaelItems::DescribeStat(const FVaelItemStatValue& StatValue)
{
	const FText Value = FText::AsNumber(FMath::RoundToInt(StatValue.Value));

	switch (StatValue.Stat)
	{
	case EVaelItemStat::MaxHealth:			return FText::Format(LOCTEXT("StatHealth", "+{0} Leben"), Value);
	case EVaelItemStat::MaxMana:			return FText::Format(LOCTEXT("StatMana", "+{0} Mana"), Value);
	case EVaelItemStat::ManaRegen:			return FText::Format(LOCTEXT("StatManaRegen", "+{0} Mana pro Sekunde"), Value);
	case EVaelItemStat::FireDamage:			return FText::Format(LOCTEXT("StatFire", "+{0} % Feuerschaden"), Value);
	case EVaelItemStat::WaterDamage:		return FText::Format(LOCTEXT("StatWater", "+{0} % Wasserschaden"), Value);
	case EVaelItemStat::EarthDamage:		return FText::Format(LOCTEXT("StatEarth", "+{0} % Erdschaden"), Value);
	case EVaelItemStat::AirDamage:			return FText::Format(LOCTEXT("StatAir", "+{0} % Luftschaden"), Value);
	case EVaelItemStat::EnvironmentPower:	return FText::Format(LOCTEXT("StatEnvironment", "+{0} % Kraft aus der Umwelt"), Value);
	case EVaelItemStat::QuickCooldown:		return FText::Format(LOCTEXT("StatQuickCooldown", "-{0} % Abklingzeit der Schnellformeln"), Value);
	case EVaelItemStat::LightningDamage:	return FText::Format(LOCTEXT("StatLightning", "+{0} % Blitzschaden"), Value);
	default:								return FText::GetEmpty();
	}
}

#undef LOCTEXT_NAMESPACE
