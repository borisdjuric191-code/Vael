// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelSmokeTest.generated.h"

class AVaelCharacter;
class UVaelCreatureData;
class UVaelFormula;

/**
 *  Automatic check of the game without anyone at the controls, started with the command line switch -VaelSmokeTest.
 *  Lets every creature fight the first player for a while, then casts every formula of the game at fresh enemies,
 *  keeps the player alive and full of mana, collects every warning and error the log writes during each step,
 *  writes a report to Saved/Logs/VaelSmokeTest.txt and quits the game.
 *  Checks that everything runs, not how it looks.
 */
UCLASS()
class AVaelSmokeTest : public AActor
{
	GENERATED_BODY()

public:

	/** Constructor */
	AVaelSmokeTest();

	/** True if the game was started to run the check */
	static bool IsRequested();

	/** Runs the steps one after another */
	virtual void Tick(float DeltaSeconds) override;

protected:

	/** Plans the steps and starts listening to the log */
	virtual void BeginPlay() override;

	/** Stops listening to the log */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** One step of the check: what it is called, what it does at its start, and how many seconds it runs */
	struct FStep
	{
		FString Name;
		TFunction<FString()> Run;
		float Duration = 1.0f;
	};

	/** The player the check plays with, null before the player has spawned */
	AVaelCharacter* GetPlayer() const;

	/** Keeps the player alive and full of mana, so every step can be cast */
	void RefillPlayer() const;

	/** Kills every creature, sets the player back to the start and spawns fresh enemies in front of them */
	FString PrepareArena(UVaelCreatureData* Data, int32 Count) const;

	/** Queues the elements of the formula and casts them in the direction the player faces */
	FString CastFormula(const UVaelFormula* Formula) const;

	/** Counts the actors still alive per class, to find looks and effects that never go away */
	FString CountLingeringActors() const;

	/** Writes the report and quits */
	void Finish();

	/** Steps still to run, the one running and the seconds left in it */
	TArray<FStep> Steps;
	int32 CurrentStep = INDEX_NONE;
	float StepTimeLeft = 0.0f;

	/** Describes what the creatures of the running step went through: damage, kills, risen servants */
	FString DescribeArena() const;

	/** Creatures spawned for the running step and their health at the start */
	mutable TArray<TWeakObjectPtr<class AVaelCreature>> ArenaCreatures;
	mutable TArray<float> ArenaStartHealth;

	/** Where the player stood when the check began */
	FVector StartLocation = FVector::ZeroVector;
	FRotator StartRotation = FRotator::ZeroRotator;

	/** Lines of the report */
	TArray<FString> Report;

	/** Listens to the log and remembers warnings and errors for the running step */
	class FLogCatcher* LogCatcher = nullptr;

	/** Warnings and errors over the whole check */
	int32 TotalWarnings = 0;
	int32 TotalErrors = 0;
};
