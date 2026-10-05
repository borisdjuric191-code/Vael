// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Magic/VaelElementTypes.h"
#include "VaelWeatherTypes.generated.h"

/** Weather of a region */
UENUM(BlueprintType)
enum class EVaelWeather : uint8
{
	Clear,
	/** Ash rain: everybody is wet, water spells are stronger, fires die sooner */
	Rain,
	/** Air is everywhere, lightning strikes near the players */
	Storm,
	/** Everybody dries, fire spells are stronger and fires burn longer, water spells are weaker */
	Drought
};

/** How a weather changes the formulas of one element */
USTRUCT(BlueprintType)
struct FVaelWeatherSpellModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weather")
	EVaelWeather Weather = EVaelWeather::Rain;

	/** Damage element of the formulas that are changed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weather")
	EVaelElement Element = EVaelElement::Water;

	/** Mana cost is multiplied by this */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weather", meta = (ClampMin = 0))
	float ManaCostMultiplier = 1.0f;

	/** Power is multiplied by this */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weather", meta = (ClampMin = 0))
	float PowerMultiplier = 1.0f;
};
