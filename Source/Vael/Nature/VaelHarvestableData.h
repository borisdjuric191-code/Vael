// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/StaticMesh.h"
#include "Items/VaelMaterial.h"
#include "Magic/VaelElementTypes.h"
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

/** The signature trait of a kind: what it does on its own, besides being gathered */
UENUM(BlueprintType)
enum class EVaelHarvestTrait : uint8
{
	/** Only stands there */
	None,
	/** Catches fire when someone comes within the trait radius: sets them burning and lays a short fire patch (Glutdistel) */
	IgniteOnTouch,
	/** Every interval a glowing drop falls and heals players under it by the trait amount (Traenenkelch) */
	HealingDrip,
	/** Bursts when a spell hits it, into a cloud that blinds everyone inside (Aschblase) */
	BurstOnHit,
	/** Glows brighter the purer its region, cold in the trait color once the region is badly corrupted (Laternenglocke) */
	PurityGlow,
	/** Soaks up rain: swells and turns the trait color while it rains (Russmoos) */
	RainSoak,
	/** Breathes: its crown slowly swells and sinks (Fleischkelch) */
	Breathing,
	/** Whistles in a storm, and already within the trait duration before one comes, every trait interval (Pfeifenmorchel) */
	StormWhistle,
	/** Puffs when someone comes within the trait radius: puts out burning people and fires there, then lies empty (Aschenstaeubling) */
	PuffOnStep,
	/** Turns the trait color the closer an open Mark source is within the trait radius, stretched towards it; all twitch at once now and then (Adernflechte) */
	MarkVeins,
	/** A water spell hitting it lets out a steam burst of the trait radius; then it is cool for the trait interval, unless fire hits it (Glutstein) */
	SteamOnWater,
	/** Clouded in its glow color in a sick region, clear in the trait color in a pure one (Russquarz) */
	PurityClarity,
	/** Only there while an open Mark source lies within the trait radius (Markdruse) */
	NeedsOpenSource
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

	/** Grows only where the region is at least this corrupted; below, it isn't there at all */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Harvest", meta = (ClampMin = 0, ClampMax = 100))
	float MinCorruption = 0.0f;

	/** Wilts where the region is more corrupted than this: bare and grey, it can't be gathered */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Harvest", meta = (ClampMin = 0, ClampMax = 100))
	float MaxCorruption = 100.0f;

	/** True if mages near it can draw an element from it, which makes that element cheaper and stronger */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Element Source")
	bool bElementSource = false;

	/** The element it gives */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Element Source", meta = (EditCondition = "bElementSource"))
	EVaelElement SourceElement = EVaelElement::Fire;

	/** How close a mage has to stand, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Element Source", meta = (EditCondition = "bElementSource", ClampMin = 0))
	float SourceRadius = 250.0f;

	/** Its signature trait */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trait")
	EVaelHarvestTrait Trait = EVaelHarvestTrait::None;

	/** Reach of the trait in cm: touch distance, drop area, cloud radius */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trait", meta = (ClampMin = 0))
	float TraitRadius = 120.0f;

	/** Seconds between drops or whistles, or until it can catch fire or steam again */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trait", meta = (ClampMin = 0.1))
	float TraitInterval = 3.0f;

	/** Strength of the trait: health a drop heals, burning damage per second */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trait", meta = (ClampMin = 0))
	float TraitAmount = 5.0f;

	/** Seconds the trait lasts: burning, fire patch, smoke cloud, steam, warning before a storm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trait", meta = (ClampMin = 0))
	float TraitDuration = 4.0f;

	/** Second color of the trait: soaked moss, a corrupted glow, a burning crown */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trait")
	FLinearColor TraitColor = FLinearColor(0.6f, 0.2f, 1.0f);

	/** Height of the placeholder in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Look", meta = (ClampMin = 5))
	float Height = 70.0f;

	/** Color of the stem, the boulder or the stalk: the region's material */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Look")
	FLinearColor BaseColor = FLinearColor(0.12f, 0.1f, 0.09f);

	/** Color of the crown, the cap or the crystals: the element's glow */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Look")
	FLinearColor GlowColor = FLinearColor(1.0f, 0.4f, 0.1f);

	/** Model of the creature factory (Tools/Kreaturenfabrik, mode "requisite"), standing on its origin. Set: it replaces the placeholder shapes,
	 *  every placed one turned, sized and tilted a little differently. Empty: the placeholder of engine shapes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Look")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** Glow of the ember parts of the model (orange in its colors) while it grows untouched, through the master material */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Look", meta = (ClampMin = 0))
	float MeshGlow = 1.5f;
};
