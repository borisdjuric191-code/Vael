// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "World/VaelWeatherTypes.h"
#include "VaelProgressSubsystem.generated.h"

/**
 *  What the group has already seen and been taught, so explanations come only once.
 *  Lives as long as the game runs; a save game will keep it later.
 */
UCLASS()
class UVaelProgressSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	/** Remembers that a weather has been explained. Returns true the first time. */
	bool MarkWeatherIntroduced(EVaelWeather Weather);

private:

	/** Weathers the players have had explained */
	UPROPERTY()
	TSet<EVaelWeather> IntroducedWeathers;
};
