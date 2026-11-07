// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Items/VaelMaterial.h"
#include "VaelBrewing.generated.h"

class AVaelCharacter;
class UVaelMaterial;

/** How big a brew is (Rezeptbuch, Rezept-Schema) */
UENUM(BlueprintType)
enum class EVaelBrewTier : uint8
{
	/** 3 ingredients: base + active + addition */
	Basic		UMETA(DisplayName = "Grundtrank"),
	/** 4 ingredients with a catalyst */
	Strong		UMETA(DisplayName = "Stärkerer Trank"),
	/** 4 to 5 ingredients, one at a time works */
	Power		UMETA(DisplayName = "Krafttrank")
};

/** The craft a potion belongs to; the class of that craft brews it 25 % stronger and 50 % longer (Rezeptbuch) */
UENUM(BlueprintType)
enum class EVaelBrewCraft : uint8
{
	/** Tamer */
	Nature		UMETA(DisplayName = "Natur"),
	/** Mage */
	Elements	UMETA(DisplayName = "Elemente"),
	/** Engineer: all vials and oils */
	ThrowAndOil	UMETA(DisplayName = "Wurf und Öl"),
	/** Prophet */
	BodyAndSoul	UMETA(DisplayName = "Leib und Seele")
};

/** One recipe of the Rezeptbuch */
struct FVaelRecipe
{
	FName Id;
	FText Name;
	EVaelBrewTier Tier = EVaelBrewTier::Basic;
	EVaelWay Way = EVaelWay::Neutral;
	EVaelBrewCraft Craft = EVaelBrewCraft::Nature;
	/** Ingredients, one of each */
	TArray<TSoftObjectPtr<UVaelMaterial>> Ingredients;
	/** What comes out */
	TSoftObjectPtr<UVaelMaterial> Result;
	FText Effect;
	FText Duration;
	/** Colour of the liquid */
	FLinearColor Color = FLinearColor::White;
};

/** Why a recipe can't be brewed right now */
enum class EVaelBrewBlock : uint8
{
	None,
	/** A shadow page: sealed until an Ordenssiegel opens it (Orden der Narbe) */
	SealedShadow,
	/** Light and shadow together only as twilight after the vow */
	SealedTwilight,
	/** Something is missing in the bag */
	Missing
};

/**
 *  The witch's kitchen at the root table of a refugium: brewing the potions of the Rezeptbuch (Lore-Bibel, tab Rezeptbuch,
 *  Tränke der Aschenmark) from the ingredients in the player's material bag. Fixed recipes, no free combination.
 */
class FVaelBrewing
{
public:

	/** Every recipe, basic ones first */
	static const TArray<FVaelRecipe>& GetRecipes();

	/** The recipes of one tier */
	static TArray<const FVaelRecipe*> GetRecipes(EVaelBrewTier Tier);

	/** Whether the player can brew it now, and why not */
	static EVaelBrewBlock GetBlock(const AVaelCharacter* Player, const FVaelRecipe& Recipe);

	/** Brews one: takes the ingredients from the bag and puts the potion in. Returns false if it can't be brewed. */
	static bool Brew(AVaelCharacter* Player, const FVaelRecipe& Recipe);

	/** True if the class of the player has this craft (its potions work stronger) */
	static bool IsOwnCraft(const AVaelCharacter* Player, EVaelBrewCraft Craft);

	static FText GetTierName(EVaelBrewTier Tier);
	static FText GetCraftName(EVaelBrewCraft Craft);
};
