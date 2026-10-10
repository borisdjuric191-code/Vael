// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelSafeZone.generated.h"

class USphereComponent;

/**
 *  A safe place like the camp of the survivors or the Gildenkontor: creatures don't walk in, don't pick players inside as
 *  their target, and nothing hurts players inside. People and traders stand in such zones.
 *  Invisible in the game; the sphere shows the radius in the editor.
 */
UCLASS()
class AVaelSafeZone : public AActor
{
	GENERATED_BODY()

	/** Shows the radius in the editor; no collision */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> Area;

protected:

	/** Radius on the ground in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Safe Zone", meta = (ClampMin = 100))
	float Radius = 1000.0f;

public:

	/** Constructor */
	AVaelSafeZone();

	/** True if the location lies in a safe zone, the margin in cm added to its radius */
	static bool IsSafe(const UWorld* World, const FVector& Location, float Margin = 0.0f);

	/** Takes the part of a horizontal move that would lead into a safe zone away, so creatures slide along its edge */
	static FVector KeepOut(const UWorld* World, const FVector& Location, const FVector& Move, float Margin);

	float GetRadius() const { return Radius; }

#if WITH_EDITOR
	virtual void OnConstruction(const FTransform& Transform) override;
#endif
};
