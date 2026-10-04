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

	/** Elements waiting to be cast, oldest first */
	const TArray<EVaelElement>& GetQueue() const { return Queue; }

	/** Number of elements the queue can hold */
	int32 GetNumSlots() const { return FMath::Clamp(UnlockedSlots, 1, VaelElements::MaxQueueSlots); }

	/** Cost and power of the cast in progress, read by the formula ability */
	const FVaelPendingCast& GetPendingCast() const { return PendingCast; }

	/** Called whenever an element is added or the queue is emptied */
	FVaelOnElementQueueChanged OnQueueChanged;

	/** Called after every cast attempt with elements in the queue */
	FVaelOnCastFinished OnCastFinished;

protected:

	/** Initialization */
	virtual void BeginPlay() override;

	/** Cleanup */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Gives the owner the ability of a known formula */
	void GrantFormula(const UVaelFormula* Formula);

	/** Slots of the queue. Three at the start, slot four and five come from the skill tree. */
	UPROPERTY(EditAnywhere, Category="Magic", meta = (ClampMin = 1, ClampMax = 5))
	int32 UnlockedSlots = 3;

private:

	/** Finishes a cast attempt: empties the queue and tells listeners */
	EVaelCastResult FinishCast(EVaelCastResult Result, const UVaelFormula* Formula);

	UAbilitySystemComponent* GetAbilitySystem() const;
	UVaelGrimoireSubsystem* GetGrimoire() const;

	/** Elements waiting to be cast */
	TArray<EVaelElement> Queue;

	/** Cast in progress */
	FVaelPendingCast PendingCast;

	/** Granted ability per known formula */
	TMap<TObjectPtr<const UVaelFormula>, FGameplayAbilitySpecHandle> FormulaAbilities;

	/** Handle of the grimoire delegate */
	FDelegateHandle FormulaLearnedHandle;
};
