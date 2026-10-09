// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Story/VaelQuest.h"
#include "VaelQuestSubsystem.generated.h"

/** How far the group has come with one quest */
USTRUCT(BlueprintType)
struct FVaelQuestProgress
{
	GENERATED_BODY()

	/** True once its predecessor is done */
	UPROPERTY(BlueprintReadOnly, Category="Quest")
	bool bActive = false;

	UPROPERTY(BlueprintReadOnly, Category="Quest")
	bool bDone = false;

	/** Index of the current step */
	UPROPERTY(BlueprintReadOnly, Category="Quest")
	int32 Step = 0;

	/** How many of the current step's count are done */
	UPROPERTY(BlueprintReadOnly, Category="Quest")
	int32 Count = 0;
};

/**
 *  The quests of the group, shared like the compendium: whatever one player does counts for everybody,
 *  and a player who joins later takes over the group's progress.
 *  Loads every quest from /Game/Vael/Story/Quests; the world reports kills, talks, places, gathering, casting and events.
 */
UCLASS()
class UVaelQuestSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	/** Loads the quests and activates those without predecessor */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** The quests of the world of the object, null outside the game */
	static UVaelQuestSubsystem* Get(const UObject* WorldContext);

	/** A creature of a kind was defeated */
	void NotifyKill(FName Kind);

	/** A plant, fungus or stone was gathered */
	void NotifyGather(FName SubjectId);

	/** A formula was cast */
	void NotifyCast(FName FormulaName);

	/** A player reached a quest marker */
	void NotifyReach(FName MarkerId);

	/** Something happened in the world, like "QuelleVersiegelt" */
	void NotifyEvent(FName EventName);

	/**
	 *  A player talks to a person. If a quest waits for this talk, the step is done and its lines are handed out (they may be empty).
	 *  Returns false if no quest waits for this person.
	 */
	bool TakeTalk(FName SpeakerKey, TArray<FText>& OutLines);

	/** True if a quest waits for a talk with this person; the HUD shows a "!" over them */
	bool IsWaitingForTalk(FName SpeakerKey) const;

	/** Every quest in its order: main quests first */
	const TArray<TObjectPtr<UVaelQuest>>& GetQuests() const { return Quests; }

	/** Active quests that are not done yet, main quests first */
	TArray<const UVaelQuest*> GetOpenQuests() const;

	/** Progress of a quest */
	FVaelQuestProgress GetProgress(const UVaelQuest* Quest) const;

	/** The current step of an open quest, null if it is done or not active */
	const FVaelQuestStep* GetCurrentStep(const UVaelQuest* Quest) const;

	/** Quest by id, null if there is none */
	const UVaelQuest* FindQuest(FName QuestId) const;

	/** Moves a quest to a step at once, for testing; a step past the last finishes it */
	void SetStep(FName QuestId, int32 Step);

	/** Every quest and its state, for reports */
	FString Describe() const;

private:

	/** Counts one for every open quest whose current step is of this type and target; an empty target in the step takes anything */
	void CountFor(EVaelQuestObjective Type, FName Target);

	/** One more of the current step; moves on when the count is reached */
	void Advance(const UVaelQuest* Quest);

	/** Finishes a quest and activates the quests that follow it */
	void Finish(const UVaelQuest* Quest);

	/** Activates a quest and tells the group */
	void Activate(const UVaelQuest* Quest);

	UPROPERTY()
	TArray<TObjectPtr<UVaelQuest>> Quests;

	UPROPERTY()
	TMap<FName, FVaelQuestProgress> Progress;
};
