// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VaelDialogue.generated.h"

/** When a hint of a character is worth saying */
UENUM(BlueprintType)
enum class EVaelHintCondition : uint8
{
	/** Always */
	Always,
	/** The players have discovered few formulas by experimenting yet */
	FewFormulasDiscovered,
	/** Scrolls still lie in the level */
	ScrollsLeft,
	/** A Mark source in the level is still open */
	MarkSourceOpen,
	/** The boss of the region still lives */
	BossAlive,
	/** The boss of the region has fallen */
	BossDefeated
};

/** A hint a character gives on later talks, if its condition holds */
USTRUCT(BlueprintType)
struct FVaelDialogueHint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue")
	EVaelHintCondition Condition = EVaelHintCondition::Always;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue", meta = (MultiLine = true))
	FText Line;
};

/**
 *  What a character says: the intro the first time, afterwards the hints that fit the state of the world.
 */
UCLASS(BlueprintType)
class UVaelDialogue : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** Name shown over the lines */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dialogue")
	FText SpeakerName;

	/** Lines of the first talk */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dialogue", meta = (MultiLine = true))
	TArray<FText> IntroLines;

	/** Hints of later talks, in order; those whose condition holds are said */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dialogue")
	TArray<FVaelDialogueHint> Hints;

	/** Most hints said in one talk */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dialogue", meta = (ClampMin = 1))
	int32 MaxHintsPerTalk = 3;

	/** Hints whose condition holds right now, at most MaxHintsPerTalk */
	TArray<FText> GatherHints(const UWorld* World) const;
};
