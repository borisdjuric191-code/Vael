// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "World/VaelWeatherTypes.h"
#include "UI/VaelWorldMap.h"
#include "VaelProgressSubsystem.generated.h"

/**
 *  What the group has already seen and been taught, so explanations come only once.
 *  Lives as long as the game runs; a save game will keep it later.
 */
class UVaelSaveGame;

UCLASS()
class UVaelProgressSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	/** Remembers that a weather has been explained. Returns true the first time. */
	bool MarkWeatherIntroduced(EVaelWeather Weather);

	/** True once the character with this name has told their first talk */
	bool HasHeardIntro(FName Speaker) const { return HeardIntros.Contains(Speaker); }

	/** Puts what was explained into the save */
	void WriteSave(UVaelSaveGame& Save) const;

	/** Takes what was explained from the save */
	void ReadSave(const UVaelSaveGame& Save);

	/** Remembers that the character has told their first talk */
	void MarkIntroHeard(FName Speaker) { HeardIntros.Add(Speaker); }

	/** Lifts the fog of the map round a world place (cm), soft at the rim; true if anything new showed */
	bool RevealMap(const UVaelWorldMap* Map, const FVector2D& World, float Radius);

	/** Lifts all the fog of the map (test) */
	void RevealWholeMap(const UVaelWorldMap* Map);

	/** How far the fog has lifted at a world place, 0 hidden .. 1 seen; always 1 on maps without fog */
	float GetRevealed(const UVaelWorldMap* Map, const FVector2D& World) const;

	/** The fog cells of a map, null while nothing of it was seen */
	const TArray<uint8>* GetMapFog(const UVaelWorldMap* Map) const;

	/** Counts up whenever any fog lifts */
	uint32 GetFogVersion() const { return FogVersion; }

	/** Camps of the map the group has found, in the order of the map */
	TArray<FVaelMapLabel> GetFoundCamps(const UVaelWorldMap* Map) const;

private:

	/** Weathers the players have had explained */
	UPROPERTY()
	TSet<EVaelWeather> IntroducedWeathers;

	/** Characters whose first talk the players have heard */
	UPROPERTY()
	TSet<FName> HeardIntros;

	/** What the group has seen of each map */
	UPROPERTY()
	TMap<FName, FVaelMapFog> MapFog;

	uint32 FogVersion = 1;
};
