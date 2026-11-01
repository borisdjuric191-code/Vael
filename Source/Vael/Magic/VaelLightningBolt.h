// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelLightningBolt.generated.h"

class UMaterialInstanceDynamic;
class UPointLightComponent;
class UProceduralMeshComponent;

/** One line of a bolt: the main channel or a branch */
struct FVaelBoltLine
{
	TArray<FVector> Points;

	/** Width against the main channel, and brightness */
	float Width = 1.0f;
	float Brightness = 1.0f;

	/** How far the leader has come along the main channel when this line starts, 0 to 1 */
	float StartAt = 0.0f;
};

/**
 *  Look of a bolt of lightning between two points, like a jump of the Kettenblitz. Only shows, the spell hits on its own.
 *  A leader feels its way along a branching channel, then the channel flashes white-hot, strikes again once or twice and glows out in its color.
 *  Sparks fly and small arcs crackle where it hit. All of it is one mesh of ribbons turned to the camera, with the material of Scripts/CreateLightningKit.py.
 */
UCLASS()
class AVaelLightningBolt : public AActor
{
	GENERATED_BODY()

public:

	/** Constructor */
	AVaelLightningBolt();

	/** Spawns a bolt from one point to another that lasts some seconds; thickness scales its width, sparks spray at its end, a dark bolt has a black core in a halo of its color */
	static AVaelLightningBolt* Spawn(AActor* Caster, const FVector& From, const FVector& To, const FLinearColor& Color, float Duration = 0.25f, float Thickness = 1.0f, bool bSparks = true, bool bDark = false);

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

protected:

	/** Builds the look */
	virtual void BeginPlay() override;

private:

	/** Grows a new channel with its branches */
	void BuildChannel();

	/** Adds a jagged line between two points to the given list of points, without the start */
	void AddJagged(TArray<FVector>& Points, const FVector& Start, const FVector& End, float Stray, int32 Depth) const;

	/** Grows a branch off a point of the channel; it may fork once more */
	void AddBranch(const FVector& Start, const FVector& Direction, float Length, float Width, float Brightness, float StartAt, int32 ForksLeft);

	/** Throws small arcs around the place it hit into a new shape */
	void BuildCrackle();

	/** Adds a ribbon along the points up to the share of its length; G is the share of white-hot core */
	void AddRibbon(const TArray<FVector>& Points, float Share, float Width, float Brightness, float Core);

	/** Adds a round flash facing the camera */
	void AddFlash(const FVector& Center, float Size, float InBrightness, float Core);

	/** Sends the geometry gathered this frame to the mesh */
	void CommitMesh();

	/** Ends, color, seconds the bolt lasts and its width */
	FVector From = FVector::ZeroVector;
	FVector To = FVector::ZeroVector;
	FLinearColor BoltColor = FLinearColor::White;
	float Duration = 0.25f;
	float Thickness = 1.0f;
	bool bSparks = true;
	bool bDark = false;

	/** Seconds since it started, and until the next shape of the small arcs */
	float Elapsed = 0.0f;
	float CrackleCountdown = 0.0f;

	/** Which strike of the bolt is running; each one starts from a slightly changed channel */
	int32 StrokeIndex = 0;

	/** Seconds after the leader arrived at which the strikes come */
	TArray<float> StrokeTimes;

	TArray<FVaelBoltLine> Lines;
	TArray<TArray<FVector>> Crackles;

	/** Sparks where it strikes, their speeds in cm/s */
	TArray<FVector> SparkVelocities;

	/** The camera this frame, and the geometry being gathered */
	FVector CameraLocation = FVector::ZeroVector;
	TArray<FVector> Vertices;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;
	TArray<int32> Triangles;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> Mesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;

	/** Flash at both ends */
	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> StartLight;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> EndLight;
};
