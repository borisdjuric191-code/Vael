// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelAshDrift.generated.h"

class UVaelFlakesComponent;

/**
 *  Ash that the wind lifts off the ground of the Aschenmark (goal 8.11. evening): every few seconds, somewhere a little
 *  away from a player, a small gust runs over the ground: a low trail of ash dust drifting along, grey flakes skating
 *  and tumbling with it. With wind weather the gusts follow the wind of the region and come more often; otherwise a
 *  light breeze from a slowly turning direction. Placed once in an open world (not streamed); does nothing elsewhere.
 */
UCLASS()
class AVaelAshDrift : public AActor
{
	GENERATED_BODY()

public:

	AVaelAshDrift();

	virtual void Tick(float DeltaSeconds) override;

	/** A gust right now near the first player (console: VaelAschenboe) */
	void GustNow();

protected:

	/** Seconds between gusts near a player, without and with wind weather */
	UPROPERTY(EditAnywhere, Category="Ash")
	FVector2D CalmInterval = FVector2D(2.5f, 6.0f);

	UPROPERTY(EditAnywhere, Category="Ash")
	FVector2D WindyInterval = FVector2D(0.8f, 2.0f);

	/** Where a gust starts: this far from the player (cm), in front of the camera so it is seen */
	UPROPERTY(EditAnywhere, Category="Ash")
	FVector2D StartDistance = FVector2D(600.0f, 1800.0f);

	/** How far a gust runs (cm) and how fast (cm/s) */
	UPROPERTY(EditAnywhere, Category="Ash")
	FVector2D RunLength = FVector2D(900.0f, 1800.0f);

	UPROPERTY(EditAnywhere, Category="Ash")
	float RunSpeed = 520.0f;

	/** Colour of the lifted ash */
	UPROPERTY(EditAnywhere, Category="Ash")
	FLinearColor AshColor = FLinearColor(0.42f, 0.39f, 0.36f);

private:

	struct FGust
	{
		FVector Location = FVector::ZeroVector;
		FVector Direction = FVector::ForwardVector;
		float Left = 0.0f;
		float NextPuff = 0.0f;
		float Width = 150.0f;
	};

	/** Starts a gust near a player */
	void StartGust(const FVector& PlayerLocation, bool bWindy);

	/** The direction the ash drifts: the wind of the region, or the slowly turning breeze */
	FVector DriftDirection(const FVector& Location, bool& bOutWindy) const;

	UPROPERTY()
	TObjectPtr<UVaelFlakesComponent> Flakes;

	TArray<FGust> Gusts;
	float NextGust = 1.0f;
	float BreezeAngle = 0.0f;
};
