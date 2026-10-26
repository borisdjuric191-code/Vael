// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelLocalWeather.generated.h"

class AVaelWeatherRain;

/** Weathers that break out where heat, moisture and cold have gathered in the air (UVaelAtmosphere) */
UENUM(BlueprintType)
enum class EVaelLocalWeather : uint8
{
	/** Much rising steam: a shower of rain */
	Shower,
	/** Steam and heat together: rain, gusts and lightning */
	Thunderstorm,
	/** Much heat over dry ground: the air rises and wind pulls in from all sides */
	Thermal,
	/** Heat and cold side by side: cold air rushes towards the heat and fans the fires */
	WindFront,
	/** Cold and moisture: a bank of fog on the ground */
	ColdFog,
	/** Much cold and moisture: a short burst of graupel */
	Graupel
};

/**
 *  A weather of its own over a round patch for half a minute or so, broken out of the air (UVaelAtmosphere).
 *  It rains, blows, strikes and fogs only inside; the regional weather goes on around it.
 */
UCLASS()
class AVaelLocalWeather : public AActor
{
	GENERATED_BODY()

public:

	AVaelLocalWeather();

	/** Lets a weather break out over a patch; Direction is where a wind front blows towards */
	static AVaelLocalWeather* Spawn(UWorld* World, EVaelLocalWeather InKind, const FVector& Center, float InRadius, float InLifetime, const FVector& Direction);

	virtual void Tick(float DeltaSeconds) override;

	EVaelLocalWeather GetKind() const { return Kind; }
	float GetRadius() const { return Radius; }

	/** True if the location lies inside */
	bool Covers(const FVector& Location) const;

	/** How strong it is right now, 0 to 1: rising at the start, dying down at the end */
	float GetStrength() const;

	/** Wind of the local weathers at a location in cm/s, zero where none blows */
	static FVector GetWindAt(const UWorld* World, const FVector& Location);

	/** True if a local shower or thunderstorm rains on the location */
	static bool IsRainingAt(const UWorld* World, const FVector& Location);

	/** Name shown to the players */
	static FText GetDisplayName(EVaelLocalWeather InKind);

protected:

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Wind of this weather at a location */
	FVector GetOwnWind(const FVector& Location) const;

	/** Rain on everybody inside: wet, puddles, fires die */
	void TickRain(float DeltaSeconds);

	/** Lightning near the players inside */
	void TickLightning(float DeltaSeconds);

	/** Fires inside grow in the wind front */
	void TickFans(float DeltaSeconds);

	/** Graupel falls in volleys near the players inside */
	void TickGraupel(float DeltaSeconds);

	/** Lays the fog banks */
	void LayFog();

	EVaelLocalWeather Kind = EVaelLocalWeather::Shower;
	float Radius = 3500.0f;
	float Lifetime = 30.0f;
	float Age = 0.0f;
	FVector Direction = FVector::ForwardVector;

	float RainStepTime = 0.0f;
	float LightningTime = 0.0f;
	float FanTime = 0.0f;
	float GraupelTime = 0.0f;

	/** Graupel lances on their way down: where they land and when */
	TArray<FVector> GraupelPoints;
	TArray<float> GraupelTimes;

	TWeakObjectPtr<AVaelWeatherRain> RainLook;
};
