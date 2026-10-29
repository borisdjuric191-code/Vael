// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Creatures/VaelCreature.h"
#include "VaelKelchreisser.generated.h"

class UAnimSequence;
class UMaterialInstanceDynamic;
class UVaelElementComponent;
class UVaelSequenceAnimInstance;

/** What a Kelchreisser is doing */
UENUM(BlueprintType)
enum class EVaelKelchreisserState : uint8
{
	/** Roams its territory at a slow, half upright walk and stands still now and then, listening */
	Territory,
	/** Heard something: stops dead and turns its flower head towards the sound */
	Listen,
	/** Walks to where the sound came from */
	Track,
	/** Throws its head up and screams: it has taken the scent */
	Scream,
	/** Gallops after the one whose scent it has */
	Hunt,
	/** Rakes with the claws of a raised arm in front of it */
	Claw,
	/** Swings an arm round sideways, palm first */
	Slap,
	/** The vines on its back strike out on their own, also over its head */
	VineBite,
	/** Walks back to its territory */
	Return
};

/**
 *  Kelchreisser: elite apex predator of the Wurzelforst, one in each territory round a giant tree (Boris 2026-10-29).
 *  Blind: it hears through the ground, never sees. Steps, spells and fights close by give a player away; standing
 *  still, creeping and rain make it deaf. Once it has a scent it screams and never lets go until the prey is far away
 *  or dead. A grown Nid mutation: the more corrupted its region, the stronger and more violet it is, and killing one
 *  takes a little corruption from the region. Plays the baked animations of SK_Kelchreisser (Tools/Kelchreisser).
 */
UCLASS()
class AVaelKelchreisser : public AVaelCreature
{
	GENERATED_BODY()

public:

	AVaelKelchreisser();

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	/** What it is doing */
	UFUNCTION(BlueprintPure, Category="Kelchreisser")
	EVaelKelchreisserState GetKelchreisserState() const { return State; }

	/** The player whose scent it has, if any */
	AActor* GetScentTarget() const { return Prey.Get(); }

	/** Takes the scent of the nearest player at once (screams first), for tests */
	void TakeScentNow();

	/** Attacks it has started (claw, slap, vine bite) and players its blows hit, for tests */
	int32 GetAttackCount() const { return AttackCount; }
	int32 GetHitCount() const { return HitCount; }

protected:

	virtual TSubclassOf<UVaelCreatureData> GetDefaultDataClass() const override;
	virtual void TickBehavior(float DeltaSeconds) override;
	virtual void OnDamageTaken(float Damage, const FGameplayTagContainer& DamageTags) override;
	virtual void Die() override;
	virtual float GetStatusTextHeight() const override;
	virtual float GetOutgoingDamageMultiplier() const override;
	virtual void OnBodyColorShown(const FLinearColor& Color, bool bHitFlash) override;

private:

	void EnterState(EVaelKelchreisserState NewState);

	void PlayAnimation(UAnimSequence* Sequence, bool bLoop, float BlendTime, float Rate = 1.0f, bool bRestart = false);

	/** Gallop, half upright walk or stand after its real speed, the animation in step with the ground */
	void UpdateGait();

	/** Turns towards a spot at its turn rate */
	void TurnTowards(const FVector& Location, float DeltaSeconds);

	/** The player it hears best right now and how sure it is (0 nothing .. 1 right next to it), with where it heard them */
	AActor* Listen(float& OutLoudness, FVector& OutWhere);

	/** Hooks onto each player's spell casting once, so casting near it is heard (players may join late) */
	void WatchPlayersCasting();

	/** Takes the scent of a player: screams the first time, then hunts */
	void TakeScent(AActor* Target);

	/** Picks an attack for the prey at this distance, or none */
	bool TryAttack(AActor* Target, float Distance);

	/** Hits every player within Range whose direction lies within HalfAngle of its forward (180 for all round) */
	int32 HitPlayersAround(float Range, float HalfAngle, float Damage, float Knockback, int32 MaxTargets, EVaelStatus Status = EVaelStatus::None, float StatusDuration = 0.0f);

	/** Rain on the leaves here, local shower or the region's weather */
	bool IsRainingHere() const;

	/** Its region's corruption, 0..1 */
	float GetCorruptionShare() const;

	/** Shows the violet Nid glow after the corruption (and the white hit flash on top) */
	void RefreshNidGlow();

	UPROPERTY(Transient)
	TObjectPtr<UVaelSequenceAnimInstance> Animator;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> IdleSequence;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> GallopSequence;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> WalkSequence;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> SlapSequence;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> ClawSequence;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> VineSequence;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> ScreamSequence;

	/** Element components it listens to, and when (world seconds) each player last cast a spell */
	TArray<TWeakObjectPtr<UVaelElementComponent>> WatchedCasters;
	TMap<TWeakObjectPtr<AActor>, float> LastCastTime;
	float ListenCountdown = 0.0f;

	TWeakObjectPtr<AActor> Prey;
	EVaelKelchreisserState State = EVaelKelchreisserState::Territory;
	float StateTime = 0.0f;
	float StateDuration = 0.0f;
	bool bStruck = false;

	/** Where it heard the sound it follows, and where it roams to in its territory */
	FVector SoundLocation = FVector::ZeroVector;
	FVector RoamGoal = FVector::ZeroVector;
	float RoamPause = 0.0f;

	/** Seconds the prey has been out of scent range, and until when (world seconds) each attack is ready again */
	float LostScentTime = 0.0f;
	float ClawReadyTime = 0.0f;
	float SlapReadyTime = 0.0f;
	float VineReadyTime = 0.0f;
	float RecoverUntil = 0.0f;

	/** It screams only on the first scent and once more when badly hurt */
	bool bHasScreamed = false;
	bool bScreamedHurt = false;

	/** Strength from its region's corruption, fixed when it appears */
	float CorruptionStrength = 1.0f;
	bool bHitFlash = false;
	int32 AttackCount = 0;
	int32 HitCount = 0;
	float MeshBaseZ = 0.0f;
};
