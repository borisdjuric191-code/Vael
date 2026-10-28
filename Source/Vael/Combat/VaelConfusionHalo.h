// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "VaelConfusionHalo.generated.h"

/**
 *  Sign of confusion over a character's head: three small spirals that circle around each other, spin and bob,
 *  their colour wandering between turquoise and raspberry pink (the colours of the Wachbeere; violet stays the Nid's).
 *  Added by AVaelCharacterBase while the character is confused and removed when it ends.
 *  Mesh and material come from Scripts/BuildStatusVerwirrung.py (custom data 0 = colour phase of each spiral).
 */
UCLASS()
class VAEL_API UVaelConfusionHalo : public UInstancedStaticMeshComponent
{
	GENERATED_BODY()

public:

	UVaelConfusionHalo();

	/** Height of the circle above the attach point, in cm */
	UPROPERTY(EditAnywhere, Category="Confusion")
	float Height = 30.0f;

	/** Radius of the circle the spirals ride on, in cm */
	UPROPERTY(EditAnywhere, Category="Confusion")
	float Radius = 30.0f;

	/** Size of a spiral against the mesh (about 21 cm wide), so it reads from the game camera */
	UPROPERTY(EditAnywhere, Category="Confusion")
	float SpiralScale = 1.6f;

	/** Turns per second of the circle and of each spiral around itself */
	UPROPERTY(EditAnywhere, Category="Confusion")
	float OrbitSpeed = 0.45f;

	UPROPERTY(EditAnywhere, Category="Confusion")
	float SpinSpeed = 1.1f;

	virtual void OnRegister() override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:

	float Time = 0.0f;
};
