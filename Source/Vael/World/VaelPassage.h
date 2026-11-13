// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelPassage.generated.h"

class USphereComponent;

/**
 *  A way through a mountain, like the buried Schlackentor from the Aschenmark into the Eisenschlund: a player who steps in
 *  comes out at the other end. A stand-in for a real tunnel (Boris 2026-11-13: coarse first, tunnels and caves later).
 *  Place one at each mouth; each sends to its Destination, which lies a few metres in front of the other mouth.
 */
UCLASS()
class AVaelPassage : public AActor
{
	GENERATED_BODY()

public:

	AVaelPassage();

	/** Where a player comes out (world X, Y in cm), on the ground there */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Passage")
	FVector2D Destination = FVector2D::ZeroVector;

	/** Shown when a player goes through, like "Durch den Stollen in den Eisenschlund" */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Passage")
	FText Message;

protected:

	virtual void BeginPlay() override;

private:

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USphereComponent> Mouth;

	UFUNCTION()
	void OnEnter(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);
};
