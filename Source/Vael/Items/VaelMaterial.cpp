// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/VaelMaterial.h"
#include "Story/VaelQuestSubsystem.h"

int32 UVaelShop::GetPrice(const UObject* WorldContext, int32 OfferIndex) const
{
	if (!Offers.IsValidIndex(OfferIndex))
	{
		return 0;
	}

	const UVaelQuestSubsystem* Quests = bGuildPrices ? UVaelQuestSubsystem::Get(WorldContext) : nullptr;
	return Quests != nullptr ? FMath::CeilToInt32(Offers[OfferIndex].Price * Quests->GetGuildPriceFactor()) : Offers[OfferIndex].Price;
}

int32 FVaelLootEntry::Roll() const
{
	if (Material.IsNull() || FMath::FRand() >= Chance)
	{
		return 0;
	}

	return FMath::RandRange(MinCount, FMath::Max(MinCount, MaxCount));
}
