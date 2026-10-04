// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelGrimoireSubsystem.h"
#include "Engine/AssetManager.h"
#include "Magic/VaelFormula.h"
#include "Vael.h"

void UVaelGrimoireSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	LoadFormulas();
}

void UVaelGrimoireSubsystem::LoadFormulas()
{
	UAssetManager& AssetManager = UAssetManager::Get();

	TArray<FPrimaryAssetId> FormulaIds;
	AssetManager.GetPrimaryAssetIdList(UVaelFormula::PrimaryAssetType, FormulaIds);

	for (const FPrimaryAssetId& FormulaId : FormulaIds)
	{
		UVaelFormula* Formula = Cast<UVaelFormula>(AssetManager.GetPrimaryAssetPath(FormulaId).TryLoad());
		if (Formula == nullptr || Formula->Elements.IsEmpty())
		{
			UE_LOG(LogVael, Warning, TEXT("Formula '%s' could not be loaded or has no elements"), *FormulaId.ToString());
			continue;
		}

		const uint32 ComboKey = Formula->GetComboKey();
		if (const TObjectPtr<UVaelFormula>* Existing = FormulasByCombo.Find(ComboKey))
		{
			UE_LOG(LogVael, Warning, TEXT("Formulas '%s' and '%s' use the same elements, the second one is ignored"), *GetNameSafe(*Existing), *GetNameSafe(Formula));
			continue;
		}

		Formulas.Add(Formula);
		FormulasByCombo.Add(ComboKey, Formula);

		if (Formula->Source == EVaelFormulaSource::Start)
		{
			KnownFormulas.Add(Formula);
		}
	}

	if (Formulas.IsEmpty())
	{
		UE_LOG(LogVael, Warning, TEXT("The grimoire found no formulas. Formula assets belong in /Game/Vael/Magic/Formulas."));
	}
	else
	{
		UE_LOG(LogVael, Log, TEXT("Grimoire loaded %d formulas, %d known from the start"), Formulas.Num(), KnownFormulas.Num());
	}
}

UVaelFormula* UVaelGrimoireSubsystem::FindFormula(TConstArrayView<EVaelElement> Elements) const
{
	const TObjectPtr<UVaelFormula>* Formula = FormulasByCombo.Find(VaelElements::MakeComboKey(Elements));
	return Formula != nullptr ? Formula->Get() : nullptr;
}

bool UVaelGrimoireSubsystem::IsFormulaKnown(const UVaelFormula* Formula) const
{
	return Formula != nullptr && KnownFormulas.Contains(Formula);
}

bool UVaelGrimoireSubsystem::LearnFormula(UVaelFormula* Formula)
{
	if (Formula == nullptr || !Formulas.Contains(Formula) || KnownFormulas.Contains(Formula))
	{
		return false;
	}

	KnownFormulas.Add(Formula);
	OnFormulaLearned.Broadcast(Formula);

	return true;
}
