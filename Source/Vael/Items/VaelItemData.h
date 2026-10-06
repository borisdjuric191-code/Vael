// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Items/VaelItemTypes.h"
#include "VaelItemData.generated.h"

/**
 *  A handmade item with fixed properties, like the legendary Sturmmantel.
 *  Rolled items don't need an asset; they come from the bases in the item settings.
 */
UCLASS(BlueprintType)
class UVaelItemData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** Name shown to the players */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
	FText DisplayName;

	/** A line of lore shown with the item */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item", meta = (MultiLine = true))
	FText Flavor;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
	EVaelItemSlot Slot = EVaelItemSlot::Chest;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
	EVaelRarity Rarity = EVaelRarity::Legendary;

	/** Properties of the item, at most four */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
	TArray<FVaelItemStatValue> Stats;

	/** Special ability, used by legendary items */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item", meta = (EditCondition = "Rarity == EVaelRarity::Legendary || Rarity == EVaelRarity::Unique"))
	FVaelLegendaryAbility LegendaryAbility;

	/** Makes an item a player can own */
	FVaelItem MakeItem() const;
};
