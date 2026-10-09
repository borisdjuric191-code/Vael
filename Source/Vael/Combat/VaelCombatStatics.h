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

	/** Seconds the target can't act after the hit, 0 for none */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell")
	float StunDuration = 0.0f;

	/** Share of the damage dealt that heals the attacker, like the Aderzug; 0 for none */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell")
	float LifeSteal = 0.0f;

	/** Charge of Mark the hit leaves in the target, which bursts later */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell")
	EVaelChargeMode Charge = EVaelChargeMode::None;

	/** Seconds until a delayed charge bursts, or how long a charge waits for the death of its target */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell")
	float ChargeTime = 0.0f;

	/** Damage of the burst of the charge to everyone around, and its radius in cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell")
	float ChargeDamage = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spell")
	float ChargeRadius = 0.0f;
};

/**
 *  Collects damage that arrives a little every frame, like from a beam or a whirlwind,
 *  and hands it out in steps, so the damage numbers stay readable.
 */
struct FVaelDamageCollector
{
	/** Adds damage for the target. Returns the collected damage once it reaches the step and starts over, 0 before. */
	float Add(const AActor* Target, float Damage, float Step)
	{
		float& Collected = CollectedDamage.FindOrAdd(Target);
		Collected += Damage;

		if (Collected < Step)
		{
			return 0.0f;
		}

		const float Dealt = Collected;
		Collected = 0.0f;
		return Dealt;
	}

private:

	/** Damage collected on each target that hasn't been dealt yet */
	TMap<TWeakObjectPtr<const AActor>, float> CollectedDamage;
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

	/** Applies a hit of nature, like lightning, to anyone who can be hurt right now: players and creatures alike */
	UFUNCTION(BlueprintCallable, Category="Vael|Combat")
	static bool ApplyNatureHit(AActor* Target, const FVaelSpellHit& Hit, const FVector& KnockbackDirection);

	/** Applies a spell to everyone the attacker may hurt within a radius, pushing them away from the center. Returns the number of targets hit. */
	UFUNCTION(BlueprintCallable, Category="Vael|Combat")
	static int32 ApplySpellHitInRadius(AActor* Attacker, const FVector& Center, float Radius, const FVaelSpellHit& Hit);

	/** Deals damage without reactions or a check who may hurt whom, for example to the caster themselves. The reaction multiplier only colors the damage number. */
	UFUNCTION(BlueprintCallable, Category="Vael|Combat")
	static void DealDamage(AActor* Attacker, AActor* Target, float Damage, EVaelElement Element, float ReactionMultiplier = 1.0f);

	/** Gives health back, never above the maximum */
	static void Heal(AActor* Target, float Amount);

	/** True if the actor currently has the condition */
	UFUNCTION(BlueprintPure, Category="Vael|Combat")
	static bool HasStatus(const AActor* Target, EVaelStatus Status);

	/** Puts a condition on the actor. A condition that is already there is renewed and keeps the longer duration. */
	UFUNCTION(BlueprintCallable, Category="Vael|Combat")
	static void ApplyStatus(AActor* Attacker, AActor* Target, EVaelStatus Status, float Duration, float DamagePerSecond = 0.0f);

	/** Ends a condition of the actor. Returns true if it had the condition. */
	UFUNCTION(BlueprintCallable, Category="Vael|Combat")
	static bool RemoveStatus(AActor* Target, EVaelStatus Status);

private:

	/** Reactions, damage, condition and knockback of a hit, without checking who may hurt whom */
	static bool ApplyHit(AActor* Attacker, AActor* Target, const FVaelSpellHit& Hit, const FVector& KnockbackDirection);

};
