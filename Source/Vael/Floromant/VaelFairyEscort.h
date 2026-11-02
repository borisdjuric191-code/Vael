// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Nature/VaelFairyFlight.h"
#include "VaelFairyEscort.generated.h"

class UInstancedStaticMeshComponent;
class UVaelPlantArmData;

/**
 *  The fairies a hand with the Feenragwurz calls (Boris 2026-11-02): real fairies, the same as in the Hain, that hover around the
 *  Floromant and follow the scent of the blossom. Sent at an enemy one distracts it for a while, then flies back and rests.
 *  Held, all of them lure the enemies around a place to them. On the shadow path the blossom forces them: they sting, and the
 *  swarm hops from enemy to enemy; with none left they sting her.
 *  Spawned by UVaelPlantArmComponent. Hovering, the instances stand in world space.
 */
UCLASS()
class AVaelFairyEscort : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UInstancedStaticMeshComponent> Fairies;

public:

	AVaelFairyEscort();

	/** Gives her her fairies; Side turns her ring so two hands don't overlap */
	void Setup(APawn* InMistress, const UVaelPlantArmData* InPlant, float Side);

	/** A fairy that is ready flies to the target; false when none is */
	bool Send(AActor* Target, bool bShadow);

	/** All ready fairies fly to a place and lure what is around it; false when none is ready */
	bool SendAll(const FVector& Place, bool bShadow);

	int32 CountReady() const;

	virtual void Tick(float DeltaSeconds) override;

private:

	enum class EDuty : uint8
	{
		Follow,
		ToEnemy,
		Distract,
		Sting,
		ToPlace,
		Lure
	};

	struct FEscortFairy : VaelFairyFlight::FFairy
	{
		EDuty Duty = EDuty::Follow;
		TWeakObjectPtr<AActor> Target;
		FVector Place = FVector::ZeroVector;

		/** Seconds left of her duty, of her rest, and until her next sting */
		float DutySeconds = 0.0f;
		float RestSeconds = 0.0f;
		float StingSeconds = 0.0f;
		bool bShadow = false;
	};

	/** The place in her ring around the Floromant */
	FVector RingPlace(int32 Index) const;

	void Serve(FEscortFairy& Fairy, float DeltaSeconds);

	/** Back to the Floromant, and rests */
	void Dismiss(FEscortFairy& Fairy);

	/** A living enemy near her other than the one she stung */
	AActor* FindNextEnemy(const FEscortFairy& Fairy) const;

	UPROPERTY(Transient)
	TObjectPtr<APawn> Mistress;

	UPROPERTY(Transient)
	TObjectPtr<const UVaelPlantArmData> Plant;

	TArray<FEscortFairy> Flock;
	VaelFairyFlight::FSettings Flight;
	float RingTurn = 0.0f;
	float Clock = 0.0f;
};
