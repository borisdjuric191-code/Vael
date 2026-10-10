// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Items/VaelItemTypes.h"
#include "Items/VaelMaterial.h"
#include "VaelItemSettings.generated.h"

/** A kind of item that can be rolled, like a robe or a ring */
USTRUCT(BlueprintType)
struct FVaelItemBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	FText Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	EVaelItemSlot Slot = EVaelItemSlot::Chest;
};

/**
 *  Rules for rolled items, the backpack and dropped items.
 *  Edited under Project Settings > Game > Vael Items, stored in DefaultGame.ini.
 */
UCLASS(config=Game, defaultconfig, meta = (DisplayName = "Vael Items"))
class UVaelItemSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	/** Constructor */
	UVaelItemSettings();

	/** Returns the settings object */
	static const UVaelItemSettings* Get() { return GetDefault<UVaelItemSettings>(); }

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** Rolls an item of a random base, at least of the given rarity */
	FVaelItem RollItem(EVaelRarity MinRarity = EVaelRarity::Common) const;

	/** Kinds of items that drop; for now those of the mage and jewelry */
	UPROPERTY(config, EditAnywhere, Category="Rolling")
	TArray<FVaelItemBase> Bases;

	/** Lowest and highest value each property rolls with; properties missing here never roll */
	UPROPERTY(config, EditAnywhere, Category="Rolling")
	TMap<EVaelItemStat, FVector2D> StatRanges;

	/** How often each rarity rolls, relative to the others; legendary items are handmade and never rolled */
	UPROPERTY(config, EditAnywhere, Category="Rolling")
	TMap<EVaelRarity, float> RarityWeights;

	/** Fewest and most properties per rarity */
	UPROPERTY(config, EditAnywhere, Category="Rolling")
	TMap<EVaelRarity, FIntPoint> StatCountByRarity;

	/** Fields of the backpack; every item takes exactly one */
	UPROPERTY(config, EditAnywhere, Category="Inventory", meta = (ClampMin = 1))
	int32 BackpackSize = 35;

	/** The owner of a dropped item picks it up this close, in cm */
	UPROPERTY(config, EditAnywhere, Category="Inventory", meta = (ClampMin = 0))
	float ItemPickupRadius = 120.0f;

	/** Players this close to something they can use, like a chest, get the prompt, in cm (prototype: 2 tiles) */
	UPROPERTY(config, EditAnywhere, Category="Inventory", meta = (ClampMin = 0))
	float InteractRange = 280.0f;

	/** On the gamepad the earth button interacts instead when something is close, but only if no enemy is this close, in cm; in safe zones always */
	UPROPERTY(config, EditAnywhere, Category="Inventory", meta = (ClampMin = 0))
	float InteractCombatRange = 1200.0f;

	/** Healing potion every player starts with, and how many */
	UPROPERTY(config, EditAnywhere, Category="Potions")
	TSoftObjectPtr<UVaelMaterial> StartingPotion = TSoftObjectPtr<UVaelMaterial>(FSoftObjectPath(TEXT("/Game/Vael/Items/Materials/DA_Material_Heiltrank.DA_Material_Heiltrank")));

	UPROPERTY(config, EditAnywhere, Category="Potions", meta = (ClampMin = 0))
	int32 StartingPotionCount = 2;

	/** Seconds after drinking before the next potion */
	UPROPERTY(config, EditAnywhere, Category="Potions", meta = (ClampMin = 0))
	float PotionCooldown = 1.5f;
};
