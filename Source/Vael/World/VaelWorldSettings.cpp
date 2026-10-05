// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelWorldSettings.h"

UVaelWorldSettings::UVaelWorldSettings()
{
	WeatherWeightsPure.Add(EVaelWeather::Clear, 1.0f);
	WeatherWeightsPure.Add(EVaelWeather::Rain, 1.0f);
	WeatherWeightsPure.Add(EVaelWeather::Storm, 1.0f);
	WeatherWeightsPure.Add(EVaelWeather::Drought, 1.0f);

	WeatherWeightsCorrupted.Add(EVaelWeather::Clear, 0.3f);
	WeatherWeightsCorrupted.Add(EVaelWeather::Rain, 1.0f);
	WeatherWeightsCorrupted.Add(EVaelWeather::Storm, 2.5f);
	WeatherWeightsCorrupted.Add(EVaelWeather::Drought, 1.5f);

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
}
