// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Items/VaelItemTypes.h"
#include "Player/VaelInteractable.h"
#include "VaelChest.generated.h"

class UStaticMeshComponent;
class UVaelItemData;

/**
 *  A chest placed in a level. Opening it drops loot for every player: each gets their own copy of the fixed items
 *  and their own rolled items. Opens once. Placeholder look: a brown box that darkens when opened.
 */
UCLASS()
class AVaelChest : public AActor, public IVaelInteractable
{
	GENERATED_BODY()

	/** Placeholder look */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Box;

protected:

	/** Handmade items every player finds inside, like the Sturmmantel */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest")
	TArray<TObjectPtr<UVaelItemData>> FixedItems;

	/** Number of rolled items every player finds inside */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest", meta = (ClampMin = 0))
	int32 RolledItems = 1;

	/** Rolled items are at least this rare */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest")
	EVaelRarity MinRarity = EVaelRarity::Magic;

public:

	/** Constructor */
	AVaelChest();

	//~Begin IVaelInteractable
	virtual bool CanInteract(const AVaelCharacter* Player) const override { return !bOpened; }
	virtual void Interact(AVaelCharacter* Player) override;
	virtual FText GetInteractPrompt() const override;
	//~End IVaelInteractable

protected:

	/** Registers the chest as something players can use */
	virtual void BeginPlay() override;

	/** Unregisters it */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Colors the placeholder */
	void RefreshLook();

	/** True once the chest has been opened */
	bool bOpened = false;
};
