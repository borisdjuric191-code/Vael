// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelCreatureSpawner.generated.h"

class UVaelCreatureData;

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

public:

	/** Constructor */
	AVaelCreatureSpawner();

	/** Spawns the creatures */
	virtual void BeginPlay() override;
};
