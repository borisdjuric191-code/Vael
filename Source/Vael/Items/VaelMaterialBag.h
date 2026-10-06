// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VaelMaterialBag.generated.h"

class UVaelMaterial;

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

	/** Called whenever the content of the bag changes */
	FVaelOnMaterialsChanged OnMaterialsChanged;

private:

	/** Number of each material in the bag */
	UPROPERTY()
	TMap<TObjectPtr<const UVaelMaterial>, int32> Counts;
};
