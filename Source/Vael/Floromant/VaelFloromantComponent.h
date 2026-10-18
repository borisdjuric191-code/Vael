// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UI/VaelHUDClassData.h"
#include "VaelFloromantComponent.generated.h"

class AVaelCharacter;
class AVaelPlant;
class UVaelPlantData;

/**
 *  What a player does as Floromant, on the buttons of the mage:
 *  R2 Dornenpeitsche, Circle Ruf des Gartens, Square sows the seed picked on the seed belt (d-pad), Cross Wurzelgriff,
 *  Triangle Wurzelsprung to an own plant, L1 + R1 Wuchern. Up to four own plants stand at once.
 *  Saft is the mana of the character, shown in its place, and flows back slowly.
 */
UCLASS()
class UVaelFloromantComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	/** Constructor */
	UVaelFloromantComponent();

	// --- Seed belt

	/** Picks one of the seeds of the belt */
	void SelectBelt(int32 Index);

	int32 GetBeltSize() const;
	int32 GetBeltSelection() const { return BeltSelection; }
	UVaelPlantData* GetBeltPlant(int32 Index) const;

	/** Name and colour of a seed of the belt, in the form the Floromant grows */
	FText GetBeltName(int32 Index) const;
	FLinearColor GetBeltColor(int32 Index) const;

	// --- Abilities

	/** Square: sows the picked seed towards the point on the ground, no farther than the throw range */
	bool Sow(const FVector& AimLocation);

	/** Circle: own and wild plants around act for a few seconds, each for its Saft */
	int32 CallGarden();

	/** Cross: roots shoot out of the ground at the aim point, bind and choke */
	bool RootGrip(const FVector& AimLocation);

	/** Triangle: leaps to the own plant nearest to the aim point */
	bool RootLeap(const FVector& AimLocation);

	/** L1 + R1: every plant around grows into its giant form and acts */
	int32 Overgrow();

	/** R2: a thorny vine lashes ahead */
	bool ThornWhip(const FVector& AimLocation);

	/** Grows the plant of a seed that has landed; the oldest plant withers if there are too many */
	AVaelPlant* PlantSeed(UVaelPlantData* Plant, bool bShadow, const FVector& GroundLocation, float Yaw);

	/** Plants of this Floromant that still stand */
	int32 GetNumPlants() const;

	/** Lets every plant of this Floromant wither, for example when the player turns back into a mage */
	void WitherAll();

	/** Seconds until the ability can be used again, and its whole cooldown; 0 for abilities without one */
	float GetCooldownLeft(EVaelHUDAbility Ability) const;
	float GetCooldownDuration(EVaelHUDAbility Ability) const;

	/** Plant data of a plant of the garden or the world by its id, like "Glimmklee"; null if there is none */
	static UVaelPlantData* FindPlantData(FName PlantId);

private:

	/** Character the component belongs to */
	AVaelCharacter* GetCharacter() const;

	/** True if the ability is ready, the player can act and has the Saft; takes the Saft and starts the cooldown */
	bool Use(EVaelHUDAbility Ability, float Cost);

	/** Where an aim lands on the ground, no farther than the range */
	FVector GroundTarget(const FVector& AimLocation, float Range) const;

	/** Own plants grow in the shadow form on the way of shadow */
	bool GrowsShadowForms() const;

	/** Makes plants within the radius act; with bGiant they also grow into their giant form. Returns how many act. */
	int32 CallPlants(float Radius, float Duration, bool bGiant, bool bPaySaft);

	int32 BeltSelection = 0;

	/** World time from which each ability can be used again */
	TMap<EVaelHUDAbility, float> ReadyTimes;

	/** Plants grown, oldest first */
	TArray<TWeakObjectPtr<AVaelPlant>> Plants;
};
