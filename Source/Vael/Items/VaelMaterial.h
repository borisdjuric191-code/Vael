// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VaelMaterial.generated.h"

/**
 *  A crafting material: an ore, a plant or a part of a creature, like a Glutdruese or a Markkristall.
 *  Materials go into the material bag of each player, which stacks them and never runs full.
 */
UCLASS(BlueprintType)
class UVaelMaterial : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** Name shown to the players */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Material")
	FText DisplayName;

	/** What the material is and what it is good for */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Material", meta = (MultiLine = true))
	FText Description;

	/** Region the material comes from; the bag is sorted by it */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Material")
	FText Region;

	/** Color of its label and placeholder look */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Material")
	FLinearColor Color = FLinearColor(0.91f, 0.82f, 0.61f);

	/** Share of the maximum health a potion heals when drunk with the potion button; 0 for materials that are no potion */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Potion", meta = (ClampMin = 0, ClampMax = 1))
	float HealFraction = 0.0f;

	bool IsPotion() const { return HealFraction > 0.0f; }
};

/** One thing a trader sells */
USTRUCT(BlueprintType)
struct FVaelShopOffer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop")
	TSoftObjectPtr<UVaelMaterial> Material;

	/** Price in guild coins */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop", meta = (ClampMin = 0))
	int32 Price = 5;
};

/**
 *  What a trader like Maren Holt sells for guild coins. Each player buys with their own purse into their own bag.
 */
UCLASS(BlueprintType)
class UVaelShop : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** Name over the shop page, like "Gildenkontor" */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shop")
	FText Title;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shop")
	TArray<FVaelShopOffer> Offers;
};

/** One possible drop of a creature */
USTRUCT(BlueprintType)
struct FVaelLootEntry
{
	GENERATED_BODY()

	/** What drops */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loot")
	TSoftObjectPtr<UVaelMaterial> Material;

	/** Chance from 0 to 1 that it drops at all */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loot", meta = (ClampMin = 0, ClampMax = 1))
	float Chance = 1.0f;

	/** How many drop, from the lowest to the highest number */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loot", meta = (ClampMin = 1))
	int32 MinCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loot", meta = (ClampMin = 1))
	int32 MaxCount = 1;

	FVaelLootEntry() = default;

	FVaelLootEntry(const TCHAR* MaterialPath, float InChance, int32 InMinCount = 1, int32 InMaxCount = 1)
		: Material(FSoftObjectPath(MaterialPath)), Chance(InChance), MinCount(InMinCount), MaxCount(InMaxCount)
	{
	}

	/** Rolls the entry: the number that drops, 0 if nothing */
	int32 Roll() const;
};
