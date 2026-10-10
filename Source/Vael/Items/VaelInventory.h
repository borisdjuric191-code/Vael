// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Items/VaelItemTypes.h"
#include "VaelInventory.generated.h"

class UAbilitySystemComponent;

DECLARE_MULTICAST_DELEGATE(FVaelOnInventoryChanged);

/**
 *  The equipment and backpack of a player. Every item takes one field of the backpack.
 *  What is worn counts: more health and mana, mana per second, stronger formulas, shorter quick slot cooldowns
 *  and the special abilities of legendary items.
 */
UCLASS(ClassGroup=(Vael), meta = (BlueprintSpawnableComponent))
class UVaelInventory : public UActorComponent
{
	GENERATED_BODY()

public:

	/** Constructor */
	UVaelInventory();

	/** Takes an item: puts it on if its place is free, otherwise into the backpack. Returns false if the backpack is full. */
	bool AddItem(const FVaelItem& Item);

	/** Puts on an item from the backpack; what was worn there goes into its field. Returns false if there is nothing to put on. */
	bool EquipFromBackpack(int32 BackpackIndex);

	/** Takes off an item into the backpack. Returns false if nothing is worn there or the backpack is full. */
	bool Unequip(EVaelEquipSlot EquipSlot);

	/** Item worn at a place, invalid if the place is empty */
	const FVaelItem& GetEquipped(EVaelEquipSlot EquipSlot) const;

	/** Items in the backpack */
	const TArray<FVaelItem>& GetBackpack() const { return Backpack; }

	/** True if no field of the backpack is free */
	bool IsBackpackFull() const;

	/** Sum of a property over everything worn, special abilities included */
	float GetStatTotal(EVaelItemStat Stat) const;

	/** True if a worn legendary item grants the tag */
	bool HasGrantedTag(const FGameplayTag& Tag) const { return AppliedTags.HasTag(Tag); }

	/** Guild coins of this player, earned only with guild contracts */
	int32 GetCoins() const { return GuildCoins; }

	/** Puts guild coins into the purse */
	void AddCoins(int32 Amount);

	/** Pays guild coins. Returns false, and pays nothing, if there aren't enough. */
	bool SpendCoins(int32 Amount);

	/** Called whenever something is taken, put on or taken off, or coins change */
	FVaelOnInventoryChanged OnInventoryChanged;

protected:

	/** Removes what the equipment gave when the player goes away */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Brings health, mana, mana per second and the granted tags in line with what is worn */
	void ApplyEquipment();

	/** Ability system of the owner */
	UAbilitySystemComponent* GetAbilitySystem() const;

	/** Worn items by place; invalid items are empty places */
	UPROPERTY()
	TArray<FVaelItem> Equipped;

	UPROPERTY()
	TArray<FVaelItem> Backpack;

	UPROPERTY()
	int32 GuildCoins = 0;

	/** What the equipment currently adds, so it can be taken away again */
	float AppliedMaxHealth = 0.0f;
	float AppliedMaxMana = 0.0f;
	FActiveGameplayEffectHandle ManaRegenHandle;
	FGameplayTagContainer AppliedTags;
};
