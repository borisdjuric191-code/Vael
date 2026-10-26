// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelRootRunner.generated.h"

class UPointLightComponent;
class UProceduralMeshComponent;

/**
 *  One strand of the root net (Boris 2026-10-26): when a rooted Floromant calls the garden, a root runs half sunk in the ground
 *  from her feet to a plant or tower, throwing up earth at its front, a light running along with it. Where it arrives the
 *  plant acts; then the root sinks back into the ground.
 */
UCLASS()
class AVaelRootRunner : public AActor
{
	GENERATED_BODY()

public:

	AVaelRootRunner();

	/** Sends a root from one place on the ground to another; the call runs when it arrives */
	static AVaelRootRunner* Run(APawn* Gardener, const FVector& From, const FVector& To, float Speed, const FLinearColor& Color, TFunction<void()> OnArrive);

	virtual void Tick(float DeltaSeconds) override;

private:

	/** Lays the path over the ground in short steps */
	void PlanPath(const FVector& From, const FVector& To);

	/** Builds the tube up to the front, as thick as Thickness says */
	void BuildMesh(float Front, float Thickness);

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> Mesh;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> Light;

	TArray<FVector> Path;
	TArray<float> PathLengths;

	TFunction<void()> Arrive;
	FLinearColor Color = FLinearColor::White;
	float Speed = 1400.0f;
	float Travelled = 0.0f;
	float SinceArrival = -1.0f;
	float SinceDust = 0.0f;
};
