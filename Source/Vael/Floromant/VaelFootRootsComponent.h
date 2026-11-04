// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "VaelFootRootsComponent.generated.h"

class UProceduralMeshComponent;
class USkeletalMeshComponent;

/**
 *  The roots that grow out of the legs of a Floromant while she stands rooted. Boris 2026-11-04: strong like the stilts of a
 *  mangrove: thick, knotty stilt roots arch out from each shin and dive into the ground around her, little rootlets fan out
 *  where they go in, thinner roots creep over the ground, and a gnarled collar holds each foot. Built in code with the root
 *  material of the plants and a tiling bark over it; they grow out in turn (collar, stilts, creepers, rootlets) and draw back
 *  with the growth the Floromant component hands over every frame.
 */
UCLASS()
class UVaelFootRootsComponent : public USceneComponent
{
	GENERATED_BODY()

public:

	/** Grows the roots out of the feet of the body as far as the growth says (0 to 1); 0 hides them, and they take a new shape next time */
	void SetGrowth(USkeletalMeshComponent* Body, float Growth);

private:

	/** One root: points of its path in world space from the leg into the ground, its thickness at the start, the share of the
	 *  whole growth in which it grows, and its own knots and ridges */
	struct FRootPath
	{
		TArray<FVector> Points;
		float Radius = 2.0f;
		float GrowStart = 0.0f;
		float GrowEnd = 1.0f;
		float Seed = 0.0f;
		float Knots = 3.0f;
		/** Thicker where it joins the leg, like a stilt root at the trunk */
		float Flare = 0.4f;
		/** Share of its thickness left at the tip */
		float Tip = 0.2f;
	};

	/** Picks the paths of the roots from where the feet stand now */
	void PlanRoots(USkeletalMeshComponent* Body);

	/** Ground height under a place, or the fallback */
	float GroundAt(const FVector& Location, float Fallback) const;

	/** Adds a root along a curve (Along 0 to 1) sampled at Count points, with a crooked sway across it */
	FRootPath& AddCurve(TFunctionRef<FVector(float)> Curve, int32 Count, float Radius, float Sway, float GrowStart, float GrowEnd);

	/** Builds the tubes as far as the growth reaches */
	void BuildMesh(float Growth);

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> Mesh;

	TArray<FRootPath> Roots;

	float ShownGrowth = -1.0f;
};
