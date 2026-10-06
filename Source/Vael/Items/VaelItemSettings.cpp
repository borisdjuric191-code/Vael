// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/VaelItemSettings.h"

#define LOCTEXT_NAMESPACE "VaelItems"

UVaelItemSettings::UVaelItemSettings()
{
	// Mage gear from the Lore-Bibel (staffs, wands, focus, robes) and jewelry everyone can wear
	const auto AddBase = [this](const FText& Name, EVaelItemSlot Slot)
	{
		FVaelItemBase& Base = Bases.AddDefaulted_GetRef();
		Base.Name = Name;
		Base.Slot = Slot;
	};

	AddBase(LOCTEXT("BaseStaff", "Stab"), EVaelItemSlot::Weapon);
	AddBase(LOCTEXT("BaseWand", "Zauberstab"), EVaelItemSlot::Weapon);
	AddBase(LOCTEXT("BaseOrb", "Fokuskugel"), EVaelItemSlot::Offhand);
	AddBase(LOCTEXT("BaseHood", "Kapuze"), EVaelItemSlot::Head);
	AddBase(LOCTEXT("BaseRobe", "Robe"), EVaelItemSlot::Chest);
	AddBase(LOCTEXT("BaseGloves", "Handschuhe"), EVaelItemSlot::Hands);
	AddBase(LOCTEXT("BaseBoots", "Stiefel"), EVaelItemSlot::Feet);
	AddBase(LOCTEXT("BaseBelt", "Gürtel"), EVaelItemSlot::Belt);
	AddBase(LOCTEXT("BaseAmulet", "Amulett"), EVaelItemSlot::Amulet);
	AddBase(LOCTEXT("BaseRing", "Ring"), EVaelItemSlot::Ring);

	StatRanges.Add(EVaelItemStat::MaxHealth, FVector2D(5.0f, 20.0f));
	StatRanges.Add(EVaelItemStat::MaxMana, FVector2D(5.0f, 20.0f));
	StatRanges.Add(EVaelItemStat::ManaRegen, FVector2D(1.0f, 3.0f));
	StatRanges.Add(EVaelItemStat::FireDamage, FVector2D(5.0f, 15.0f));
	StatRanges.Add(EVaelItemStat::WaterDamage, FVector2D(5.0f, 15.0f));
	StatRanges.Add(EVaelItemStat::EarthDamage, FVector2D(5.0f, 15.0f));
	StatRanges.Add(EVaelItemStat::AirDamage, FVector2D(5.0f, 15.0f));
	StatRanges.Add(EVaelItemStat::EnvironmentPower, FVector2D(5.0f, 15.0f));
	StatRanges.Add(EVaelItemStat::QuickCooldown, FVector2D(5.0f, 10.0f));

	RarityWeights.Add(EVaelRarity::Common, 60.0f);
	RarityWeights.Add(EVaelRarity::Magic, 30.0f);
	RarityWeights.Add(EVaelRarity::Rare, 10.0f);

	StatCountByRarity.Add(EVaelRarity::Common, FIntPoint(1, 1));
	StatCountByRarity.Add(EVaelRarity::Magic, FIntPoint(2, 2));
	StatCountByRarity.Add(EVaelRarity::Rare, FIntPoint(3, 4));
}

FVaelItem UVaelItemSettings::RollItem(EVaelRarity MinRarity) const
{
	FVaelItem Item;
	if (Bases.IsEmpty())
	{
		return Item;
	}

	// Rarity by weight, among those at least as rare as asked for
	float TotalWeight = 0.0f;
	for (const TPair<EVaelRarity, float>& Pair : RarityWeights)
	{
		TotalWeight += Pair.Key >= MinRarity ? FMath::Max(Pair.Value, 0.0f) : 0.0f;
	}

	Item.Rarity = MinRarity;
	float Pick = FMath::FRand() * TotalWeight;
	for (const TPair<EVaelRarity, float>& Pair : RarityWeights)
	{
		if (Pair.Key < MinRarity || Pair.Value <= 0.0f)
		{
			continue;
		}

		Item.Rarity = Pair.Key;
		Pick -= Pair.Value;
		if (Pick <= 0.0f)
		{
			break;
		}
	}

	const FVaelItemBase& Base = Bases[FMath::RandRange(0, Bases.Num() - 1)];
	Item.DisplayName = Base.Name;
	Item.Slot = Base.Slot;

	// Different properties, each once
	TArray<EVaelItemStat> Pool;
	StatRanges.GetKeys(Pool);

	const FIntPoint* CountRange = StatCountByRarity.Find(Item.Rarity);
	const int32 NumStats = FMath::Min(CountRange != nullptr ? FMath::RandRange(CountRange->X, CountRange->Y) : 1, FMath::Min(Pool.Num(), VaelItems::MaxStats));

	for (int32 StatIndex = 0; StatIndex < NumStats; ++StatIndex)
	{
		const int32 PoolIndex = FMath::RandRange(0, Pool.Num() - 1);
		const EVaelItemStat Stat = Pool[PoolIndex];
		Pool.RemoveAtSwap(PoolIndex);

		const FVector2D Range = StatRanges.FindRef(Stat);

		FVaelItemStatValue& StatValue = Item.Stats.AddDefaulted_GetRef();
		StatValue.Stat = Stat;
		StatValue.Value = FMath::RoundToFloat(FMath::FRandRange(Range.X, Range.Y));
	}

	return Item;
}

#undef LOCTEXT_NAMESPACE
