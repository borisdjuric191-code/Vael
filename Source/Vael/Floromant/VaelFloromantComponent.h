// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UI/VaelHUDClassData.h"
#include "VaelFloromantComponent.generated.h"

class AVaelCharacter;
class AVaelPlant;
class UVaelPlantData;
class UVaelPlantArmComponent;
class UVaelPlantArmData;

/**
 *  What a player does as Floromant, on its own controls (IMC_Floromant, see AVaelPlayerController):
 *  the hands turn into plants of the seed belt and fire while L1 / R1 are held, Square Dornenpeitsche, Cross Wurzelsprung
 *  to an own plant, L2 + R2 together Ruf des Gartens (plants grow into their giant form). Up to four own plants stand at once.
 *  Sowing, Wurzelgriff and the old call of the garden stay in the code but have no button any more.
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

	// --- Plant arms: the seed belt picks a plant of the repertoire and lays it on a hand, which turns into it and fires

	/** Plants of the seed belt of the hands */
	int32 GetHandBeltSize() const;
	UVaelPlantArmData* GetHandBeltPlant(int32 Index) const;
	int32 GetHandBeltSelection() const { return HandBeltSelection; }

	/** Pages through the seed belt of the hands, +1 down, -1 up */
	void PageHandBelt(int32 Step);

	/** Lays the picked plant on a hand */
	void PutOnHand(bool bLeft);

	/** Holds or lets go of the fire button of a hand */
	void SetHandFiring(bool bLeft, bool bFiring);

	/** The hand, made when first asked for */
	UVaelPlantArmComponent* GetHand(bool bLeft);

	/** True while a hand fires; the Floromant has landed and braces itself */
	bool IsFiring() const;

	/** How far each arm is stretched out to fire, 0 to 1 */
	float GetStance(bool bLeft) const;

	/** Share of the walking speed left while firing */
	float GetMoveSpeedScale() const;

	/** Lets both hands turn back into hands at once */
	void ClearHands();

	// --- Keimpelz: the moss pelt that reloads the plant bars

	/** Keimpelz in the pelt now, and its share of the most it holds */
	float GetKeimpelz() const { return Keimpelz; }
	float GetKeimpelzFraction() const;

	/** Adds Keimpelz to the pelt, for example from picking it after rain */
	void AddKeimpelz(float Amount);

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// --- Placeholders of the new controls, not built yet

	/** L2 / R2 (Q / E): the hand takes root as a tower */
	void RootTower(bool bLeft);

	/** L1 + R1 + Cross (both mouse buttons + Space): the hands cross; a tap shoots fast, holding charges */
	void CrossHands(bool bCharged);

	/** Triangle held (R): fills the plant bars of both hands from the Keimpelz pelt, as far as it reaches. Returns the shots reloaded. */
	int32 ReloadHands();

	/** Touchpad (Tab): Herbarium and repertoire */
	void OpenHerbarium();

private:

	/** Tells the player a control has no ability behind it yet */
	void ShowNotBuilt(const FText& Name) const;

	/** The two hands, made when first needed */
	UPROPERTY(Transient)
	TObjectPtr<UVaelPlantArmComponent> LeftHand;

	UPROPERTY(Transient)
	TObjectPtr<UVaelPlantArmComponent> RightHand;

	int32 HandBeltSelection = 0;

	/** Keimpelz in the pelt */
	float Keimpelz = 0.0f;

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
