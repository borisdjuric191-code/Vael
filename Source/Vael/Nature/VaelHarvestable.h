// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Magic/VaelElementTypes.h"
#include "Player/VaelInteractable.h"
#include "VaelHarvestable.generated.h"

class UMaterialInstanceDynamic;
class UPointLightComponent;
class USoundAttenuation;
class UStaticMesh;
class UStaticMeshComponent;
class UVaelHarvestableData;

/**
 *  A plant, fungus or stone placed in a level that players can pick or mine.
 *  Gathering gives every player the loot of its data and the compendium a sample; then it lies bare until it has grown back.
 *  Players near it, or watching it through the spyglass, sight and observe it for the compendium.
 *  It grows only within the corruption range of its data, can be a source of an element for mages,
 *  and acts out its signature trait: catching fire, dripping healing drops, bursting into smoke, glowing, soaking up rain or breathing.
 *  Placeholder look built from engine shapes in the colors of its data: stem and crown, stalk and cap, or boulder and crystals.
 */
UCLASS()
class AVaelHarvestable : public AActor, public IVaelInteractable
{
	GENERATED_BODY()

	/** Stem, stalk or boulder */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Base;

	/** Crown, cap or crystals: what is taken when gathering */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Crown;

	/** A falling drop of a healing plant */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Drop;

	/** Light of a glowing or burning crown */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPointLightComponent> Glow;

	/** Model of the creature factory, if the data names one; replaces the placeholder shapes */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Model;

protected:

	/** What it is */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Harvest")
	TObjectPtr<UVaelHarvestableData> Data;

public:

	/** Constructor */
	AVaelHarvestable();

	//~Begin IVaelInteractable
	virtual bool CanInteract(const AVaelCharacter* Player) const override { return IsAvailable(); }
	virtual void Interact(AVaelCharacter* Player) override;
	virtual FText GetInteractPrompt() const override;
	//~End IVaelInteractable

	/** Id of its compendium entry, none without data */
	FName GetCompendiumId() const;

	/** True while it lies bare after gathering or bursting */
	bool IsHarvested() const { return bHarvested; }

	/** True if it grows here now: not gathered, not wilted, and the region is corrupted enough for it */
	bool IsAvailable() const { return Data != nullptr && !bHarvested && !bAbsent && !bWilted; }

	/** True if a mage at the location can draw the element from it */
	bool ProvidesElement(EVaelElement Element, const FVector& Location) const;

	/** A spell of an element hit or exploded at a location: bubbles within the radius burst, glowing stones let out steam under water or heat up again under fire */
	static void NotifySpellImpact(const UWorld* World, const FVector& Location, float Radius, EVaelElement Element);

	/** Builds the placeholder from the data, also while placing it in the editor */
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Animates breathing, falling drops and flames */
	virtual void Tick(float DeltaSeconds) override;

protected:

	/** Registers it as something players can use and starts its trait */
	virtual void BeginPlay() override;

	/** Unregisters it */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Shapes, sizes and colors the placeholder for its kind and state */
	void RefreshLook();

	/** Grown back: can be gathered again */
	void Regrow();

	/** Lies bare until it grows back */
	void BecomeBare();

	/** Sights and observes it for the compendium when players are near or watch it through the spyglass */
	void UpdateResearch();

	/** Follows the region: corruption decides whether it grows or wilts, the weather whether it is soaked, purity how bright it glows */
	void UpdateSurroundings();

	/** The frequent part of the trait: touching, dripping */
	void UpdateTrait();

	/** Catches fire: everyone close starts burning, a short fire patch is laid */
	void Ignite();

	/** Bursts into a blinding cloud */
	void Burst();

	/** Puffs a cloud of ash that puts out burning people and fires, then lies empty */
	void Puff();

	/** Lets out its heat as a steam burst and cools down */
	void Steam();

	/** Whistles, louder the closer the storm */
	void Whistle(float Loudness);

	/** Plays the whistle tone, made in code until the Klangbibel has its sound */
	void PlayWhistleSound(float Volume);

	/** Looks for the closest open Mark source: how strongly the veins color and which way they stretch */
	void UpdateMarkVeins();

	/** True if a glowing stone is cool after letting out steam */
	bool IsCooled() const;

	/** A drop lands: players below are healed */
	void LandDrop();

	/** Engine shapes the placeholder is built from */
	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> ConeMesh;

	/** True after gathering until it has grown back */
	bool bHarvested = false;

	/** True while the region is less corrupted than it needs */
	bool bAbsent = false;

	/** True while the region is more corrupted than it bears */
	bool bWilted = false;

	/** True while it rains on a plant that soaks up rain */
	bool bSoaked = false;

	/** Share from 0 to 1 of the purity of its region, for the glow */
	float Purity = 1.0f;

	/** Game time until which it burns, and from which it can catch fire, puff or let out steam again */
	float BurnEndTime = -1.0f;
	float NextTraitTime = 0.0f;

	/** Seconds since a puff began, negative while none plays */
	float PuffAge = -1.0f;

	/** Game time until which a whistling crown shakes */
	float WhistleEndTime = -1.0f;

	/** Seconds until the next whistle */
	float WhistleCountdown = 0.0f;

	/** How far a whistle carries */
	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> WhistleAttenuation;

	/** True while the look shows a cooled stone */
	bool bShownCooled = false;

	/** Material of the model, for its ember glow */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ModelMaterial;

	/** Size of this one model: every placed one is a little different */
	FVector ModelScale = FVector::OneVector;

	/** Turns, sizes and tilts the model by its place, so neighbours differ but each one stays the same every time */
	void VaryModel();

	/** How strongly veins color towards the Mark, 0 to 1, and the yaw they stretch along */
	float VeinStrength = 0.0f;
	float VeinYaw = 0.0f;

	/** Seconds the current drop has been falling, negative while none falls */
	float DropAge = -1.0f;

	/** Seconds until the next drop */
	float DropCountdown = 0.0f;

	/** Scale of the crown as built, before breathing and flames */
	FVector CrownScale = FVector::OneVector;

	FTimerHandle RegrowTimer;
	FTimerHandle ResearchTimer;
	FTimerHandle SurroundingsTimer;
	FTimerHandle TraitTimer;
};
