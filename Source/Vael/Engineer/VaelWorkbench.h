// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/VaelInteractable.h"
#include "VaelWorkbench.generated.h"

class UPointLightComponent;

/**
 *  The engineer's workbench in his corner of the Glutkammer (SM_GK_Werkbank is dressed in the level, this is the place to use it).
 *  It opens the page of building (EVaelMenuPage::Craft) with everything: weapons, shots, devices, armour; his pouches fill up from
 *  his stock as he steps up to it. Only the engineer works here.
 */
UCLASS(meta = (DisplayName = "Vael Workbench"))
class AVaelWorkbench : public AActor, public IVaelInteractable
{
	GENERATED_BODY()

public:

	AVaelWorkbench();

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
	TObjectPtr<UPointLightComponent> LampGlow;
};
