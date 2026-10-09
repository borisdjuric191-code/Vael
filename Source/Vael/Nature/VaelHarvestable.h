// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Magic/VaelElementTypes.h"
#include "Player/VaelInteractable.h"
#include "VaelHarvestable.generated.h"

class UPointLightComponent;
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

	/** A spell hit or exploded at a location: kinds that burst on a hit and stand within the radius burst */
	static void NotifySpellImpact(const UWorld* World, const FVector& Location, float Radius);

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

	/** Game time until which it burns, and from which it can catch fire again */
	float BurnEndTime = -1.0f;
	float NextIgniteTime = 0.0f;

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
