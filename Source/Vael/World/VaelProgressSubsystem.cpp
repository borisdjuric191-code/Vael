// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelProgressSubsystem.h"
#include "Save/VaelSaveGame.h"

bool UVaelProgressSubsystem::MarkWeatherIntroduced(EVaelWeather Weather)
{
	bool bAlreadyIntroduced = false;
	IntroducedWeathers.Add(Weather, &bAlreadyIntroduced);

	return !bAlreadyIntroduced;
}

void UVaelProgressSubsystem::WriteSave(UVaelSaveGame& Save) const
{
	Save.HeardIntros = HeardIntros;
	Save.IntroducedWeathers = IntroducedWeathers;
}

void UVaelProgressSubsystem::ReadSave(const UVaelSaveGame& Save)
{
	HeardIntros = Save.HeardIntros;
	IntroducedWeathers = Save.IntroducedWeathers;
}
