// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VaelMaterialBag.generated.h"

class UVaelMaterial;
struct FVaelLootEntry;
struct FVaelSavedPlayer;

DECLARE_MULTICAST_DELEGATE(FVaelOnMaterialsChanged);

/** A stack of one material in the bag */
USTRUCT(BlueprintType)
struct FVaelMaterialStack
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Materials")
	TObjectPtr<const UVaelMaterial> Material;

	UPROPERTY(BlueprintReadOnly, Category="Materials")
	int32 Count = 0;
};

/**
 *  The material bag of a player: ores, plants and parts of creatures, stacked and never full.
 *  Every player has their own; in co-op each gets their own copy of the basic materials from the same source.
 */
UCLASS(ClassGroup=(Vael), meta = (BlueprintSpawnableComponent))
class UVaelMaterialBag : public UActorComponent
{
	GENERATED_BODY()

public:

	/** Puts materials into the bag */
	UFUNCTION(BlueprintCallable, Category="Materials")
	void AddMaterial(const UVaelMaterial* Material, int32 Count = 1);

	/** Takes materials out of the bag. Returns false, and takes nothing, if there aren't enough. */
	UFUNCTION(BlueprintCallable, Category="Materials")
	bool RemoveMaterial(const UVaelMaterial* Material, int32 Count = 1);

	/** Number of a material in the bag */
	UFUNCTION(BlueprintPure, Category="Materials")
	int32 GetCount(const UVaelMaterial* Material) const;

	/** Everything in the bag, sorted by region and name */
	UFUNCTION(BlueprintPure, Category="Materials")
	TArray<FVaelMaterialStack> GetStacks() const;

	/**
	 *  Uses one of a material by its Use: drinking it, or pouring it over a plant. Placeholder: not built yet,
	 *  it only tells the player so and returns false; nothing is taken out of the bag.
	 */
	UFUNCTION(BlueprintCallable, Category="Materials")
	bool UseMaterial(const UVaelMaterial* Material);

	/** Rolls the loot once and gives every player a copy: into their bag with a pickup label above the source, or on the ground if it has a model */
	static void GiveToGroup(const AActor* Source, const TArray<FVaelLootEntry>& Loot);

	/** Puts the content of the bag into the save of a player */
	void WriteSave(FVaelSavedPlayer& Saved) const;

	/** Replaces the content of the bag with the save of a player */
	void ReadSave(const FVaelSavedPlayer& Saved);

	/** Tabs of the bag in the inventory: 1 potions, 2 crystals, 3 ingredients and materials, 4 seeds (0 is the backpack of gear) */
	static constexpr int32 NumTabs = 5;
	static bool IsInTab(const UVaelMaterial* Material, int32 Tab);

	/** The stacks of one tab, in the order of GetStacks */
	TArray<FVaelMaterialStack> GetStacks(int32 Tab) const;

	/** The belt: four places for potions, used with the potion button */
	static constexpr int32 BeltSize = 4;
	const TArray<TObjectPtr<const UVaelMaterial>>& GetBelt() const { return Belt; }
	int32 GetActiveBelt() const { return ActiveBelt; }

	/** Puts a potion on the belt (on the active place if it is free, else the first free one, else the active one), or
	 *  takes it off if it is on already. Returns the place, -1 when taken off or not a potion. */
	int32 ToggleBelt(const UVaelMaterial* Potion);

	/** The next belt place that holds something becomes the active one */
	void CycleBelt();

	/** Uses the potion of the active belt place. Returns false if the place is empty or the bag has none of it left. */
	bool UseBelt();

	/** Called whenever the content of the bag changes */
	FVaelOnMaterialsChanged OnMaterialsChanged;

private:

	/** Number of each material in the bag */
	UPROPERTY()
	TMap<TObjectPtr<const UVaelMaterial>, int32> Counts;

	UPROPERTY()
	TArray<TObjectPtr<const UVaelMaterial>> Belt;

	int32 ActiveBelt = 0;
};
