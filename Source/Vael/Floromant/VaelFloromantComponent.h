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

	/** The hand if it was made already, else null */
	UVaelPlantArmComponent* FindHand(bool bLeft) const { return bLeft ? LeftHand.Get() : RightHand.Get(); }

	/** True while a hand fires; the Floromant has landed and braces itself */
	bool IsFiring() const;

	/** How far each arm is stretched out to fire, 0 to 1 */
	float GetStance(bool bLeft) const;

	/** How far a shield hand is raised before the face, 0 to 1 */
	float GetShieldStance(bool bLeft) const;

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

	// --- Flight: hovering is quick and safe, landing gives fire power; up high she is out of reach of the ground, rooted she hits hardest

	/** R3: rise high or sink back; with bRiseToggles off it follows the button */
	void ToggleRise();
	void SetRiseHeld(bool bHeld);

	/** How far she has risen up high, 0 to 1 */
	float GetRise() const { return Rise; }

	/** True while she is up high and nothing on the ground reaches her */
	bool IsUpHigh() const { return Rise > 0.5f; }

	/** How far the roots of her feet have grown, 0 to 1; rooted she stands anchored */
	float GetRootGrowth() const { return RootGrowth; }
	bool IsRooted() const { return RootGrowth > 0.0f; }

	/** How wet her wings are, 0 dry to 1 soaked */
	float GetWingWetness() const { return WingWetness; }

	/** Strength of her hands: stronger while rooted */
	float GetHandPower() const;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// --- Placeholders of the new controls, not built yet

	/** L2 / R2 (Q / E): the plant of the hand goes into the ground ahead and grows into a tower; the arm grows back meanwhile.
	 *  Two towers close together fuse into a symbiosis */
	void RootTower(bool bLeft);

	/** Towers of this Floromant standing now */
	int32 GetNumTowers() const;

	/** L1 + R1 + Cross (both mouse buttons + Space): the arms of both firing hands grow together and the pair acts at once
	 *  (symbiosis at the player); a tap acts fast, holding acts bigger */
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

	/** Towers standing, oldest first */
	TArray<TWeakObjectPtr<class AVaelPlantTower>> Towers;

	/** Knot of the crossed hands, how long it still shows, and the way it faced */
	UPROPERTY(Transient)
	TObjectPtr<class UVaelSymbiosisKnotComponent> CrossKnot;

	float CrossShowLeft = 0.0f;
	FVector CrossAim = FVector::ForwardVector;

	/** Her feet on the ground, where the roots start */
	FVector GetFeet() const;

	/** Raises, grows and lowers the glass dome while a shield hand is held; it costs Saft per second */
	void TickShield(float DeltaTime);

	/** The glass dome of her raised Glimmklee hand */
	TWeakObjectPtr<class AVaelGlimmDome> ShieldDome;

	/** Two Rußrose arms crossed: the roses wander forward, grow into one and shoot the green heart */
	void ShootRoseHeart(UVaelPlantArmComponent* Left, UVaelPlantArmComponent* Right);

	/** Keeps the knot of the crossed hands between them while it shows */
	void UpdateCrossKnot(float DeltaTime);

	/** Rising, rooting and wet wings, once per frame */
	void TickFlight(float DeltaTime);

	/** Grows or pulls back the roots of the feet */
	void UpdateFootRoots();

	/** Roots of her feet while she stands rooted, made when first needed */
	UPROPERTY(Transient)
	TObjectPtr<class UVaelFootRootsComponent> FootRoots;

	bool bRiseWanted = false;
	float Rise = 0.0f;

	/** Seconds she has stood firing on the spot, growth of the roots, seconds of letting go left (negative while not letting go) */
	float StillFiringTime = 0.0f;
	float RootGrowth = 0.0f;
	float UprootTimeLeft = -1.0f;

	float WingWetness = 0.0f;

	/** Character the component belongs to */
	AVaelCharacter* GetCharacter() const;

	/** True if the ability is ready, the player can act and has the Saft; takes the Saft and starts the cooldown */
	bool Use(EVaelHUDAbility Ability, float Cost);

	/** Where an aim lands on the ground, no farther than the range */
	FVector GroundTarget(const FVector& AimLocation, float Range) const;

	/** Own plants grow in the shadow form on the way of shadow */
	bool GrowsShadowForms() const;

	/** Makes plants within the radius act; with bGiant they also grow into their giant form. Returns how many act. */
	int32 CallPlants(float Radius, float Duration, bool bGiant, bool bPaySaft, bool bRootWave = false);

	int32 BeltSelection = 0;

	/** World time from which each ability can be used again */
	TMap<EVaelHUDAbility, float> ReadyTimes;

	/** Plants grown, oldest first */
	TArray<TWeakObjectPtr<AVaelPlant>> Plants;
};
