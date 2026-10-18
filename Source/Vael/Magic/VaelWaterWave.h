// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "VaelWaterWave.generated.h"

/** A drop of spray thrown off a wave, in world space; a lifetime of 0 marks a free slot */
struct FVaelWaveDrop
{
	FVector Location = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	float Age = 0.0f;
	float Lifetime = 0.0f;
	float Size = 0.0f;

	/** A bigger lump of water that stretches along its flight */
	bool bClump = false;
};

/** Vertices of one section of the wave mesh, kept between frames so the mesh is only updated */
struct FVaelWaveSection
{
	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;
	TArray<int32> Triangles;

	void Reset(int32 NumVertices);
};

/**
 *  A wave of water as in nature, built as a mesh every frame: a long back, a steep face and a lip that curls over and breaks,
 *  dark blue at its foot, pale at the crest, boiling foam, a torn veil of spray thrown up from the breaking crest and fine drops.
 *  Height, breaking and crest change along its width all the time, so it never looks even.
 *  The Welle rolls out over a cone and runs out as foam; the Flutwelle rides with its spell and crashes down when it ends.
 *  Its look comes from M_Water_Wave (UVaelMagicSettings::WaveMaterial).
 */
UCLASS()
class AVaelWaterWave : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UProceduralMeshComponent> Mesh;

public:

	/** Constructor */
	AVaelWaterWave();

	/**
	 *  Welle: rolls out from Origin (on the ground) along Facing, over HalfAngle degrees to each side, and reaches Range after RollTime seconds.
	 *  Null without the wave material.
	 */
	static AVaelWaterWave* SpawnCone(UWorld* World, const FVector& Origin, const FRotator& Facing, float Range, float HalfAngle, float RollTime, float Height, float Violence);

	/** Flutwelle: rides along with Carrier, HalfWidth to each side, its foot Foot cm above (below) the carrier. Null without the wave material. */
	static AVaelWaterWave* SpawnCarried(AActor* Carrier, float HalfWidth, float Height, float Foot, float Violence);

	/** The carrier is gone: the wave rears up once more, crashes down where it is and runs out as foam */
	void Crash();

	/** Moves and reshapes the wave, throws and moves the spray */
	virtual void Tick(float DeltaSeconds) override;

private:

	/** Shape of the wave right now, worked out from its age */
	void UpdateState(float DeltaSeconds);

	/** Builds the sheet of water, the veil of spray and the drops into the mesh */
	void BuildMesh();

	/** Throws new drops off the crest and moves the flying ones */
	void UpdateDrops(float DeltaSeconds);

	/** Where column Across (-1 .. 1) of the wave stands, in actor space, and which way it faces */
	void GetColumn(float Across, FVector& OutBase, FVector& OutForward) const;

	/** Number of columns along the width of the wave */
	int32 GetNumColumns() const;

	/** Cone or carried */
	bool bCarried = false;

	/** The spell the Flutwelle rides with */
	TWeakObjectPtr<AActor> Carrier;

	/** Height of the carrier's foot relative to it */
	float CarrierFoot = 0.0f;

	/** Welle: reach in cm, half angle of the cone in radians, seconds to roll out */
	float Range = 500.0f;
	float Spread = 0.6f;
	float RollTime = 0.85f;

	/** Flutwelle: half its width in cm */
	float HalfWidth = 200.0f;

	/** Full height in cm and how wild it is */
	float MaxHeight = 150.0f;
	float Violence = 0.5f;

	/** A random number per wave, so no two look alike */
	float Seed = 0.0f;

	/** Seconds since spawn; when the Flutwelle crashed, or below 0 */
	float Elapsed = 0.0f;
	float CrashTime = -1.0f;

	/** Shape right now: distance rolled out (Welle), height, how far the crest has broken, how far it has collapsed into foam, overall fade */
	float Distance = 0.0f;
	float Height = 0.0f;
	float Break = 0.0f;
	float Collapse = 0.0f;
	float Fade = 0.0f;

	/** How fast the wave moves, cm/s, and the Flutwelle's last velocity */
	float Speed = 0.0f;
	FVector Velocity = FVector::ZeroVector;
	FVector LastCarrierLocation = FVector::ZeroVector;

	/** Per column, filled while building the sheet: the tip of the lip in actor space, its facing, how far it has broken, its height */
	TArray<FVector> LipTips;
	TArray<FVector> LipForwards;
	TArray<float> LipBreaks;
	TArray<float> LipHeights;

	/** Flying drops, a fixed pool */
	TArray<FVaelWaveDrop> Drops;

	/** Drops owed to the crest from the last frames */
	float DropDebt = 0.0f;

	/** Sheet, spray veil and drops */
	FVaelWaveSection Sheet;
	FVaelWaveSection Spray;
	FVaelWaveSection DropMesh;

	/** True once the mesh sections exist and are only updated */
	bool bMeshCreated = false;
};
