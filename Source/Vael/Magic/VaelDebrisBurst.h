// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelDebrisBurst.generated.h"

class UStaticMeshComponent;

/** How a burst of debris looks and flies */
struct FVaelDebris
{
	/** Pieces in the burst */
	int32 Count = 12;

	/** The pieces start spread over a disc of this radius in cm */
	float Spread = 40.0f;

	/** Outward and upward speed in cm/s, each piece gets between half and all of it */
	float Speed = 350.0f;
	float Lift = 300.0f;

	/** Fall in cm/s² (below 0 rises), slowing by the air, seconds each piece lasts */
	float Gravity = 980.0f;
	float Drag = 1.5f;
	float Lifetime = 0.6f;

	/** Smallest and largest piece in cm */
	float MinSize = 6.0f;
	float MaxSize = 14.0f;

	/** Color and glow of the pieces; a glowing piece fades instead of shrinking at the end */
	FLinearColor Color = FLinearColor(0.42f, 0.36f, 0.28f);
	float Glow = 0.0f;

	/** Share of the pieces that are rocks of the earth orb (three times as large as the others), 0 for none */
	float RockShare = 0.0f;

	/** True for soft puffs of the additive glow material, false for solid pieces */
	bool bSoft = false;

	/** True if the pieces stop on the ground below the burst instead of falling through it */
	bool bLandOnGround = true;
};

/**
 *  Pieces flung from a point that tumble, fall and vanish: dust, pebbles, rocks, drops of lava, shards.
 *  Shared by the looks of earth spells. Built from spheres and the rock of the earth orb until a Niagara effect exists.
 */
UCLASS()
class AVaelDebrisBurst : public AActor
{
	GENERATED_BODY()

public:

	/** Constructor */
	AVaelDebrisBurst();

	/** Flings debris from a location; returns null if nothing could be spawned */
	static AVaelDebrisBurst* Spawn(const UObject* WorldContext, const FVector& Location, const FVaelDebris& Debris);

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

private:

	/** Builds the pieces */
	void Build(const FVaelDebris& InDebris);

	FVaelDebris Debris;

	/** Seconds since the burst */
	float Elapsed = 0.0f;

	/** The pieces, where each started, how fast and how it tumbles, its size and whether it is a rock */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Pieces;

	TArray<FVector> Origins;
	TArray<FVector> Velocities;
	TArray<FRotator> Tumbles;
	TArray<float> Sizes;
	TArray<float> Delays;

	/** Height of the ground the pieces land on */
	float GroundZ = -UE_BIG_NUMBER;
};
