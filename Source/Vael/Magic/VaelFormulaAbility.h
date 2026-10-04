// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "VaelFormulaAbility.generated.h"

class UVaelElementComponent;
class UVaelFormula;

/**
 *  Ability that performs a formula. The formula asset is the source object of the ability,
 *  mana cost and power come from the cast the element component has prepared.
 *  Handles projectiles and cones; formulas that work differently derive from this class and override ExecuteFormula.
 */
UCLASS()
class UVaelFormulaAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:

	/** Constructor */
	UVaelFormulaAbility();

	//~Begin UGameplayAbility
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	//~End UGameplayAbility

protected:

	/** Performs the formula. Power scales its damage, 1 is the normal strength. */
	virtual void ExecuteFormula(const UVaelFormula& Formula, AActor* Caster, float Power);

	/** Spawns the projectile of the formula in the aim direction */
	void FireProjectile(const UVaelFormula& Formula, AActor* Caster, float Power);

	/** Hits every enemy in a cone in the aim direction */
	void HitCone(const UVaelFormula& Formula, AActor* Caster, float Power);

	/** Direction on the ground in which the caster aims */
	static FVector GetAimDirection(const AActor* Caster);

	/** Element component of the avatar, holds the prepared cast */
	static UVaelElementComponent* GetElementComponent(const FGameplayAbilityActorInfo* ActorInfo);
};
