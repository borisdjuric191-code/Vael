// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelWeatherWind.generated.h"

class AVaelRegion;
class UVaelCloudCardsComponent;
class UVaelWindStreaksComponent;

/**
 *  The look of the wind weather (and the wind of a storm) around the players: dark streaks racing over the land,
 *  ash and sand torn off the ground drifting downwind. Strong and many in the gusts, few in the lulls.
 *  Spawned by the region while its wind blows and a player is inside; it follows the middle of the players.
 */
UCLASS()
class AVaelWeatherWind : public AActor
{
	GENERATED_BODY()

public:

	AVaelWeatherWind();

	/** Lets the wind of the region show around its players */
	static AVaelWeatherWind* Spawn(AVaelRegion* InRegion);

	virtual void Tick(float DeltaSeconds) override;

private:

	/** One streak or card blown over the land: where it is, how long it has been on its way and how long it lasts */
	struct FGust
	{
		FVector Place = FVector::ZeroVector;
		float Age = 0.0f;
		float Life = 2.0f;
		float Size = 100.0f;
		float Height = 60.0f;
		float Wobble = 0.0f;
	};

	void Build();

	/** Starts a gust anew upwind of the players, on the ground there */
	void Restart(FGust& Gust, const FVector& Middle, const FVector& Direction, float MinLife, float MaxLife);

	/** Middle of the players in the region; false if none is there */
	bool FindMiddle(FVector& OutMiddle) const;

	TWeakObjectPtr<AVaelRegion> Region;

	UPROPERTY(Transient)
	TObjectPtr<UVaelWindStreaksComponent> Streaks;

	UPROPERTY(Transient)
	TObjectPtr<UVaelCloudCardsComponent> Ash;

	UPROPERTY(Transient)
	TObjectPtr<UVaelCloudCardsComponent> Sand;

	TArray<FGust> StreakGusts;
	TArray<FGust> AshGusts;
	TArray<FGust> SandGusts;
};
