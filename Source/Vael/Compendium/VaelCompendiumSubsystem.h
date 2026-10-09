// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Compendium/VaelCompendiumEntry.h"
#include "VaelCompendiumSubsystem.generated.h"

/** How far the group has come with one entry */
USTRUCT(BlueprintType)
struct FVaelResearchProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Compendium")
	EVaelResearchStage Stage = EVaelResearchStage::Unknown;

	/** Seconds the kind has been watched so far */
	UPROPERTY(BlueprintReadOnly, Category="Compendium")
	float ObservedSeconds = 0.0f;

	/** Kills, harvests or finds so far, each one a sample for the study */
	UPROPERTY(BlueprintReadOnly, Category="Compendium")
	int32 Samples = 0;
};

/**
 *  Albrun's compendium of creatures, plants and stones, shared by the whole group.
 *  Loads every entry from /Game/Vael/Compendium and moves them through the research stages:
 *  sighted, observed (watched for a while), defeated (killed, picked or mined) and researched (samples studied at the camp).
 */
UCLASS()
class UVaelCompendiumSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	/** Loads the entries */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** The compendium of the world of the object, null outside the game */
	static UVaelCompendiumSubsystem* Get(const UObject* WorldContext);

	/** A kind was in sight of a player */
	void Sight(FName SubjectId);

	/** A kind was watched for some seconds */
	void Observe(FName SubjectId, float Seconds);

	/** One of a kind was killed, picked or mined: a sample, and stage "defeated" */
	void AddSample(FName SubjectId);

	/** Studies the samples brought to the camp: every entry with enough of them is researched. Returns how many became researched. */
	int32 StudyAtCamp();

	/** Raises an entry to a stage at once, for testing */
	void SetStage(FName SubjectId, EVaelResearchStage Stage);

	/** Entry of a kind, null if there is none */
	const UVaelCompendiumEntry* FindEntry(FName SubjectId) const;

	/** Entries of a book in their order */
	TArray<const UVaelCompendiumEntry*> GetEntries(EVaelCompendiumBook Book) const;

	/** Progress of a kind */
	FVaelResearchProgress GetProgress(FName SubjectId) const;

	/** Share of an entry's observation already done, 0 to 1 */
	float GetObservationShare(FName SubjectId) const;

	/** Every entry and how many of them reached each stage, for reports */
	FString Describe() const;

private:

	/** Moves an entry up to a stage, never down, and tells the group */
	void Reach(FName SubjectId, EVaelResearchStage Stage);

	UPROPERTY()
	TArray<TObjectPtr<UVaelCompendiumEntry>> Entries;

	TMap<FName, FVaelResearchProgress> Progress;
};
