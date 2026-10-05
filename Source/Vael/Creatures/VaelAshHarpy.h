// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Creatures/VaelCreature.h"
#include "VaelAshHarpy.generated.h"

/** What an ash harpy is doing */
UENUM(BlueprintType)
enum class EVaelHarpyState : uint8
{
	/** Circles its home */
	Idle,
	/** Circles its target */
	Circling,
	/** Hangs in the air and aims, right before a dive */
	Warning,
	/** Shoots down at its target */
	Diving,
	/** Climbs back up after a dive */
	Recovering
};

/**
 *  Aschharpyie: circles the players in the air and dives at them after a short warning.
 *  Wind throws it around. The flying body is only drawn high up; the collision stays low so spells still hit.
 *  With the elder data it calls more harpies with a screech.
 */
UCLASS()
class AVaelAshHarpy : public AVaelCreature
{
	GENERATED_BODY()

public:

	/** Constructor */
	AVaelAshHarpy();

	/** Initialization */
	virtual void BeginPlay() override;

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

	/** Pushes the harpy through the air instead of launching it */
	virtual void ApplyKnockback(const FVector& Direction, float Speed) override;

	/** Starts circling the players right away, for harpies called by a screech */
	void StartCircling();

	/** What the harpy is doing right now */
	UFUNCTION(BlueprintPure, Category="Creature")
	EVaelHarpyState GetHarpyState() const { return State; }

protected:

	virtual TSubclassOf<UVaelCreatureData> GetDefaultDataClass() const override;
	virtual void TickBehavior(float DeltaSeconds) override;
	virtual void OnDamageTaken(float Damage, const FGameplayTagContainer& DamageTags) override;
	virtual float GetStatusTextHeight() const override;

private:

	/** Switches to a new state */
	void EnterState(EVaelHarpyState NewState);

	/** Calls more harpies if there are few near */
	void Screech();

	/** Current state */
	EVaelHarpyState State = EVaelHarpyState::Idle;

	/** Seconds in the current state */
	float StateTime = 0.0f;

	/** Seconds until the next dive */
	float DiveCooldown = 0.0f;

	/** Seconds until the next screech */
	float ScreechCooldown = 0.0f;

	/** Angle on the circle in radians */
	float CircleAngle = 0.0f;

	/** Player the harpy is about to dive at */
	TWeakObjectPtr<AActor> DiveTarget;

	/** Point the harpy aims its dive at */
	FVector DivePoint = FVector::ZeroVector;

	/** Direction of the current dive */
	FVector DiveDirection = FVector::ForwardVector;

	/** Players the current dive has already touched */
	TSet<TWeakObjectPtr<AActor>> DiveHits;

	/** True if a player saw the current dive from close by */
	bool bDiveWitnessed = false;

	/** World time of the last hit by air, which throws the harpy farther */
	float LastAirHitTime = -1.0f;

	/** Current height of the body above the capsule */
	float BodyHeight = 0.0f;
};
