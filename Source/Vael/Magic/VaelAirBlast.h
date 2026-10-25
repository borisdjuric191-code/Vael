// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelAirBlast.generated.h"

class UVaelCloudCardsComponent;
class UVaelWindStreaksComponent;

/**
 *  The look of the wind spells, made of what the wind does to the world (Boris 2026-10-25): shimmering air that bends
 *  what lies behind it, thin bright streaks racing through it and ash and sand torn off the ground.
 *  A blast rolls out through a cone (Windstoß) or all around (the ring where a Böe lands); a trail follows a rider (Böe).
 */
UCLASS()
class AVaelAirBlast : public AActor
{
	GENERATED_BODY()

public:

	AVaelAirBlast();

	/** A blast from the hand through a cone of this reach and half angle (180 for a ring all around) */
	static AVaelAirBlast* SpawnBlast(AActor* Caster, const FVector& Origin, const FVector& Direction, float Range, float HalfAngle);

	/** Wind around a rider for these seconds, leaving shimmer and dust along the path */
	static AVaelAirBlast* SpawnTrail(APawn* Rider, float Duration);

	virtual void Tick(float DeltaSeconds) override;

private:

	void BuildParts(int32 NumShimmer, int32 NumStreaks, int32 NumDust);
	void TickBlast();
	void TickTrail(float DeltaSeconds);

	/** Where the wave front of the blast is after this many seconds, and when it reaches a distance */
	float WaveDistance(float Time) const;
	float WaveArrival(float Distance) const;

	UPROPERTY(Transient)
	TObjectPtr<UVaelCloudCardsComponent> Shimmer;

	UPROPERTY(Transient)
	TObjectPtr<UVaelCloudCardsComponent> Sand;

	UPROPERTY(Transient)
	TObjectPtr<UVaelCloudCardsComponent> Ash;

	UPROPERTY(Transient)
	TObjectPtr<UVaelWindStreaksComponent> Streaks;

	/** Per shimmer card, streak and dust card: its angle off the aim, height, speed or distance, start time */
	TArray<float> ShimmerAngles;
	TArray<float> ShimmerHeights;
	TArray<float> StreakAngles;
	TArray<float> StreakCurls;
	TArray<float> StreakHeights;
	TArray<float> StreakSpeeds;
	TArray<float> StreakDelays;
	TArray<FVector> DustPlaces;
	TArray<float> DustStarts;

	/** Trail: where shimmer and dust were left, and when */
	TArray<FVector> TrailPlaces;
	TArray<float> TrailTimes;
	int32 NextTrailCard = 0;
	FVector LastTrailPlace = FVector::ZeroVector;

	TWeakObjectPtr<APawn> Rider;
	FVector Origin = FVector::ZeroVector;
	FVector Aim = FVector::ForwardVector;
	float Range = 400.0f;
	float HalfAngle = 45.0f;
	float GroundZ = 0.0f;
	float Duration = 1.0f;
	float Age = 0.0f;
	bool bTrail = false;
	int32 NumPebbleBursts = 0;
};
