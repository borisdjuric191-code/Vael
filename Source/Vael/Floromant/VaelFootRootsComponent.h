// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "VaelFootRootsComponent.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UProceduralMeshComponent;
class USkeletalMeshComponent;

/**
 *  The roots of a Floromant while she stands rooted. Boris 2026-11-04/05: strong like the stilts of a mangrove, and clear where
 *  they come out of her: roots wind round each leg from the toes up to the knee and follow its bones, from the shin thick
 *  knotty stilts arch out and dive into the ground, rootlets fan out where they go in, thinner roots creep over the ground, a
 *  gnarled collar holds each foot, moss grows over them. While she fires rooted, pulses of earth power run up the roots into
 *  her legs.
 */
UCLASS()
class UVaelFootRootsComponent : public USceneComponent
{
	GENERATED_BODY()

public:

	/** Grows the roots out of the legs of the body as far as the growth says (0 to 1); 0 hides them, and they take a new shape
	 *  next time. bFiring sends pulses of earth power up the roots. */
	void SetGrowth(USkeletalMeshComponent* Body, float Growth, bool bFiring, float DeltaTime);

private:

	/** One root in the ground: points of its path in world space from the leg into the ground, its thickness at the start, the
	 *  share of the whole growth in which it grows, its own knots and ridges */
	struct FRootPath
	{
		TArray<FVector> Points;
		float Radius = 2.0f;
		float GrowStart = 0.0f;
		float GrowEnd = 1.0f;
		float Seed = 0.0f;
		float Knots = 3.0f;
		float Flare = 0.4f;
		float Tip = 0.2f;
		/** Pulses run along it from the ground up (stilts, creepers: reversed) and which foot it belongs to */
		bool bPulseReversed = true;
		int32 Foot = 0;
	};

	/** A root wound round a leg: which leg, where round the leg it starts, how often it turns, its thickness */
	struct FLegRoot
	{
		int32 Foot = 0;
		float Phase = 0.0f;
		float Turns = 1.3f;
		float Radius = 1.2f;
		float GrowStart = 0.0f;
		float GrowEnd = 0.6f;
		float Seed = 0.0f;
	};

	/** A pulse of earth power: on a ground root (Root >= 0) or a leg root (Leg >= 0), how far it has run (0 to 1) */
	struct FPulse
	{
		int32 Root = INDEX_NONE;
		int32 Leg = INDEX_NONE;
		float Along = 0.0f;
	};

	void PlanRoots(USkeletalMeshComponent* Body);
	float GroundAt(const FVector& Location, float Fallback) const;
	FRootPath& AddCurve(TFunctionRef<FVector(float)> Curve, int32 Count, float Radius, float Sway, float GrowStart, float GrowEnd);

	/** The way of a leg root now: from the toes over the ankle up to the knee, wound round the leg as the bones stand */
	void LegPath(USkeletalMeshComponent* Body, const FLegRoot& Leg, TArray<FVector>& OutPoints) const;

	/** Adds a tube along points, from Share0 to Share1 of the whole root, with thickness, knots, ridges and the way of its UVs */
	void AddTube(TArray<FVector>& Vertices, TArray<FVector>& Normals, TArray<FVector2D>& UVs, TArray<struct FProcMeshTangent>& Tangents, TArray<int32>& Triangles,
		const TArray<FVector>& Path, const TArray<float>& Shares, float Radius, float Flare, float Tip, float Knots, float Seed, bool bPointedEnd, int32 Sides) const;

	/** Builds the roots in the ground and on the legs as far as the growth reaches, and the pulses */
	void BuildMesh(USkeletalMeshComponent* Body, float Growth);

	/** Moss tufts on the arches of the stilts, at the collar and where roots go in */
	void PlaceMoss(float Growth);

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> Mesh;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> MossTufts;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> MossMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MossLook;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PulseMaterial;

	TArray<FRootPath> Roots;
	TArray<FLegRoot> LegRoots;
	TArray<FPulse> Pulses;

	/** Where the moss sits: place, up direction, size */
	TArray<FTransform> MossPlaces;


	float ShownGrowth = -1.0f;
	float PulseCountdown = 0.0f;
};
