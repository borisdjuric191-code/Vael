// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Creatures/VaelCreature.h"
#include "VaelRankenluchs.generated.h"

class UAnimSequence;
class UMaterialInstanceDynamic;
class UVaelSequenceAnimInstance;

/** What a Rankenluchs is doing */
UENUM(BlueprintType)
enum class EVaelLynxState : uint8
{
	/** Lies flat and sunk in the undergrowth, only back and crest showing; can't be aimed at */
	Ambush,
	/** Rises out of the ambush */
	Waking,
	/** Runs in on its prey from its side of the pack */
	Hunt,
	/** Crouches, leaps and bites */
	Pounce,
	/** Backs off and circles before the next pounce */
	Retreat,
	/** Walks back to where it lay */
	Return,
	/** Lies down again */
	Settle,
	/** The last of its pack, badly hurt: runs off to hide */
	Flee
};

/**
 *  Rankenluchs: pack hunter of the Wurzelforst, plays the baked animations of SK_Rankenluchs.
 *  Values in UVaelRankenluchsData; the pack is every lynx within PackRange of the one that wakes.
 */
UCLASS()
class AVaelRankenluchs : public AVaelCreature
{
	GENERATED_BODY()

public:

	/** Constructor */
	AVaelRankenluchs();

	/** Sets up mesh, animations and materials */
	virtual void BeginPlay() override;

	/** Light or shadow form, and the sinking into the undergrowth */
	virtual void Tick(float DeltaSeconds) override;

	/** Not aimed at while it lies in ambush */
	virtual bool IsHiddenFromAim() const override;

	/** What it is doing */
	UFUNCTION(BlueprintPure, Category="Rankenluchs")
	EVaelLynxState GetLynxState() const { return State; }

	/** Wakes it after the delay (seconds), like the call of its pack; also for tests */
	void WakeUp(float Delay = 0.0f);

	/** True while the last of a pack hides hurt: the Tamer may bind it now (capture condition "Letzter des Rudels") */
	UFUNCTION(BlueprintPure, Category="Rankenluchs")
	bool IsCapturable() const;

	/** 0 light form .. 1 shadow form, as shown right now */
	float GetShadowForm() const { return ShadowForm; }

protected:

	virtual TSubclassOf<UVaelCreatureData> GetDefaultDataClass() const override;
	virtual void TickBehavior(float DeltaSeconds) override;
	virtual void OnDamageTaken(float Damage, const FGameplayTagContainer& DamageTags) override;
	virtual void Die() override;
	virtual TArray<FVaelLootEntry> GetLoot() const override;

private:

	void EnterState(EVaelLynxState NewState);

	/** Plays the animation of the state; walking and running follow the speed */
	void PlayAnimation(UAnimSequence* Sequence, bool bLoop, float BlendTime, float Rate = 1.0f);

	/** Walks or runs and plays the matching gait */
	void Move(const FVector& Direction, float Speed);

	/** Calls the pack: every lynx in ambush within PackRange wakes a moment later */
	void AlarmPack();

	/** True if no other lynx of its pack is alive */
	bool IsLastOfPack() const;

	/** Its place in the pack around the prey: -1, 0, 1, ... (0 straight in, others from the sides) */
	int32 GetPackSide() const;

	UPROPERTY(Transient)
	TObjectPtr<UVaelSequenceAnimInstance> Animator;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> IdleSequence;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> CamouflageSequence;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> WakeSequence;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> PounceSequence;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> WalkSequence;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> RunSequence;

	TWeakObjectPtr<AActor> Prey;
	EVaelLynxState State = EVaelLynxState::Ambush;
	float StateTime = 0.0f;
	float StateDuration = 0.0f;
	float WakeCountdown = -1.0f;
	float PounceCooldown = 0.0f;
	float NoPreyTime = 0.0f;
	bool bLeapHit = false;
	bool bBitten = false;
	FVector LeapDirection = FVector::ForwardVector;
	float RetreatSide = 1.0f;

	/** How far the mesh is sunk into the ground right now, 0 up .. 1 AmbushSink */
	float Sunk = 1.0f;
	float MeshBaseZ = 0.0f;

	/** Light (0) to shadow (1) form, shown and aimed for; checked against the region now and then */
	float ShadowForm = 0.0f;
	float ShadowTarget = 0.0f;
	float FormCheckCountdown = 0.0f;
	bool bFormShown = false;

	/** Where it flees to, and until when (world seconds) it can be bound; it flees only once */
	FVector FleeGoal = FVector::ZeroVector;
	float CaptureEndTime = -1.0f;
	bool bHasFled = false;
};
