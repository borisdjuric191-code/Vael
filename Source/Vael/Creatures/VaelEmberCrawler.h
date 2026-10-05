// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Creatures/VaelCreature.h"
#include "VaelEmberCrawler.generated.h"

/** What an ember crawler is doing */
UENUM(BlueprintType)
enum class EVaelCrawlerState : uint8
{
	/** Moves under the ash, hard to hurt except with earth */
	Burrowed,
	/** Breaks out of the ground next to a player */
	Surfacing,
	/** Glows and explodes at the end */
	Fuse,
	/** Water has put the fuse out, it shakes off the steam */
	Doused,
	/** Fights on with its claws */
	Crawling
};

/**
 *  Glutkriecher: burrows towards the players, surfaces next to them and explodes into a fire.
 *  Water or ice during the fuse puts it out; it then fights on with its claws.
 *  Watching an explosion from close by teaches the players a formula.
 */
UCLASS()
class AVaelEmberCrawler : public AVaelCreature
{
	GENERATED_BODY()

public:

	/** Constructor */
	AVaelEmberCrawler();

	/** Initialization */
	virtual void BeginPlay() override;

	/** Burrowed crawlers take less damage, except from earth */
	virtual float GetIncomingDamageMultiplier(const FGameplayTagContainer& DamageTags) const override;

	/** What the crawler is doing right now */
	UFUNCTION(BlueprintPure, Category="Creature")
	EVaelCrawlerState GetCrawlerState() const { return State; }

protected:

	virtual TSubclassOf<UVaelCreatureData> GetDefaultDataClass() const override;
	virtual void TickBehavior(float DeltaSeconds) override;
	virtual void OnDamageTaken(float Damage, const FGameplayTagContainer& DamageTags) override;

private:

	/** Switches to a new state and updates the look */
	void EnterState(EVaelCrawlerState NewState);

	/** Water or ice puts out a burning fuse: called on a hit of water and when the crawler freezes */
	void OnDoused(const FGameplayTag Tag, int32 NewCount);

	/** Hurts everything around, leaves a fire and dies */
	void Explode();

	/** Current state */
	EVaelCrawlerState State = EVaelCrawlerState::Burrowed;

	/** Seconds in the current state */
	float StateTime = 0.0f;

	/** Seconds until the next claw attack */
	float ClawCooldown = 0.0f;

	/** Body scale outside of the ground */
	FVector SurfacedBodyScale = FVector::OneVector;
};
