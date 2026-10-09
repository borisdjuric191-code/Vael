// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/VaelCombatStatics.h"
#include "Magic/VaelElementTypes.h"
#include "VaelGroundStrike.generated.h"

class UStaticMeshComponent;

/** A patch on the ground a strike leaves where it lands, like the fire of a meteor */
struct FVaelStrikeAftermath
{
	EVaelElement Element = EVaelElement::Fire;

	/** Radius of the patch in cm, 0 for none */
	float Radius = 0.0f;

	/** Seconds the patch lasts */
	float Lifetime = 0.0f;

	/** Damage per second to the enemies of the attacker */
	float DamagePerSecond = 0.0f;

	EVaelGroundEffect Effect = EVaelGroundEffect::None;
};

/**
 *  An attack that breaks out of the ground after a warning, like the bone spikes of a preacher.
 *  Shows a flat warning disc first, then hurts the enemies of its instigator inside its radius once.
 *  Placeholder look: a disc and a cone from the engine shapes.
 */
UCLASS()
class AVaelGroundStrike : public AActor
{
	GENERATED_BODY()

private:

	/** Placeholder warning on the ground */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> WarningDisc;

	/** Brighter disc that grows inside the warning until it fills it at the moment of the strike */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> WarningFill;

	/** Placeholder spike shown when the strike lands */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Spike;

protected:

	/** Seconds the spike stays visible after the strike */
	UPROPERTY(EditAnywhere, Category="Strike", meta = (ClampMin = 0))
	float SpikeDuration = 0.4f;

	/** Height of the spike in cm */
	UPROPERTY(EditAnywhere, Category="Strike", meta = (ClampMin = 0))
	float SpikeHeight = 140.0f;

public:

	/** Constructor */
	AVaelGroundStrike();

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

	/** Spawns a strike on the ground that lands after the delay. The attacker decides whom it hurts: a pawn its enemies, any other actor only players. */
	static AVaelGroundStrike* SpawnStrike(AActor* Attacker, const FVector& GroundLocation, const FVaelSpellHit& InHit, float InRadius, float InDelay, const FLinearColor& Color);

	/** Lets the strike leave a patch on the ground when it lands and shake the camera. Call right after spawning. */
	void SetAftermath(const FVaelStrikeAftermath& InAftermath, float InShake = 0.0f);

	/** Chooses what rises where the strike lands instead of the plain spike. Call right after spawning. */
	void SetLook(EVaelStrikeLook InLook) { Look = InLook; }

private:

	/** Hurts everything inside the radius and shows the spike */
	void Strike();

	/** What the strike does to each target */
	FVaelSpellHit Hit;

	/** Reach in cm */
	float Radius = 100.0f;

	/** Seconds from spawning until the strike lands */
	float Delay = 1.0f;

	/** Seconds since spawning */
	float Elapsed = 0.0f;

	/** True once the strike has landed */
	bool bStruck = false;

	/** Patch the strike leaves behind */
	FVaelStrikeAftermath Aftermath;

	/** Strength of the camera shake when the strike lands, 0 for none */
	float Shake = 0.0f;

	/** Scale of the warning disc and of the spike at full size */
	FVector WarningScale = FVector::OneVector;
	FVector SpikeScale = FVector::OneVector;

	/** What rises where the strike lands */
	EVaelStrikeLook Look = EVaelStrikeLook::Spike;

	/** Builds the cluster of bone or rock spikes, its glow at the base and the debris it throws up */
	void BuildSpikeCluster();

	/** Spikes of the cluster; where each stands, how it leans, its height and width in cm */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> ClusterSpikes;

	TArray<FVector> ClusterBases;
	TArray<FRotator> ClusterLeans;
	TArray<float> ClusterHeights;
	TArray<float> ClusterWidths;

	/** Glow on the ground at the foot of the cluster, and its light */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> ClusterGlow;

	UPROPERTY(Transient)
	TObjectPtr<class UPointLightComponent> ClusterLight;
};
