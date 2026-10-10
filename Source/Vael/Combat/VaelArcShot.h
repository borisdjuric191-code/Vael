// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelArcShot.generated.h"

class UStaticMeshComponent;

/**
 *  A shot lobbed in an arc, like the one of the Spannhornkaefer. Only the look: it flies from start to end
 *  in a fixed time and vanishes; the damage comes from the ground strike that marks the impact.
 *  Placeholder look: a small sphere of the engine shapes that tumbles.
 */
UCLASS()
class AVaelArcShot : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Shell;

public:

	/** Constructor */
	AVaelArcShot();

	/** Flies along the arc */
	virtual void Tick(float DeltaSeconds) override;

	/** Lobs a shot from start to end, arriving after the flight time, the arc this high above the higher end */
	static AVaelArcShot* Lob(UWorld* World, const FVector& Start, const FVector& End, float FlightTime, float Apex, const FLinearColor& Color, float Size = 22.0f);

private:

	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	float FlightTime = 1.0f;
	float Apex = 300.0f;
	float Age = 0.0f;
	FRotator Spin = FRotator::ZeroRotator;
};
