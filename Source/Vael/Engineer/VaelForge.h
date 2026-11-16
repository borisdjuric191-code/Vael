// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/VaelInteractable.h"
#include "VaelForge.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

/**
 *  The forge of the engineer (goal 16.11.: "Die Esse und das Lager sollen einen Platz haben, wo gebaut und geschmiedet werden
 *  kann"). Set beside a workbench, in his workshop of the Glutkammer and at the forge of the camp in the Aschenmark. It opens the
 *  page of building like the workbench, but on the tab of refining (Schlackeneisen, gehärtete Platten, Messing), and everything
 *  can be built here that the workbench builds. The coals breathe; when he builds something here, sparks fly off the anvil and
 *  the coals flare up.
 *  The mesh is optional (SM_IW_Esse in the workshop is dressed separately; Mesh can carry its own look where no dressing is).
 */
UCLASS(meta = (DisplayName = "Vael Forge"))
class AVaelForge : public AActor, public IVaelInteractable
{
	GENERATED_BODY()

public:

	AVaelForge();

	virtual void Tick(float DeltaSeconds) override;

	//~Begin IVaelInteractable
	virtual bool CanInteract(const AVaelCharacter* Player) const override;
	virtual void Interact(AVaelCharacter* Player) override;
	virtual FText GetInteractPrompt() const override;
	//~End IVaelInteractable

	/** Where the sparks fly from (the anvil), relative to the forge */
	UPROPERTY(EditAnywhere, Category = "Forge")
	FVector SparkOffset = FVector(0.0f, 0.0f, 90.0f);

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPointLightComponent> CoalGlow;

	float BaseIntensity = 60.0f;
	float Flare = 0.0f;
	float Clock = 0.0f;

	/** The build of each engineer near it that was answered with sparks already */
	TMap<TWeakObjectPtr<const AActor>, float> Answered;
};
