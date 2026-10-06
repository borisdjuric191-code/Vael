// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Items/VaelMaterial.h"
#include "World/VaelWeatherTypes.h"
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

	/** Constructor */
	UVaelWorldSettings();

	/** Returns the settings object */
	static const UVaelWorldSettings* Get() { return GetDefault<UVaelWorldSettings>(); }

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** Seconds a weather lasts, picked at random in this range */
	UPROPERTY(config, EditAnywhere, Category="Weather")
	FVector2D WeatherDuration = FVector2D(55.0f, 85.0f);

	/** Seconds of calm, clear sky at the start of a game before the weather changes for the first time */
	UPROPERTY(config, EditAnywhere, Category="Weather", meta = (ClampMin = 0))
	float CalmStartDuration = 240.0f;

	/** Chance weights of the next weather in a region free of corruption; weathers missing here never come */
	UPROPERTY(config, EditAnywhere, Category="Weather")
	TMap<EVaelWeather, float> WeatherWeightsPure;

	/** Chance weights of the next weather in a fully corrupted region: the weather grows wilder in between */
	UPROPERTY(config, EditAnywhere, Category="Weather")
	TMap<EVaelWeather, float> WeatherWeightsCorrupted;

	/** Seconds the first explanation of a weather stays on screen */
	UPROPERTY(config, EditAnywhere, Category="Weather", meta = (ClampMin = 0))
	float WeatherExplanationDuration = 9.0f;

	/** How each weather changes the formulas of an element, by their damage element */
	UPROPERTY(config, EditAnywhere, Category="Weather|Spells")
	TArray<FVaelWeatherSpellModifier> SpellModifiers;

	/** A wet mage casting lightning takes this share of the damage of the formula */
	UPROPERTY(config, EditAnywhere, Category="Weather|Spells", meta = (ClampMin = 0))
	float WetLightningBacklashShare = 0.5f;

	/** Seconds a fire lasts in the rain, as a share of its normal lifetime */
	UPROPERTY(config, EditAnywhere, Category="Weather|Rain", meta = (ClampMin = 0, ClampMax = 1))
	float RainFireLifetimeShare = 0.375f;

	/** Seconds players and creatures in the rain stay wet after each refresh */
	UPROPERTY(config, EditAnywhere, Category="Weather|Rain", meta = (ClampMin = 0))
	float RainWetDuration = 1.0f;

	/** Seconds a fire lasts in a drought, as a multiple of its normal lifetime */
	UPROPERTY(config, EditAnywhere, Category="Weather|Drought", meta = (ClampMin = 1))
	float DroughtFireLifetimeMultiplier = 1.6f;

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

	/** Region corruption removed for every marked creature killed in it */
	UPROPERTY(config, EditAnywhere, Category="Corruption", meta = (ClampMin = 0))
	float CleansingPerMarkedKill = 2.0f;

	/** Region corruption added for every Mark element cast in it */
	UPROPERTY(config, EditAnywhere, Category="Corruption", meta = (ClampMin = 0))
	float RegionCorruptionPerMarkElement = 1.2f;

	/** Personal corruption a mage gains for every Mark element cast */
	UPROPERTY(config, EditAnywhere, Category="Corruption", meta = (ClampMin = 0))
	float PlayerCorruptionPerMarkElement = 4.5f;

	/** From this personal corruption on, every Mark element cast hurts the caster (prototype: 60) */
	UPROPERTY(config, EditAnywhere, Category="Corruption", meta = (ClampMin = 0, ClampMax = 100))
	float CorruptionPainThreshold = 60.0f;

	/** Damage to the caster for every Mark element cast above the pain threshold */
	UPROPERTY(config, EditAnywhere, Category="Corruption", meta = (ClampMin = 0))
	float CorruptionPainPerMarkElement = 4.0f;

	/** From this personal corruption on, the Mark may whisper to the caster */
	UPROPERTY(config, EditAnywhere, Category="Corruption", meta = (ClampMin = 0, ClampMax = 100))
	float CorruptionWhisperThreshold = 85.0f;

	/** Chance per Mark cast that the Mark whispers above the threshold */
	UPROPERTY(config, EditAnywhere, Category="Corruption", meta = (ClampMin = 0, ClampMax = 1))
	float CorruptionWhisperChance = 0.35f;

	/** What every marked creature drops on top of its own loot: the crystals offered at Mark sources */
	UPROPERTY(config, EditAnywhere, Category="Loot")
	FVaelLootEntry MarkedLoot = FVaelLootEntry(TEXT("/Game/Vael/Items/Materials/DA_Material_Markkristall.DA_Material_Markkristall"), 1.0f);

	/** Health a health orb gives back (prototype: 25) */
	UPROPERTY(config, EditAnywhere, Category="Loot", meta = (ClampMin = 0))
	float HealthOrbAmount = 25.0f;

	/** Mana a mana orb gives back (prototype: 35) */
	UPROPERTY(config, EditAnywhere, Category="Loot", meta = (ClampMin = 0))
	float ManaOrbAmount = 35.0f;

	/** Players this close to an orb pick it up, in cm (prototype: 0.9 tiles) */
	UPROPERTY(config, EditAnywhere, Category="Loot", meta = (ClampMin = 0))
	float OrbPickupRadius = 126.0f;

	/** Region corruption an unsealed Mark source adds per minute */
	UPROPERTY(config, EditAnywhere, Category="Mark Sources", meta = (ClampMin = 0))
	float SourceCorruptionPerMinute = 1.0f;

	/** A source stops adding corruption once its region has this much */
	UPROPERTY(config, EditAnywhere, Category="Mark Sources", meta = (ClampMin = 0, ClampMax = 100))
	float SourceCorruptionCap = 60.0f;

	/** Region corruption a sealed source takes away */
	UPROPERTY(config, EditAnywhere, Category="Mark Sources", meta = (ClampMin = 0))
	float SourceSealCleansing = 30.0f;

	/** Seconds a player can stand in a source before it attacks them */
	UPROPERTY(config, EditAnywhere, Category="Mark Sources", meta = (ClampMin = 0))
	float SourceGraceTime = 3.0f;

	/** Seconds between two attacks of the source on a player standing in it */
	UPROPERTY(config, EditAnywhere, Category="Mark Sources", meta = (ClampMin = 0.1))
	float SourceStrikeInterval = 1.5f;

	/** Warning time before an attack of the source breaks out of the ground */
	UPROPERTY(config, EditAnywhere, Category="Mark Sources", meta = (ClampMin = 0))
	float SourceStrikeWarning = 0.7f;

	/** Damage of an attack of the source */
	UPROPERTY(config, EditAnywhere, Category="Mark Sources", meta = (ClampMin = 0))
	float SourceStrikeDamage = 12.0f;

	/** Reach of an attack of the source in cm */
	UPROPERTY(config, EditAnywhere, Category="Mark Sources", meta = (ClampMin = 0))
	float SourceStrikeRadius = 110.0f;

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
