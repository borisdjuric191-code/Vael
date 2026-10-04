// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Magic/VaelElementTypes.h"
#include "VaelCombatStatics.generated.h"

/** What a spell does to a single target it hits */
USTRUCT(BlueprintType)
struct FVaelSpellHit
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell")
	float Damage = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell")
	EVaelElement Element = EVaelElement::Fire;

	/** Speed at which the target is pushed away, 0 for none */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell")
	float Knockback = 0.0f;

	/** True for lightning, which hits wet targets harder */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell")
	bool bLightning = false;

	/** Condition the hit leaves on the target */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell")
	EVaelStatus Status = EVaelStatus::None;

	/** Seconds the condition lasts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell")
	float StatusDuration = 0.0f;

	/** Damage per second of the condition, only used by burning */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell")
	float StatusDamagePerSecond = 0.0f;
};

/**
 *  Shared combat helpers for spells and attacks.
 */
UCLASS()
class UVaelCombatStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** True if the attacker may hurt the target: players don't hurt players, creatures don't hurt creatures */
	UFUNCTION(BlueprintPure, Category="Vael|Combat")
	static bool CanDamage(const AActor* Attacker, const AActor* Target);

	/**
	 *  Applies a spell to a target: resolves the reactions with the conditions of the target,
	 *  then deals damage, knockback and the condition of the hit. Returns false if the target can't be hurt.
	 */
	UFUNCTION(BlueprintCallable, Category="Vael|Combat")
	static bool ApplySpellHit(AActor* Attacker, AActor* Target, const FVaelSpellHit& Hit, const FVector& KnockbackDirection);

	/** Deals damage without reactions or a check who may hurt whom, for example to the caster themselves */
	UFUNCTION(BlueprintCallable, Category="Vael|Combat")
	static void DealDamage(AActor* Attacker, AActor* Target, float Damage, EVaelElement Element);

	/** True if the actor currently has the condition */
	UFUNCTION(BlueprintPure, Category="Vael|Combat")
	static bool HasStatus(const AActor* Target, EVaelStatus Status);

	/** Puts a condition on the actor. A condition that is already there is renewed and keeps the longer duration. */
	UFUNCTION(BlueprintCallable, Category="Vael|Combat")
	static void ApplyStatus(AActor* Attacker, AActor* Target, EVaelStatus Status, float Duration, float DamagePerSecond = 0.0f);

	/** Ends a condition of the actor. Returns true if it had the condition. */
	UFUNCTION(BlueprintCallable, Category="Vael|Combat")
	static bool RemoveStatus(AActor* Target, EVaelStatus Status);
};
