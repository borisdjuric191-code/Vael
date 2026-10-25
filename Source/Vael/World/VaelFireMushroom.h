// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelFireMushroom.generated.h"

/**
 *  What a Feuerball leaves where it bursts: a burning patch on the ground and a cloud of fire and smoke that rears up
 *  out of it in two surges, the second higher than the first. Built from the pack fire (UVaelFireSettings::MushroomSmoke),
 *  each surge emitting a moment and left to climb.
 */
UCLASS()
class AVaelFireMushroom : public AActor
{
	GENERATED_BODY()

public:

	AVaelFireMushroom();

	/** Lights the patch and sends up the cloud from a burst of this radius in cm */
	static AVaelFireMushroom* Spawn(UWorld* World, const FVector& Location, float BurstRadius);

	virtual void Tick(float DeltaSeconds) override;

private:

	/** One surge of burning smoke at a height above the ground, in cm */
	void Surge(float Height, float Size);

	float Radius = 150.0f;
	float Age = 0.0f;
	int32 SurgesDone = 0;
};
