// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class APawn;

namespace VaelTravel
{
	/**
	 *  Puts a pawn on the ground at a world place (X, Y in cm), down from high above onto whatever is there.
	 *  In the open world the ground there may not be loaded yet: the pawn then waits up in the air, held still,
	 *  until it has streamed in (at most 15 s).
	 */
	void Land(APawn* Pawn, const FVector2D& Place);
}
