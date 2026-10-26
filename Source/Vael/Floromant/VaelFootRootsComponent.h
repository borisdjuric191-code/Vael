// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "VaelFootRootsComponent.generated.h"

class UProceduralMeshComponent;
class USkeletalMeshComponent;

/**
 *  The roots that grow out of the feet of a Floromant when she stands firing on the spot: a few thin, crooked tubes from each
 *  foot that reach outwards and down into the ground. Built in code with the root material of the plants; they grow and
 *  draw back with the growth the Floromant component hands over every frame.
 */
UCLASS()
class UVaelFootRootsComponent : public USceneComponent
{
	GENERATED_BODY()

public:

	/** Grows the roots out of the feet of the body as far as the growth says (0 to 1); 0 hides them, and they take a new shape next time */
	void SetGrowth(USkeletalMeshComponent* Body, float Growth);

private:

	/** One root: points of its path in world space, from the foot into the ground, and its thickness at the foot */
	struct FRootPath
	{
		TArray<FVector> Points;
		float Radius = 2.0f;
	};

	/** Picks the paths of the roots from where the feet stand now */
	void PlanRoots(USkeletalMeshComponent* Body);

	/** Builds the tubes as far as the growth reaches */
	void BuildMesh(float Growth);

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> Mesh;

	TArray<FRootPath> Roots;

	float ShownGrowth = -1.0f;
};
