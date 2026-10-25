// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelFireMushroom.generated.h"

/**
 *  The mushroom cloud of burning smoke a Feuerball sends up where it bursts: a stem of fire and smoke climbs fast,
 *  then a cap of dark smoke rolls out at its top and curls down. Built from puffs of the pack fire
 *  (UVaelFireSettings::MushroomSmoke), each emitting a moment and left to drift on its wind.
 */
UCLASS()
class AVaelFireMushroom : public AActor
{
	GENERATED_BODY()

public:

	AVaelFireMushroom();

	/** Sends up a mushroom cloud from a burst of this radius in cm */
	static AVaelFireMushroom* Spawn(UWorld* World, const FVector& Location, float BurstRadius);

	virtual void Tick(float DeltaSeconds) override;

private:

	/** Puffs a moment of smoke at a place, its wind blowing that way */
	void Puff(const FVector& Place, const FVector& Wind, float Size, bool bBurning);

	float Radius = 150.0f;
	float Age = 0.0f;
	float NextStemCountdown = 0.0f;
	int32 CapRingsDone = 0;
};
