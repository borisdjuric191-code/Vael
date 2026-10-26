// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Compendium/VaelCompendiumSubsystem.h"
#include "Items/VaelItemTypes.h"
#include "Nature/VaelGarden.h"
#include "Story/VaelQuestSubsystem.h"
#include "World/VaelWeatherTypes.h"
#include "VaelSaveGame.generated.h"

/** What one player slot owns */
USTRUCT()
struct FVaelSavedPlayer
{
	GENERATED_BODY()

	/** Worn items by place; invalid items are empty places */
	UPROPERTY()
	TArray<FVaelItem> Equipped;

	UPROPERTY()
	TArray<FVaelItem> Backpack;

	UPROPERTY()
	int32 GuildCoins = 0;

	/** Number of each material in the bag, by the path of the material asset */
	UPROPERTY()
	TMap<FSoftObjectPath, int32> Materials;

	/** Formulas on the quick slots, by the path of the formula asset; an empty path is an empty slot */
	UPROPERTY()
	TArray<FSoftObjectPath> QuickSlots;

	/** Corruption by the Mark, 0 to 100 */
	UPROPERTY()
	float Corruption = 0.0f;
};

/** State of a region */
USTRUCT()
struct FVaelSavedRegion
{
	GENERATED_BODY()

	UPROPERTY()
	float Corruption = 0.0f;

	/** Corruption that can't be cleansed any more */
	UPROPERTY()
	float ScarredCorruption = 0.0f;
};

/**
 *  The save game of Vael: what the group has reached together, what every player slot owns, and what changed in the world for good.
 *  Positions, living creatures, things lying on the ground and plants that grow back are not kept: the group wakes up at the camp.
 */
UCLASS()
class UVaelSaveGame : public USaveGame
{
	GENERATED_BODY()

public:

	/** Raised whenever old saves can't be read any more; such a save is ignored */
	static constexpr int32 CurrentVersion = 1;

	UPROPERTY()
	int32 Version = CurrentVersion;

	/** When it was written; with several saves the fresher one is the main save */
	UPROPERTY()
	FDateTime SavedAt;

	// Quests and the decisions of the story

	UPROPERTY()
	TMap<FName, FVaelQuestProgress> QuestProgress;

	UPROPERTY()
	TSet<FName> StoryFlags;

	UPROPERTY()
	float GuildPriceFactor = 1.0f;

	UPROPERTY()
	TMap<FName, float> AggroFactors;

	// Grimoire

	UPROPERTY()
	TArray<FSoftObjectPath> KnownFormulas;

	UPROPERTY()
	TArray<FSoftObjectPath> EchoedFormulas;

	UPROPERTY()
	bool bMarkAwakened = false;

	// Compendium

	UPROPERTY()
	TMap<FName, FVaelResearchProgress> Research;

	// What was explained already

	UPROPERTY()
	TSet<FName> HeardIntros;

	UPROPERTY()
	TSet<EVaelWeather> IntroducedWeathers;

	// World

	/** Things of the levels that changed for good, as "Level.Actor": opened chests, picked up scrolls, sealed sources */
	UPROPERTY()
	TSet<FName> WorldMarks;

	/** Kinds of creatures that exist once and were defeated, like the Glutkönigin */
	UPROPERTY()
	TSet<FName> DefeatedUniques;

	/** Regions by "Level.Actor" */
	UPROPERTY()
	TMap<FName, FVaelSavedRegion> Regions;

	// Seed garden

	/** Plants Albrun has researched, by their id */
	UPROPERTY()
	TSet<FName> ResearchedPlants;

	/** What grows in the pot, none while it is empty, and the seconds it still needs */
	UPROPERTY()
	FName PotPlant;

	UPROPERTY()
	float PotSecondsLeft = 0.0f;

	/** Plants the players have sown */
	UPROPERTY()
	TArray<FVaelSavedPlant> SownPlants;

	/** Seconds the garden has been played; the green around a plant grows by it */
	UPROPERTY()
	float GardenClock = 0.0f;

	/** Seconds since the dawn of the current day, -1 for a new game (the start hour); and whether the first night has come */
	UPROPERTY()
	float TimeOfDaySeconds = -1.0f;

	UPROPERTY()
	bool bDayCycleIntroduced = false;

	// Players by slot, 0 to 3

	UPROPERTY()
	TMap<int32, FVaelSavedPlayer> Players;
};
