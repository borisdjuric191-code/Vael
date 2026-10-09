// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/VaelCombatStatics.h"
#include "VaelRockWall.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/**
 *  One block of a rock wall raised by a mage. Blocks creatures and projectiles like any wall,
 *  gives mages next to it the earth element and crumbles after its lifetime.
 *  Look: rocks of the earth orb that break out of the ground with dust and crumble at the end; a box in the color of earth while the rock pack is missing.
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

	/** Crumbles instead of vanishing when its lifetime is over */
	virtual void LifeSpanExpired() override;

	/** Raises a block standing on the ground at the location, facing the yaw. Returns null if it can't be spawned. */
	static AVaelRockWall* RaiseBlock(UWorld* World, const FVector& GroundLocation, float Yaw, float Size, float Height, float Lifetime, APawn* InInstigator, const FVaelSpellHit* InContactHit = nullptr);

protected:

	/** Builds the rocks of the look and throws up dust where they break out of the ground */
	virtual void BeginPlay() override;

private:

	/** Sizes collision and look */
	void SetBlockSize(float Size, float Height);

	/** Height of the block once it has fully risen */
	float FullHeight = 0.0f;

	/** Seconds since the block appeared */
	float RiseTime = 0.0f;

	/** Edge length of the block */
	float BlockSize = 0.0f;

	/** Rocks of the earth orb that make the look instead of the box, while the pack is installed, and where each sits once risen */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Rocks;

	TArray<FVector> RockPlaces;
	TArray<FVector> RockScales;

	/** A wall of bone and flesh hurts enemies that touch it with this hit, like the Narbenwall */
	bool bHurtsOnContact = false;
	FVaelSpellHit ContactHit;

	/** Seconds until enemies touching it are hurt again */
	float ContactCountdown = 0.0f;

	/** Hurts the enemies touching the block */
	void HurtTouchingEnemies();

	/** True once the block has begun to crumble, and seconds since then */
	bool bCrumbling = false;
	float CrumbleTime = 0.0f;
};
