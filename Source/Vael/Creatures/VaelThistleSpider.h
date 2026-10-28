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

	/** Lurks like a thistle, so the director puts it right on the group's way */
	virtual bool IsAmbusher() const override { return true; }

	/** Sent by the director: rises when the group comes within its aggro range */
	virtual void ProwlTowards(const FVector& GroupCentre, float GroupDistance) override;

	UFUNCTION(BlueprintPure, Category="Creature")
	EVaelThistleState GetThistleState() const { return State; }

	/** Swells and bursts now, wherever it is (console VaelDistelPlatzen); a wet one still won't */
	void ForceBurst() { StartBurst(); }

	/** Legs the factory leg IK found and steps they took, for tests */
	int32 GetLegCount() const;
	int32 GetStepCount() const;

protected:

	virtual TSubclassOf<UVaelCreatureData> GetDefaultDataClass() const override;

	virtual void TickBehavior(float DeltaSeconds) override;

	/** Getting hurt wakes it; fire makes it burst */
	virtual void OnDamageTaken(float Damage, const FGameplayTagContainer& DamageTags) override;

private:

	/** Poses it between a thistle bud in the undergrowth (1) and a standing spider (0) */
	void ApplyLurkPose(float Share);

	void StartBurst();

	/** The down, the swarm, and the empty husk falls */
	void Burst();

	/** Rears up, lunges and strikes; true while a bite is under way */
	bool UpdateBite(float DeltaSeconds, AActor* Target);

	EVaelThistleState State = EVaelThistleState::Lurking;
	float StateTime = 0.0f;
	float HuntTime = 0.0f;
	float BiteCooldown = 0.0f;

	/** Seconds into the current bite, negative while not biting, and whether it already struck */
	float BiteTime = -1.0f;
	bool bBiteStruck = false;
};

/**
 *  Distelsämling: a seed spider of a burst Distelspinne, the same model small. The swarm surrounds the nearest player:
 *  each runs to its own place on a ring around them and leaps from there up to chest height, legs tucked, through the
 *  player to the other side; touching a player in the air hurts (Boris 2026-10-28). Lands, crouches a moment, takes a
 *  new place where it landed and leaps back, so a standing player is jumped through from all sides. Burning ones set
 *  those near them on fire.
 *  Values in UVaelThistleSeedlingData.
 */
UCLASS()
class AVaelThistleSeedling : public AVaelCreature
{
	GENERATED_BODY()

public:

	AVaelThistleSeedling();

	virtual void BeginPlay() override;

	/** Leaps taken and players hit by them, for tests */
	int32 GetLeapCount() const { return LeapCount; }
	int32 GetLeapHitCount() const { return LeapHitCount; }

protected:

	virtual TSubclassOf<UVaelCreatureData> GetDefaultDataClass() const override;

	virtual void TickBehavior(float DeltaSeconds) override;

private:

	/** Launches the leap at the target, through it */
	void StartLeap(const AActor* Target);

	/** Its place on the ring around its target (angle in radians) and how long it has been running for it */
	float PlaceAngle = 0.0f;
	float PlaceTime = 0.0f;
	float LeapCooldown = 0.0f;
	float FireSpreadCountdown = 0.0f;

	/** The current leap: its target, seconds in the air, whether it already hit; then the pause after landing */
	TWeakObjectPtr<AActor> LeapTarget;
	bool bLeaping = false;
	float LeapClock = 0.0f;
	bool bLeapHit = false;
	float LandPauseLeft = 0.0f;
	int32 LeapCount = 0;
	int32 LeapHitCount = 0;
};
