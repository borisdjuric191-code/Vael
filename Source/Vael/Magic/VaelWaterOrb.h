// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelWaterOrb.generated.h"

class UNiagaraComponent;
class UPointLightComponent;
class UStaticMeshComponent;
class UVaelElementSource;

/**
 *  A floating ball of water from the water kit: turns slowly and bobs up and down.
 *  Its source sets the material of the ball, the color of its light and the color of its particles
 *  (passed to the Niagara user parameter User.WaterColor). Derived in the editor as BP_WaterOrb,
 *  where the Niagara system NS_Water_Orb goes into the Particles component.
 */
UCLASS()
class AVaelWaterOrb : public AActor
{
	GENERATED_BODY()

private:

	/** Pivot that bobs and turns, the ball and its effects hang on it */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> Pivot;

	/** The ball of water, about 1 m across */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Ball;

	/** Bubbles, drops and mist; empty until a Niagara system is chosen in the Blueprint */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraComponent> Particles;

	/** Soft light in the color of the source */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPointLightComponent> Glow;

protected:

	/** Whose water it is: sets material, light color and particle color */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water Orb")
	TObjectPtr<UVaelElementSource> Source;

	/** Turning speed in degrees per second */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water Orb")
	float SpinSpeed = 25.0f;

	/** How far the ball floats up and down, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water Orb", meta = (ClampMin = 0))
	float BobHeight = 8.0f;

	/** Up and down movements per second */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water Orb", meta = (ClampMin = 0))
	float BobFrequency = 0.4f;

	/** Brightness of the light with the clearest, brightest source */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water Orb", meta = (ClampMin = 0))
	float GlowIntensity = 1500.0f;

public:

	/** Constructor */
	AVaelWaterOrb();

	/** Shows the source in the editor as soon as it is chosen */
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Bobs and turns */
	virtual void Tick(float DeltaSeconds) override;

	/** Changes whose water it is, at any time */
	UFUNCTION(BlueprintCallable, Category="Water Orb")
	void SetSource(UVaelElementSource* NewSource);

	/** Name of the Niagara user parameter that receives the particle color */
	static const FName WaterColorParameter;

private:

	/** Applies the source to ball, light and particles */
	void ApplySource();

	/** Seconds the orb has been moving */
	float MotionTime = 0.0f;
};
