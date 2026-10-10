// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Creatures/VaelCreature.h"
#include "VaelEmberQueen.generated.h"

/** What the ember queen is doing */
UENUM(BlueprintType)
enum class EVaelQueenState : uint8
{
	/** Sleeps until a player comes close or hurts her */
	Sleeping,
	/** Rises, the fight starts after a moment */
	Waking,
	/** Fights */
	Fighting
};

/**
 *  Glutkoenigin, the boss of the Aschenmark and mother of all ember crawlers.
 *  Bites players next to her, spits volleys of fire and calls her brood out of the ash.
 *  Below a share of her health she also sends out rings of flame and charges.
 *  Her health grows with the number of players. Water slows and weakens her, fire hardly hurts her.
 */
UCLASS()
class AVaelEmberQueen : public AVaelCreature
{
	GENERATED_BODY()

public:

	/** Constructor */
	AVaelEmberQueen();

	/** Initialization */
	virtual void BeginPlay() override;

	/** What she is doing right now */
	UFUNCTION(BlueprintPure, Category="Creature")
	EVaelQueenState GetQueenState() const { return State; }

	/** True once her health has fallen below the share of the second phase */
	UFUNCTION(BlueprintPure, Category="Creature")
	bool IsInSecondPhase() const;

	virtual bool IsBossFightActive() const override { return State != EVaelQueenState::Sleeping && !IsDead(); }

protected:

	virtual TSubclassOf<UVaelCreatureData> GetDefaultDataClass() const override;
	virtual void TickBehavior(float DeltaSeconds) override;
	virtual void OnHealthChanged(float OldValue, float NewValue) override;
	virtual bool AlwaysShowStatusText() const override { return State != EVaelQueenState::Sleeping; }
	virtual void Die() override;
	virtual void OnBodyColorShown(const FLinearColor& Color, bool bHitFlash) override;

private:

	/** Wakes her up and scales her health to the number of players */
	void WakeUp();

	/** Spits a volley of fire at the target */
	void Spit(const AActor* Target, bool bSecondPhase);

	/** Calls crawlers out of the ash around her if there are few near */
	void CallBrood();

	/** Updates a running charge */
	void TickCharge(float DeltaSeconds);

	/** Current state */
	EVaelQueenState State = EVaelQueenState::Sleeping;

	/** Seconds in the current state */
	float StateTime = 0.0f;

	float SpitCooldown = 0.0f;
	float BiteCooldown = 0.0f;
	float BroodCooldown = 0.0f;
	float RingCooldown = 0.0f;
	float ChargeCooldown = 0.0f;

	/** True while a charge is running, including its windup */
	bool bCharging = false;

	/** Seconds since the charge started */
	float ChargeTime = 0.0f;

	/** Direction of the current charge */
	FVector ChargeDirection = FVector::ForwardVector;

	/** Players the current charge has already hit */
	TSet<TWeakObjectPtr<AActor>> ChargeHits;
};
