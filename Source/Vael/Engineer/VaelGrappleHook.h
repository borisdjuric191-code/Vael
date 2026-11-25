// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelGrappleHook.generated.h"

class AVaelCharacterBase;
class UStaticMeshComponent;
class UVaelEngineerComponent;

/**
 *  The Wurfhaken of the engineer (Fallen-Paket 25.11.): a Greifklaue with the Klammerfinger of a Schleieraffe on a rope of
 *  Rankenfaser. It flies straight ahead and snags the first enemy on its way. A small one is yanked to him (into his traps,
 *  in front of his sling), a heavy one or a boss holds fast, and he pulls himself to it instead. With nothing on the way it
 *  is reeled back in. Placeholder look: shapes of the engine, a thin rope from his hand to the hook.
 */
UCLASS()
class AVaelGrappleHook : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Head;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Rope;

public:

	AVaelGrappleHook();

	virtual void Tick(float DeltaSeconds) override;

	/** Throws a hook from this spot in this direction (flat) */
	static AVaelGrappleHook* Throw(UVaelEngineerComponent* Engineer, const FVector& From, const FVector& Direction);

	/** What it snagged, and whether it pulled the engineer instead of the enemy (for the tests) */
	AVaelCharacterBase* GetSnagged() const { return Snagged.Get(); }
	bool PullsEngineer() const { return bPullEngineer; }

private:

	enum class EPhase : uint8 { Flying, Pulling, Reeling };

	void UpdateRope();
	void Snag(AVaelCharacterBase* Target);

	TWeakObjectPtr<UVaelEngineerComponent> Engineer;
	TWeakObjectPtr<AVaelCharacterBase> Snagged;
	EPhase Phase = EPhase::Flying;
	FVector Direction = FVector::ForwardVector;
	float Travelled = 0.0f;
	float PhaseStart = 0.0f;
	bool bPullEngineer = false;
};
