// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelAvalanche.generated.h"

class UStaticMeshComponent;
class UVaelCloudCardsComponent;
class UVaelFlakesComponent;

/**
 *  The look of the Lawine (Boris 2026-11-02): a wall of powder snow rolls out over the cone, widening as it goes, with
 *  lumps of ice and rock tumbling in it, each trailing snow; flakes are thrown up at its front and come down after. Where
 *  it passed it leaves drifts of snow (AVaelSnowDrift) that slow the enemies and melt in fire. The enemies it caught are
 *  crusted with ice for as long as they stay frozen. The formula does the damage, the push and the freezing.
 */
UCLASS()
class AVaelAvalanche : public AActor
{
	GENERATED_BODY()

public:

	AVaelAvalanche();

	/** Rolls an avalanche from the caster's feet along the cone; Caught are the ones it hit, frozen for FrozenSeconds */
	static AVaelAvalanche* Spawn(APawn* Caster, const FVector& Direction, float InRange, float InHalfAngle, const TArray<AActor*>& Caught, float FrozenSeconds);

	virtual void Tick(float DeltaSeconds) override;

private:

	/** A lump of ice or rock rolling in the snow */
	struct FLump
	{
		TObjectPtr<UStaticMeshComponent> Mesh;
		float Angle = 0.0f;
		float Lag = 0.0f;
		float Size = 20.0f;
		float Bounce = 0.0f;
		FQuat Start = FQuat::Identity;
		float NextFlake = 0.0f;
		float GroundZ = 0.0f;
	};

	/** The crust of ice on one enemy it caught */
	struct FCrust
	{
		TWeakObjectPtr<AActor> Target;
		TArray<TObjectPtr<UStaticMeshComponent>> Pieces;
		TArray<float> Sizes;
		float Appear = 0.0f;
		float Melt = 0.0f;
	};

	void Build(const TArray<AActor*>& Caught, float FrozenSeconds);

	/** A lump mesh of the earth orb's rock in a colour */
	UStaticMeshComponent* MakeLump(const FLinearColor& Color, float Glow);

	/** Ground height below a point, the avalanche's own where none is found */
	float GroundAt(const FVector& Location) const;

	/** Direction of a line through the cone at this angle from its middle, in degrees */
	FVector Along(float Angle) const;

	/** How far the front has come, in cm from the caster */
	float FrontDistance() const;

	void UpdateCloud(float Front);
	void UpdateLumps(float Front, float DeltaSeconds);
	void UpdateCrusts();
	void ThrowSnow(float Front, float DeltaSeconds);
	void LayDrifts(float Front);

	UPROPERTY(Transient)
	TObjectPtr<UVaelCloudCardsComponent> Cloud;

	UPROPERTY(Transient)
	TObjectPtr<UVaelFlakesComponent> Flakes;

	/** Per cloud card: angle across the cone, how far out (trail cards) or 0 (front cards), its row, size and phase */
	TArray<float> CardAngles;
	TArray<float> CardDistances;
	TArray<int32> CardRows;
	TArray<float> CardSizes;
	TArray<float> CardPhases;

	TArray<FLump> Lumps;
	TArray<FCrust> Crusts;

	/** Drifts still to lay: where across and how far out */
	TArray<FVector2D> DriftSpots;

	FVector Forward = FVector::ForwardVector;
	float Range = 560.0f;
	float HalfAngle = 40.0f;
	float Age = 0.0f;
	float Life = 3.0f;
	float SnowCredit = 0.0f;

	/** The rock mesh of the earth orb and its size across in cm */
	UPROPERTY(Transient)
	TObjectPtr<class UStaticMesh> RockMesh;
	float RockSize = 100.0f;
};
