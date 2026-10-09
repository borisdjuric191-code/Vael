// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VaelAutoAimComponent.generated.h"

class APawn;

/**
 *  Soft lock for aiming with a gamepad, like in Diablo or Path of Exile.
 *  An enemy that comes close is locked on and aimed at, and stays the target while the player moves.
 *  Flicking the right stick towards another enemy jumps to it; holding the stick away from the target for a moment
 *  lets go of it and aims freely, until the stick is released again.
 */
UCLASS(ClassGroup=(Vael), meta = (BlueprintSpawnableComponent))
class UVaelAutoAimComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	/** An enemy closer than this is locked on by itself, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Auto Aim", meta = (ClampMin = 0))
	float AcquireRange = 900.0f;

	/** A target further away than this is let go, in cm; also the reach of the stick when switching */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Auto Aim", meta = (ClampMin = 0))
	float KeepRange = 1500.0f;

	/** The stick picks enemies within this angle of where it points, in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Auto Aim", meta = (ClampMin = 1, ClampMax = 90))
	float SwitchConeDegrees = 30.0f;

	/** Seconds the stick has to point at another enemy to jump to it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Auto Aim", meta = (ClampMin = 0))
	float SwitchHoldSeconds = 0.12f;

	/** The stick counts as pointing away from the target beyond this angle, in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Auto Aim", meta = (ClampMin = 1, ClampMax = 180))
	float BreakAngleDegrees = 50.0f;

	/** Seconds the stick has to point away from the target, at no other enemy, to let go of it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Auto Aim", meta = (ClampMin = 0))
	float BreakHoldSeconds = 0.45f;

	/** Seconds after letting go of a target, and releasing the stick, before an enemy is locked on by itself again */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Auto Aim", meta = (ClampMin = 0))
	float ReacquireDelay = 0.6f;

	/** Constructor */
	UVaelAutoAimComponent();

	/**
	 *  Updates the lock for this frame.
	 *  StickDirection is where the right stick points in the world, zero while it rests. Disabled: no target, as with the mouse.
	 */
	void UpdateTarget(float DeltaSeconds, const APawn* Player, const FVector& StickDirection, bool bEnabled);

	/** The locked enemy, null if there is none */
	AActor* GetTarget() const { return Target.Get(); }

private:

	/** True if the actor is a living enemy of the player within the distance */
	static bool IsValidTarget(const AActor* Candidate, const APawn* Player, float MaxDistance);

	/** The enemy the stick points at best, null if none lies within the cone */
	AActor* FindInDirection(const APawn* Player, const FVector& Direction) const;

	/** The closest enemy within the acquire range, null if there is none */
	AActor* FindClosest(const APawn* Player) const;

	/** The locked enemy */
	TWeakObjectPtr<AActor> Target;

	/** Seconds the stick has pointed at another enemy, or away from the target */
	float SwitchTime = 0.0f;
	float BreakTime = 0.0f;

	/** Seconds until enemies are locked on by themselves again */
	float SuppressTime = 0.0f;

	/** Enemy the stick has been pointing at */
	TWeakObjectPtr<AActor> SwitchCandidate;
};
