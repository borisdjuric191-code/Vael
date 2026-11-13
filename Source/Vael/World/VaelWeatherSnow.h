// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelWeatherSnow.generated.h"

class AVaelRegion;
class ULocalFogVolumeComponent;
class UVaelFlakesComponent;

/**
 *  The look of snow and blizzard around the players (Sturmzinnen, Boris 2026-11-13): flakes of the flake system fall,
 *  sway, drift with the wind and lie on the ground for a while; in a blizzard they come thick and nearly sideways, and a
 *  white haze swallows the view. Spawned by the region while it snows and a player is inside; follows the middle of the players.
 */
UCLASS()
class AVaelWeatherSnow : public AActor
{
	GENERATED_BODY()

public:

	AVaelWeatherSnow();

	static AVaelWeatherSnow* Spawn(AVaelRegion* InRegion);

	virtual void Tick(float DeltaSeconds) override;

private:

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UVaelFlakesComponent> Flakes;

	/** White haze of a blizzard around the players */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<ULocalFogVolumeComponent> Whiteout;

	UPROPERTY(Transient)
	TObjectPtr<AVaelRegion> Region;

	/** Flakes owed from the last frames */
	float Credit = 0.0f;
	float ShownWhiteout = 0.0f;
};
