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

	/** Formulas cast from a quick slot cost this much more mana than by hand */
	UPROPERTY(config, EditAnywhere, Category="Quick Slots", meta = (ClampMin = 0))
	float QuickManaCostMultiplier = 1.5f;

	/** Formulas cast from a quick slot hit with this share of their power: combos by hand are stronger */
	UPROPERTY(config, EditAnywhere, Category="Quick Slots", meta = (ClampMin = 0))
	float QuickPowerMultiplier = 0.85f;

	/** Lightning damage against wet targets is multiplied by this */
	UPROPERTY(config, EditAnywhere, Category="Reactions", meta = (ClampMin = 0))
	float LightningOnWetMultiplier = 2.0f;

	/** Earth damage against frozen targets is multiplied by this, the target thaws */
	UPROPERTY(config, EditAnywhere, Category="Reactions", meta = (ClampMin = 0))
	float EarthOnFrozenMultiplier = 2.5f;

	/** Fire damage against wet targets is multiplied by this, the target dries */
	UPROPERTY(config, EditAnywhere, Category="Reactions", meta = (ClampMin = 0))
	float FireOnWetMultiplier = 0.6f;

	/** Seconds a water hit leaves its target wet if the formula doesn't say otherwise */
	UPROPERTY(config, EditAnywhere, Category="Reactions", meta = (ClampMin = 0))
	float DefaultWetDuration = 3.0f;

	/** How far a gust of wind carries a fire, in cm */
	UPROPERTY(config, EditAnywhere, Category="Reactions", meta = (ClampMin = 0))
	float FireSpreadDistance = 308.0f;

	/** Seconds a fire spread by wind burns */
	UPROPERTY(config, EditAnywhere, Category="Reactions", meta = (ClampMin = 0))
	float FireSpreadLifetime = 4.5f;

	/** Damage per second of a fire spread by wind if the original fire deals none */
	UPROPERTY(config, EditAnywhere, Category="Reactions", meta = (ClampMin = 0))
	float FireSpreadDamagePerSecond = 12.0f;

	/** Highest number of fires a single gust can spread */
	UPROPERTY(config, EditAnywhere, Category="Reactions", meta = (ClampMin = 0))
	int32 MaxFireSpreads = 3;

	/** Extra power per element drawn from the environment while the caster is free of Mark */
	UPROPERTY(config, EditAnywhere, Category="Environment", meta = (ClampMin = 0))
	float EnvironmentPowerBonusPure = 0.45f;

	/** Extra power per element drawn from the environment once the caster is corrupted by Mark */
	UPROPERTY(config, EditAnywhere, Category="Environment", meta = (ClampMin = 0))
	float EnvironmentPowerBonusCorrupted = 0.22f;

	/** A fire provides the fire element up to this distance beyond its edge, in cm */
	UPROPERTY(config, EditAnywhere, Category="Environment", meta = (ClampMin = 0))
	float FireSourceMargin = 168.0f;

	/** Rock or walls within this distance provide the earth element, in cm */
	UPROPERTY(config, EditAnywhere, Category="Environment", meta = (ClampMin = 0))
	float EarthSourceDistance = 170.0f;

	/** Chance to discover a free formula when its elements are combined for the first time */
	UPROPERTY(config, EditAnywhere, Category="Discovery", meta = (ClampMin = 0, ClampMax = 1))
	float DiscoveryChance = 0.7f;

	/** Damage the caster takes when experimenting fails */
	UPROPERTY(config, EditAnywhere, Category="Discovery", meta = (ClampMin = 0))
	float UnstableSelfDamage = 5.0f;

	/** Damage to enemies around the caster when experimenting fails */
	UPROPERTY(config, EditAnywhere, Category="Discovery", meta = (ClampMin = 0))
	float UnstableDamage = 10.0f;

	/** Reach of the discharge around the caster when experimenting fails, in cm */
	UPROPERTY(config, EditAnywhere, Category="Discovery", meta = (ClampMin = 0))
	float UnstableRadius = 250.0f;

	/** Speed at which the discharge pushes enemies away, in cm/s */
	UPROPERTY(config, EditAnywhere, Category="Discovery", meta = (ClampMin = 0))
	float UnstableKnockback = 420.0f;

	/** Placeholder color per element */
	UPROPERTY(config, EditAnywhere, Category="Appearance")
	TMap<EVaelElement, FLinearColor> ElementColors;
};
