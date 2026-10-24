// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/VaelCombatStatics.h"
#include "Magic/VaelWaterMesh.h"
#include "VaelSapJet.generated.h"

class UProceduralMeshComponent;
class UVaelElementSource;
class UVaelPlantArmData;

/**
 *  The water jet of the Taublatt arm while its button is held (Boris 2026-10-24): turquoise plant water pressed out of the
 *  barrel of nested funnels. Not the column of the mage's Geysir: a narrow twisted jet that widens, foams in a spiral and
 *  fans into spray where it ends. It hits everyone along it a little, leaves them wet and keeps pushing them away; walls stop
 *  it; plants it touches are watered, and where it ends the water gathers in a puddle. It heals no one.
 *  The arm moves it to its barrel every frame; Stop lets it tear off at the barrel and fly out.
 */
UCLASS()
class AVaelSapJet : public AActor
{
	GENERATED_BODY()

	/** The jet, rebuilt every frame along X of the actor */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UProceduralMeshComponent> Tube;

	/** Its spray, in world space */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UProceduralMeshComponent> Spray;

public:

	AVaelSapJet();

	/** Starts a jet of the plant's water for the Floromant */
	static AVaelSapJet* StartJet(APawn* Gardener, const UVaelPlantArmData* Plant, const FVector& Muzzle, const FVector& Direction);

	/** Moves the jet to the barrel; Pressure 0 to 1 swells it when a pump wave arrives */
	void Aim(const FVector& Muzzle, const FVector& Direction, float Pressure);

	/** Tears the jet off the barrel: its tail flies out and it is gone */
	void Stop();

	bool IsStopping() const { return bStopping; }

	/** How far it reaches now in cm */
	float GetLength() const { return Length; }

	virtual void Tick(float DeltaSeconds) override;

private:

	/** Hits, pushes, wets and waters everything along the jet */
	void UpdateHits(float DeltaSeconds);

	/** Sheds spray along the jet and fans it where it ends */
	void UpdateSpray(float DeltaSeconds);

	/** Builds the tube of this frame */
	void BuildTube();

	UPROPERTY(Transient)
	TObjectPtr<const UVaelPlantArmData> Plant;

	UPROPERTY(Transient)
	TObjectPtr<UVaelElementSource> Source;

	/** Where the jet ends now and where it could reach, in cm; it shoots out quickly at the start */
	float Length = 0.0f;
	float Reach = 0.0f;

	/** After Stop: how far the torn tail has flown off the barrel, in cm */
	float TornOff = 0.0f;
	bool bStopping = false;

	float Pressure = 0.0f;
	float Clock = 0.0f;
	float Seed = 0.0f;

	/** Where it ends, on a wall or in the air */
	FVector EndLocation = FVector::ZeroVector;
	bool bEndsOnWall = false;

	FVaelDamageCollector CollectedDamage;
	float PushCountdown = 0.0f;
	float PuddleCountdown = 0.0f;

	FVaelWaveSection TubeSection;
	FVaelWaveSection SpraySection;
	FVaelWaterDrops Drops;
	bool bSprayCreated = false;
};
