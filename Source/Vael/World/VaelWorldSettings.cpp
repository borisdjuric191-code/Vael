// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelWorldSettings.h"

UVaelWorldSettings::UVaelWorldSettings()
{
	WeatherWeightsPure.Add(EVaelWeather::Clear, 1.0f);
	// Rain comes more often than the rest: the Keimpelz and its seeds hang on it
	WeatherWeightsPure.Add(EVaelWeather::Rain, 2.5f);
	WeatherWeightsPure.Add(EVaelWeather::Storm, 1.0f);
	WeatherWeightsPure.Add(EVaelWeather::Drought, 1.0f);
	WeatherWeightsPure.Add(EVaelWeather::Wind, 1.5f);

	WeatherWeightsCorrupted.Add(EVaelWeather::Clear, 0.3f);
	WeatherWeightsCorrupted.Add(EVaelWeather::Rain, 2.5f);
	WeatherWeightsCorrupted.Add(EVaelWeather::Storm, 2.5f);
	WeatherWeightsCorrupted.Add(EVaelWeather::Drought, 1.5f);
	WeatherWeightsCorrupted.Add(EVaelWeather::Wind, 1.5f);
	// Snow and blizzard come only where a region offers them (the Sturmzinnen); a blizzard only when it is corrupted
	WeatherWeightsPure.Add(EVaelWeather::Snow, 0.8f);
	WeatherWeightsPure.Add(EVaelWeather::Blizzard, 0.0f);
	WeatherWeightsCorrupted.Add(EVaelWeather::Snow, 1.0f);
	WeatherWeightsCorrupted.Add(EVaelWeather::Blizzard, 2.5f);

	// Rain favors water; drought is the mirror image: fire gains what water gains in the rain, water loses it
	const auto AddModifier = [this](EVaelWeather Weather, EVaelElement Element, float ManaCostMultiplier, float PowerMultiplier)
	{
		FVaelWeatherSpellModifier& Modifier = SpellModifiers.AddDefaulted_GetRef();
		Modifier.Weather = Weather;
		Modifier.Element = Element;
		Modifier.ManaCostMultiplier = ManaCostMultiplier;
		Modifier.PowerMultiplier = PowerMultiplier;
	};

	AddModifier(EVaelWeather::Rain, EVaelElement::Water, 0.75f, 1.25f);
	AddModifier(EVaelWeather::Drought, EVaelElement::Fire, 0.75f, 1.25f);
	AddModifier(EVaelWeather::Drought, EVaelElement::Water, 1.25f, 0.75f);
	// Snow is water (my reading); in a blizzard the wind carries air and smothers fire
	AddModifier(EVaelWeather::Snow, EVaelElement::Water, 0.85f, 1.1f);
	AddModifier(EVaelWeather::Snow, EVaelElement::Fire, 1.1f, 0.9f);
	AddModifier(EVaelWeather::Blizzard, EVaelElement::Air, 0.8f, 1.2f);
	AddModifier(EVaelWeather::Blizzard, EVaelElement::Fire, 1.25f, 0.8f);
}
