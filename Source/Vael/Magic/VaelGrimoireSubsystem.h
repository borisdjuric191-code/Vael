// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Magic/VaelElementTypes.h"
#include "VaelGrimoireSubsystem.generated.h"

class UVaelFormula;

DECLARE_MULTICAST_DELEGATE_TwoParams(FVaelOnFormulaLearned, const UVaelFormula* /*Formula*/, const FText& /*Reason*/);

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

	/** Adds a formula to the grimoire. Returns false if it was already known. The reason is shown to the players. */
	bool LearnFormula(UVaelFormula* Formula, const FText& Reason = FText::GetEmpty());

	/** Remembers that the players heard the echo of a sealed formula, which reveals its hint in the grimoire */
	void AddEcho(const UVaelFormula* Formula);

	/** True if the players have heard the echo of the formula */
	bool HasEcho(const UVaelFormula* Formula) const;

	/** Every formula of the game, in the order of the grimoire */
	const TArray<TObjectPtr<UVaelFormula>>& GetAllFormulas() const { return Formulas; }

	/** Formulas the players have learned */
	const TArray<TObjectPtr<UVaelFormula>>& GetKnownFormulas() const { return KnownFormulas; }

	/** Called when the players learn a formula */
	FVaelOnFormulaLearned OnFormulaLearned;

	/** True once the Mark has awakened in the group: the fifth element can be chosen (Act III) */
	bool IsMarkAwakened() const { return bMarkAwakened; }

	/** Awakens the Mark for the whole group and teaches the formulas that come with it. Returns false if it was awake already. */
	bool AwakenMark(const FText& Reason);

	/** True if the formula uses the Mark while it still sleeps, so it can't be cast */
	bool IsBlockedByMark(const UVaelFormula* Formula) const;

private:

	/** Loads all formula assets through the asset manager */
	void LoadFormulas();

	/** Every formula of the game */
	UPROPERTY()
	TArray<TObjectPtr<UVaelFormula>> Formulas;

	/** Formulas the players have learned */
	UPROPERTY()
	TArray<TObjectPtr<UVaelFormula>> KnownFormulas;

	/** Sealed formulas whose echo the players have heard */
	UPROPERTY()
	TSet<TObjectPtr<UVaelFormula>> EchoedFormulas;

	/** True once the Mark has awakened */
	bool bMarkAwakened = false;

	/** Formulas by the key of their element combination */
	UPROPERTY()
	TMap<uint32, TObjectPtr<UVaelFormula>> FormulasByCombo;
};
