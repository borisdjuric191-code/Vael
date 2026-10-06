// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/VaelCombatStatics.h"
#include "VaelSpellVortex.generated.h"

class UStaticMeshComponent;

/** What a whirlwind does and how it moves */
struct FVaelVortexSettings
{
	/** Element and condition of each hit; its damage is replaced by the collected damage */
	FVaelSpellHit Hit;

	/** What a hit does once the whirlwind has turned into a fire whirl */
	FVaelSpellHit FireHit;

	/** Damage per second to everyone inside */
	float DamagePerSecond = 0.0f;

	/** Extra damage per second as a fire whirl */
	float FireDamageBonus = 0.0f;

	/** Damage collects on each enemy and is dealt once it reaches this much */
	float DamageStep = 6.0f;

	/** Wandering speed in cm/s */
	float Speed = 476.0f;

	/** Radius in cm */
	float Radius = 224.0f;

	/** Seconds until it dies down */
	float Lifetime = 4.2f;

	/** Speed at which enemies inside are dragged towards the middle, in cm/s */
	float PullSpeed = 500.0f;

	/** Seconds between two fires a fire whirl leaves behind, 0 for none */
	float FireInterval = 0.0f;

	/** Radius of each fire in cm */
	float FireRadius = 0.0f;

	/** Seconds each fire burns */
	float FireLifetime = 0.0f;

	/** Damage per second of each fire to the enemies of the caster */
	float FireDamagePerSecond = 0.0f;

	/** Placeholder colors */
	FLinearColor Color = FLinearColor::White;
	FLinearColor FireColor = FLinearColor(1.0f, 0.54f, 0.24f);
};

/**
 *  A whirlwind that wanders slowly in the direction it was cast, like the Wirbelsturm.
 *  It drags enemies towards its middle and grinds them, bounces off walls and dies down after a few seconds.
 *  Passing over a fire turns it into a fire whirl: it burns harder, sets enemies on fire and leaves fires behind.
 */
UCLASS()
class AVaelSpellVortex : public AActor
{
	GENERATED_BODY()

	/** Placeholder look: an upside-down cone */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Mesh;

public:

	/** Constructor */
	AVaelSpellVortex();

	/** Lets a whirlwind of the caster loose on the ground in front of them, wandering in a horizontal direction */
	static AVaelSpellVortex* Launch(APawn* Caster, const FVector& Location, const FVector& Direction, const FVaelVortexSettings& InSettings);

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

	/** True once it has turned into a fire whirl */
	bool IsFiery() const { return bFiery; }

protected:

	/** Starts the lifetime and colors the placeholder */
	virtual void BeginPlay() override;

private:

	/** Wanders on, bounces off walls and follows the ground */
	void Move(float DeltaSeconds);

	/** Turns into a fire whirl over a fire */
	void CatchFire();

	/** Drags in and hurts everyone inside */
	void HitEnemies(float DeltaSeconds);

	/** Leaves fires behind as a fire whirl */
	void LayFires(float DeltaSeconds);

	/** Colors the placeholder */
	void RefreshColor();

	/** What the whirlwind does */
	FVaelVortexSettings Settings;

	/** Horizontal velocity in cm/s */
	FVector Velocity = FVector::ZeroVector;

	/** Damage collected on each enemy that hasn't been dealt yet */
	FVaelDamageCollector CollectedDamage;

	/** True once it has turned into a fire whirl */
	bool bFiery = false;

	/** Seconds until the next fire */
	float NextFireCountdown = 0.0f;
};
