// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "VaelItemTypes.generated.h"

class UVaelItemData;

/** Where an item is worn */
UENUM(BlueprintType)
enum class EVaelItemSlot : uint8
{
	Weapon,
	Offhand,
	Head,
	Chest,
	Hands,
	Feet,
	Belt,
	Amulet,
	Ring
};

/** The places of the equipment; rings have two */
UENUM(BlueprintType)
enum class EVaelEquipSlot : uint8
{
	Weapon,
	Offhand,
	Head,
	Chest,
	Hands,
	Feet,
	Belt,
	Amulet,
	Ring1,
	Ring2,
	Count UMETA(Hidden)
};

/** How rare an item is; rarer items carry more properties */
UENUM(BlueprintType)
enum class EVaelRarity : uint8
{
	Common		UMETA(DisplayName = "Gewöhnlich"),
	Magic		UMETA(DisplayName = "Magisch"),
	Rare		UMETA(DisplayName = "Selten"),
	Legendary	UMETA(DisplayName = "Legendär"),
	Unique		UMETA(DisplayName = "Einzigartig")
};

/** A property an item can give */
UENUM(BlueprintType)
enum class EVaelItemStat : uint8
{
	/** More maximum health */
	MaxHealth,
	/** More maximum mana */
	MaxMana,
	/** More mana per second */
	ManaRegen,
	/** Percent more damage of fire formulas */
	FireDamage,
	/** Percent more damage of water formulas */
	WaterDamage,
	/** Percent more damage of earth formulas */
	EarthDamage,
	/** Percent more damage of air formulas */
	AirDamage,
	/** Percent more power from elements drawn from the environment */
	EnvironmentPower,
	/** Percent shorter cooldown of the quick slots */
	QuickCooldown,
	/** Percent more damage of lightning formulas like the chain lightning; only on legendary abilities */
	LightningDamage
};

/** One property of an item and its value */
USTRUCT(BlueprintType)
struct FVaelItemStatValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	EVaelItemStat Stat = EVaelItemStat::MaxHealth;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	float Value = 0.0f;
};

/** The special ability of a legendary item, on top of its properties */
USTRUCT(BlueprintType)
struct FVaelLegendaryAbility
{
	GENERATED_BODY()

	/** What the ability does, shown with the item */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item", meta = (MultiLine = true))
	FText Description;

	/** Tags the wearer carries, like Gear.WeatherWard for protection against rain and lightning */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	FGameplayTagContainer GrantedTags;

	/** Properties that come with the ability, not counted against the limit of four */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	TArray<FVaelItemStatValue> BonusStats;
};

/** One item a player owns: rolled from a base or made from an item asset */
USTRUCT(BlueprintType)
struct FVaelItem
{
	GENERATED_BODY()

	/** Name shown to the players */
	UPROPERTY(BlueprintReadOnly, Category="Item")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category="Item")
	EVaelItemSlot Slot = EVaelItemSlot::Chest;

	UPROPERTY(BlueprintReadOnly, Category="Item")
	EVaelRarity Rarity = EVaelRarity::Common;

	/** Up to four properties */
	UPROPERTY(BlueprintReadOnly, Category="Item")
	TArray<FVaelItemStatValue> Stats;

	/** Asset the item was made from, for legendary and unique items; null for rolled items */
	UPROPERTY(BlueprintReadOnly, Category="Item")
	TObjectPtr<const UVaelItemData> Source;

	/** True if this holds an item */
	bool IsValid() const { return !DisplayName.IsEmpty(); }

	/** The special ability, if the item has one */
	const FVaelLegendaryAbility* GetLegendaryAbility() const;
};

namespace VaelItems
{
	/** Highest number of properties an item rolls or carries */
	inline constexpr int32 MaxStats = 4;

	/** Equipment places an item of the slot fits into */
	void GetEquipSlots(EVaelItemSlot Slot, TArray<EVaelEquipSlot>& OutEquipSlots);

	/** Color of a rarity, for names and placeholders */
	FLinearColor GetRarityColor(EVaelRarity Rarity);

	/** Short text of a property, like "+12 Leben" or "+8 % Feuerschaden" */
	FText DescribeStat(const FVaelItemStatValue& StatValue);
}
