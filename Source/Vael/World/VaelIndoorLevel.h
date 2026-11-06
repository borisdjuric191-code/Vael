// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelIndoorLevel.generated.h"

/**
 *  Marks a level as an indoor place, like the refugia under the Wurzelpfade (the Glutkammer below the crater).
 *  One of these in a level is enough: the game gives it no sky that follows the clock (no sun, no moon), no local weather
 *  (fog banks, storms, rain patches), and no training dummies or test areas of the test level. The level brings its own light.
 */
UCLASS(meta = (DisplayName = "Vael Indoor Level"))
class AVaelIndoorLevel : public AActor
{
	GENERATED_BODY()

public:

	/** Constructor */
	AVaelIndoorLevel();

	/** True when the world holds an indoor marker */
	static bool IsIndoor(const UWorld* World);
};
