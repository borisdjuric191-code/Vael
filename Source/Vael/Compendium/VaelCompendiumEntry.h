// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VaelCompendiumEntry.generated.h"

/** The books of Albrun's compendium */
UENUM(BlueprintType)
enum class EVaelCompendiumBook : uint8
{
	/** Creatures */
	Bestiary,
	/** Plants */
	Herbarium,
	/** Stones, ores and crystals */
	Stones
};

/** How far the group has researched an entry */
UENUM(BlueprintType)
enum class EVaelResearchStage : uint8
{
	/** Never seen */
	Unknown,
	/** Seen once: outline, name, region */
	Sighted,
	/** Watched for a while without a fight: behaviour, first sketch */
	Observed,
	/** Beaten, picked or mined: values, loot, weaknesses */
	Defeated,
	/** Samples studied at the camp: everything, with Albrun's notes */
	Researched
};

/**
 *  One entry of the compendium: a kind of creature, plant or stone, and what each research stage reveals about it.
 *  The texts are Albrun's, written as notes of a curious scholar.
 */
UCLASS(BlueprintType)
class UVaelCompendiumEntry : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** Type of the asset manager under which entries are found */
	static const FPrimaryAssetType PrimaryAssetType;

	/** What the entry is about: the compendium id of a creature or of a plant or stone */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Entry")
	FName SubjectId;

	/** Book the entry stands in */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Entry")
	EVaelCompendiumBook Book = EVaelCompendiumBook::Bestiary;

	/** Name of the kind */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Entry")
	FText DisplayName;

	/** Where it lives or lies, like "Aschenmark" */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Entry")
	FText Region;

	/** Stage 1, sighted: a first impression of its outline */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Entry", meta = (MultiLine = true))
	FText Glimpse;

	/** Stage 2, observed: how it behaves */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Entry", meta = (MultiLine = true))
	FText Behaviour;

	/** Stage 3, defeated or harvested: weaknesses, what it leaves behind */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Entry", meta = (MultiLine = true))
	FText Weakness;

	/** Stage 4, researched: what it is good for, for every class */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Entry", meta = (MultiLine = true))
	FText Uses;

	/** Stage 4: how the Mark changes it */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Entry", meta = (MultiLine = true))
	FText MarkedForm;

	/** Stage 4: Albrun's note in the margin */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Entry", meta = (MultiLine = true))
	FText AlbrunNote;

	/** Seconds of watching it needs to count as observed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Research", meta = (ClampMin = 0))
	float ObserveSeconds = 6.0f;

	/** Kills, harvests or finds whose samples are needed to research it at the camp */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Research", meta = (ClampMin = 1))
	int32 StudyCount = 3;

	/** Order within the book, lower first */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Entry")
	int32 SortOrder = 0;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
