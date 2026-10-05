// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelLightningStrike.generated.h"

class AVaelRegion;
class UStaticMeshComponent;

/**
 *  Lightning of a storm: a pale warning on the ground, then a strike that hurts players and creatures alike.
 *  Values come from the project settings (Vael World). Placeholder look: a disc and a tall thin column.
 */
UCLASS()
class AVaelLightningStrike : public AActor
{
	GENERATED_BODY()

private:

	/** Placeholder warning on the ground */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> WarningDisc;

	/** Placeholder bolt */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Bolt;

public:

	/** Constructor */
	AVaelLightningStrike();

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

	/** Spawns a strike on the ground of a region */
	static AVaelLightningStrike* SpawnStrike(AVaelRegion* Region, const FVector& GroundLocation);

private:

	/** Hurts everything near and shows the bolt */
	void Strike();

	/** Region whose storm sent the strike */
	TWeakObjectPtr<AVaelRegion> Region;

	/** Seconds since spawning */
	float Elapsed = 0.0f;

	/** True once the strike has landed */
	bool bStruck = false;
};
