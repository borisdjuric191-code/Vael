// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelSpellGust.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 *  Look of a gust of wind along a cone, like the Windstoß. Only shows, the cone itself hits in UVaelFormulaAbility.
 *  A ring of pressure bursts at the hand, a curved wall of air rolls out to the end of the cone,
 *  streaks of wind race past it and dust is kicked up where the wall passes. Built from glowing spheres until a Niagara effect exists.
 */
UCLASS()
class AVaelSpellGust : public AActor
{
	GENERATED_BODY()

public:

	/** Constructor */
	AVaelSpellGust();

	/** Spawns a gust at the hand blowing in a horizontal direction */
	static AVaelSpellGust* Spawn(AActor* Caster, const FVector& Location, const FVector& Direction, float Range, float HalfAngle, const FLinearColor& Color);

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

protected:

	/** Builds the look */
	virtual void BeginPlay() override;

private:

	/** Adds a hidden sphere to the look, in a glowing material with its color, glow and rim */
	UStaticMeshComponent* AddShape(UMaterialInterface* Material, const FLinearColor& ShapeColor, float Glow, float Rim);

	/** The parts of the gust, each over its own share of the time */
	void AnimateHandRing();
	void AnimateWave();
	void AnimateStreaks();
	void AnimateDust();

	/** Distance of the wall of air from the hand at a time in seconds, and the time it reaches a distance */
	float GetWaveDistance(float Time) const;
	float GetWaveArrival(float Distance) const;

	/** Sets the glow of a shape that has a glowing material */
	static void SetGlow(UStaticMeshComponent* Shape, float Glow);

	/** Reach in cm, half opening angle in degrees and color of the gust */
	float Range = 450.0f;
	float HalfAngle = 45.0f;
	FLinearColor GustColor = FLinearColor::White;

	/** Height of the ground below the hand, relative to the gust */
	float GroundHeight = -90.0f;

	/** Seconds since the gust started */
	float Elapsed = 0.0f;

	/** Engine sphere and plain material, used where the glowing materials don't exist */
	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> PlainMaterial;

	/** Flat ring of pressure at the hand */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> HandRing;

	/** Pieces of the wall of air side by side along its arc: the bright front, then the faint row behind it */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> WaveFront;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> WaveBack;

	/** Streaks of wind; each has a direction within the cone as yaw, a height, a start, a length */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Streaks;

	TArray<float> StreakYaws;
	TArray<float> StreakHeights;
	TArray<float> StreakStarts;
	TArray<float> StreakLengths;

	/** Grains of dust; each waits on the ground until the wall of air reaches it, then flies off */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Dust;

	TArray<float> DustStarts;
	TArray<FVector> DustOrigins;
	TArray<float> DustSizes;
	TArray<FVector> DustVelocities;

	/** Light riding on the wall of air */
	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> WaveLight;
};
