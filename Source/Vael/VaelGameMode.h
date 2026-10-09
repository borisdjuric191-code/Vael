// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "VaelGameMode.generated.h"

class AVaelCharacter;
class AVaelSharedCamera;
class AVaelTrainingDummy;
class UVaelCreatureData;

/**
 *  Game Mode for local co-op with one shared isometric camera
 *  Sets the default gameplay framework classes
 */
UCLASS()
class AVaelGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	/** Highest number of players on one screen */
	static constexpr int32 MaxLocalPlayers = 4;

	/** Constructor */
	AVaelGameMode();

	/** Gives every joining player a free player slot */
	/** Starts the automatic check when the game was launched with -VaelSmokeTest */
	virtual void StartPlay() override;

	virtual void PostLogin(APlayerController* NewPlayer) override;

	/** Spawns joining players next to the group instead of at the player start */
	virtual void RestartPlayer(AController* NewPlayer) override;

	/** Returns the camera shared by all players, spawning it on first use */
	AVaelSharedCamera* GetSharedCamera();

	/** Returns the placeholder color of a player slot */
	FLinearColor GetPlayerColor(int32 PlayerSlot) const;

	/** Called by a player who goes down. Once every player is down, the group wakes up at the start after a moment. */
	void OnPlayerDowned(AVaelCharacter* Player);

	/** Spawns creatures of one kind on the ground around a location. Returns the number spawned. */
	int32 SpawnCreatureGroup(UVaelCreatureData* Data, const FVector& Center, int32 Count, float Spread);

protected:

	/** Camera class shared by all players */
	UPROPERTY(EditDefaultsOnly, Category="Camera")
	TSubclassOf<AVaelSharedCamera> SharedCameraClass;

	/** Placeholder color per player slot */
	UPROPERTY(EditDefaultsOnly, Category="Players")
	TArray<FLinearColor> PlayerColors;

	/** Distance from an existing player at which a joining player appears */
	UPROPERTY(EditDefaultsOnly, Category="Players", meta = (ClampMin = 0))
	float JoinSpawnRadius = 150.0f;

	/** Temporary: targets to test spells on until the real creatures exist. Spawned around the first player. */
	UPROPERTY(EditDefaultsOnly, Category="Testing")
	TSubclassOf<AVaelTrainingDummy> TrainingDummyClass;

	/** Temporary: number of training dummies, 0 for none */
	UPROPERTY(EditDefaultsOnly, Category="Testing", meta = (ClampMin = 0))
	int32 TrainingDummyCount = 3;

	/** Temporary: distance of the training dummies from the first player */
	UPROPERTY(EditDefaultsOnly, Category="Testing", meta = (ClampMin = 0))
	float TrainingDummyDistance = 400.0f;

	/** Temporary: puts a camp fire and a puddle next to the first player to test elements from the environment, until levels bring their own */
	UPROPERTY(EditDefaultsOnly, Category="Testing")
	bool bSpawnTestAreas = true;

	/** Temporary: distance of the test areas from the first player */
	UPROPERTY(EditDefaultsOnly, Category="Testing", meta = (ClampMin = 0))
	float TestAreaDistance = 450.0f;

	/** Temporary: spawns a few ember crawlers, ash harpies and a preacher around the first player; levels with creature spawners leave all test setup out */
	UPROPERTY(EditDefaultsOnly, Category="Testing")
	bool bSpawnTestCreatures = true;

	/** Temporary: distance of the test creatures from the first player */
	UPROPERTY(EditDefaultsOnly, Category="Testing", meta = (ClampMin = 0))
	float TestCreatureDistance = 1800.0f;

	/** Seconds after the last player went down until the group wakes up at the start */
	UPROPERTY(EditDefaultsOnly, Category="Players", meta = (ClampMin = 0))
	float AllDownRespawnDelay = 2.5f;

	/** Seconds nothing can hurt the players after waking up at the start */
	UPROPERTY(EditDefaultsOnly, Category="Players", meta = (ClampMin = 0))
	float RespawnInvulnerability = 2.0f;

private:

	/** Spawns the training dummies in a ring around a location */
	void SpawnTrainingDummies(const FVector& Center);

	/** Spawns a camp fire and a puddle on the ground near a location */
	void SpawnTestAreas(const FVector& Center);

	/** Spawns the test creatures around a location */
	void SpawnTestCreatures(const FVector& Center);

	/** Brings every player back to the start with full health and mana */
	void RespawnGroup();

	/** Timer of the group waking up at the start */
	FTimerHandle RespawnTimer;

	/** True once the training dummies exist */
	bool bTrainingDummiesSpawned = false;

	/** Returns the lowest player slot that no other player uses */
	int32 FindFreePlayerSlot(const APlayerController* ForPlayer) const;

	/** Camera shared by all players */
	UPROPERTY(Transient)
	TObjectPtr<AVaelSharedCamera> SharedCamera;
};
