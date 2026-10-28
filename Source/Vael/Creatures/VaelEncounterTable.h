// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VaelEncounterTable.generated.h"

class UVaelCreatureData;

/** One kind of pack that may appear in front of the group, and how often */
USTRUCT(BlueprintType)
struct FVaelEncounterPack
{
	GENERATED_BODY()

	/** Values of the creatures. Empty: the defaults of the creature kind below. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pack")
	TObjectPtr<UVaelCreatureData> CreatureData;

	/** Kind of creature used when no data asset is set, with the values of the prototype */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pack")
	TSubclassOf<UVaelCreatureData> CreatureKind;

	/** How many come together, picked in this range (one player; more players bring more, see the table) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pack")
	FIntPoint Count = FIntPoint(2, 3);

	/** Distance of the pack members from the first one, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pack", meta = (ClampMin = 0))
	float Spread = 260.0f;

	/** How likely this pack is picked compared with the others */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pack", meta = (ClampMin = 0))
	float Weight = 1.0f;

	/** Weight at night as a share of the weight by day (night hunters above 1) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pack", meta = (ClampMin = 0))
	float NightWeight = 1.0f;

	/** Only on ground in this height range, in cm (moor creatures low, ridge creatures high) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pack")
	FVector2D GroundHeight = FVector2D(-1.0e6f, 1.0e6f);
};

/**
 *  Which packs come at the players while they walk through a region, Diablo-like (Boris 2026-10-28): not fixed groups
 *  on the map but packs that keep appearing ahead of the group, out of sight. Used by AVaelEncounterDirector.
 *  Special creatures (bosses, nests, unique ones) stay with AVaelCreatureSpawner at fixed places.
 */
UCLASS(BlueprintType)
class VAEL_API UVaelEncounterTable : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Packs")
	TArray<FVaelEncounterPack> Packs;

	/** Walked distance of the group between two packs, in cm, picked in this range */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pacing")
	FVector2D DistancePerPack = FVector2D(3500.0f, 5500.0f);

	/** At night packs come this much more often (the walked distance is divided by it) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pacing", meta = (ClampMin = 0.1))
	float NightPace = 1.35f;

	/** Seconds after the start before the first pack */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pacing", meta = (ClampMin = 0))
	float StartGrace = 20.0f;

	/** Distance from the group to a new pack, in cm: beyond what the camera shows */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement")
	FVector2D SpawnDistance = FVector2D(3000.0f, 4000.0f);

	/** How far a pack may lie to the side of the walking direction, in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement", meta = (ClampMin = 0, ClampMax = 180))
	float SpawnCone = 50.0f;

	/** Lurkers (lynx, thistle spider) lie closer to the walking direction, so the group really comes past them, in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement", meta = (ClampMin = 0, ClampMax = 180))
	float AmbushCone = 15.0f;

	/** No pack on ground lower than this, in cm: water planes have no collision, so the ground below them would count */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement")
	float MinGroundHeight = -1.0e6f;

	/** Ground steeper than this is no place for a pack (cosine of the slope: 0.8 is about 37 degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement", meta = (ClampMin = 0, ClampMax = 1))
	float MinGroundNormalZ = 0.8f;

	/** At most this many creatures of the director alive at once (one player) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Limits", meta = (ClampMin = 1))
	int32 MaxAlive = 18;

	/** Every further player adds this share to pack sizes and to the limit */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Limits", meta = (ClampMin = 0))
	float PerExtraPlayer = 0.35f;

	/** Creatures of the director farther than this from every player go away, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Limits", meta = (ClampMin = 1000))
	float DespawnDistance = 9000.0f;

	/** Creatures the group has left behind (behind its walking direction, not fighting) go away from this distance on, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Limits", meta = (ClampMin = 1000))
	float BehindDespawnDistance = 4500.0f;
};
