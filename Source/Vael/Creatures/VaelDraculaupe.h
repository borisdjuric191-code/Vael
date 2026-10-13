// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Creatures/VaelCreature.h"
#include "VaelDraculaupe.generated.h"

class AVaelCharacter;
class UStaticMeshComponent;

/** What a caterpillar is doing */
UENUM(BlueprintType)
enum class EVaelCaterpillarState : uint8
{
	/** Crawls: towards a player, or around its home */
	Crawl,
	/** Has touched a player and spins itself in; the moth hatches when the time is up */
	Cocoon
};

/**
 *  Draculaupe (idea by Boris): a slow caterpillar whose touch does no harm. Touching a player it spins itself into a cocoon
 *  of white threads at once, and its alarm calls the other caterpillars to that player. After a few seconds a moth hatches
 *  that follows the player and rains on them, unless the cocoon is destroyed first.
 *  With the data of the Tauraupe it is the healed animal of a pure land: it only wanders, and its moth rains healing water.
 */
UCLASS()
class AVaelDraculaupe : public AVaelCreature
{
	GENERATED_BODY()

public:

	/** Constructor */
	AVaelDraculaupe();

	/** Builds cocoon and threads */
	virtual void BeginPlay() override;

	/** Crawls, spins, swells */
	virtual void Tick(float DeltaSeconds) override;

	/** A cocoon takes little damage */
	virtual float GetIncomingDamageMultiplier(const FGameplayTagContainer& DamageTags) const override;

	/** What it is doing */
	UFUNCTION(BlueprintPure, Category="Creature")
	EVaelCaterpillarState GetCaterpillarState() const { return State; }

	/** Touches the player: spins itself in and calls the others. Also for tests. */
	void Touch(AVaelCharacter* Player);

	/** Called by the alarm of another caterpillar: crawls to the player faster */
	void Alarm(AVaelCharacter* Player);

	/** True once another caterpillar has called it */
	bool IsAlarmed() const { return bAlarmed; }

protected:

	virtual TSubclassOf<UVaelCreatureData> GetDefaultDataClass() const override;
	virtual void TickBehavior(float DeltaSeconds) override;
	virtual void Die() override;
	virtual float GetStatusTextHeight() const override { return 110.0f; }

private:

	/** The moth leaves the cocoon and the caterpillar is gone */
	void Hatch();

	/** Finds the back bones of the factory mesh */
	void SetupSpine();

	/** Lets a wave run through the back, or scoots a mesh without back bones */
	void PoseFactoryMesh(float DeltaSeconds);

	EVaelCaterpillarState State = EVaelCaterpillarState::Crawl;

	/** Seconds in the cocoon */
	float CocoonAge = 0.0f;

	/** The player it touched, or was called to */
	TWeakObjectPtr<AVaelCharacter> Prey;
	bool bAlarmed = false;

	/** Where it wanders to, and seconds until it picks a new place */
	FVector RoamPoint = FVector::ZeroVector;
	float RoamCountdown = 0.0f;

	/** Back bones from the hind end to the head, and where they rest in component space */
	TArray<FName> SpineBones;
	TArray<FTransform> SpineRest;

	/** Progress of the crawl in waves, grows with the way it has gone */
	float CrawlPhase = 0.0f;
	FVector LastLocation = FVector::ZeroVector;

	/** How strongly the back arches, eased from frame to frame */
	float ArchShown = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Cocoon;

	/** Threads whirling around it while it spins */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Threads;
};
