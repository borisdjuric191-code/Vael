// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "VaelWorldSettings.generated.h"

/**
 *  Rules of weather and corruption that apply to every region. The defaults are the values of the browser prototype (1 tile = 140 cm).
 *  Edited under Project Settings > Game > Vael World, stored in DefaultGame.ini.
 */
UCLASS(config=Game, defaultconfig, meta = (DisplayName = "Vael World"))
class UVaelWorldSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	/** Returns the settings object */
	static const UVaelWorldSettings* Get() { return GetDefault<UVaelWorldSettings>(); }

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** Seconds a weather lasts, picked at random in this range */
	UPROPERTY(config, EditAnywhere, Category="Weather")
	FVector2D WeatherDuration = FVector2D(55.0f, 85.0f);

	/** Chance weights of the next weather in a region free of corruption: clear, rain, storm */
	UPROPERTY(config, EditAnywhere, Category="Weather")
	FVector WeatherWeightsPure = FVector(1.0f, 1.0f, 1.0f);

	/** Chance weights of the next weather in a fully corrupted region: the weather grows wilder in between */
	UPROPERTY(config, EditAnywhere, Category="Weather")
	FVector WeatherWeightsCorrupted = FVector(0.3f, 1.0f, 2.5f);

	/** Seconds a fire lasts in the rain, as a share of its normal lifetime */
	UPROPERTY(config, EditAnywhere, Category="Weather|Rain", meta = (ClampMin = 0, ClampMax = 1))
	float RainFireLifetimeShare = 0.375f;

	/** Seconds creatures in the rain stay wet after each refresh */
	UPROPERTY(config, EditAnywhere, Category="Weather|Rain", meta = (ClampMin = 0))
	float RainWetDuration = 1.0f;

	/** Seconds between two lightning strikes in a storm, picked at random in this range */
	UPROPERTY(config, EditAnywhere, Category="Weather|Storm")
	FVector2D LightningInterval = FVector2D(3.5f, 6.0f);

	/** Share by which full corruption shortens the time between two strikes */
	UPROPERTY(config, EditAnywhere, Category="Weather|Storm", meta = (ClampMin = 0, ClampMax = 0.9))
	float LightningCorruptionSpeedup = 0.4f;

	/** Distance of a strike from the player it falls near, picked at random in this range, in cm */
	UPROPERTY(config, EditAnywhere, Category="Weather|Storm")
	FVector2D LightningDistance = FVector2D(280.0f, 840.0f);

	/** Seconds the warning on the ground shows before the strike */
	UPROPERTY(config, EditAnywhere, Category="Weather|Storm", meta = (ClampMin = 0))
	float LightningWarning = 0.9f;

	/** Players this close to a strike are hit, in cm */
	UPROPERTY(config, EditAnywhere, Category="Weather|Storm", meta = (ClampMin = 0))
	float LightningPlayerRadius = 182.0f;

	UPROPERTY(config, EditAnywhere, Category="Weather|Storm", meta = (ClampMin = 0))
	float LightningPlayerDamage = 14.0f;

	/** Creatures this close to a strike are hit, in cm */
	UPROPERTY(config, EditAnywhere, Category="Weather|Storm", meta = (ClampMin = 0))
	float LightningCreatureRadius = 210.0f;

	/** Damage to creatures, doubled against wet ones like every lightning */
	UPROPERTY(config, EditAnywhere, Category="Weather|Storm", meta = (ClampMin = 0))
	float LightningCreatureDamage = 30.0f;

	/** Corruption of a region at the start, 0 to 100, for regions that don't set their own */
	UPROPERTY(config, EditAnywhere, Category="Corruption", meta = (ClampMin = 0, ClampMax = 100))
	float DefaultCorruption = 0.0f;

	/** Region corruption added for every Mark element cast in it */
	UPROPERTY(config, EditAnywhere, Category="Corruption", meta = (ClampMin = 0))
	float RegionCorruptionPerMarkElement = 1.2f;

	/** Personal corruption a mage gains for every Mark element cast */
	UPROPERTY(config, EditAnywhere, Category="Corruption", meta = (ClampMin = 0))
	float PlayerCorruptionPerMarkElement = 4.5f;

	/** Below this region corruption no creature is marked */
	UPROPERTY(config, EditAnywhere, Category="Corruption|Marked Creatures", meta = (ClampMin = 0, ClampMax = 100))
	float MarkedCreatureThreshold = 40.0f;

	/** Chance of a creature to be marked is the region corruption divided by this */
	UPROPERTY(config, EditAnywhere, Category="Corruption|Marked Creatures", meta = (ClampMin = 1))
	float MarkedCreatureChanceDivisor = 170.0f;

	/** Health of marked creatures is multiplied by this */
	UPROPERTY(config, EditAnywhere, Category="Corruption|Marked Creatures", meta = (ClampMin = 1))
	float MarkedHealthMultiplier = 1.6f;

	/** Damage of marked creatures is multiplied by this */
	UPROPERTY(config, EditAnywhere, Category="Corruption|Marked Creatures", meta = (ClampMin = 1))
	float MarkedDamageMultiplier = 1.5f;
};
