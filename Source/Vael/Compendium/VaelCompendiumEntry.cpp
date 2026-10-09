// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compendium/VaelCompendiumEntry.h"

const FPrimaryAssetType UVaelCompendiumEntry::PrimaryAssetType(TEXT("VaelCompendiumEntry"));

FPrimaryAssetId UVaelCompendiumEntry::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(PrimaryAssetType, GetFName());
}
