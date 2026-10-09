// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Creatures/VaelCreature.h"
#include "VaelEmberCrawler.generated.h"

class UMaterialInterface;
class USceneComponent;
class UStaticMeshComponent;

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

	/** Moves the body, legs and claws */
	virtual void Tick(float DeltaSeconds) override;

	/** Burrowed crawlers take less damage, except from earth */
	virtual float GetIncomingDamageMultiplier(const FGameplayTagContainer& DamageTags) const override;

	/** What the crawler is doing right now */
	UFUNCTION(BlueprintPure, Category="Creature")
	EVaelCrawlerState GetCrawlerState() const { return State; }

protected:

	virtual TSubclassOf<UVaelCreatureData> GetDefaultDataClass() const override;
	virtual void TickBehavior(float DeltaSeconds) override;
	virtual void OnDamageTaken(float Damage, const FGameplayTagContainer& DamageTags) override;
	virtual void OnBodyColorShown(const FLinearColor& Color, bool bHitFlash) override;

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

	/** Builds the look from shell, gland, head, claws and six legs */
	void BuildRig();

	/** Moves the look: sinking and rising, the gait of the legs, the snapping claws, the swelling gland, and puffs of dust, sparks or steam */
	void AnimateRig(float DeltaSeconds);

	/** Adds a part of the look to the rig: a sphere in a look material, or a cone if asked */
	UStaticMeshComponent* AddRigPart(UMaterialInterface* Material, const FLinearColor& Color, float Glow, bool bCone = false);

	/** Lays a stretched sphere between two points of the rig, as a segment of a leg or a claw */
	static void PlaceSegment(UStaticMeshComponent* Segment, const FVector& Start, const FVector& End, float Width);

	/** Everything of the look hangs on this, so it can sink, rise, lunge and tremble as one */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Rig;

	/** Plates of ash crust, the glowing gland under them, head, eyes */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> ShellParts;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Gland;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Eyes;

	/** Upper and lower segment of each leg, left legs first from the front */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> LegUpper;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> LegLower;

	/** Arm and pincer of each claw, left first */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> ClawArms;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> ClawTips;

	/** Mound of ash over the burrowed crawler */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Mound;

	/** State the look last showed, to notice when the crawler breaks out of the ground */
	EVaelCrawlerState ShownState = EVaelCrawlerState::Burrowed;

	/** Progress of the gait in steps, grows with the distance walked */
	float GaitPhase = 0.0f;

	/** How far the rig is out of the ground, 0 under it, 1 fully out */
	float Emergence = 0.0f;

	/** Seconds since the last claw attack, for the snap of the pincers; negative for none yet */
	float SinceClawAttack = -1.0f;

	/** Seconds until the next puff of dust, spark or steam */
	float PuffCountdown = 0.0f;

	/** Glow of gland and eyes before pulses, set by the color of the state */
	float GlandGlow = 3.0f;

	/** Color of the gland, eyes and the crust */
	FLinearColor GlowColor = FLinearColor(1.0f, 0.25f, 0.05f);
	FLinearColor CrustColor = FLinearColor(0.08f, 0.05f, 0.04f);
};
