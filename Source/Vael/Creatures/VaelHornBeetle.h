// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Creatures/VaelCreature.h"
#include "VaelHornBeetle.generated.h"

class UMaterialInstanceDynamic;
class UPoseableMeshComponent;
class USoundAttenuation;
class UStaticMeshComponent;
class UVaelLegIKComponent;

/** What a horn beetle is doing */
UENUM(BlueprintType)
enum class EVaelBeetleState : uint8
{
	/** Walks to its shooting distance */
	Roam,
	/** Spreads its horns, ratcheting; fire now burns the thread */
	Tension,
	/** The thread has locked into the middle horn ("Klack"), the shot follows */
	Locked,
	/** Fire burned the thread: it can't attack and takes more damage */
	Defenseless
};

/**
 *  Spannhornkaefer: artillery beetle of the Wurzelforst. Keeps its distance, spreads its horns with a ratchet until the
 *  thread locks with a "Klack" and lobs a shot in an arc at where its target stood; the impact is marked on the ground.
 *  Fire on the tensed thread burns it through and leaves the beetle defenseless for a while.
 *  Looks: the Tripo mesh of the creature factory, legs moved by UVaelLegIKComponent, horns and thread posed here.
 */
UCLASS()
class AVaelHornBeetle : public AVaelCreature
{
	GENERATED_BODY()

	/** The thread between the horn tips, shown while it is tensed */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Thread;

public:

	/** Constructor */
	AVaelHornBeetle();

	/** Sets up the mesh and the legs */
	virtual void BeginPlay() override;

	/** Defenseless beetles take more damage */
	virtual float GetIncomingDamageMultiplier(const FGameplayTagContainer& DamageTags) const override;

	/** What the beetle is doing */
	UFUNCTION(BlueprintPure, Category="Creature")
	EVaelBeetleState GetBeetleState() const { return State; }

	/** The legs, for tests */
	const UVaelLegIKComponent* GetLegs() const { return GetFactoryLegs(); }

	/** Shots fired so far, for tests */
	int32 GetShotCount() const { return ShotCount; }

	/** Starts spreading the horns at once, for tests */
	void StartTension();

protected:

	virtual TSubclassOf<UVaelCreatureData> GetDefaultDataClass() const override;
	virtual void TickBehavior(float DeltaSeconds) override;
	virtual void OnDamageTaken(float Damage, const FGameplayTagContainer& DamageTags) override;
	virtual void Die() override;
	virtual float GetStatusTextHeight() const override { return 120.0f; }

private:

	/** Switches to a new state */
	void EnterState(EVaelBeetleState NewState);

	/** Lobs the shot at the target's place and marks the impact */
	void Shoot();

	/** Fire burned the thread */
	void BurnThread();

	/** Turns towards a location at the turn speed */
	void TurnTowards(const FVector& Location, float DeltaSeconds);

	/** Poses horns and thread on top of the legs, every frame */
	void PoseHorns();

	/** Plays a click of the ratchet, or the louder "Klack" */
	void PlayClick(bool bKlack);

	EVaelBeetleState State = EVaelBeetleState::Roam;

	/** Seconds in the current state */
	float StateTime = 0.0f;

	/** Seconds until the next shot may start */
	float ShotCooldown = 0.0f;

	/** Seconds until the next click of the ratchet */
	float ClickCountdown = 0.0f;

	/** Who the shot goes at */
	TWeakObjectPtr<AActor> ShotTarget;

	/** Spread of the horns shown, 0 resting to 1 fully spread, and the spread it moves to */
	float SpreadShown = 0.0f;
	float SpreadTarget = 0.0f;

	/** Jitter of the horns from the last click, decays */
	float Tremble = 0.0f;

	/** Seconds the burned thread still glows */
	float BurnGlow = 0.0f;

	int32 ShotCount = 0;

	/** Reference transforms of the horn bones in component space */
	FTransform RestHornFirst = FTransform::Identity;
	FTransform RestHornSecond = FTransform::Identity;
	FTransform RestHornMiddle = FTransform::Identity;
	FTransform RestBody = FTransform::Identity;

	/** Direction of the turn that spreads each side horn outwards */
	float HornSpreadSign[2] = { 1.0f, -1.0f };

	/** Tips of the side horns in the reference pose, matched to the horn bones by their side */
	FVector HornTips[2] = { FVector::ZeroVector, FVector::ZeroVector };

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ThreadMaterial;

	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> ClickAttenuation;
};
