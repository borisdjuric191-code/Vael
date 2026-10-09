// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelLightningBolt.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

/**
 *  Look of a bolt of lightning between two points, like a jump of the Kettenblitz. Only shows, the spell hits on its own.
 *  A jagged, white-hot line in a halo of its color with a few forks; it jumps into a new shape several times and fades,
 *  sparks spray where it strikes. Built from glowing spheres until a Niagara effect exists.
 */
UCLASS()
class AVaelLightningBolt : public AActor
{
	GENERATED_BODY()

public:

	/** Constructor */
	AVaelLightningBolt();

	/** Spawns a bolt from one point to another that lasts some seconds; thickness scales its width, sparks spray at its end */
	static AVaelLightningBolt* Spawn(AActor* Caster, const FVector& From, const FVector& To, const FLinearColor& Color, float Duration = 0.25f, float Thickness = 1.0f, bool bSparks = true);

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

protected:

	/** Builds the look */
	virtual void BeginPlay() override;

private:

	/** Throws the bolt into a new jagged shape */
	void Rejag();

	/** Lays the core and halo of one piece of the bolt between two points */
	void PlaceSegment(int32 Index, const FVector& Start, const FVector& End, float Width);

	/** Ends, color, seconds the bolt lasts and its width */
	FVector From = FVector::ZeroVector;
	FVector To = FVector::ZeroVector;
	FLinearColor BoltColor = FLinearColor::White;
	float Duration = 0.25f;
	float Thickness = 1.0f;
	bool bSparks = true;

	/** Seconds since the bolt struck and until it jumps into a new shape */
	float Elapsed = 0.0f;
	float RejagCountdown = 0.0f;

	/** Brightness of each piece for this shape, so single pieces flicker */
	TArray<float> SegmentFlicker;

	/** White-hot line and colored halo of each piece of the bolt, the main line first, then the forks */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Cores;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Halos;

	/** Sparks where the bolt strikes, their speeds in cm/s */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Sparks;

	TArray<FVector> SparkVelocities;

	/** Flash at both ends */
	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> StartLight;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> EndLight;
};
