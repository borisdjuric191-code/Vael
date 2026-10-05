// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Creatures/VaelCreature.h"
#include "VaelPreacher.generated.h"

/**
 *  Prediger der Narbe: a human servant of the Mark. Keeps its distance, walks sideways,
 *  shoots Mark bolts and calls a line of bone spikes out of the ground after a visible windup.
 *  Watching the spikes from close by teaches the players a formula, its corpse another one.
 */
UCLASS()
class AVaelPreacher : public AVaelCreature
{
	GENERATED_BODY()

public:

	/** Constructor */
	AVaelPreacher();

	/** Initialization */
	virtual void BeginPlay() override;

protected:

	virtual TSubclassOf<UVaelCreatureData> GetDefaultDataClass() const override;
	virtual void TickBehavior(float DeltaSeconds) override;

private:

	/** Shoots a Mark bolt at the target */
	void ShootBolt(const AActor* Target);

	/** Places the warnings of a spike line towards the target and starts the windup */
	void StartSpikeLine(const AActor* Target);

	/** Seconds until the next bolt */
	float BoltCooldown = 0.0f;

	/** Seconds until the next spike line */
	float SpikeCooldown = 0.0f;

	/** Seconds the current windup has left, 0 while not calling spikes */
	float WindupRemaining = 0.0f;

	/** Direction of the current spike line */
	FVector SpikeDirection = FVector::ForwardVector;

	/** Length of the current spike line in cm */
	float SpikeLineLength = 0.0f;

	/** Random offset that keeps preachers from walking sideways in step */
	float StrafePhase = 0.0f;
};
