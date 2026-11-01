// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "VaelBeamFunnel.generated.h"

class UMaterialInterface;
class UProceduralMeshComponent;

/**
 *  What leads a beam out of the open palm: a cone of streaming fire that opens from the hand, flows over the ball of the source
 *  and runs into the beam, with streaks and sparks racing along it. The beam itself stays as it is.
 *  All in world space, built anew every frame, because hand and aim never stand still.
 */
UCLASS()
class UVaelBeamFunnelComponent : public USceneComponent
{
	GENERATED_BODY()

public:

	UVaelBeamFunnelComponent();

	/** Takes the fire of the beam for its skin and the material of the lightning for its streaks */
	void Init(UMaterialInterface* FireMaterial, UMaterialInterface* StreakMaterial);

	/** Lays it from the palm over the ball into the beam */
	void Update(const FVector& Palm, const FVector& Direction, const FVector& BallCenter, float BallRadius, float BeamRadius, float DeltaSeconds);

private:

	/** Distance from the axis at a share of the way from the palm to the mouth */
	float RadiusAt(float Share) const;

	/** A place on the skin: share of the way, angle around the axis, how far off the skin */
	FVector SkinPoint(float Share, float Angle, float Lift) const;

	/** Adds a ribbon turned to the camera along the points */
	void AddRibbon(const TArray<FVector>& Points, float Width, float Brightness, float Core);

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> Mesh;

	/** The frame of this update */
	FVector Origin = FVector::ZeroVector;
	FVector Axis = FVector::ForwardVector;
	FVector Across = FVector::RightVector;
	FVector Over = FVector::UpVector;
	FVector Camera = FVector::ZeroVector;
	float Reach = 100.0f;
	float WaistRadius = 40.0f;
	float MouthRadius = 40.0f;
	float WaistShare = 0.5f;
	float Age = 0.0f;

	/** The ribbons gathered this frame */
	TArray<FVector> Vertices;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;
	TArray<int32> Triangles;

	bool bBuilt = false;
};
