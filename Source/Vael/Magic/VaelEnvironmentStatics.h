// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Magic/VaelElementTypes.h"
#include "VaelEnvironmentStatics.generated.h"

/**
 *  Tells which elements a mage can draw from the surroundings.
 *  Elements from the environment make formulas cheaper and stronger.
 */
UCLASS()
class UVaelEnvironmentStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	 *  True if the element is present around the actor:
	 *  water when standing in it, fire when next to it, earth when next to rock or walls,
	 *  air from storms, Mark from open Mark sources.
	 */
	UFUNCTION(BlueprintPure, Category="Vael|Magic")
	static bool IsElementInEnvironment(const AActor* Caster, EVaelElement Element);

	/**
	 *  How much a wave of water swells from the water around the caster, from 0 (none) to 1 (full size and damage):
	 *  puddles, ponds, water plants and rain within reach. The puddles are drawn into the wave and drain away; ponds and plants stay.
	 */
	static float DrawWaveWater(const AActor* Caster);

	/**
	 *  Where a mage draws the water of the environment from, for the strand that runs up to the orb: a water plant nearby,
	 *  else the water the mage stands in, a little ahead of the feet. Gives the color of that water before it turns into spell water.
	 *  False if there is no such water.
	 */
	static bool FindWaterDrawPoint(const AActor* Caster, FVector& OutLocation, FLinearColor& OutColor);
};
