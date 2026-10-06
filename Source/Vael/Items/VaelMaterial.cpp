// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/VaelMaterial.h"

int32 FVaelLootEntry::Roll() const
{
	if (Material.IsNull() || FMath::FRand() >= Chance)
	{
		return 0;
	}

	return FMath::RandRange(MinCount, FMath::Max(MinCount, MaxCount));
}
