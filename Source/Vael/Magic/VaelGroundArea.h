// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Magic/VaelElementTypes.h"
#include "VaelGroundArea.generated.h"

class UStaticMeshComponent;

/**
 *  A round patch of an element on the ground: a fire, a puddle, later mud or steam.
 *  Mages standing in or next to it draw its element from the environment.
 *  Fires left by a caster burn the caster's enemies, can be spread by wind and put out by water.
 *  Can be placed in levels by hand or spawned by formulas. Placeholder look: a flat disc in the color of the element.
 */
UCLASS()
class AVaelGroundArea : public AActor
{
	GENERATED_BODY()

private:

	/** Placeholder look */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Disc;

protected:

	/** Element of the area */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Area")
	EVaelElement Element = EVaelElement::Fire;

	/** Radius in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Area", meta = (ClampMin = 1))
	float Radius = 150.0f;

	/** Seconds until the area disappears, 0 for never */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Area", meta = (ClampMin = 0))
	float Lifetime = 0.0f;

	/** Damage per second to the enemies of whoever created the area. Areas placed in a level hurt nobody. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Area", meta = (ClampMin = 0))
	float DamagePerSecond = 0.0f;

	/** If false, water can't put this fire out, like a camp fire */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Area")
	bool bExtinguishable = true;

public:

	/** Constructor */
	AVaelGroundArea();

	/** Applies the properties to the placeholder look */
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Initialization */
	virtual void BeginPlay() override;

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

	/** Spawns an area on the ground. The instigator decides who its damage hurts. */
	static AVaelGroundArea* SpawnArea(UWorld* World, const FVector& Location, EVaelElement InElement, float InRadius, float InLifetime, float InDamagePerSecond, APawn* InInstigator, bool bInExtinguishable = true);

	/** Puts out every fire that touches the given circle. Returns the number of fires put out. */
	static int32 ExtinguishFires(const UWorld* World, const FVector& Location, float InRadius);

	/** Carries the fires inside a cone further in its direction. Returns the number of new fires. */
	static int32 SpreadFires(APawn* Caster, const FVector& Origin, const FVector& Direction, float Range, float HalfAngleDegrees);

	/** True if a location is inside the area or closer to its edge than the extra distance */
	bool IsInRange(const FVector& Location, float ExtraDistance = 0.0f) const;

	EVaelElement GetElement() const { return Element; }
	float GetRadius() const { return Radius; }

private:

	/** Sizes and colors the disc */
	void RefreshLook();

	/** Seconds since the last damage step */
	float DamageStepTime = 0.0f;
};
