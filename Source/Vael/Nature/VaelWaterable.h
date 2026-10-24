// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "VaelWaterable.generated.h"

UINTERFACE(MinimalAPI)
class UVaelWaterable : public UInterface
{
	GENERATED_BODY()
};

/**
 *  A plant that can be watered, like with the water of the Taublatt arm (Boris 2026-10-24: its water does not heal anyone,
 *  it waters plants and is a water source for the garden).
 */
class IVaelWaterable
{
	GENERATED_BODY()

public:

	/** Gets water; Amount is about 1 for a full Tauperle, a jet gives it a little every frame */
	virtual void ReceiveWater(float Amount, AActor* Giver) = 0;
};

namespace VaelWatering
{
	/** Plants add themselves in BeginPlay and leave in EndPlay, so watering never walks all actors of the level */
	void Register(AActor* Plant);
	void Unregister(AActor* Plant);

	/** Waters every plant within the radius around the location; returns how many got water */
	int32 WaterAround(UWorld* World, const FVector& Location, float Radius, float Amount, AActor* Giver);

	/** Waters every plant along the line from Start to End within the half width; returns how many got water */
	int32 WaterAlong(UWorld* World, const FVector& Start, const FVector& End, float HalfWidth, float Amount, AActor* Giver);
}
