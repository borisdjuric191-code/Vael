// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelCameraZone.generated.h"

/**
 *  A place where the shared camera steps back, so a landmark that is too big for the close view shows whole, like a great
 *  Mondtraufe or the Urbaum (Boris 2026-11-03). While a player stands within Radius the camera keeps at least Distance from
 *  the group and leans its view a little towards Focus; across the outer EdgeWidth it blends in and out softly.
 *  It only sets a floor: a group spread wider still zooms out further as usual.
 */
UCLASS()
class AVaelCameraZone : public AActor
{
	GENERATED_BODY()

public:

	AVaelCameraZone();

	/** Radius around the zone in cm where the full step back holds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera Zone", meta = (ClampMin = 100))
	float Radius = 1500.0f;

	/** Width of the soft edge outside Radius, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera Zone", meta = (ClampMin = 0))
	float EdgeWidth = 800.0f;

	/** Least distance of the camera to the group inside the zone, in cm (the close view is 1600) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera Zone", meta = (ClampMin = 0))
	float Distance = 3500.0f;

	/** What the camera should show, relative to the zone (the landmark's middle) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera Zone", meta = (MakeEditWidget = true))
	FVector Focus = FVector::ZeroVector;

	/** How far the view leans from the group towards Focus, 0 not at all, 1 all the way */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera Zone", meta = (ClampMin = 0, ClampMax = 1))
	float FocusPull = 0.3f;

	/** How much of the zone a place feels, 1 within Radius fading to 0 at its edge */
	float GetWeight(const FVector& Location) const;

	FVector GetFocusLocation() const { return GetActorTransform().TransformPosition(Focus); }
};
