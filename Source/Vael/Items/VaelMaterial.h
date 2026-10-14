// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Magic/VaelElementTypes.h"
#include "VaelMaterial.generated.h"

/** What kind of crafting ingredient a material is; recipes ask for kinds, not for one certain plant */
UENUM(BlueprintType)
enum class EVaelIngredientKind : uint8
{
	/** Not an ingredient of the witch's kitchen, like a potion */
	None			UMETA(DisplayName = "Keine Zutat"),
	Petal			UMETA(DisplayName = "Blütenblatt"),
	Leaf			UMETA(DisplayName = "Blatt"),
	Pollen			UMETA(DisplayName = "Pollen"),
	Nectar			UMETA(DisplayName = "Nektar"),
	Sap				UMETA(DisplayName = "Saft"),
	Dew				UMETA(DisplayName = "Tau"),
	Seed			UMETA(DisplayName = "Samen"),
	Fiber			UMETA(DisplayName = "Faser"),
	Dust			UMETA(DisplayName = "Staub"),
	Glitter			UMETA(DisplayName = "Glitzer"),
	Ember			UMETA(DisplayName = "Glut"),
	/** Fungi change the form of a brew, like a cloud or incense */
	Catalyst		UMETA(DisplayName = "Katalysator"),
	/** Ores and parts of creatures the engineer builds with */
	Part			UMETA(DisplayName = "Bauteil")
};

/** How an ingredient works: pure, marked, both at once, or open to everyone */
UENUM(BlueprintType)
enum class EVaelWay : uint8
{
	Neutral		UMETA(DisplayName = "neutral"),
	Light		UMETA(DisplayName = "Licht"),
	Shadow		UMETA(DisplayName = "Schatten"),
	Twilight	UMETA(DisplayName = "Zwielicht")
};

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

	/** What kind of ingredient it is in the witch's kitchen; None for materials that are no ingredient */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ingredient")
	EVaelIngredientKind Kind = EVaelIngredientKind::None;

	/** The element it carries, if any */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ingredient")
	bool bHasElement = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ingredient", meta = (EditCondition = "bHasElement"))
	EVaelElement Element = EVaelElement::Fire;

	/** Light, shadow, twilight or neutral; the way of a brew follows from its ingredients */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ingredient")
	EVaelWay Way = EVaelWay::Neutral;

	bool IsIngredient() const { return Kind != EVaelIngredientKind::None; }

	/** Share of the maximum health a potion heals when drunk with the potion button; 0 for materials that are no potion */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Potion", meta = (ClampMin = 0, ClampMax = 1))
	float HealFraction = 0.0f;

	/** After the heal it keeps healing: this share of the maximum health, spread evenly over these seconds */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Potion", meta = (ClampMin = 0, ClampMax = 1, EditCondition = "HealFraction > 0"))
	float RegenFraction = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Potion", meta = (ClampMin = 0, EditCondition = "HealFraction > 0"))
	float RegenSeconds = 0.0f;

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

	/** A trader of the guild: the prices follow what the group decided about the guild */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shop")
	bool bGuildPrices = true;

	/** What an offer costs right now, in guild coins */
	int32 GetPrice(const UObject* WorldContext, int32 OfferIndex) const;
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
