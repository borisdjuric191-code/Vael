// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelCreatureSpawner.generated.h"

class UVaelCreatureData;
class AVaelCreature;

/**
 *  Place in a level to put a group of creatures there when the game starts.
 *  Takes a creature data asset, or without one the default values of a kind of creature.
 */
UCLASS()
class AVaelCreatureSpawner : public AActor
{
	GENERATED_BODY()

protected:

	/** Values of the creatures. Empty: the defaults of the creature kind below. */
	UPROPERTY(EditAnywhere, Category="Spawner")
	TObjectPtr<UVaelCreatureData> CreatureData;

	/** Kind of creature used when no data asset is set, with the values of the prototype */
	UPROPERTY(EditAnywhere, Category="Spawner", meta = (EditCondition = "CreatureData == nullptr"))
	TSubclassOf<UVaelCreatureData> CreatureKind;

	/** Number of creatures */
	UPROPERTY(EditAnywhere, Category="Spawner", meta = (ClampMin = 1))
	int32 Count = 3;

	/** Distance of the creatures from the spawner, except the first which stands on it, in cm */
	UPROPERTY(EditAnywhere, Category="Spawner", meta = (ClampMin = 0))
	float Spread = 150.0f;

	/** Wakes only when a player comes near and goes to sleep when all are far; a defeated group comes back after a while.
	 *  Off: the group stands there from the start, as long as its cell is loaded (the old way, clearings and bosses). */
	UPROPERTY(EditAnywhere, Category="Spawner")
	bool bNearPlayersOnly = false;

	/** A player nearer than this wakes the group (cm) */
	UPROPERTY(EditAnywhere, Category="Spawner", meta = (EditCondition = "bNearPlayersOnly"))
	float WakeDistance = 6500.0f;

	/** All players farther than this: the group goes away unseen (cm) */
	UPROPERTY(EditAnywhere, Category="Spawner", meta = (EditCondition = "bNearPlayersOnly"))
	float SleepDistance = 13000.0f;

	/** After the group was defeated it comes back after this many seconds, never in sight */
	UPROPERTY(EditAnywhere, Category="Spawner", meta = (EditCondition = "bNearPlayersOnly"))
	float RespawnSeconds = 300.0f;

	/** Health and damage of the group, 1 = as made; grows with the distance from the start of the world */
	UPROPERTY(EditAnywhere, Category="Spawner", meta = (ClampMin = 0.1))
	float Strength = 1.0f;

	/** Chance that one of the group comes as an elite */
	UPROPERTY(EditAnywhere, Category="Spawner", meta = (ClampMin = 0, ClampMax = 1))
	float EliteChance = 0.0f;

public:

	/** Constructor */
	AVaelCreatureSpawner();

	/** True if the level has at least one spawner or is an open world, which makes it a real level instead of the test level */
	static bool LevelHasSpawners(const UWorld* World);

	/** Spawns the creatures */
	virtual void BeginPlay() override;

	/** Near-player spawners: wake, sleep, count the defeat */
	virtual void Tick(float DeltaSeconds) override;

private:

	/** Spawns the creatures once the ground below has loaded; in an open world it can stream in a moment after the spawner */
	void SpawnWhenGroundIsThere();

	/** Times the ground was looked for */
	int32 GroundTries = 0;

	/** Spawns the group now with its strength and elite chance */
	void SpawnGroup();

	/** Nearest player, in cm (a huge number if none) */
	float NearestPlayerDistance() const;

	/** The creatures of the group while it is awake */
	TArray<TWeakObjectPtr<AVaelCreature>> Group;
};
