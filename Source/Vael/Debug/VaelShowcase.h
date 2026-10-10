// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelShowcase.generated.h"

/**
 *  Debug help: takes screenshots from the game camera at fixed times and quits, so a creature or a place can be
 *  looked at without anybody at the PC. Started by the game mode with the command line switch
 *  -VaelShowcase=<number of shots> and -VaelShowcaseInterval=<seconds between them>, like -VaelShowcase=8 -VaelShowcaseInterval=1.5. Pictures land in Saved/Screenshots.
 *  -VaelShowcaseFire=<second> burns the thread of every horn beetle that tenses at that time, to see the burned state.
 *  -VaelShowcaseCloseup follows the first creature with a close camera instead of the game camera.
 */
UCLASS()
class AVaelShowcase : public AActor
{
	GENERATED_BODY()

public:

	/** Constructor */
	AVaelShowcase();

	/** True if the command line asks for a showcase */
	static bool IsRequested();

	/** Counts down and takes the pictures */
	virtual void Tick(float DeltaSeconds) override;

protected:

	/** Reads the command line */
	virtual void BeginPlay() override;

private:

	float Interval = 1.5f;
	int32 NumShots = 6;
	int32 ShotsTaken = 0;
	float Countdown = 3.0f;
	float FireTime = -1.0f;
	float Age = 0.0f;

	/** Close camera that follows a creature, if asked for */
	UPROPERTY(Transient)
	TObjectPtr<class ACameraActor> Closeup;
};
