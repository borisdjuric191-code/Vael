// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Magic/VaelWaterMesh.h"
#include "VaelWaterGeyser.generated.h"

class UProceduralMeshComponent;

/**
 *  A geyser of boiling water as in nature, built as a mesh every frame: a lumpy column shoots out of the ground, dark at its foot and
 *  white with foam at its top, foams over in a crown that falls back as a torn skirt, steams, throws drops high up and rains them down,
 *  while a ring of white water runs out over the ground. Then it sinks back, its water stays as a puddle.
 *  Its look comes from M_Water_Wave (UVaelMagicSettings::WaveMaterial), its values from Water Kit|Geyser.
 */
UCLASS()
class AVaelWaterGeyser : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UProceduralMeshComponent> Mesh;

public:

	/** Constructor */
	AVaelWaterGeyser();

	/** Bursts out of the ground at GroundLocation; Radius is the reach of the strike. The instigator owns the puddle it leaves. Null without the wave material. */
	static AVaelWaterGeyser* Spawn(UWorld* World, const FVector& GroundLocation, float Radius, APawn* InInstigator);

	/** Grows, heaves and falls back, throws and moves the drops */
	virtual void Tick(float DeltaSeconds) override;

private:

	/** Shape of the geyser right now, worked out from its age */
	void UpdateState();

	/** Builds the column, the crown, the ring on the ground, the steam and the drops into the mesh */
	void BuildMesh();

	/** Throws new drops from the top, the foot and the falling column */
	void ThrowDrops(float DeltaSeconds);

	/** How far the column sways off its foot at Height share (0 foot, 1 top) */
	FVector GetSway(float HeightShare) const;

	/** Where a point of the column at Height share (0 foot, 1 top) and Angle stands, in actor space */
	FVector GetColumnPoint(float HeightShare, float Angle) const;

	/** Reach of the strike, radius of the column at its foot, full height in cm, how wild it is */
	float Radius = 220.0f;
	float ColumnRadius = 70.0f;
	float MaxHeight = 520.0f;
	float Violence = 1.0f;

	/** Seconds from bursting out to running out */
	float Lifetime = 1.7f;

	/** A random number per geyser, so no two look alike */
	float Seed = 0.0f;

	/** Seconds since it burst out */
	float Elapsed = 0.0f;

	/** Shape right now: height of the column, width against its full width, how far the crown has spread, ring on the ground, collapse into foam, overall fade */
	float Height = 0.0f;
	float Width = 1.0f;
	float Crown = 0.0f;
	float RingRadius = 0.0f;
	float RingFade = 0.0f;
	float Collapse = 0.0f;
	float Fade = 0.0f;

	/** How far the top of the column sways off its foot, in actor space, and how solid the crown is */
	FVector SwayTop = FVector::ZeroVector;
	float CrownFade = 0.0f;

	/** True once its water has been poured onto the ground */
	bool bPoured = false;

	/** Flying drops */
	FVaelWaterDrops Drops;

	/** Column, crown, ring on the ground, steam and drops */
	FVaelWaveSection Column;
	FVaelWaveSection CrownSheet;
	FVaelWaveSection Ring;
	FVaelWaveSection Steam;
	FVaelWaveSection DropMesh;

	/** True once the mesh sections exist and are only updated */
	bool bMeshCreated = false;
};
