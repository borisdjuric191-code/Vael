// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "VaelCompendiumSettings.generated.h"

/**
 *  How researching for the compendium works: when a kind counts as seen and watched, and how the spyglass behaves.
 *  Project Settings → Game → Vael Compendium.
 */
UCLASS(config=Game, defaultconfig, meta = (DisplayName = "Vael Compendium"))
class UVaelCompendiumSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	static const UVaelCompendiumSettings* Get() { return GetDefault<UVaelCompendiumSettings>(); }

	/** A creature closer than this to a player, in cm, counts as sighted */
	UPROPERTY(config, EditAnywhere, Category="Research", meta = (ClampMin = 0))
	float SightRange = 1500.0f;

	/** Players this close to a calm creature, in cm, watch it even without the spyglass */
	UPROPERTY(config, EditAnywhere, Category="Research", meta = (ClampMin = 0))
	float ObserveRange = 700.0f;

	/** A creature counts as calm when nothing has hurt it for this many seconds */
	UPROPERTY(config, EditAnywhere, Category="Research", meta = (ClampMin = 0))
	float CalmSeconds = 3.0f;

	/** How far from the player the spyglass reaches, in cm */
	UPROPERTY(config, EditAnywhere, Category="Spyglass", meta = (ClampMin = 0))
	float SpyglassReach = 750.0f;

	/** Radius of the focus circle of the spyglass, in cm */
	UPROPERTY(config, EditAnywhere, Category="Spyglass", meta = (ClampMin = 10))
	float SpyglassFocusRadius = 170.0f;

	/** Watching through the spyglass counts this many times as much */
	UPROPERTY(config, EditAnywhere, Category="Spyglass", meta = (ClampMin = 0))
	float SpyglassObserveRate = 2.0f;

	/** Creatures only notice a player behind a spyglass closer than this, in cm */
	UPROPERTY(config, EditAnywhere, Category="Spyglass", meta = (ClampMin = 0))
	float SpyglassUnnoticedDistance = 250.0f;
};
