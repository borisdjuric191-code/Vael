// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Magic/VaelElementTypes.h"
#include "VaelGroundShock.generated.h"

class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 *  Look of a burst around the caster that runs out over the ground, like the Erdbeben or the Markstoß. Only shows, the spell hits on its own.
 *  Built from glowing spheres, the rock of the earth orb and bursts of debris until a Niagara effect exists.
 */
UCLASS()
class AVaelGroundShock : public AActor
{
	GENERATED_BODY()

public:

	/** Constructor */
	AVaelGroundShock();

	/** Spawns the look at the caster's feet, reaching as far as the burst */
	static AVaelGroundShock* Spawn(AActor* Caster, const FVector& Feet, float Radius, EVaelNovaLook Look, const FLinearColor& Color);

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

protected:

	/** Builds the look */
	virtual void BeginPlay() override;

private:

	/** The two looks */
	void BuildQuake();
	void BuildMarkPulse();
	void AnimateQuake();
	void AnimateMarkPulse();

	/** Distance of the front of the shock from the center at a time in seconds, and the time it reaches a distance */
	float GetFrontDistance(float Time) const;
	float GetFrontArrival(float Distance) const;

	EVaelNovaLook Look = EVaelNovaLook::Quake;
	float Radius = 600.0f;
	FLinearColor ShockColor = FLinearColor::White;

	/** Seconds since the burst */
	float Elapsed = 0.0f;

	/** Seconds the front needs to reach the edge, and the whole look lasts */
	float FrontTime = 0.4f;
	float LookTime = 1.0f;

	/** Flat ring running over the ground */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> GroundRing;

	/** Dome of pressure and the black hole at its heart, for the Mark */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Dome;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Hole;

	/** Rocks breaking out of the ground, where each stands, how high it gets, when the front reaches it and whether it has burst yet */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Rocks;

	TArray<FVector> RockPlaces;
	TArray<float> RockHeights;
	TArray<float> RockArrivals;
	TArray<bool> RockBurst;

	/** Flash of the burst */
	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> Flash;
};
