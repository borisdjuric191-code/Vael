// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelProgressSubsystem.h"

bool UVaelProgressSubsystem::MarkWeatherIntroduced(EVaelWeather Weather)
{
	bool bAlreadyIntroduced = false;
	IntroducedWeathers.Add(Weather, &bAlreadyIntroduced);

	return !bAlreadyIntroduced;
}
