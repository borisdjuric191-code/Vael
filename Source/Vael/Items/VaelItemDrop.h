// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Items/VaelItemTypes.h"
#include "VaelItemDrop.generated.h"

class AVaelCharacter;
class UStaticMeshComponent;

/**
 *  An item lying on the ground for one player. Loot is personal: only its owner can pick it up, by walking over it,
 *  and it shows their color. Placeholder look: a pillar of light in the color of the rarity on a disc in the player color.
 */
UCLASS()
class AVaelItemDrop : public AActor
{
	GENERATED_BODY()

	/** Placeholder pillar in the color of the rarity */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Pillar;

	/** Placeholder disc in the color of the owner */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> OwnerDisc;

public:

	/** Constructor */
	AVaelItemDrop();

	/** Drops an item for a player on the ground near a location */
	static AVaelItemDrop* DropItem(UWorld* World, const FVector& Location, const FVaelItem& InItem, AVaelCharacter* InOwner);

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

	const FVaelItem& GetItem() const { return Item; }

	/** Player the item belongs to */
	AVaelCharacter* GetOwningPlayer() const { return OwningPlayer.Get(); }

protected:

	/** Colors the placeholder */
	virtual void BeginPlay() override;

private:

	/** The item */
	UPROPERTY()
	FVaelItem Item;

	/** Player the item belongs to */
	TWeakObjectPtr<AVaelCharacter> OwningPlayer;

	/** True once the owner was told that the backpack is full, so it is said only once */
	bool bToldBackpackFull = false;
};
