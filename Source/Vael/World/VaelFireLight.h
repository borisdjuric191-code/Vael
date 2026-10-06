// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelFireLight.generated.h"

class UPointLightComponent;

/**
 *  Warm light of a fire, torch or candle that flickers on its own.
 *  Placed in a level next to a fire mesh or effect; the flicker also runs in the editor viewport, so it can be tuned there.
 */
UCLASS()
class AVaelFireLight : public AActor
{
	GENERATED_BODY()

	/** The light itself */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPointLightComponent> Light;

protected:

	/** Mean brightness in candela */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fire Light", meta = (ClampMin = 0))
	float BaseIntensity = 60.0f;

	/** How far the brightness swings around the mean, 0 = steady light, 0.3 = calm fire, 0.6 = restless torch */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fire Light", meta = (ClampMin = 0, ClampMax = 1))
	float FlickerStrength = 0.3f;

	/** How fast the light flickers, about one change per second at 1 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fire Light", meta = (ClampMin = 0.05))
	float FlickerSpeed = 3.0f;

public:

	/** Constructor */
	AVaelFireLight();

	/** Applies the mean brightness, so the editor shows it right away */
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Initialization */
	virtual void BeginPlay() override;

	/** Flickers */
	virtual void Tick(float DeltaSeconds) override;

	/** Flickers in the editor viewport too */
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

private:

	/** Own time of the noise, so several fires never flicker in step */
	float NoiseTime = 0.0f;
};
