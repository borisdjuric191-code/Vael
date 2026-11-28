// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VaelHitFeedbackSubsystem.generated.h"

class AActor;
class UMaterialInstanceDynamic;
class UMeshComponent;
struct FGameplayTagContainer;

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
	static void OnDamageTaken(AActor* Target, float Damage, float EffectMultiplier, bool bTargetIsPlayer, const FGameplayTagContainer* DamageTags = nullptr,
		bool bOverTime = false, float HealthBefore = -1.0f);

	/** Shakes the shared camera, 1 is a heavy blow */
	static void Shake(const UObject* WorldContext, float Strength);

	//~Begin FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	//~End FTickableGameObject

	virtual void Deinitialize() override;

private:


	/** Lets the game run at normal speed again */
	void EndHitStop();

	/** A short bright flash over every mesh of a hit enemy, sparks where the blow landed, a harder stop on the killing blow
	 *  (Boris 2026-11-27: hits need more satisfaction) */
	void Flash(AActor* Target, const FLinearColor& Color, float Strength);
	void Sparks(AActor* Target, const FLinearColor& Color, float Glow, bool bKill) const;
	void StartHitStop(float Duration);

	struct FFlash
	{
		TWeakObjectPtr<AActor> Target;
		TArray<TWeakObjectPtr<UMeshComponent>> Meshes;
		double Start = 0.0;
		float Strength = 1.0f;
		int32 Slot = 0;
	};
	TArray<FFlash> Flashes;

	/** One flash material per running flash, reused */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> FlashMaterials;

	/** Real time before which no new sparks are thrown at the same target (fast weapons hit many times a second) */
	TMap<TWeakObjectPtr<AActor>, double> LastSparks;

	/** Real time at which the running hit-stop ends, 0 while none runs */
	double HitStopEndTime = 0.0;

	/** Real time before which no new hit-stop starts */
	double NextHitStopTime = 0.0;
};
