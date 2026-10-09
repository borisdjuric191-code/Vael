// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/VaelCombatStatics.h"
#include "VaelMarkCharge.generated.h"

class UStaticMeshComponent;

/**
 *  A charge of Mark stuck in a target, like the splinter of the Kernsplitter or the boiling blood of the Blutsieden.
 *  It rides along with the target and bursts after its time, or only if the target dies within its time.
 *  A burst of a death charge charges those it hits again, so the deaths can run through a crowd.
 *  Placeholder look: a glowing point on the target that pulses faster towards the burst.
 */
UCLASS()
class AVaelMarkCharge : public AActor
{
	GENERATED_BODY()

public:

	/** Constructor */
	AVaelMarkCharge();

	/** Leaves the charge of the hit in the target; a target keeps one charge of each kind, a new one renews it */
	static AVaelMarkCharge* Attach(AActor* Attacker, AActor* Target, const FVaelSpellHit& InHit);

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

protected:

	/** Builds the look */
	virtual void BeginPlay() override;

private:

	/** Hurts everyone around the given place and goes */
	void Burst(const FVector& Location);

	/** The hit that left the charge, with its mode, time, damage and radius */
	FVaelSpellHit Hit;

	/** Who placed the charge and what it sits in */
	TWeakObjectPtr<AActor> Attacker;
	TWeakObjectPtr<AActor> Target;

	/** Seconds since the charge was placed */
	float Age = 0.0f;

	/** Where the target was last seen, for a burst after it is gone */
	FVector LastLocation = FVector::ZeroVector;

	/** Glowing point of the look */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Glow;
};
