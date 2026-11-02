// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelAshRain.generated.h"

class UPointLightComponent;
class UVaelCloudCardsComponent;
class UVaelFlakesComponent;

/**
 *  The look of the Ascheregen (Boris 2026-11-02): a low black cloud gathers over the patch and glimmers from inside like
 *  covered embers; soot flakes flutter down slowly, glowing embers fall faster and cool on the way. Where an ember lands
 *  still hot the ground flares up, now and then into a small fire. A grey haze lies under the cloud (it blinds), the flakes
 *  pile into a layer of ash that stays a little after the rain and fades. The area of the spell does the damage and blinding.
 */
UCLASS()
class AVaelAshRain : public AActor
{
	GENERATED_BODY()

	/** Embers glimmering inside the cloud */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPointLightComponent> Glimmer;

public:

	AVaelAshRain();

	/** Lets ash rain over a patch of this radius around the ground point for this many seconds; fires it lights hurt the enemies of the instigator */
	static AVaelAshRain* Spawn(UWorld* World, const FVector& Ground, float InRadius, float InSeconds, float InFireDamagePerSecond, APawn* InInstigator);

	/** Stops the rain early, the cloud thins and the ash fades */
	void Stop();

	virtual void Tick(float DeltaSeconds) override;

private:

	void Build();

	/** Lets one flake fall from the cloud onto the patch, from a height between the cloud and the ground */
	void Drop(bool bEmber, float HeightShare);

	/** An ember landed hot: the ground flares and, now and then, a small fire starts */
	void OnEmberLanded(const FVector& Ground);

	UPROPERTY(Transient)
	TObjectPtr<UVaelCloudCardsComponent> Cloud;

	UPROPERTY(Transient)
	TObjectPtr<UVaelCloudCardsComponent> Haze;

	UPROPERTY(Transient)
	TObjectPtr<UVaelFlakesComponent> Flakes;

	TArray<FVector> CloudPlaces;
	TArray<float> CloudSizes;
	TArray<FVector> HazePlaces;
	TArray<float> HazeSizes;

	/** Fires lit so far */
	TArray<FVector> FireSpots;

	float Radius = 400.0f;
	float Seconds = 6.0f;
	float FireDamagePerSecond = 0.0f;
	float Age = 0.0f;
	float SootCredit = 0.0f;
	float EmberCredit = 0.0f;
	bool bRaining = false;
};
