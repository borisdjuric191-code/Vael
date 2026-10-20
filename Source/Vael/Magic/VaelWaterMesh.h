// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UProceduralMeshComponent;
class UWorld;

/**
 *  Shared parts of water in motion that is built as a mesh every frame (waves, the Wassergeschoss, the Geysir), drawn with M_Water_Wave.
 *  Vertex colour: R height in the water (dark to pale), G foam, B 1 for a drop quad, A how solid it is.
 */

/** A drop of spray, in world space; a lifetime of 0 marks a free slot */
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

/** Vertices of one section of a water mesh, kept between frames so the mesh is only updated */
struct FVaelWaveSection
{
	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;
	TArray<int32> Triangles;

	void Reset(int32 NumVertices);

	/** Creates the section the first time, only updates it afterwards */
	void Apply(UProceduralMeshComponent* Mesh, int32 SectionIndex, bool bCreate) const;
};

/** A fixed pool of drops: thrown, slowed by the air, falling, gone when they land; drawn as quads facing the camera */
struct FVaelWaterDrops
{
	TArray<FVaelWaveDrop> Pool;

	/** Drops owed from the last frames, for emitters that throw at a rate */
	float Debt = 0.0f;

	/** Pool of the given size, all free */
	void Init(int32 Num);

	/** A free slot, or null when all are flying */
	FVaelWaveDrop* FindFree();

	/** Fills a free slot with a new drop; false when all are flying */
	bool Throw(const FVector& Location, const FVector& Velocity, float Lifetime, float Size, bool bClump);

	/** Moves the flying drops; a drop is gone when it falls below GroundZ */
	void Update(float DeltaSeconds, float GroundZ, float Gravity = 980.0f, float Drag = 0.6f);

	/** Quads of all slots into Out, in the space of Transform; Fade dims them all, Foam is the foam of a small drop (clumps a little more) */
	void Build(FVaelWaveSection& Out, const FTransform& Transform, const FVector& CameraLocation, float Fade, float Foam = 0.5f) const;

	/** Triangles of the quads, once */
	void BuildTriangles(FVaelWaveSection& Out) const;
};

namespace VaelWaterMesh
{
	/** Perlin noise from -1 to 1 */
	float Noise(float X, float Y);

	/** Normals of a grid of Columns x Rows vertices, row by row within each column */
	void ComputeGridNormals(const TArray<FVector>& Vertices, int32 Columns, int32 Rows, TArray<FVector>& OutNormals);

	/** Triangles of a grid of Columns x Rows vertices */
	void BuildGridTriangles(int32 Columns, int32 Rows, TArray<int32>& OutTriangles);

	/**
	 *  Triangles of a tube of Rings x (Around + 1) vertices (ring by ring, the last point of a ring repeats the first), its strips around
	 *  sorted so the side facing away from the camera is drawn first: see-through water then never shows its back over its front.
	 *  AxisA and AxisB span the rings (the point of angle a lies along AxisA cos a + AxisB sin a), ToCamera in the same space.
	 */
	void BuildViewSortedTubeTriangles(int32 Rings, int32 Around, const FVector& AxisA, const FVector& AxisB, const FVector& ToCamera, TArray<int32>& OutTriangles);

	/** Where the first player looks from, or Fallback without one */
	FVector GetCameraLocation(const UWorld* World, const FVector& Fallback);
}
