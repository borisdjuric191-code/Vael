// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VaelQuest.generated.h"

/** What a step of a quest asks for */
UENUM(BlueprintType)
enum class EVaelQuestObjective : uint8
{
	/** Talk to a person: Target is the name of their dialogue asset, like DA_Dialogue_Edda */
	Talk,
	/** Defeat creatures: Target is the kind of creature, like Glutkriecher */
	Kill,
	/** Reach a place: Target is the id of a quest marker in the level */
	Reach,
	/** Pick or mine: Target is the compendium id of the plant, fungus or stone; none counts everything */
	Gather,
	/** Cast formulas: Target is the asset name of a formula; none counts every formula */
	Cast,
	/** Something happens in the world, like a Mark source being sealed: Target is the name of the event */
	Event
};

/** One step of a quest */
USTRUCT(BlueprintType)
struct FVaelQuestStep
{
	GENERATED_BODY()

	/** What the players are asked, shown in the tracker, like "Säubert die Felder" */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
	FText Objective;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
	EVaelQuestObjective Type = EVaelQuestObjective::Talk;

	/** Who or what the step is about, see the objective types */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
	FName Target;

	/** How many it takes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest", meta = (ClampMin = 1))
	int32 Count = 1;

	/** For a talk: what the person says. Empty: their first talk, or their usual hints */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest", meta = (MultiLine = true))
	TArray<FText> Lines;
};

/**
 *  A quest of the group: steps done one after the other, given by a person.
 *  A quest becomes active by itself as soon as the quest it follows is done; one without a predecessor is active from the start.
 */
UCLASS(BlueprintType)
class UVaelQuest : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** Type of the asset manager under which quests are found */
	static const FPrimaryAssetType PrimaryAssetType;

	/** Id other quests refer to, like "Q1_NachDemFieber" */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest")
	FName QuestId;

	/** Title shown to the players */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest")
	FText Title;

	/** Who gives it, like "Edda" */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest")
	FText GiverName;

	/** What it is about, for the quest page */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest", meta = (MultiLine = true))
	FText Summary;

	/** Part of the main story, shown first */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest")
	bool bMainQuest = false;

	/** Quest that has to be done first; none: active from the start */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest")
	FName Prerequisite;

	/** The steps in order */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest")
	TArray<FVaelQuestStep> Steps;

	/** Order among quests, lower first */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest")
	int32 SortOrder = 0;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
