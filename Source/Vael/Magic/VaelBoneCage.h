// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelBoneCage.generated.h"

class UStaticMeshComponent;

/**
 *  Bones break out of the ground around a target, bend over it and hold it, like the Markkaefig. Only shows, the stun of the spell does the holding.
 *  When its time is up the cage crumbles.
 */
UCLASS()
class AVaelBoneCage : public AActor
{
	GENERATED_BODY()

public:

	AVaelBoneCage();

	/** Grows a cage around the target that stands for the seconds */
	static AVaelBoneCage* Spawn(AActor* Caster, const AActor* Target, float Seconds);

	virtual void Tick(float DeltaSeconds) override;

protected:

	virtual void BeginPlay() override;

private:

	/** Size of the cage and how long it stands */
	float CageRadius = 80.0f;
	float CageHeight = 180.0f;
	float Seconds = 3.0f;
	float Age = 0.0f;

	/** Lower and upper part of each bone, then the glow at its foot */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Lower;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Upper;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Glows;
};
