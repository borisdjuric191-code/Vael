// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/VaelCombatStatics.h"
#include "VaelSpellOrbit.generated.h"

class UStaticMeshComponent;

/** How a circle of stones is set up */
struct FVaelOrbitSettings
{
	int32 Count = 4;
	float Radius = 224.0f;
	float Speed = 300.0f;
	float StoneSize = 45.0f;
	float Lifetime = 6.0f;
};

/**
 *  Stones circling a caster for some seconds, like the Steinkreis. They follow the caster, hit every enemy they touch
 *  (each enemy at most every half second) and stop the projectiles of enemies. A caster has one circle at a time.
 *  Look (Boris 2026-11-02, a shield for the mage): big flat slabs of the earth orb's rock standing on edge, facing outwards,
 *  whirling so closely round the caster that hardly a projectile gets through; brown spheres while the pack is missing.
 */
UCLASS()
class AVaelSpellOrbit : public AActor
{
	GENERATED_BODY()

public:

	/** Constructor */
	AVaelSpellOrbit();

	/** Starts a circle of stones around the caster, replacing one they already have */
	static AVaelSpellOrbit* Start(APawn* Caster, const FVaelSpellHit& InHit, const FVaelOrbitSettings& InSettings);

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

	/** Throws the stones off when the time is up */
	virtual void LifeSpanExpired() override;

protected:

	/** Builds the stones */
	virtual void BeginPlay() override;

private:

	/** Location of a stone in the world */
	FVector GetStoneLocation(int32 StoneIndex) const;

	/** Lets the stones break apart and goes */
	void Shatter();

	/** Breaks the projectiles of enemies that reach a slab */
	void BlockProjectiles(float DeltaSeconds);

	/** Thickness, width and height of a slab in cm */
	FVector SlabSize = FVector(22.0f, 100.0f, 95.0f);

	/** Projectiles broken on the slabs so far */
	int32 NumBlocked = 0;

public:

	/** Projectiles broken on the slabs so far */
	int32 GetBlockedCount() const { return NumBlocked; }

	FVaelSpellHit Hit;
	FVaelOrbitSettings Settings;

	/** Seconds since the circle started */
	float Age = 0.0f;

	/** Stones and how each tumbles */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Stones;

	TArray<FRotator> Tumbles;

	/** Scale of a stone at full size */
	FVector StoneScale = FVector::OneVector;

	/** When each enemy was last hit */
	TMap<TWeakObjectPtr<AActor>, float> LastHitTimes;
};
