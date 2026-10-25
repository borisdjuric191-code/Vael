// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelHailStorm.generated.h"

class UInstancedStaticMeshComponent;
class UVaelCloudCardsComponent;

/**
 *  The look of the Hagelsturm: a dark storm cloud gathers over the patch, lances of ice rain down from it all over,
 *  and a volley lands on each point a strike of the spell hits, just as it hits (the strikes, AVaelGroundStrike with
 *  the Hail look, do the damage and shatter the ice). Gone shortly after the last strike.
 */
UCLASS()
class AVaelHailStorm : public AActor
{
	GENERATED_BODY()

public:

	AVaelHailStorm();

	/** Lets hail fall over a patch of this radius around the ground point; Points and Delays are the strikes and their seconds */
	static AVaelHailStorm* Spawn(AActor* Caster, const FVector& Center, float InRadius, const TArray<FVector>& Points, const TArray<float>& Delays);

	virtual void Tick(float DeltaSeconds) override;

private:

	void Build();

	/** Lets a lance of ice fall so it lands on the ground point */
	void Drop(const FVector& Landing);

	struct FStone
	{
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float GroundZ = 0.0f;
		float Length = 40.0f;
		bool bActive = false;
	};

	UPROPERTY(Transient)
	TObjectPtr<UVaelCloudCardsComponent> Cloud;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Lances;

	TArray<FStone> Stones;
	TArray<FTransform> StoneTransforms;
	TArray<FVector> CloudPlaces;
	TArray<float> CloudSizes;
	TArray<FVector> StrikePoints;
	TArray<float> StrikeDelays;
	TArray<bool> VolleysDone;

	float Radius = 400.0f;
	float Age = 0.0f;
	float LastStrike = 0.0f;
	float DropCredit = 0.0f;
	int32 NumLanded = 0;
};
