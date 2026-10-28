// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Creatures/VaelCreature.h"
#include "VaelPfuhlnatter.generated.h"

class UAnimSequence;
class UVaelSequenceAnimInstance;

/** What a Pfuhlnatter is doing */
UENUM(BlueprintType)
enum class EVaelSnakeState : uint8
{
	/** Lies still and flat, fins folded */
	Lurk,
	/** Crawls in waves towards its prey, or home */
	Crawl,
	/** Rears its front up, the fins rise trembling */
	Rear,
	/** Reared, swaying its neck, turning with the prey */
	Threaten,
	/** Strikes with its head */
	Bite,
	/** Lays its front down again to crawl */
	Lower
};

/**
 *  Pfuhlnatter: the sea-serpent-like water snake of the Wurzelforst moor, plays the baked animations of SK_Pfuhlnatter.
 *  Crawls up flat with its fins folded, rears up near its prey (the dahlia crown and fins rise) and strikes from there.
 *  Values in UVaelPfuhlnatterData. Poison spit and puddles, swimming, growth with age, rain roaming, constricting,
 *  shedding, brood and taming come in their own packages.
 */
UCLASS()
class AVaelPfuhlnatter : public AVaelCreature
{
	GENERATED_BODY()

public:

	/** Constructor */
	AVaelPfuhlnatter();

	/** Sets up mesh and animations */
	virtual void BeginPlay() override;

	/** Crawl speed of the animation after the real speed */
	virtual void Tick(float DeltaSeconds) override;

	/** What it is doing */
	UFUNCTION(BlueprintPure, Category="Pfuhlnatter")
	EVaelSnakeState GetSnakeState() const { return State; }

	/** Strikes so far, for tests */
	int32 GetBiteCount() const { return BiteCount; }

protected:

	virtual TSubclassOf<UVaelCreatureData> GetDefaultDataClass() const override;
	virtual void TickBehavior(float DeltaSeconds) override;
	virtual void OnDamageTaken(float Damage, const FGameplayTagContainer& DamageTags) override;
	virtual void Die() override;

private:

	void EnterState(EVaelSnakeState NewState);

	/** Plays an animation if the mesh has one */
	void PlayAnimation(UAnimSequence* Sequence, bool bLoop, float BlendTime, float Rate = 1.0f);

	/** Turns smoothly towards a spot instead of snapping round */
	void TurnTowards(const FVector& Location, float DeltaSeconds);

	UPROPERTY(Transient)
	TObjectPtr<UVaelSequenceAnimInstance> Animator;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> CrawlSequence;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> RearSequence;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> ThreatenSequence;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> BiteSequence;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> LowerSequence;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> DeathSequence;

	TWeakObjectPtr<AActor> Prey;
	EVaelSnakeState State = EVaelSnakeState::Lurk;
	float StateTime = 0.0f;
	float StateDuration = 0.0f;
	float BiteCooldown = 0.0f;
	float NoPreyTime = 0.0f;
	bool bBitten = false;
	int32 BiteCount = 0;
};
