// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/VaelInteractable.h"
#include "VaelHarvestable.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UVaelHarvestableData;

/**
 *  A plant, fungus or stone placed in a level that players can pick or mine.
 *  Gathering gives every player the loot of its data and the compendium a sample; then it lies bare until it has grown back.
 *  Players near it, or watching it through the spyglass, sight and observe it for the compendium.
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

protected:

	/** What it is */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Harvest")
	TObjectPtr<UVaelHarvestableData> Data;

public:

	/** Constructor */
	AVaelHarvestable();

	//~Begin IVaelInteractable
	virtual bool CanInteract(const AVaelCharacter* Player) const override { return Data != nullptr && !bHarvested; }
	virtual void Interact(AVaelCharacter* Player) override;
	virtual FText GetInteractPrompt() const override;
	//~End IVaelInteractable

	/** Id of its compendium entry, none without data */
	FName GetCompendiumId() const;

	/** True while it lies bare after gathering */
	bool IsHarvested() const { return bHarvested; }

	/** Builds the placeholder from the data, also while placing it in the editor */
	virtual void OnConstruction(const FTransform& Transform) override;

protected:

	/** Registers it as something players can use and starts watching for researchers */
	virtual void BeginPlay() override;

	/** Unregisters it */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Shapes, sizes and colors the placeholder for its kind and state */
	void RefreshLook();

	/** Grown back: can be gathered again */
	void Regrow();

	/** Sights and observes it for the compendium when players are near or watch it through the spyglass */
	void UpdateResearch();

	/** Engine shapes the placeholder is built from */
	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> ConeMesh;

	/** True after gathering until it has grown back */
	bool bHarvested = false;

	FTimerHandle RegrowTimer;
	FTimerHandle ResearchTimer;
};
