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

	// Order of the grimoire: Mark last, then by how the formula is found, then the simpler ones first
	Formulas.Sort([](const UVaelFormula& A, const UVaelFormula& B)
	{
		const bool bMarkA = A.Elements.Contains(EVaelElement::Mark);
		const bool bMarkB = B.Elements.Contains(EVaelElement::Mark);
		if (bMarkA != bMarkB)
		{
			return bMarkB;
		}

		if (A.Source != B.Source)
		{
			return A.Source < B.Source;
		}

		if (A.Elements.Num() != B.Elements.Num())
		{
			return A.Elements.Num() < B.Elements.Num();
		}

		const int32 NumDistinctA = TSet<EVaelElement>(A.Elements).Num();
		const int32 NumDistinctB = TSet<EVaelElement>(B.Elements).Num();
		if (NumDistinctA != NumDistinctB)
		{
			return NumDistinctA < NumDistinctB;
		}

		return A.GetComboKey() < B.GetComboKey();
	});

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

bool UVaelGrimoireSubsystem::LearnFormula(UVaelFormula* Formula, const FText& Reason)
{
	if (Formula == nullptr || !Formulas.Contains(Formula) || KnownFormulas.Contains(Formula))
	{
		return false;
	}

	KnownFormulas.Add(Formula);
	OnFormulaLearned.Broadcast(Formula, Reason);

	return true;
}

bool UVaelGrimoireSubsystem::AwakenMark(const FText& Reason)
{
	if (bMarkAwakened)
	{
		return false;
	}

	bMarkAwakened = true;
	UE_LOG(LogVael, Log, TEXT("The Mark awakens in the group"));

	// Formulas that come with the Mark itself are known from now on
	for (UVaelFormula* Formula : Formulas)
	{
		if (Formula->Source == EVaelFormulaSource::Mark)
		{
			LearnFormula(Formula, Reason);
		}
	}

	return true;
}

bool UVaelGrimoireSubsystem::IsBlockedByMark(const UVaelFormula* Formula) const
{
	return !bMarkAwakened && Formula != nullptr && Formula->Elements.Contains(EVaelElement::Mark);
}

void UVaelGrimoireSubsystem::AddEcho(const UVaelFormula* Formula)
{
	if (Formula != nullptr)
	{
		EchoedFormulas.Add(const_cast<UVaelFormula*>(Formula));
	}
}

bool UVaelGrimoireSubsystem::HasEcho(const UVaelFormula* Formula) const
{
	return Formula != nullptr && EchoedFormulas.Contains(const_cast<UVaelFormula*>(Formula));
}
