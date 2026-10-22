// Copyright Epic Games, Inc. All Rights Reserved.

#include "Floromant/VaelPlantArmData.h"
#include "Floromant/VaelPlantData.h"
#include "VaelAssets.h"

FText UVaelPlantArmData::GetDisplayName(bool bShadow) const
{
	const UVaelPlantData* PlantData = VaelAssets::LoadOptional(Plant);
	return PlantData != nullptr ? PlantData->GetName(bShadow) : FText::FromString(GetName());
}

FLinearColor UVaelPlantArmData::GetColor(bool bShadow) const
{
	const UVaelPlantData* PlantData = VaelAssets::LoadOptional(Plant);
	return PlantData == nullptr ? FLinearColor(0.4f, 0.95f, 1.0f) : bShadow ? PlantData->ShadowColor : PlantData->Color;
}
