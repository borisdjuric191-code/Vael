// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/VaelInteractable.h"
#include "VaelStorageChest.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

/**
 *  The store chest of a camp (goal 16.11., Boris: "eine Inventarbox im Lager"): opens the inventory of whoever uses it. For the
 *  engineer it is where his stock waits: his pouches fill up from it as he opens it, like at the workbench. The first time an
 *  engineer opens it in a game he finds what the survivors put aside for him (enough to mix his first shots and refine his
 *  first metal), once per save (story flag Ingenieur.Grundstock).
 *  Its mesh is the old wooden chest of the packs unless the level gives it another one.
 */
UCLASS(meta = (DisplayName = "Vael Storage Chest"))
class AVaelStorageChest : public AActor, public IVaelInteractable
{
	GENERATED_BODY()

public:

	AVaelStorageChest();

	//~Begin IVaelInteractable
	virtual bool CanInteract(const AVaelCharacter* Player) const override;
	virtual void Interact(AVaelCharacter* Player) override;
	virtual FText GetInteractPrompt() const override;
	//~End IVaelInteractable

	/** Whether the engineer finds the starting materials inside (only the chest of the first camp) */
	UPROPERTY(EditAnywhere, Category = "Chest")
	bool bStarterStock = true;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** A small warm light on the lid, so the chest is found in the dark camp */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPointLightComponent> Glint;

	/** Gives the engineer his starting materials, once per game; returns how many kinds he found */
	int32 GiveStarterStock(AVaelCharacter* Player) const;
};
