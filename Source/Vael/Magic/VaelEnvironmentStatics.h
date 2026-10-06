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
};
