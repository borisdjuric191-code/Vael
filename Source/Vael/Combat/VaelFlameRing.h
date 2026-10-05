// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/VaelCombatStatics.h"
#include "VaelFlameRing.generated.h"

class UInstancedStaticMeshComponent;

/**
 *  A ring that grows outwards along the ground and hurts every enemy of its instigator it passes, once.
 *  Dodging through the line avoids it. Placeholder look: a circle of glowing spheres.
 */
UCLASS()
class AVaelFlameRing : public AActor
{
	GENERATED_BODY()

private:

	/** Placeholder look: one sphere per point of the circle */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInstancedStaticMeshComponent> Flames;

protected:

	/** Number of spheres forming the circle */
	UPROPERTY(EditAnywhere, Category="Appearance", meta = (ClampMin = 3))
	int32 FlameCount = 48;

	/** Height of the spheres above the ground in cm */
	UPROPERTY(EditAnywhere, Category="Appearance", meta = (ClampMin = 0))
	float FlameHeight = 40.0f;

public:

	/** Constructor */
	AVaelFlameRing();

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

	/** Spawns a ring on the ground growing from the location */
	static AVaelFlameRing* SpawnRing(APawn* Attacker, const FVector& GroundLocation, const FVaelSpellHit& InHit, float InSpeed, float InMaxRadius, float InHalfWidth, const FLinearColor& Color);

private:

	/** Places the spheres along the current radius */
	void RefreshFlames();

	/** What the ring does to each target */
	FVaelSpellHit Hit;

	/** Speed at which the radius grows, in cm/s */
	float Speed = 700.0f;

	/** Radius at which the ring disappears, in cm */
	float MaxRadius = 1500.0f;

	/** Targets this close to the ring line are hit, in cm */
	float HalfWidth = 60.0f;

	/** Current radius in cm */
	float Radius = 70.0f;

	/** Targets that have already been hit */
	TSet<TWeakObjectPtr<AActor>> HitActors;
};
