// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "VaelTimeSettings.generated.h"

/**
 *  Day and night (Boris 2026-10-26): a full day lasts 40 minutes, about 24 of them daylight, a dawn and a dusk, and
 *  10 minutes of night. The night is dark and dangerous in mood, never so dark that players depend on light.
 *  Edited under Project Settings > Game > Vael Time, stored in DefaultGame.ini.
 */
UCLASS(config=Game, defaultconfig, meta = (DisplayName = "Vael Time"))
class UVaelTimeSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	static const UVaelTimeSettings* Get() { return GetDefault<UVaelTimeSettings>(); }

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** Real seconds of each part of the day */
	UPROPERTY(config, EditAnywhere, Category="Clock", meta = (ClampMin = 10))
	float DawnSeconds = 180.0f;

	UPROPERTY(config, EditAnywhere, Category="Clock", meta = (ClampMin = 10))
	float DaySeconds = 1440.0f;

	UPROPERTY(config, EditAnywhere, Category="Clock", meta = (ClampMin = 10))
	float DuskSeconds = 180.0f;

	UPROPERTY(config, EditAnywhere, Category="Clock", meta = (ClampMin = 10))
	float NightSeconds = 600.0f;

	/** Hour of the clock a new game starts at */
	UPROPERTY(config, EditAnywhere, Category="Clock", meta = (ClampMin = 0, ClampMax = 24))
	float StartHour = 8.0f;

	/** The campaign starts calm: the first day lasts this many seconds longer before the cycle is introduced. Off while testing (Boris 2026-10-26) */
	UPROPERTY(config, EditAnywhere, Category="Clock")
	bool bLongFirstDay = false;

	UPROPERTY(config, EditAnywhere, Category="Clock", meta = (ClampMin = 0))
	float FirstDayExtraSeconds = 1500.0f;

	/** Highest the sun climbs at noon, in degrees over the horizon, and the moon at midnight */
	UPROPERTY(config, EditAnywhere, Category="Sky", meta = (ClampMin = 5, ClampMax = 90))
	float SunHighest = 55.0f;

	UPROPERTY(config, EditAnywhere, Category="Sky", meta = (ClampMin = 5, ClampMax = 90))
	float MoonHighest = 45.0f;

	/** Moonlight against the sun of the level, its color, and the share of the sky light left at night */
	UPROPERTY(config, EditAnywhere, Category="Sky", meta = (ClampMin = 0, ClampMax = 1))
	float MoonShare = 0.12f;

	UPROPERTY(config, EditAnywhere, Category="Sky")
	FLinearColor MoonColor = FLinearColor(0.55f, 0.66f, 1.0f);

	UPROPERTY(config, EditAnywhere, Category="Sky", meta = (ClampMin = 0, ClampMax = 1))
	float NightSkyShare = 0.3f;

	/** Color of the low sun at dawn and dusk */
	UPROPERTY(config, EditAnywhere, Category="Sky")
	FLinearColor LowSunColor = FLinearColor(1.0f, 0.45f, 0.22f);

	/** Haze at night: its color against the level's haze, and how much denser it gets */
	UPROPERTY(config, EditAnywhere, Category="Sky")
	FLinearColor NightFogColor = FLinearColor(0.04f, 0.06f, 0.11f);

	UPROPERTY(config, EditAnywhere, Category="Sky", meta = (ClampMin = 0))
	float NightFogDensity = 1.3f;

	/** The picture at night: exposure lowered by this many stops, less color, a cold tint */
	UPROPERTY(config, EditAnywhere, Category="Picture", meta = (ClampMin = 0))
	float NightExposureDrop = 0.9f;

	UPROPERTY(config, EditAnywhere, Category="Picture", meta = (ClampMin = 0, ClampMax = 1))
	float NightSaturation = 0.7f;

	UPROPERTY(config, EditAnywhere, Category="Picture")
	FLinearColor NightTint = FLinearColor(0.78f, 0.85f, 1.0f);

	/** A soft light around every player at night, so the dark stays a mood and never hides the player */
	UPROPERTY(config, EditAnywhere, Category="Picture", meta = (ClampMin = 0))
	float PlayerLightIntensity = 9.0f;

	UPROPERTY(config, EditAnywhere, Category="Picture", meta = (ClampMin = 0))
	float PlayerLightRadius = 900.0f;

	UPROPERTY(config, EditAnywhere, Category="Picture")
	FLinearColor PlayerLightColor = FLinearColor(1.0f, 0.82f, 0.62f);

	/** Glowing plants, stones and eyes shine this many times as bright in the deep night */
	UPROPERTY(config, EditAnywhere, Category="Nature", meta = (ClampMin = 1))
	float NightGlow = 1.8f;

	/** Healing plants (the Tränenkelch) give this many times their dew when picked at dawn */
	UPROPERTY(config, EditAnywhere, Category="Nature", meta = (ClampMin = 1))
	int32 DawnDewYield = 2;

	/** The night cools the air around every player by this much a second (UVaelAtmosphere); with moisture it ends in fog */
	UPROPERTY(config, EditAnywhere, Category="Nature", meta = (ClampMin = 0))
	float NightColdPerSecond = 0.6f;

	/** Chance of morning mist around the players when the dawn comes; never in a drought */
	UPROPERTY(config, EditAnywhere, Category="Nature", meta = (ClampMin = 0, ClampMax = 1))
	float MorningMistChance = 0.5f;

	/** How much more easily the Prophet fills light by day and shadow by night (and less the other way round), and twilight at dawn and dusk */
	UPROPERTY(config, EditAnywhere, Category="Prophet", meta = (ClampMin = 0, ClampMax = 1))
	float PathTimeBonus = 0.3f;

	UPROPERTY(config, EditAnywhere, Category="Prophet", meta = (ClampMin = 0))
	float TwilightBonus = 0.5f;
};
