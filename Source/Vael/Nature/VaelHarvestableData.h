// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Items/VaelMaterial.h"
#include "VaelHarvestableData.generated.h"

/** What a harvestable is, which decides how it is gathered and how its placeholder looks */
UENUM(BlueprintType)
enum class EVaelHarvestKind : uint8
{
	/** Picked by hand: a stem with a crown */
	Plant,
	/** Gathered by hand: a stem with a flat cap */
	Fungus,
	/** Mined: a boulder with glowing crystals */
	Stone
};

/**
 *  One kind of plant, fungus or stone that players can pick or mine, like the Glutdistel or the Glutstein.
 *  Gathering puts its loot into the bag of every player and gives a sample to the compendium; then it grows back.
 */
UCLASS(BlueprintType)
class UVaelHarvestableData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** Name shown in the interact prompt */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Harvest")
	FText DisplayName;

	/** Id of its entry in the compendium */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Harvest")
	FName CompendiumId;

	/** Plant, fungus or stone */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Harvest")
	EVaelHarvestKind Kind = EVaelHarvestKind::Plant;

	/** What gathering it gives every player */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Harvest")
	TArray<FVaelLootEntry> Loot;

	/** Seconds until it has grown back after gathering, 0 for never */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Harvest", meta = (ClampMin = 0))
	float RegrowSeconds = 90.0f;

	/** Height of the placeholder in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Look", meta = (ClampMin = 5))
	float Height = 70.0f;

	/** Color of the stem, the boulder or the stalk: the region's material */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Look")
	FLinearColor BaseColor = FLinearColor(0.12f, 0.1f, 0.09f);

	/** Color of the crown, the cap or the crystals: the element's glow */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Look")
	FLinearColor GlowColor = FLinearColor(1.0f, 0.4f, 0.1f);
};
