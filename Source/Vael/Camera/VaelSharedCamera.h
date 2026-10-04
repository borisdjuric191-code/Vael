// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelSharedCamera.generated.h"

class UCameraComponent;

/**
 *  Fixed-angle isometric camera shared by all local players.
 *  Follows the centre of the group and zooms out so that everyone stays in frame.
 */
UCLASS()
class AVaelSharedCamera : public AActor
{
	GENERATED_BODY()

public:

	/** Constructor */
	AVaelSharedCamera();

	/** Initialization */
	virtual void BeginPlay() override;

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

	/** Returns the fixed world yaw of the camera, used to make movement input camera-relative */
	float GetCameraYaw() const { return CameraYaw; }

	/** Removes the part of a move direction that would take a pawn out of the area the camera can frame */
	FVector ConstrainMoveDirection(const FVector& PawnLocation, const FVector& Direction) const;

	/** Returns the camera component **/
	UCameraComponent* GetCameraComponent() const { return Camera.Get(); }

protected:

	/** Shared camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UCameraComponent> Camera;

	/** Downward tilt of the camera in degrees */
	UPROPERTY(EditAnywhere, Category="Camera", meta = (ClampMin = -89, ClampMax = -10))
	float CameraPitch = -45.0f;

	/** World yaw of the camera in degrees */
	UPROPERTY(EditAnywhere, Category="Camera")
	float CameraYaw = 45.0f;

	/** Horizontal field of view in degrees */
	UPROPERTY(EditAnywhere, Category="Camera", meta = (ClampMin = 10, ClampMax = 120))
	float FieldOfView = 55.0f;

	/** Distance to the group centre with a single player or a tight group */
	UPROPERTY(EditAnywhere, Category="Camera", meta = (ClampMin = 0))
	float MinDistance = 1600.0f;

	/** Furthest the camera zooms out; players can't move further apart than this distance can frame */
	UPROPERTY(EditAnywhere, Category="Camera", meta = (ClampMin = 0))
	float MaxDistance = 4500.0f;

	/** Free space kept between the outermost player and the screen edge, in world units */
	UPROPERTY(EditAnywhere, Category="Camera", meta = (ClampMin = 0))
	float FramePadding = 350.0f;

	/** How quickly the camera follows the group centre */
	UPROPERTY(EditAnywhere, Category="Camera", meta = (ClampMin = 0))
	float FollowSpeed = 6.0f;

	/** How quickly the camera zooms in and out */
	UPROPERTY(EditAnywhere, Category="Camera", meta = (ClampMin = 0))
	float ZoomSpeed = 3.0f;

private:

	/** Tangent of the half angle that a group has to fit into, taking the viewport aspect ratio into account */
	float GetFitTangent() const;

	/** Centre of all player pawns, unsmoothed */
	FVector GroupCenter = FVector::ZeroVector;

	/** Smoothed point the camera looks at */
	FVector FocusPoint = FVector::ZeroVector;

	/** Smoothed distance to the focus point */
	float CurrentDistance = 0.0f;

	/** Furthest a player may be from the group centre */
	float LeashRadius = 0.0f;

	/** Number of player pawns followed in the last update */
	int32 NumTrackedPlayers = 0;

	/** If true, the next update skips smoothing */
	bool bSnapNextUpdate = true;
};
