// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "VaelCharacterBase.generated.h"

class UAbilitySystemComponent;
class UVaelAttributeSet;

/**
 *  Base of every character that can fight: owns the ability system and the core attributes.
 */
UCLASS(abstract)
class AVaelCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

private:

	/** Abilities, effects and attributes of this character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	/** Health and mana */
	UPROPERTY()
	TObjectPtr<UVaelAttributeSet> AttributeSet;

protected:

	/** Health at the start, also the maximum */
	UPROPERTY(EditAnywhere, Category="Attributes", meta = (ClampMin = 1))
	float StartingHealth = 100.0f;

	/** Mana at the start, also the maximum */
	UPROPERTY(EditAnywhere, Category="Attributes", meta = (ClampMin = 0))
	float StartingMana = 100.0f;

	/** Mana regained per second */
	UPROPERTY(EditAnywhere, Category="Attributes", meta = (ClampMin = 0))
	float ManaRegenPerSecond = 0.0f;

public:

	/** Constructor */
	AVaelCharacterBase();

	/** Sets up the ability system */
	virtual void PostInitializeComponents() override;

	/** Initialization */
	virtual void BeginPlay() override;

	//~Begin IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~End IAbilitySystemInterface

	/** Returns the core attributes */
	const UVaelAttributeSet* GetAttributeSet() const { return AttributeSet.Get(); }

	UFUNCTION(BlueprintPure, Category="Attributes")
	float GetHealth() const;

	UFUNCTION(BlueprintPure, Category="Attributes")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintPure, Category="Attributes")
	float GetMana() const;

	UFUNCTION(BlueprintPure, Category="Attributes")
	float GetMaxMana() const;

	/** Pushes the character along the ground */
	UFUNCTION(BlueprintCallable, Category="Combat")
	virtual void ApplyKnockback(const FVector& Direction, float Speed);

	/** Names of the conditions the character has right now, for display. Empty if there are none. */
	UFUNCTION(BlueprintPure, Category="Combat")
	FText GetStatusText() const;

protected:

	/** Called when the character freezes or thaws. Frozen characters can't move. */
	virtual void OnFrozenChanged(const FGameplayTag Tag, int32 NewCount);
};
