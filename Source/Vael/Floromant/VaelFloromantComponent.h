// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Floromant/VaelFloromantSettings.h"
#include "UI/VaelHUDClassData.h"
#include "VaelFloromantComponent.generated.h"

class AVaelCharacter;
class AVaelPlant;

/**
 *  What a player does as Floromant: picks one of the seeds with the buttons that queue elements for the mage,
 *  throws it with the cast button and keeps up to four plants at once. Seeds cost Saft, which flows back slowly;
 *  Saft is the mana of the character, shown in its place.
 */
UCLASS()
class UVaelFloromantComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	/** Constructor */
	UVaelFloromantComponent();

	/** Picks the seed the next throw uses */
	void SelectSeed(EVaelSeedKind Kind);

	/** Seed the next throw uses */
	EVaelSeedKind GetSelectedSeed() const { return SelectedSeed; }

	/** Throws the picked seed towards the point on the ground, no farther than the throw range. Returns false without Saft, during the cooldown or while down. */
	bool ThrowSeed(const FVector& AimLocation);

	/** Grows the plant of a seed that has landed; the oldest plant withers if there are too many */
	void PlantSeed(EVaelSeedKind Kind, const FVector& GroundLocation, float Yaw);

	/** Plants of this Floromant that still stand */
	int32 GetNumPlants() const;

	/** Lets every plant of this Floromant wither, for example when the player turns back into a mage */
	void WitherAll();

	/** The seed belt for the HUD: number of seeds, the picked one, name and colour of each */
	int32 GetBeltSize() const { return static_cast<int32>(EVaelSeedKind::Count); }
	int32 GetBeltSelection() const { return static_cast<int32>(SelectedSeed); }
	FText GetBeltName(int32 Index) const;
	FLinearColor GetBeltColor(int32 Index) const;

	/** Seconds until the ability can be used again, and its whole cooldown; 0 for abilities without one */
	float GetCooldownLeft(EVaelHUDAbility Ability) const;
	float GetCooldownDuration(EVaelHUDAbility Ability) const;

private:

	/** Character the component belongs to */
	AVaelCharacter* GetCharacter() const;

	EVaelSeedKind SelectedSeed = EVaelSeedKind::Thistle;

	/** World time from which the next throw is allowed */
	float NextThrowTime = 0.0f;

	/** Plants grown, oldest first */
	TArray<TWeakObjectPtr<AVaelPlant>> Plants;
};
