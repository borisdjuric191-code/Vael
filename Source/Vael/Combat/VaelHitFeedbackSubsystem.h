// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VaelHitFeedbackSubsystem.generated.h"

class AActor;

/**
 *  Makes hits felt: a short hit-stop on heavy hits, camera shake and staggering creatures.
 *  Values under Project Settings > Game > Vael UI > Hit Feedback.
 */
UCLASS()
class UVaelHitFeedbackSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:

	/** Reacts to damage an actor took. The effect multiplier says how well the hit worked (weakness and reaction). */
	static void OnDamageTaken(AActor* Target, float Damage, float EffectMultiplier, bool bTargetIsPlayer);

	/** Shakes the shared camera, 1 is a heavy blow */
	static void Shake(const UObject* WorldContext, float Strength);

	//~Begin FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	//~End FTickableGameObject

	virtual void Deinitialize() override;

private:

	/** Nearly stops the game for a moment */
	void StartHitStop();

	/** Lets the game run at normal speed again */
	void EndHitStop();

	/** Real time at which the running hit-stop ends, 0 while none runs */
	double HitStopEndTime = 0.0;

	/** Real time before which no new hit-stop starts */
	double NextHitStopTime = 0.0;
};
