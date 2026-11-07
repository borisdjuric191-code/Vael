// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/VaelInteractable.h"
#include "VaelBalanceBoard.generated.h"

class UPointLightComponent;

/**
 *  The root board of a refugium (Wurzeltafel, room profile point 4): the mirror of Die Waage. Interacting opens a page with
 *  the player's balance, the region's balance and what the beds do to it. The first time the group comes down into the
 *  Glutkammer Albrun explains the balance here and the first bed is sown guided (it glows until it holds a plant).
 */
UCLASS(meta = (DisplayName = "Vael Balance Board"))
class AVaelBalanceBoard : public AActor, public IVaelInteractable
{
	GENERATED_BODY()

public:

	AVaelBalanceBoard();

	/** Story flag set once Albrun has explained the balance, and once the first bed is sown */
	static const FName ExplainedFlag;
	static const FName FirstBedFlag;

	//~Begin IVaelInteractable
	virtual bool CanInteract(const AVaelCharacter* Player) const override;
	virtual void Interact(AVaelCharacter* Player) override;
	virtual FText GetInteractPrompt() const override;
	//~End IVaelInteractable

	virtual void Tick(float DeltaSeconds) override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Albrun's words about the balance, the first time */
	void Explain();

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPointLightComponent> Glow;

	float ExplainAt = -1.0f;
};

/**
 *  A place to rest in a refugium (by the Glutbecken): any time of day the group rests 8 hours, heals, and purifies with a
 *  Klartrunk or Lichttropfen (Die Waage).
 */
UCLASS(meta = (DisplayName = "Vael Rest Spot"))
class AVaelRestSpot : public AActor, public IVaelInteractable
{
	GENERATED_BODY()

public:

	AVaelRestSpot();

	//~Begin IVaelInteractable
	virtual bool CanInteract(const AVaelCharacter* Player) const override { return true; }
	virtual void Interact(AVaelCharacter* Player) override;
	virtual FText GetInteractPrompt() const override;
	//~End IVaelInteractable

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
