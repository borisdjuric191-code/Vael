// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/VaelInteractable.h"
#include "VaelBrewTable.generated.h"

class UPointLightComponent;

/**
 *  The witch's kitchen of a refugium: the root table in the middle of the Glutkammer. Walking up to it and pressing interact
 *  opens the brewing page (FVaelBrewing): the recipes of the Rezeptbuch, brewed from what the player carries.
 *  The table itself is dressed in the level; this actor is only the place to use it and the warm light over the kettle.
 */
UCLASS(meta = (DisplayName = "Vael Brew Table"))
class AVaelBrewTable : public AActor, public IVaelInteractable
{
	GENERATED_BODY()

public:

	AVaelBrewTable();

	//~Begin IVaelInteractable
	virtual bool CanInteract(const AVaelCharacter* Player) const override;
	virtual void Interact(AVaelCharacter* Player) override;
	virtual FText GetInteractPrompt() const override;
	//~End IVaelInteractable

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPointLightComponent> KettleGlow;
};
