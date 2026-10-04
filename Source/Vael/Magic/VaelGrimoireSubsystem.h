// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Magic/VaelElementTypes.h"
#include "VaelGrimoireSubsystem.generated.h"

class UVaelFormula;

DECLARE_MULTICAST_DELEGATE_OneParam(FVaelOnFormulaLearned, const UVaelFormula* /*Formula*/);

/**
 *  The grimoire of the group: knows every formula of the game and which of them the players have learned.
 *  All players share one grimoire.
 */
UCLASS()
class UVaelGrimoireSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	//~Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	//~End USubsystem

	/** Returns the formula for a combination of elements in any order, or null if there is none */
	UVaelFormula* FindFormula(TConstArrayView<EVaelElement> Elements) const;

	/** True if the players have learned the formula */
	bool IsFormulaKnown(const UVaelFormula* Formula) const;

	/** Adds a formula to the grimoire. Returns false if it was already known. */
	bool LearnFormula(UVaelFormula* Formula);

	/** Every formula of the game */
	const TArray<TObjectPtr<UVaelFormula>>& GetAllFormulas() const { return Formulas; }

	/** Formulas the players have learned */
	const TArray<TObjectPtr<UVaelFormula>>& GetKnownFormulas() const { return KnownFormulas; }

	/** Called when the players learn a formula */
	FVaelOnFormulaLearned OnFormulaLearned;

private:

	/** Loads all formula assets through the asset manager */
	void LoadFormulas();

	/** Every formula of the game */
	UPROPERTY()
	TArray<TObjectPtr<UVaelFormula>> Formulas;

	/** Formulas the players have learned */
	UPROPERTY()
	TArray<TObjectPtr<UVaelFormula>> KnownFormulas;

	/** Formulas by the key of their element combination */
	UPROPERTY()
	TMap<uint32, TObjectPtr<UVaelFormula>> FormulasByCombo;
};
