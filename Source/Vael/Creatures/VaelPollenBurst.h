// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelPollenBurst.generated.h"

class UVaelCloudCardsComponent;

/**
 *  A burst of seed down and pollen, made of the soft cloud cards of the mist kit: puffs swell outwards and upwards
 *  from a point, the main colour with some puffs of a second one, and drift apart and fade within a couple of seconds.
 *  The Distelspinne bursts in one when it sets free its seed spiders (Boris 2026-10-28). Removes itself.
 */
UCLASS()
class VAEL_API AVaelPollenBurst : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UVaelCloudCardsComponent> Main;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UVaelCloudCardsComponent> Accent;

public:

	AVaelPollenBurst();

	/** Bursts at a location: the cloud reaches about Radius (cm) and lasts Lifetime seconds */
	static AVaelPollenBurst* Spawn(UWorld* World, const FVector& Location, float Radius, const FLinearColor& Color, const FLinearColor& AccentColor, float Lifetime = 1.8f);

	virtual void Tick(float DeltaSeconds) override;

private:

	struct FPuff
	{
		FVector Direction = FVector::UpVector;
		float Reach = 100.0f;
		float Size = 100.0f;
		float Delay = 0.0f;
		bool bAccent = false;
		int32 Card = 0;
	};

	void Setup(float InRadius, const FLinearColor& Color, const FLinearColor& AccentColor, float InLifetime);

	TArray<FPuff> Puffs;
	float Radius = 300.0f;
	float Lifetime = 1.8f;
	float Age = 0.0f;
};
