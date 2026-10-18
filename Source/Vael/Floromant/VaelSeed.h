// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelSeed.generated.h"

class AVaelCharacter;
class UVaelPlantData;
class UPointLightComponent;
class UStaticMeshComponent;

/** A seed thrown by a Floromant: flies in an arc, glowing in the colour of its plant, and grows into the plant where it lands */
UCLASS()
class AVaelSeed : public AActor
{
	GENERATED_BODY()

	/** Glow around the seed */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPointLightComponent> Light;

public:

	/** Constructor */
	AVaelSeed();

	/** Throws a seed from the hand of the Floromant to the ground location. Returns null if it can't be spawned. */
	static AVaelSeed* Throw(AVaelCharacter* Gardener, UVaelPlantData* Plant, bool bShadow, const FVector& From, const FVector& GroundTarget);

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

protected:

	/** Builds the look */
	virtual void BeginPlay() override;

private:

	/** Grows the plant where the seed lands */
	void Land();

	UPROPERTY(Transient)
	TObjectPtr<UVaelPlantData> Plant;

	bool bShadow = false;
	TWeakObjectPtr<AVaelCharacter> Gardener;

	/** Where it was thrown from and where it lands, and the yaw of the throw */
	FVector Start = FVector::ZeroVector;
	FVector Target = FVector::ZeroVector;
	float ThrowYaw = 0.0f;

	/** Seconds since the throw */
	float Elapsed = 0.0f;

	/** Kernel and husk of the seed */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Shapes;
};
