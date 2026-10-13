// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;
class UWorld;
struct FHitResult;

namespace VaelGround
{
	/**
	 *  Finds the ground along a line, usually from a point downwards: the first surface characters can stand on,
	 *  static or movable alike. Spells, pickups and characters along the line are skipped. Returns false if there is none.
	 */
	bool TraceGround(const UWorld* World, const FVector& Start, const FVector& End, FHitResult& OutHit, const AActor* IgnoredActor = nullptr);

	/** Keeps what is painted on the ground, like the green of the seed garden, off everything the actor is made of right now */
	void RefuseDecals(AActor* Actor);
}
