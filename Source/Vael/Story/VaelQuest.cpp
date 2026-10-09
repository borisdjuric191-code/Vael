// Copyright Epic Games, Inc. All Rights Reserved.

#include "Story/VaelQuest.h"

const FPrimaryAssetType UVaelQuest::PrimaryAssetType(TEXT("VaelQuest"));

FPrimaryAssetId UVaelQuest::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(PrimaryAssetType, GetFName());
}
