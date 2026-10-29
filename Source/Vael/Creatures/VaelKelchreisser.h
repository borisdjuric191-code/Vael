// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Creatures/VaelCreature.h"
#include "VaelKelchreisser.generated.h"

class UAnimSequence;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UVaelElementComponent;
class UVaelSequenceAnimInstance;

/** What a Kelchreisser's body is doing (its vines strike on their own, see FVaelVineStrike) */
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
	/** Rears and rakes with the claws of a raised arm */
	Claw,
	/** A wide circling swing with the open clawed hand */
	Swipe,
	/** Lunges and bites with the flower head */
	Bite,
	/** Leaps at prey that is still some way off */
	Leap,
	/** Walks back to its territory */
	Return
};

/** One vine striking out at a player on its own: draws back, shoots out (up to 10 m), bites, pulls back in */
struct FVaelVineStrike
{
	/** The vine's bones (root to bud) and its bud's jaws */
	TArray<FName> Bones;
	FName UpperJaw;
	FName LowerJaw;

	/** Seconds into the strike, below 0 while it rests; world seconds when it may strike again */
	float Time = -1.0f;
	float ReadyTime = 0.0f;

	TWeakObjectPtr<AActor> Victim;
	FVector CoilPoint = FVector::ZeroVector;
	FVector BitePoint = FVector::ZeroVector;
	bool bBitten = false;
};

/** A drop of slobber: hangs from a lip on a thread, tears off, falls and splashes on the ground */
struct FVaelDrool
{
	/** Seconds since it began to run out of the mouth, below 0 while unused */
	float Age = -1.0f;
	float HangTime = 1.0f;
	float Size = 6.0f;
	/** Which lip it hangs from (0, 1), and where it is */
	int32 Lip = 0;
	FVector Location = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	float GroundZ = 0.0f;
	/** Seconds since it hit the ground, below 0 while in the air */
	float Landed = -1.0f;
	bool bFree = false;
};

/**
 *  Kelchreisser: elite apex predator of the Wurzelforst, one at every giant tree (Boris 2026-10-29). Blind: it hears
 *  through the ground, never sees. Steps, spells and fights close by give a player away; creeping and rain make it
 *  deaf. Once it has a scent it screams and never lets go until the prey is far away or dead: it gallops, leaps,
 *  bites, rakes and swipes, and its vines strike out on their own at everyone round it, several metres far.
 *  A grown Nid mutation: the more corrupted its region, the stronger and more violet it is, and killing one takes a
 *  little corruption from the region. Plays the baked animations of SK_Kelchreisser (Tools/Kelchreisser).
 */
UCLASS()
class AVaelKelchreisser : public AVaelCreature
{
	GENERATED_BODY()

public:

	AVaelKelchreisser();

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	/** What its body is doing */
	UFUNCTION(BlueprintPure, Category="Kelchreisser")
	EVaelKelchreisserState GetKelchreisserState() const { return State; }

	/** The player whose scent it has, if any */
	AActor* GetScentTarget() const { return Prey.Get(); }

	/** Takes the scent of the nearest player at once (screams first), for tests */
	void TakeScentNow();

	/** Attacks it has started (body and vines) and players its blows hit, for tests */
	int32 GetAttackCount() const { return AttackCount; }
	int32 GetHitCount() const { return HitCount; }
	int32 GetVineStrikeCount() const { return VineStrikeCount; }

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

	/** Turns towards a spot at its turn rate (times Speed) */
	void TurnTowards(const FVector& Location, float DeltaSeconds, float Speed = 1.0f);

	/** The player it hears best right now and how sure it is (0 nothing .. 1 right next to it), with where it heard them */
	AActor* Listen(float& OutLoudness, FVector& OutWhere);

	/** Hooks onto each player's spell casting once, so casting near it is heard (players may join late) */
	void WatchPlayersCasting();

	/** Rain on the leaves here, local shower or the region's weather */
	bool IsRainingHere() const;

	/** Takes the scent of a player: screams the first time, then hunts */
	void TakeScent(AActor* Target);

	/** Picks an attack for the prey at this distance, or none */
	bool TryAttack(AActor* Target, float Distance);

	/** Hits every player within Range of Centre whose direction from it lies within HalfAngle of its forward (180 for
	 *  all round), at most MaxTargets */
	int32 HitPlayersAround(const FVector& Centre, float Range, float HalfAngle, float Damage, float Knockback, int32 MaxTargets,
		EVaelStatus Status = EVaelStatus::None, float StatusDuration = 0.0f);

	/** Finds its vines' bones in the mesh */
	void SetupVines();

	/** Starts, runs and lays out the vines' own strikes, and hands them to the animation */
	void UpdateVines(float DeltaSeconds);

	/** Sets hands and feet down onto the real ground under them (slopes, roots), the body lowered to match */
	void UpdateGrounding(float DeltaSeconds);

	/** Slobber running from the flower's lower lips; Fling throws a few drops out of the mouth (a bite, a scream) */
	void SetupDrool();
	void UpdateDrool(float DeltaSeconds);
	void FlingDrool(int32 Count);
	FVector GetLip(int32 Lip) const;

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
	TObjectPtr<UAnimSequence> SwipeSequence;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> ClawSequence;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> BiteSequence;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> LeapSequence;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> ScreamSequence;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> LurkSequence;

	/** Element components it listens to, and when (world seconds) each player last cast a spell */
	TArray<TWeakObjectPtr<UVaelElementComponent>> WatchedCasters;
	TMap<TWeakObjectPtr<AActor>, float> LastCastTime;
	float ListenCountdown = 0.0f;

	TArray<FVaelVineStrike> Vines;

	/** How much higher the ground is under each hand and foot than under the body (cm, smoothed), and the pelvis' shift */
	TArray<float> GroundLifts;
	float PelvisShift = 0.0f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> DroolThreads;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> DroolBlobs;
	TArray<FVaelDrool> Drool;
	/** The lips' tips relative to the lower inner jaw bones (turn only, cm), and seconds to the next drop */
	FVector LipOffset[2];
	float DroolCountdown = 0.0f;

	TWeakObjectPtr<AActor> Prey;
	EVaelKelchreisserState State = EVaelKelchreisserState::Territory;
	float StateTime = 0.0f;
	float StateDuration = 0.0f;
	bool bStruck = false;

	/** Where it heard the sound it follows, and where it roams to in its territory */
	FVector SoundLocation = FVector::ZeroVector;
	FVector RoamGoal = FVector::ZeroVector;
	float RoamPause = 0.0f;

	/** Where a leap goes and how fast */
	FVector LeapDirection = FVector::ForwardVector;
	float LeapSpeed = 0.0f;

	/** Seconds the prey has been out of scent range, and until when (world seconds) each attack is ready again */
	float LostScentTime = 0.0f;
	float ClawReadyTime = 0.0f;
	float SwipeReadyTime = 0.0f;
	float BiteReadyTime = 0.0f;
	float LeapReadyTime = 0.0f;
	float RecoverUntil = 0.0f;

	/** It screams only on the first scent and once more when badly hurt */
	bool bHasScreamed = false;
	bool bScreamedHurt = false;

	/** Strength from its region's corruption, fixed when it appears */
	float CorruptionStrength = 1.0f;
	bool bHitFlash = false;
	float MeshBaseZ = 0.0f;
	int32 AttackCount = 0;
	int32 HitCount = 0;
	int32 VineStrikeCount = 0;
};
