// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Creatures/VaelCreature.h"
#include "VaelPfuhlnatter.generated.h"

class AVaelCharacter;
class UAnimSequence;
class UMaterialInstanceDynamic;
class UVaelSequenceAnimInstance;

/** What a Pfuhlnatter is doing */
UENUM(BlueprintType)
enum class EVaelSnakeState : uint8
{
	/** Lies still where it last crawled */
	Lurk,
	/** Slithers after its prey, or circles it until it may strike again, or crawls home */
	Hunt,
	/** Darts in at its prey to coil round it */
	Rush,
	/** Winds itself round its prey, which can't move, and squeezes */
	Constrict,
	/** Lets go: the coils sink to the ground, then it slithers on */
	Release,
	/** Rears its front up, trembles with its fins open and its throat glowing, spits a gob of poison and lays down again */
	Spit,
	/** Wallows in a puddle of its poison and heals */
	Wallow
};

/**
 *  Pfuhlnatter: the sea-serpent-like water snake of the Wurzelforst moor, on the baked animations of SK_Pfuhlnatter.
 *  It always slithers: its head goes straight and calm, the spine is laid onto the path the head took in waves that
 *  stand on the ground, so the body winds behind it through every curve; crest and fins lie flat while it crawls.
 *  It picks its attacks: a spit (it rears up like a cobra, trembles with its fins open and its throat glowing, and spits
 *  a gob of poison that leaves a puddle), or it darts in, coils round its prey (which can't move meanwhile) and squeezes
 *  until it lets go or a hit makes it (co-op rescue). Hurt, it wallows in a puddle of its poison to heal.
 *  Values in UVaelPfuhlnatterData. Swimming, growth with age, rain roaming, shedding, brood and taming come later.
 */
UCLASS()
class AVaelPfuhlnatter : public AVaelCreature
{
	GENERATED_BODY()

public:

	/** Constructor */
	AVaelPfuhlnatter();

	/** Sets up mesh, animations and the path its body lies on */
	virtual void BeginPlay() override;

	/** Lays the body onto its path */
	virtual void Tick(float DeltaSeconds) override;

	/** Lets go of its prey when it is removed */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** What it is doing */
	UFUNCTION(BlueprintPure, Category="Pfuhlnatter")
	EVaelSnakeState GetSnakeState() const { return State; }

	/** Times it has coiled round prey, spat and wallowed so far, for tests */
	int32 GetConstrictCount() const { return ConstrictCount; }
	int32 GetSpitCount() const { return SpitCount; }
	int32 GetWallowCount() const { return WallowCount; }

	/** For tests: its next attack is a spit (true) or a coil */
	void SetNextAttackSpit(bool bSpit) { bNextSpit = bSpit; StrikeCooldown = 0.0f; }

	/** The player it holds right now, if any */
	AVaelCharacter* GetHeldPlayer() const { return Held.Get(); }

protected:

	virtual TSubclassOf<UVaelCreatureData> GetDefaultDataClass() const override;
	virtual void TickBehavior(float DeltaSeconds) override;
	virtual void OnDamageTaken(float Damage, const FGameplayTagContainer& DamageTags) override;
	virtual void Die() override;

private:

	void EnterState(EVaelSnakeState NewState);

	/** Slithers: the head goes straight, the body winds behind it */
	/** Steers: only ever forward, turning no tighter than MinTurnRadius */
	virtual void MoveInDirection(const FVector& Direction, float Speed) override;

	void Slither(const FVector& Direction, float Speed, float DeltaSeconds);

	/** Where the neck joint is in the world, at the height of the ground under the snake */
	FVector GetNeckLocation() const;

	/** Adds a point to the path (with its height above the ground) and drops what the body no longer needs */
	void AddPathPoint(const FVector& Location, float Lift);

	/** The joints of the spine laid back along the path from the neck, handed to the animation */
	void LayBodyOnPath();

	/** Holds or frees the player it coils round */
	void HoldPlayer(AVaelCharacter* Player);
	void FreePlayer();

	/** Lifts the front joints off the path into a cobra's arch (Rear 0..1), leaning forward by Thrust (-1 back .. 1 strike) */
	void RearUp(TArrayView<FVector> Joints, int32 Count, float Weight) const;

	/** Spits the gob: lobbed from the head to where the prey will be */
	void SpitPoison();

	/** Its next attack after a strike: spit or coil */
	void PickNextAttack();

	/** Turns on the spot towards a point, calmly */
	void TurnTowards(const FVector& Location, float DeltaSeconds);

	UPROPERTY(Transient)
	TObjectPtr<UVaelSequenceAnimInstance> Animator;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> CrawlSequence;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> DeathSequence;

	/** Spine bones from the tail root to the head (the head only gives the last joint), and the joint distances in cm */
	TArray<FName> SpineBones;
	TArray<float> SpineLengths;

	/** Where each spine joint stands in the Tripo pose (mesh space, before BodyScale): reared, the front takes this shape
	 *  again, so the crown skin over the neck sits as it was modelled instead of stretching */
	TArray<FVector> RestJoints;

	/** The joints as last shown (world), followed over a few frames so the body slides instead of twitching */
	TArray<FVector> SmoothJoints;

	/** Phase of each fin bone in the calm sway wave */
	TArray<float> FinPhases;
	float BodyLength = 0.0f;

	/** Fins and crown with the scale they are drawn in to right now */
	TArray<FName> FinBones;
	TArray<float> FinScales;
	TArray<bool> FinIsCrown;
	float FinDraw = 1.0f;

	/** The path the head took, oldest first, at ground height, and how far above the ground each point lies (coils) */
	TArray<FVector> Path;
	TArray<float> PathLift;

	/** The point the body is laid back from: the neck while it crawls, the head of the coil while it winds round prey */
	FVector Lead = FVector::ZeroVector;
	float LeadLift = 0.0f;

	/** Distance along the path at each point (the phase of the body's waves) */
	TArray<float> PathRun;

	/** Where the head looks, and how much (it eases in and out) */
	FVector LookPoint = FVector::ZeroVector;
	float LookWeight = 0.0f;

	TWeakObjectPtr<AActor> Prey;
	TWeakObjectPtr<AVaelCharacter> Held;
	EVaelSnakeState State = EVaelSnakeState::Lurk;
	float StateTime = 0.0f;
	float StrikeCooldown = 0.0f;
	float NoPreyTime = 0.0f;
	float SqueezeCountdown = 0.0f;
	float DamageWhileHolding = 0.0f;
	int32 ConstrictCount = 0;

	/** The coil: round this point, starting at this angle, winding this way (1 or -1) */
	FVector CoilCentre = FVector::ZeroVector;
	float CoilStartAngle = 0.0f;
	float CoilSense = 1.0f;

	/** Where its neck was when it began to coil: the head runs from there onto the coil */
	FVector CoilEntry = FVector::ZeroVector;

	/** Breathing (radians) and the slow shift of the waves while it lies still */
	float Breath = 0.0f;
	float IdleShift = 0.0f;

	/** Side it circles its prey on while it waits to strike again */
	float CircleSide = 1.0f;

	/** Its body's material, for the glowing throat before a spit */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SkinMaterial;

	/** How far it is reared (0 lying .. 1 up), and the neck's lean (-1 drawn back .. 1 striking) */
	float Rear = 0.0f;
	float Thrust = 0.0f;
	float ThroatGlow = 0.0f;
	bool bSpat = false;
	bool bNextSpit = false;
	int32 SpitCount = 0;

	/** Fins spread back to their rest pose (0 folded as crawling .. 1 spread) */
	float FinOpen = 0.0f;
	TArray<float> FinOpens;

	/** The puddle it wallows in, and where it is in its round through it */
	TWeakObjectPtr<class AVaelPoisonPuddle> WallowPuddle;
	float WallowAngle = 0.0f;
	int32 WallowCount = 0;
};
