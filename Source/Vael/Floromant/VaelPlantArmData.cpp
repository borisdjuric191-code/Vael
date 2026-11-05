// Copyright Epic Games, Inc. All Rights Reserved.

#include "Floromant/VaelPlantArmData.h"
#include "Floromant/VaelPlantData.h"
#include "VaelAssets.h"
#include "Engine/Texture.h"

FText UVaelPlantArmData::GetDisplayName(bool bShadow) const
{
	const UVaelPlantData* PlantData = VaelAssets::LoadOptional(Plant);
	return PlantData != nullptr ? PlantData->GetName(bShadow) : FText::FromString(GetName());
}

FLinearColor UVaelPlantArmData::GetColor(bool bShadow) const
{
	const UVaelPlantData* PlantData = VaelAssets::LoadOptional(Plant);
	return PlantData == nullptr ? FLinearColor(0.4f, 0.95f, 1.0f) : bShadow && !PlantData->bLightOnly ? PlantData->ShadowColor : PlantData->Color;
}

bool UVaelPlantArmData::IsLightOnly() const
{
	const UVaelPlantData* PlantData = VaelAssets::LoadOptional(Plant);
	return PlantData != nullptr && PlantData->bLightOnly;
}

UTexture* UVaelPlantArmData::LoadShadowTexture() const
{
	if (!ShadowTexture.IsNull())
	{
		return VaelAssets::LoadOptional(ShadowTexture);
	}
	// By name next to the plant arm: DA_Pflanzenarm_<Id> -> /Game/Vael/Characters/Floromant/Pflanzenarm/<Id>/T_<Id>_Schatten
	FString Id = GetName();
	Id.RemoveFromStart(TEXT("DA_Pflanzenarm_"));
	const FString Path = FString::Printf(TEXT("/Game/Vael/Characters/Floromant/Pflanzenarm/%s/T_%s_Schatten.T_%s_Schatten"), *Id, *Id, *Id);
	return LoadObject<UTexture>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
}

const TArray<FVaelGeneTier>& UVaelPlantArmData::GetGeneTiers() const
{
	return GeneTiers.IsEmpty() ? GetDefaultGeneTiers() : GeneTiers;
}

const TArray<FVaelGeneTier>& UVaelPlantArmData::GetDefaultGeneTiers()
{
	static TArray<FVaelGeneTier> Tiers;
	if (Tiers.IsEmpty())
	{
		auto Gene = [](const TCHAR* Key, const TCHAR* Name, const TCHAR* Key2, const TCHAR* Description, EVaelGeneEffect Effect, float Amount)
		{
			FVaelGene Result;
			Result.Name = FText::AsLocalizable_Advanced(TEXT("VaelGenes"), Key, Name);
			Result.Description = FText::AsLocalizable_Advanced(TEXT("VaelGenes"), Key2, Description);
			Result.Effect = Effect;
			Result.Amount = Amount;
			return Result;
		};
		FVaelGeneTier& First = Tiers.AddDefaulted_GetRef();
		First.Options.Add(Gene(TEXT("Kraeftig"), TEXT("Kräftiger Wuchs"), TEXT("KraeftigText"), TEXT("+15 % Schaden"), EVaelGeneEffect::Damage, 0.15f));
		First.Options.Add(Gene(TEXT("Sparsam"), TEXT("Sparsamer Saft"), TEXT("SparsamText"), TEXT("Schüsse kosten 20 % weniger Saft"), EVaelGeneEffect::SaftCost, 0.2f));
		FVaelGeneTier& Second = Tiers.AddDefaulted_GetRef();
		Second.Options.Add(Gene(TEXT("Trieb"), TEXT("Schneller Trieb"), TEXT("TriebText"), TEXT("+15 % Feuertempo"), EVaelGeneEffect::FireSpeed, 0.15f));
		Second.Options.Add(Gene(TEXT("Wurzel"), TEXT("Tiefe Wurzel"), TEXT("WurzelText"), TEXT("+12 % Schaden"), EVaelGeneEffect::Damage, 0.12f));
		FVaelGeneTier& Third = Tiers.AddDefaulted_GetRef();
		Third.Options.Add(Gene(TEXT("Bluete"), TEXT("Volle Blüte"), TEXT("BlueteText"), TEXT("+25 % Schaden"), EVaelGeneEffect::Damage, 0.25f));
		Third.Options.Add(Gene(TEXT("Nidscheu"), TEXT("Nidscheu"), TEXT("NidscheuText"), TEXT("Die Schattenform trinkt 40 % weniger Nid"), EVaelGeneEffect::NidPrice, 0.4f));
	}
	return Tiers;
}
