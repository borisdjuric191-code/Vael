// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelPfuhlnatterPopulation.generated.h"

class AVaelPfuhlnatter;

/**
 *  The Pfuhlnattern of a moor (Boris 2026-10-29): they breed. Grown snakes (the elites) lay broods of young at their
 *  nests now and then, so the fewer the players kill, the more there are, growing faster the more grown ones there
 *  are, up to a limit. Young ones grow up with play time (AVaelPfuhlnatter ages). Broods hatch only out of sight.
 *  The ages and the count are not saved yet: a new game starts with StartCount snakes.
 *  Placed in the Pfuhlmoor by Scripts/PlacePfuhlnattern.py, with nests on the banks.
 */
UCLASS()
class VAEL_API AVaelPfuhlnatterPopulation : public AActor
{
	GENERATED_BODY()

public:

	AVaelPfuhlnatterPopulation();

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	/** Living snakes of this moor, and how many of them are grown */
	UFUNCTION(BlueprintPure, Category="Pfuhlnatter")
	int32 GetSnakeCount() const;

	int32 GetGrownCount() const;

	/** Lets every grown snake lay its brood at once, for tests */
	void BreedNow();

protected:

	/** Where the snakes lie and their broods hatch (world cm, on the banks) */
	UPROPERTY(EditAnywhere, Category="Population")
	TArray<FVector> Nests;

	/** Snakes when the game starts, aged as UVaelPfuhlnatterData picks them (mostly young, some grown, few old) */
	UPROPERTY(EditAnywhere, Category="Population", meta = (ClampMin = 0))
	int32 StartCount = 9;

	/** Of these, at least this many are grown (elites), so the moor breeds from the start */
	UPROPERTY(EditAnywhere, Category="Population", meta = (ClampMin = 0))
	int32 StartGrown = 2;

	/** Never more than this many in the moor */
	UPROPERTY(EditAnywhere, Category="Population", meta = (ClampMin = 1))
	int32 MaxCount = 26;

	/** Seconds of play between two breeding times (a game day is 2400) */
	UPROPERTY(EditAnywhere, Category="Population", meta = (ClampMin = 10))
	float BreedInterval = 1200.0f;

	/** Chance of each grown snake to lay a brood at a breeding time, and how many young a brood has */
	UPROPERTY(EditAnywhere, Category="Population", meta = (ClampMin = 0, ClampMax = 1))
	float BroodChance = 0.5f;

	UPROPERTY(EditAnywhere, Category="Population")
	FIntPoint BroodSize = FIntPoint(1, 3);

	/** Age (game days) of a young snake when it hatches */
	UPROPERTY(EditAnywhere, Category="Population", meta = (ClampMin = 0))
	float HatchAge = 0.5f;

	/** A brood hatches only at a nest no player is nearer than this (cm), so it never appears in view */
	UPROPERTY(EditAnywhere, Category="Population", meta = (ClampMin = 0))
	float HiddenDistance = 3000.0f;

private:

	/** Spawns one snake of this age (below 0: picked) at a nest, unless the moor is full; true if it hatched */
	bool Hatch(float Age, int32 Nest);

	/** Nest no player is near, starting with the one asked for; INDEX_NONE if every nest is watched */
	int32 FindHiddenNest(int32 Preferred) const;

	TArray<TWeakObjectPtr<AVaelPfuhlnatter>> Snakes;
	float NextBreed = 0.0f;
	bool bStarted = false;
};
