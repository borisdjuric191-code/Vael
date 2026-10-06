// Copyright Epic Games, Inc. All Rights Reserved.

#include "Story/VaelDialogue.h"
#include "Creatures/VaelEmberQueen.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Magic/VaelFormula.h"
#include "Magic/VaelFormulaScroll.h"
#include "Magic/VaelGrimoireSubsystem.h"
#include "World/VaelMarkSource.h"

namespace
{
	/** Below this many formulas found by experimenting the players get the hint to try pairs */
	constexpr int32 FewDiscoveredFormulas = 3;

	bool IsConditionMet(const UWorld* World, EVaelHintCondition Condition)
	{
		switch (Condition)
		{
		case EVaelHintCondition::FewFormulasDiscovered:
		{
			const UGameInstance* GameInstance = World->GetGameInstance();
			const UVaelGrimoireSubsystem* Grimoire = GameInstance != nullptr ? GameInstance->GetSubsystem<UVaelGrimoireSubsystem>() : nullptr;
			if (Grimoire == nullptr)
			{
				return false;
			}

			int32 NumDiscovered = 0;
			for (const UVaelFormula* Formula : Grimoire->GetKnownFormulas())
			{
				NumDiscovered += Formula->Source == EVaelFormulaSource::Free ? 1 : 0;
			}
			return NumDiscovered < FewDiscoveredFormulas;
		}

		case EVaelHintCondition::ScrollsLeft:
			return TActorIterator<AVaelFormulaScroll>(World) ? true : false;

		case EVaelHintCondition::MarkSourceOpen:
			for (TActorIterator<AVaelMarkSource> It(World); It; ++It)
			{
				if (!It->IsSealed())
				{
					return true;
				}
			}
			return false;

		case EVaelHintCondition::BossAlive:
		case EVaelHintCondition::BossDefeated:
		{
			bool bAlive = false;
			for (TActorIterator<AVaelEmberQueen> It(World); It; ++It)
			{
				bAlive |= !It->IsDead();
			}
			return Condition == EVaelHintCondition::BossAlive ? bAlive : !bAlive;
		}

		default:
			return true;
		}
	}
}

TArray<FText> UVaelDialogue::GatherHints(const UWorld* World) const
{
	TArray<FText> Lines;
	if (World == nullptr)
	{
		return Lines;
	}

	for (const FVaelDialogueHint& Hint : Hints)
	{
		if (Lines.Num() >= MaxHintsPerTalk)
		{
			break;
		}

		if (IsConditionMet(World, Hint.Condition))
		{
			Lines.Add(Hint.Line);
		}
	}

	return Lines;
}
