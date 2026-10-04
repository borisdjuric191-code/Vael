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

	/** Applies damage and knockback of a spell to a target. Returns false if the target can't be hurt. */
	UFUNCTION(BlueprintCallable, Category="Vael|Combat")
	static bool ApplySpellHit(AActor* Attacker, AActor* Target, const FVaelSpellHit& Hit, const FVector& KnockbackDirection);
};
