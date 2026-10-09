// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "Magic/VaelElementTypes.h"
#include "VaelCharacterBase.generated.h"

class UAbilitySystemComponent;
class UVaelAttributeSet;
struct FOnAttributeChangeData;

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

	/** Corruption by the Mark, 0 to 100 */
	UFUNCTION(BlueprintPure, Category="Attributes")
	float GetCorruption() const;

	/** Pushes the character along the ground */
	UFUNCTION(BlueprintCallable, Category="Combat")
	virtual void ApplyKnockback(const FVector& Direction, float Speed);

	/** Keeps the character from acting for some seconds after a heavy blow. Only creatures can be stunned. */
	virtual void ApplyStun(float Duration) {}

	/** Drags the character towards a location for one frame, like a whirlwind does; it never passes the location */
	virtual void ApplyPull(const FVector& Location, float Speed, float DeltaSeconds);

	/** Takes away the sight of the character for some seconds, so it starts no attacks. Only creatures can be blinded. */
	virtual void ApplyBlind(float Duration) {}

	/** Names of the conditions the character has right now, for display. Empty if there are none. */
	UFUNCTION(BlueprintPure, Category="Combat")
	FText GetStatusText() const;

	/** Incoming damage carrying the given tags (element tags among them) is multiplied by this */
	virtual float GetIncomingDamageMultiplier(const FGameplayTagContainer& DamageTags) const { return 1.0f; }

	/** True while nothing can hurt the character, for example during a dodge roll */
	/** True for players and the servants they summon, false for their enemies */
	virtual bool IsOnPlayerSide() const { return IsPlayerControlled(); }

	UFUNCTION(BlueprintPure, Category="Combat")
	virtual bool IsInvulnerable() const { return false; }

	/** True if the character is out of the fight: a dead creature or a player who is down */
	UFUNCTION(BlueprintPure, Category="Combat")
	virtual bool IsDefeated() const { return false; }

	/** False if a condition can't be put on the character, for example bosses can't be frozen */
	virtual bool CanReceiveStatus(EVaelStatus Status) const { return true; }

	/** Damage this character deals is multiplied by this, for example by marked creatures */
	virtual float GetOutgoingDamageMultiplier() const { return 1.0f; }

	/** Called by the attributes right before damage is taken off the health, with the final amount and the tags of the damage */
	virtual void OnDamageTaken(float Damage, const FGameplayTagContainer& DamageTags) {}

protected:

	/** Called whenever the health changes */
	virtual void OnHealthChanged(float OldValue, float NewValue) {}

	/** Called when the character freezes or thaws. Frozen characters can't move. */
	virtual void OnFrozenChanged(const FGameplayTag Tag, int32 NewCount);

private:

	/** Forwards health changes to OnHealthChanged */
	void HandleHealthChanged(const FOnAttributeChangeData& ChangeData);
};
