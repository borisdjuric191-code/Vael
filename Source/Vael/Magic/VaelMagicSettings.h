// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Magic/VaelElementTypes.h"
#include "VaelMagicSettings.generated.h"

/**
 *  Rules of the magic system that apply to every formula.
 *  Edited under Project Settings > Game > Vael Magic, stored in DefaultGame.ini.
 */
UCLASS(config=Game, defaultconfig, meta = (DisplayName = "Vael Magic"))
class UVaelMagicSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	/** Constructor */
	UVaelMagicSettings();

	/** Returns the settings object */
	static const UVaelMagicSettings* Get() { return GetDefault<UVaelMagicSettings>(); }

	/** Mana cost of a hand-cast formula made of the given number of elements, some of them drawn from the environment */
	float GetManaCost(int32 NumElements, int32 NumEnvironmentElements) const;

	/** Returns the placeholder color of an element */
	FLinearColor GetElementColor(EVaelElement Element) const;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** Mana cost by number of elements in the formula; the first entry is for one element */
	UPROPERTY(config, EditAnywhere, Category="Mana")
	TArray<float> ManaCostByElementCount;

	/** Mana saved for every element drawn from the environment */
	UPROPERTY(config, EditAnywhere, Category="Mana", meta = (ClampMin = 0))
	float EnvironmentCostReduction = 4.0f;

	/** No formula gets cheaper than this */
	UPROPERTY(config, EditAnywhere, Category="Mana", meta = (ClampMin = 0))
	float MinManaCost = 2.0f;

	/** Placeholder color per element */
	UPROPERTY(config, EditAnywhere, Category="Appearance")
	TMap<EVaelElement, FLinearColor> ElementColors;
};
