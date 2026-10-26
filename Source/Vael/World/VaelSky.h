// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelSky.generated.h"

class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UPointLightComponent;
class UPostProcessComponent;
class USkyLightComponent;

/**
 *  The sky of a level following the clock (UVaelTimeOfDay): a sun that rises, crosses and sets, a cold moon at night,
 *  the sky light, the haze and the picture darkening into the night, and a soft light around every player so the
 *  dark stays a mood. The sun of the level is only the model: its light, color and direction at noon; the sky hides it
 *  and moves a sun of its own, so the level needs no movable sun.
 */
UCLASS()
class AVaelSky : public AActor
{
	GENERATED_BODY()

public:

	AVaelSky();

	/** Spawns the sky of the world unless it has one */
	static void EnsureSky(UWorld* World);

	virtual void Tick(float DeltaSeconds) override;

protected:

	virtual void BeginPlay() override;

private:

	/** Takes over the sun, sky light, haze and exposure the level was built with */
	void Capture();

	/** Gives every player the soft night light and sets how bright it shines */
	void UpdatePlayerLights(float Night);

	UPROPERTY(Transient)
	TObjectPtr<UDirectionalLightComponent> Sun;

	UPROPERTY(Transient)
	TObjectPtr<UDirectionalLightComponent> Moon;

	UPROPERTY(Transient)
	TObjectPtr<UPostProcessComponent> Picture;

	UPROPERTY(Transient)
	TObjectPtr<USkyLightComponent> SkyLight;

	UPROPERTY(Transient)
	TObjectPtr<UExponentialHeightFogComponent> Fog;

	UPROPERTY(Transient)
	TMap<TObjectPtr<APawn>, TObjectPtr<UPointLightComponent>> PlayerLights;

	/** What the level had: the sun at noon, the sky light, the haze and the exposure */
	float BaseSunIntensity = 3.0f;
	FLinearColor BaseSunColor = FLinearColor(1.0f, 0.82f, 0.68f);
	float BaseYaw = -30.0f;
	float BaseSkyIntensity = 1.0f;
	float BaseFogDensity = 0.02f;
	FLinearColor BaseFogColor = FLinearColor(0.2f, 0.18f, 0.16f);
	float BaseExposureBias = 0.0f;

	float PlayerLightTime = 0.0f;
};
