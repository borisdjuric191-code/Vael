// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/VaelItemData.h"

FVaelItem UVaelItemData::MakeItem() const
{
	FVaelItem Item;
	Item.DisplayName = DisplayName;
	Item.Slot = Slot;
	Item.Rarity = Rarity;
	Item.Stats = Stats;
	Item.Source = this;

	if (Item.Stats.Num() > VaelItems::MaxStats)
	{
		Item.Stats.SetNum(VaelItems::MaxStats);
	}

	return Item;
}
