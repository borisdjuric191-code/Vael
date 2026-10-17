// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelStreamTest.generated.h"

class AVaelCharacter;

/**
 *  Automatic check of an open world level, started with the command line switch -VaelStreamTest.
 *  Lets the first player run along the route of the level (target points tagged "VaelRoute" plus their number as a second tag),
 *  there and back again. Waits whenever the ground ahead has not streamed in yet and counts those waits,
 *  notes which cells are loaded, which region the player is in and how many creatures live near them,
 *  and checks that the creatures at the start come back without doubles after their cell unloaded and loaded again.
 *  Writes a report to Saved/Logs/VaelStreamTest.txt and quits the game.
 */
UCLASS()
class AVaelStreamTest : public AActor
{
	GENERATED_BODY()

public:

	/** Constructor */
	AVaelStreamTest();

	/** True if the game was started to run the check */
	static bool IsRequested();

	/** Moves the player and samples the world */
	virtual void Tick(float DeltaSeconds) override;

protected:

	/** Reads the route and starts listening to the log */
	virtual void BeginPlay() override;

	/** Stops listening to the log */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** What the check is doing right now */
	enum class EPhase : uint8
	{
		WarmUp,
		Out,
		AtEnd,
		Back,
		AtStart,
		Done
	};

	/** The player the check runs with, null before the player has spawned */
	AVaelCharacter* GetPlayer() const;

	/** Keeps the player alive, so passing enemies can't end the run */
	void RefillPlayer() const;

	/** Moves the player one tick along the route; false while the ground ahead is missing */
	bool MovePlayer(AVaelCharacter* Player, float DeltaSeconds);

	/** Adds the route points that have streamed in since the last call, returns how many */
	int32 AddLoadedRoutePoints();

	/** Writes one line about where the player is and what is loaded */
	void Sample(const AVaelCharacter* Player);

	/** Living creatures of the level within this distance of a location, servants not counted */
	int32 CountCreaturesNear(const FVector& Location, float Radius) const;

	/** Number of streamed cells that are loaded and shown */
	int32 CountVisibleCells() const;

	/** Moves on to the next phase and notes it */
	void EnterPhase(EPhase NewPhase);

	/** Writes the report and quits */
	void Finish();

	/** Points of the route in order, the one the player runs to, and the direction of the run */
	TArray<FVector> Route;
	int32 TargetIndex = 0;
	int32 RouteStep = 1;

	/** Number of the last route point added */
	int32 LastRouteNumber = 0;

	EPhase Phase = EPhase::WarmUp;
	float PhaseTime = 0.0f;
	float SampleTimeLeft = 0.0f;

	/** Creatures near the start before the run, and after coming back */
	int32 StartCreatures = 0;
	int32 ReturnCreatures = 0;

	/** Waits for missing ground: how many, how long altogether in real seconds, and the longest */
	int32 GroundWaits = 0;
	double GroundWaitSeconds = 0.0;
	double LongestGroundWait = 0.0;
	double GroundWaitStart = 0.0;
	bool bWaitingForGround = false;

	/** Most cells shown at once, and the longest real frame time while running */
	int32 MaxVisibleCells = 0;
	double LastFrameRealTime = 0.0;
	double LongestFrame = 0.0;
	FVector LongestFrameAt = FVector::ZeroVector;

	/** Name of the region the player was in at the last sample */
	FString LastRegionName;

	/** Lines of the report */
	TArray<FString> Report;

	/** Listens to the log and remembers warnings and errors */
	class FLogCatcher* LogCatcher = nullptr;
};
