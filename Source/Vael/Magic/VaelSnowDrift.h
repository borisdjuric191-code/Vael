// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelSnowDrift.generated.h"

class UProceduralMeshComponent;

/**
 *  A drift of snow the Lawine leaves in its fan (Boris 2026-11-02): a low, uneven mound that slows the walking enemies of
 *  its caster, lies a few seconds and thaws away. Fire close by, a fire beam or a burning hit melts it in moments into a
 *  puddle, and the enemies standing in it get wet (water, ice, fire, water, as with the hail ice).
 */
UCLASS()
class AVaelSnowDrift : public AActor
{
	GENERATED_BODY()

public:

	AVaelSnowDrift();

	/** Lays a drift of this radius on the ground point */
	static AVaelSnowDrift* Spawn(UWorld* World, const FVector& Ground, float InRadius, APawn* InInstigator);

	/** Share of its speed the actor walks at in the snow of an enemy, 1 where there is none (flying ones aren't slowed) */
	static float GetSpeedMultiplierAt(const AActor* Victim);

	/** Sets every drift touching the circle melting, like a fire landing there */
	static void MeltAround(const UWorld* World, const FVector& Location, float InRadius);

	/** Sets every drift within Width of the line melting, like a fire beam sweeping over it */
	static void MeltAlong(const UWorld* World, const FVector& Start, const FVector& End, float Width);

	virtual void Tick(float DeltaSeconds) override;

private:

	/** Builds the mound */
	void Build();

	/** True if fire burns close enough to melt it */
	bool IsFireClose() const;

	/** Turns to water: a puddle, a puff of steam, wet enemies */
	void BecomeWater();

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> Mound;

	float Radius = 120.0f;
	float Age = 0.0f;

	/** Seconds since fire began to melt it, below 0 while none has */
	float MeltAge = -1.0f;
	float NextFireCheck = 0.0f;
};
