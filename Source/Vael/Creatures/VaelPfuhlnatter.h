// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Creatures/VaelCreature.h"
#include "VaelPfuhlnatter.generated.h"

class AVaelCharacter;
class UAnimSequence;
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
	Release
};

/**
 *  Pfuhlnatter: the sea-serpent-like water snake of the Wurzelforst moor, on the baked animations of SK_Pfuhlnatter.
 *  It always slithers: its head swings along a wavy path and the spine is laid onto the path the head took, so the body
 *  follows every curve like a real snake's; crest and fins lie flat while it crawls. In reach it darts in, coils round
 *  its prey (which can't move meanwhile) and squeezes until it lets go or a hit makes it (co-op rescue).
 *  Values in UVaelPfuhlnatterData. Poison spit (reared, trembling) and wallowing in its puddles come next; then
 *  swimming, growth with age, rain roaming, shedding, brood and taming.
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

	/** Times it has coiled round prey so far, for tests */
	int32 GetConstrictCount() const { return ConstrictCount; }

	/** The player it holds right now, if any */
	AVaelCharacter* GetHeldPlayer() const { return Held.Get(); }

protected:

	virtual TSubclassOf<UVaelCreatureData> GetDefaultDataClass() const override;
	virtual void TickBehavior(float DeltaSeconds) override;
	virtual void OnDamageTaken(float Damage, const FGameplayTagContainer& DamageTags) override;
	virtual void Die() override;

private:

	void EnterState(EVaelSnakeState NewState);

	/** Slithers: the head swings round the wanted direction in waves that run with the distance crawled */
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

	UPROPERTY(Transient)
	TObjectPtr<UVaelSequenceAnimInstance> Animator;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> CrawlSequence;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> DeathSequence;

	/** Spine bones from the tail root to the head (the head only gives the last joint), and the joint distances in cm */
	TArray<FName> SpineBones;
	TArray<float> SpineLengths;
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

	/** Phase of the waves of its head, running with the distance crawled */
	float WavePhase = 0.0f;

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

	/** Side it circles its prey on while it waits to strike again */
	float CircleSide = 1.0f;
};
