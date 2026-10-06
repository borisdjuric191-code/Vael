// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Creatures/VaelCreature.h"
#include "VaelSourceGuardian.generated.h"

class AVaelMarkSource;

/**
 *  Quellwaechter: rises out of a Mark source after the offering and seals it when it falls.
 *  A hybrid of the creatures around the source with a swarm consciousness: strikes up close, bursts like an ember crawler,
 *  dives like a harpy and calls its swarm out of the source. Screams at shares of its health; each scream makes the swarm denser.
 *  Its health grows with the number of players.
 */
UCLASS()
class AVaelSourceGuardian : public AVaelCreature
{
	GENERATED_BODY()

public:

	/** Lets a guardian rise out of a source, which it seals when it falls */
	static AVaelSourceGuardian* RiseFromSource(AVaelMarkSource* Source, UVaelCreatureData* Data);

	virtual bool IsBossFightActive() const override { return !IsDead(); }

protected:

	virtual TSubclassOf<UVaelCreatureData> GetDefaultDataClass() const override;
	virtual void TickBehavior(float DeltaSeconds) override;
	virtual void OnHealthChanged(float OldValue, float NewValue) override;
	virtual bool AlwaysShowStatusText() const override { return true; }
	virtual void Die() override;

private:

	/** Scales the health to the number of players and announces the fight */
	void Rise();

	/** Throws players back, makes the swarm denser */
	void Scream();

	/** Warns, then bursts around itself */
	void StartBurst();

	/** Calls creatures of its swarm out of the source */
	void CallSwarm();

	/** Updates a running dive */
	void TickDive(float DeltaSeconds);

	/** The source the guardian rose from */
	TWeakObjectPtr<AVaelMarkSource> Source;

	/** Creatures of the swarm it called that may still live */
	TArray<TWeakObjectPtr<AVaelCreature>> Swarm;

	/** True until the guardian has risen and fights */
	bool bRising = true;

	/** Seconds since rising began */
	float RiseTime = 0.0f;

	/** Number of screams so far */
	int32 NumScreams = 0;

	/** Index of the next swarm creature kind */
	int32 NextSwarmKind = 0;

	float StrikeCooldown = 0.0f;
	float BurstCooldown = 0.0f;
	float DiveCooldown = 0.0f;
	float SwarmCooldown = 0.0f;

	/** True while a dive runs, including its warning */
	bool bDiving = false;

	/** Seconds since the dive started */
	float DiveTime = 0.0f;

	/** Direction of the current dive */
	FVector DiveDirection = FVector::ForwardVector;

	/** Players the current dive has already hit */
	TSet<TWeakObjectPtr<AActor>> DiveHits;
};
