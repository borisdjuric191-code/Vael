// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "VaelHandWater.generated.h"

class UInstancedStaticMeshComponent;
class USkeletalMeshComponent;

/**
 *  Water on the hands of a mage gathering a wave (Boris 2026-11-02): beads wind round the forearm towards the hand and grow
 *  until the throw, drops run off the fingers and are drawn out into threads as the hand swings. With the throw the water
 *  flies off forward; afterwards the hands drip a little longer. Gone on its own once the last drop has fallen.
 */
UCLASS()
class UVaelHandWaterComponent : public USceneComponent
{
	GENERATED_BODY()

public:

	UVaelHandWaterComponent();

	/** Gathers water round the right hand of the body, or round both; it is fullest at the release. Earth: no coil, grit and
	 *  small stones trickle off the fists instead (Boris 2026-11-02) */
	void Start(USkeletalMeshComponent* InBody, bool bInBothHands, float InRelease, bool bInEarth = false);

	/** The throw or the blow: what clings to the hands leaves them this way; zero bursts it up all round, as where fists hit the ground */
	void Fling(const FVector& Forward);

	/** Stops gathering at once; what is in the air still falls */
	void Finish();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:

	struct FDrop
	{
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float Size = 3.0f;
		float Age = 0.0f;
	};

	/** Elbow, wrist and fingertips of a hand (0 right, 1 left); false without those bones */
	bool GetHand(int32 Side, FVector& OutElbow, FVector& OutWrist, FVector& OutTips) const;

	void Shed(const FVector& Location, const FVector& Velocity, float Size);

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Beads;

	TWeakObjectPtr<USkeletalMeshComponent> Body;

	TArray<FDrop> Drops;
	TArray<FTransform> Frame;

	/** Where each wrist was last frame, for how fast the hand swings */
	FVector LastWrist[2];
	bool bHasLastWrist = false;

	bool bBothHands = false;
	bool bEarth = false;
	bool bFlung = false;
	bool bFinished = false;
	float Release = 0.3f;
	float Age = 0.0f;
	float FlungAge = 0.0f;
	float DripCredit[2] = { 0.0f, 0.0f };
};
