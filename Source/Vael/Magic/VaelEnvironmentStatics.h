// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Magic/VaelElementTypes.h"
#include "VaelEnvironmentStatics.generated.h"

class UPrimitiveComponent;

/** Where an element drawn from the environment came from, for the look that carries it to the orb */
struct FVaelElementDraw
{
	/** False if the element came from everywhere at once (rain, a storm) or from nothing that can be shown */
	bool bHasPoint = false;

	/** Where it left its source */
	FVector From = FVector::ZeroVector;

	/** Color of the water it was drawn from */
	FLinearColor Color = FLinearColor::White;
};

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
	 *  water when standing in it or in rain, fire when next to it (campfires and Brandherde too), earth when next to rock
	 *  that isn't worn out, air from storms, Mark from open Mark sources.
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

	/**
	 *  A mage takes an element from the environment: the source gives some of itself. Rock gets chunks torn out and is
	 *  worn out after a few times, a fire burns lower, a puddle shrinks. Call it only if IsElementInEnvironment was true.
	 */
	static FVaelElementDraw DrawFromEnvironment(AActor* Caster, EVaelElement Element);

	/**
	 *  Rock next to the caster that can still give earth: placed rocks, stones and ruins (by their mesh name or the earth tag)
	 *  and rock walls. The point lies on the rock, on the side of the caster. False if there is none.
	 */
	static bool FindRock(const AActor* Caster, UPrimitiveComponent*& OutRock, int32& OutItem, FVector& OutPoint);

	/** True if the component is rock that gives earth, worn out or not */
	static bool IsRock(const UPrimitiveComponent* Component);

	/** The nearest fire spot (campfire, Brandherd) the caster can draw fire from, null if none */
	static class AVaelFireSpot* FindFireSpot(const AActor* Caster);
};
