// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Creatures/VaelCreature.h"
#include "VaelBloodMoth.generated.h"

class AVaelCharacter;
class AVaelRainBloom;
class UStaticMeshComponent;

/** What a moth is doing */
UENUM(BlueprintType)
enum class EVaelMothState : uint8
{
	/** Climbs out of its cocoon into the air */
	Rising,
	/** Circles above its player and rains */
	Raining,
	/** Its rain is spent: it flies off and is gone */
	Leaving
};

/**
 *  Nosferalter (idea by Boris): the moth that hatches from the cocoon of a Draculaupe. It follows the player its caterpillar touched,
 *  circles above them, and single drops of blood fall around it for a while. The drops hurt whoever they hit; where they soak
 *  the earth a flower grows now and then, most of them bad. It can be shot down, which ends its rain at once.
 *  The flying body is only drawn high up; the collision stays low so spells still hit.
 *  With the data of the Taufalter its rain heals and only the good flowers grow.
 */
UCLASS()
class AVaelBloodMoth : public AVaelCreature
{
	GENERATED_BODY()

public:

	/** Constructor */
	AVaelBloodMoth();

	/** Initialization */
	virtual void BeginPlay() override;

	/** Flies, flaps, lets its drops fall */
	virtual void Tick(float DeltaSeconds) override;

	/** The player it follows; without one it takes the closest */
	void SetPrey(AVaelCharacter* Player);

	/** What it is doing */
	UFUNCTION(BlueprintPure, Category="Creature")
	EVaelMothState GetMothState() const { return State; }

	/** Drops that have landed and flowers grown so far, for tests */
	int32 GetDropCount() const { return NumDrops; }
	int32 GetBloomCount() const { return NumBlooms; }

	/** Forgets that a harvest flower was already promised in this session, for tests */
	static void ResetFirstHarvest() { bFirstHarvestGrown = false; }

protected:

	virtual TSubclassOf<UVaelCreatureData> GetDefaultDataClass() const override;
	virtual void TickBehavior(float DeltaSeconds) override;
	virtual float GetStatusTextHeight() const override;

private:

	/** A drop on its way down */
	struct FVaelMothDrop
	{
		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;

		/** Seconds since it fell off, negative while the shape is free */
		float Age = -1.0f;
	};

	/** The player it follows right now, the closest one if its own is gone */
	AVaelCharacter* FindPrey() const;

	/** Lets a drop fall somewhere around it */
	void ReleaseDrop();

	/** A drop has reached the ground: it hits whoever stands there, and a flower may grow */
	void LandDrop(const FVector& Location);

	/** Grows a flower at the place if one fits there */
	void TryGrowBloom(const FVector& Location);

	/** Finds the wing bones of the factory mesh */
	void SetupWings();

	/** Lays the body into the air and beats the wings */
	void PoseFactoryMesh(float DeltaSeconds);

	EVaelMothState State = EVaelMothState::Rising;

	/** Seconds in the current state, and how long this rain lasts */
	float StateTime = 0.0f;
	float RainLength = 45.0f;

	TWeakObjectPtr<AVaelCharacter> Prey;

	/** Angle on its circle in radians */
	float CircleAngle = 0.0f;

	/** Seconds until the next drop */
	float DropCountdown = 0.0f;

	/** Height of the body above the capsule as shown */
	float BodyHeight = 0.0f;

	TArray<FVaelMothDrop> Drops;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> DropShapes;

	int32 NumDrops = 0;
	int32 NumBlooms = 0;

	/** Flowers of this rain that still stand, to keep new ones apart */
	TArray<TWeakObjectPtr<AVaelRainBloom>> Blooms;

	/** Inner and outer bone of each wing and where they rest in component space */
	FName WingBones[2][2];
	FTransform WingRest[2][2];
	bool bHasWings = false;

	/** Height of the middle of the mesh above its feet, in cm: it leans around this point */
	float MeshMiddle = 0.0f;

	/** Progress of the wing beat in beats, the lean forward in degrees and the size as shown */
	float FlapPhase = 0.0f;
	float LeanShown = 0.0f;
	float SizeShown = 0.2f;

	/** Seconds since it died, for the fall */
	float DeadTime = 0.0f;

	/** True once the first rain of this session has let its harvest flower grow: the first find is certain */
	static bool bFirstHarvestGrown;
};
