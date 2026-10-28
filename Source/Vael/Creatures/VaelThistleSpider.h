// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Creatures/VaelCreature.h"
#include "VaelThistleSpider.generated.h"

/** What a Distelspinne is doing */
UENUM(BlueprintType)
enum class EVaelThistleState : uint8
{
	/** Sunk into the undergrowth, looks like a thistle */
	Lurking,
	/** Rises out of the undergrowth */
	Rising,
	/** Walks at its target and bites */
	Hunting,
	/** Swells before it bursts into seed spiders */
	Bursting
};

/**
 *  Distelspinne (Wurzelforst): an eight-legged plant spider, its body an armored thistle head. Lurks sunk into the
 *  undergrowth (hidden from aiming) until a player comes close or hurts it, rises, hunts and bites. After a few seconds
 *  of fighting, or when badly hurt, it swells and bursts: a cloud of seed down slows the players around it and a swarm
 *  of Distelsämlinge runs out. Fire makes it burst at once and the swarm comes out burning; a wet thistle can't burst.
 *  Values in UVaelThistleSpiderData. Looks: Tripo mesh with 8 mirrored legs, moved by the factory leg IK.
 */
UCLASS()
class AVaelThistleSpider : public AVaelCreature
{
	GENERATED_BODY()

public:

	AVaelThistleSpider();

	virtual void BeginPlay() override;

	/** While it lurks the aim help passes over it */
	virtual bool IsHiddenFromAim() const override;

	UFUNCTION(BlueprintPure, Category="Creature")
	EVaelThistleState GetThistleState() const { return State; }

	/** Legs the factory leg IK found and steps they took, for tests */
	int32 GetLegCount() const;
	int32 GetStepCount() const;

protected:

	virtual TSubclassOf<UVaelCreatureData> GetDefaultDataClass() const override;

	virtual void TickBehavior(float DeltaSeconds) override;

	/** Getting hurt wakes it; fire makes it burst */
	virtual void OnDamageTaken(float Damage, const FGameplayTagContainer& DamageTags) override;

private:

	void StartBurst();

	/** The down, the swarm, and the empty husk falls */
	void Burst();

	EVaelThistleState State = EVaelThistleState::Lurking;
	float StateTime = 0.0f;
	float HuntTime = 0.0f;
	float BiteCooldown = 0.0f;
};

/**
 *  Distelsämling: a seed spider of a burst Distelspinne, the same model small. Runs at the nearest player from its own
 *  side of the swarm and nips; burning ones set those near them on fire. Values in UVaelThistleSeedlingData.
 */
UCLASS()
class AVaelThistleSeedling : public AVaelCreature
{
	GENERATED_BODY()

public:

	AVaelThistleSeedling();

	virtual void BeginPlay() override;

protected:

	virtual TSubclassOf<UVaelCreatureData> GetDefaultDataClass() const override;

	virtual void TickBehavior(float DeltaSeconds) override;

private:

	/** Where around its target it runs to, so the swarm surrounds instead of queueing */
	FVector SwarmOffset = FVector::ZeroVector;
	float BiteCooldown = 0.0f;
	float FireSpreadCountdown = 0.0f;
};
