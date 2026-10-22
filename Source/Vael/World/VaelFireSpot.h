// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/VaelFireComponent.h"
#include "VaelFireSpot.generated.h"

/**
 *  A fire on its own: placed in a level for campfires, torches, braziers and other fire lights, or spawned where fire
 *  lands, on the ground or on a wall. Lives for a while (or for ever), then dies down and leaves its scorch mark to cool.
 */
UCLASS()
class AVaelFireSpot : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UVaelFireComponent> Fire;

public:

	/** Constructor */
	AVaelFireSpot();

	/** Shape of a placed fire: a campfire by default */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fire")
	FVaelFireShape Shape;

	/** Seconds it burns, 0 for ever (placed fires) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fire", meta = (ClampMin = 0))
	float BurnTime = 0.0f;

	/** Lights a fire where something burning landed; a wall normal makes it a fire on the wall */
	static AVaelFireSpot* Spawn(UWorld* World, const FVector& Location, const FVaelFireShape& InShape, float InBurnTime);

	/** A blast of flames where something burning explodes: they bloom out wide in a moment and are gone */
	static AVaelFireSpot* SpawnBurst(UWorld* World, const FVector& Location, float Radius);

	/** Lets it die down now */
	void PutOut(float FadeOut = 0.8f);

	UVaelFireComponent* GetFire() const { return Fire; }

protected:

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:

	float Age = 0.0f;
	bool bPutOut = false;

	/** A blast: blooms out over its burn time instead of burning steadily */
	bool bBurst = false;
};
