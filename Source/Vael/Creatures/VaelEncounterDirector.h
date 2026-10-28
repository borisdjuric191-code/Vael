// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelEncounterDirector.generated.h"

class AVaelCreature;
class UVaelEncounterTable;
struct FVaelEncounterPack;

/**
 *  Place one in a region level: while the group walks, packs from the encounter table keep appearing ahead of it,
 *  out of sight, Diablo-like (Boris 2026-10-28). Every few dozen metres walked (sooner at night) one pack is picked by
 *  weight, the time of day and the ground height, and put on dry, not too steep ground in front of the group, never
 *  in a safe zone. More players bring bigger packs. Creatures left far behind go away, so the limit frees up.
 *  Standing still brings nothing: only walking does.
 */
UCLASS()
class VAEL_API AVaelEncounterDirector : public AActor
{
	GENERATED_BODY()

protected:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounters")
	TObjectPtr<UVaelEncounterTable> Table;

	/** Switches it off without removing it, e.g. for tests */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounters")
	bool bEnabled = true;

public:

	AVaelEncounterDirector();

	virtual void BeginPlay() override;

	/** Follows the group, counts the walked distance and sends packs */
	virtual void Tick(float DeltaSeconds) override;

	/** Sends a pack right now, ahead of the group (console: VaelBegegnung); returns how many came */
	UFUNCTION(BlueprintCallable, Category="Vael|Encounters")
	int32 SpawnPackNow();

	/** The director of the world, or null */
	static AVaelEncounterDirector* Get(const UWorld* World);

private:

	/** Middle of all living players and how many there are */
	bool GetGroup(FVector& OutCentre, int32& OutPlayers) const;

	/** Picks a pack that fits the time of day and the ground at a spot; null if none fits */
	const FVaelEncounterPack* PickPack(float GroundZ, float Night) const;

	/** Drops dead and far creatures from the list, despawning the far ones */
	void CleanUp(const FVector& Centre);

	/** Distance walked since the last pack, and the distance the next one needs */
	float Walked = 0.0f;
	float NextPackAt = 0.0f;

	/** Where the group was a moment ago, and its smoothed walking direction */
	FVector LastCentre = FVector::ZeroVector;
	FVector Heading = FVector::ZeroVector;
	bool bHasLastCentre = false;

	float Age = 0.0f;

	TArray<TWeakObjectPtr<AVaelCreature>> Spawned;
};
