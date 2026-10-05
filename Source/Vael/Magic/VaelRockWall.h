// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelRockWall.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/**
 *  One block of a rock wall raised by a mage. Blocks creatures and projectiles like any wall,
 *  gives mages next to it the earth element and crumbles after its lifetime.
 *  Placeholder look: a box in the color of earth that rises out of the ground.
 */
UCLASS()
class AVaelRockWall : public AActor
{
	GENERATED_BODY()

private:

	/** Blocks everything that walls block */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> Collision;

	/** Placeholder look */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Block;

protected:

	/** Seconds the block needs to rise out of the ground */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Wall", meta = (ClampMin = 0))
	float RiseDuration = 0.17f;

public:

	/** Constructor */
	AVaelRockWall();

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

	/** Raises a block standing on the ground at the location, facing the yaw. Returns null if it can't be spawned. */
	static AVaelRockWall* RaiseBlock(UWorld* World, const FVector& GroundLocation, float Yaw, float Size, float Height, float Lifetime, APawn* InInstigator);

private:

	/** Sizes collision and look */
	void SetBlockSize(float Size, float Height);

	/** Height of the block once it has fully risen */
	float FullHeight = 0.0f;

	/** Seconds since the block appeared */
	float RiseTime = 0.0f;
};
