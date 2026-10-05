// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "Magic/VaelElementTypes.h"
#include "VaelElementComponent.generated.h"

class UAbilitySystemComponent;
class UVaelFormula;
class UVaelGrimoireSubsystem;

/** The cast the element component hands to the formula ability */
struct FVaelPendingCast
{
	/** Mana the cast costs */
	float ManaCost = 0.0f;

	/** Strength of the cast, 1 is normal */
	float Power = 1.0f;
};

DECLARE_MULTICAST_DELEGATE(FVaelOnElementQueueChanged);
DECLARE_MULTICAST_DELEGATE_TwoParams(FVaelOnCastFinished, EVaelCastResult /*Result*/, const UVaelFormula* /*Formula*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FVaelOnLightningBacklash, float /*Damage*/);

/**
 *  The element queue of a mage: collects elements and casts the formula they add up to.
 *  The owner needs an ability system component, formulas are performed as gameplay abilities.
 */
UCLASS(ClassGroup=(Vael), meta = (BlueprintSpawnableComponent))
class UVaelElementComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	/** Constructor */
	UVaelElementComponent();

	/** Adds an element to the queue. Returns false if the queue is full. */
	UFUNCTION(BlueprintCallable, Category="Magic")
	bool AddElement(EVaelElement Element);

	/** Empties the queue without casting */
	UFUNCTION(BlueprintCallable, Category="Magic")
	void ClearQueue();

	/** Casts the formula the queued elements add up to and empties the queue */
	UFUNCTION(BlueprintCallable, Category="Magic")
	EVaelCastResult CastQueue();

	/** Number of quick slots of every mage */
	static constexpr int32 NumQuickSlots = 4;

	/** Casts the formula on a quick slot: costs more mana, hits a bit weaker and the slot has to cool down. The queue stays as it is. */
	UFUNCTION(BlueprintCallable, Category="Magic")
	EVaelCastResult CastQuickSlot(int32 SlotIndex);

	/** Puts a known formula on a quick slot, or empties the slot with null. A formula on another slot moves over. */
	void AssignQuickSlot(int32 SlotIndex, UVaelFormula* Formula);

	/** Formula on a quick slot, null if the slot is empty */
	UVaelFormula* GetQuickSlotFormula(int32 SlotIndex) const;

	/** Share of the cooldown of a quick slot still to go, 0 when it is ready */
	float GetQuickSlotCooldownFraction(int32 SlotIndex) const;

	/** Elements waiting to be cast, oldest first */
	const TArray<EVaelElement>& GetQueue() const { return Queue; }

	/** True if the element in the given slot was drawn from the environment when it was queued */
	bool IsFromEnvironment(int32 SlotIndex) const { return QueueFromEnvironment.IsValidIndex(SlotIndex) && QueueFromEnvironment[SlotIndex]; }

	/** Number of elements the queue can hold */
	int32 GetNumSlots() const { return FMath::Clamp(UnlockedSlots, 1, VaelElements::MaxQueueSlots); }

	/** Cost and power of the cast in progress, read by the formula ability */
	const FVaelPendingCast& GetPendingCast() const { return PendingCast; }

	/** Called whenever an element is added or the queue is emptied */
	FVaelOnElementQueueChanged OnQueueChanged;

	/** Called after every cast attempt with elements in the queue */
	FVaelOnCastFinished OnCastFinished;

	/** Called when a lightning formula cast while wet hurts the caster */
	FVaelOnLightningBacklash OnLightningBacklash;

protected:

	/** Initialization */
	virtual void BeginPlay() override;

	/** Cleanup */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Gives the owner the ability of a known formula */
	void GrantFormula(const UVaelFormula* Formula);

	/** Grants a newly learned formula and puts it on an empty quick slot */
	void OnFormulaLearned(const UVaelFormula* Formula, const FText& Reason);

	/** Slots of the queue. Three at the start, slot four and five come from the skill tree. */
	UPROPERTY(EditAnywhere, Category="Magic", meta = (ClampMin = 1, ClampMax = 5))
	int32 UnlockedSlots = 3;

private:

	/** Finishes a cast attempt: empties the queue and tells listeners */
	EVaelCastResult FinishCast(EVaelCastResult Result, const UVaelFormula* Formula);

	/** Pays for and performs a formula. The queue is left alone. */
	EVaelCastResult ActivateFormula(UVaelFormula* Formula, int32 NumElements, int32 NumEnvironmentElements, bool bFromQuickSlot);

	/** Puts a formula of several elements on the first empty quick slot, if it isn't on one already */
	void FillEmptyQuickSlot(const UVaelFormula* Formula);

	/** Experimenting went wrong: hurts the caster and throws nearby enemies back */
	void UnstableDischarge();

	/** For every queued element, whether it was drawn from the environment */
	TArray<bool> QueueFromEnvironment;

	UAbilitySystemComponent* GetAbilitySystem() const;
	UVaelGrimoireSubsystem* GetGrimoire() const;

	/** Elements waiting to be cast */
	TArray<EVaelElement> Queue;

	/** Cast in progress */
	FVaelPendingCast PendingCast;

	/** Granted ability per known formula */
	TMap<TObjectPtr<const UVaelFormula>, FGameplayAbilitySpecHandle> FormulaAbilities;

	/** Formula per quick slot */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UVaelFormula>> QuickSlots;

	/** World time at which each quick slot is ready again */
	TArray<float> QuickSlotReadyTimes;

	/** Cooldown each quick slot started with, in seconds */
	TArray<float> QuickSlotCooldowns;

	/** Handle of the grimoire delegate */
	FDelegateHandle FormulaLearnedHandle;
};
