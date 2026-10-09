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
 *  With a cast animation the spell appears at its cast point; casting again during the animation releases the first spell at once.
 *  Handles every delivery of EVaelSpellDelivery; formulas that work differently derive from this class and override ExecuteFormula.
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
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~End UGameplayAbility

	/** Direction on the ground in which the caster aims */
	static FVector GetAimDirection(const AActor* Caster);

protected:

	/** Performs the formula. Power scales its damage, 1 is the normal strength. */
	virtual void ExecuteFormula(const UVaelFormula& Formula, AActor* Caster, float Power);

	/** Spawns the projectile of the formula in the aim direction */
	void FireProjectile(const UVaelFormula& Formula, AActor* Caster, float Power);

	/** Hits every enemy in a cone in the aim direction */
	void HitCone(const UVaelFormula& Formula, AActor* Caster, float Power);

	/** Strikes the nearest enemy in the aim direction and jumps on to others nearby */
	void HitChain(const UVaelFormula& Formula, AActor* Caster, float Power);

	/** Places the patch of the formula on the ground at the aimed point */
	void PlaceGroundArea(const UVaelFormula& Formula, AActor* Caster, float Power);

	/** Lets the caster rush in the aim direction; the dash ends in a burst */
	void StartDash(const UVaelFormula& Formula, AActor* Caster, float Power);

	/** Raises a row of rock blocks across the aim direction at the aimed point; enemies where a block rises are hurt and pushed away */
	void RaiseWall(const UVaelFormula& Formula, AActor* Caster, float Power);

	/** Starts the channeled beam of the formula from the hands of the caster */
	void StartBeam(const UVaelFormula& Formula, AActor* Caster, float Power);

	/** Hits everyone around the caster at once */
	void HitNova(const UVaelFormula& Formula, AActor* Caster, float Power);

	/** Lets a whirlwind loose in front of the caster that wanders in the aim direction */
	void LaunchVortex(const UVaelFormula& Formula, AActor* Caster, float Power);

	/** Starts a storm around the caster, or renews it */
	void StartAura(const UVaelFormula& Formula, AActor* Caster, float Power);

	/** Calls a line of spikes out of the ground in the aim direction; with ground area values every second spike leaves a patch */
	void CallSpikeLine(const UVaelFormula& Formula, AActor* Caster, float Power);

	/** Calls a single strike down on the aimed point; it lands after a warning and may leave a patch */
	void CallStrike(const UVaelFormula& Formula, AActor* Caster, float Power);

	/** Calls the servant of the formula to the aimed point */
	void SummonServant(const UVaelFormula& Formula, AActor* Caster, float Power);

	/** Lets stones circle around the caster */
	void StartOrbit(const UVaelFormula& Formula, AActor* Caster, float Power);

	/** Point on the ground the caster aims at: the mouse cursor, or with a gamepad the nearest enemy in the aim direction. Stays within range and in front of walls. */
	static FVector FindGroundTarget(AActor* Caster, float MaxRange);

	/** Element component of the avatar, holds the prepared cast */
	static UVaelElementComponent* GetElementComponent(const FGameplayAbilityActorInfo* ActorInfo);

	/** Where projectiles start: the casting hand after a cast animation, otherwise in front of the caster */
	FVector GetProjectileStart(const AActor* Caster, const FVector& AimDirection) const;

private:

	/** The cast animation reached its cast point */
	UFUNCTION()
	void OnCastPoint(FGameplayEventData Payload);

	/** The cast animation has blended out, was interrupted or couldn't play */
	UFUNCTION()
	void OnCastMontageEnded();

	/** The channeled spell has ended */
	UFUNCTION()
	void OnChannelEnded();

	/** Performs the formula of the current cast, once */
	void ReleaseSpell();

	/** Keeps the ability and the cast loop running while the caster channels. Returns false if nothing is channeled. */
	bool WaitForChannelEnd();

	/** Formula of the current cast */
	UPROPERTY(Transient)
	TObjectPtr<const UVaelFormula> CastFormula;

	/** Strength of the current cast, taken when it started; the element component forgets it right away */
	float CastPower = 1.0f;

	/** True once the formula of the current cast has been performed */
	bool bSpellReleased = false;

	/** True while a cast animation plays, so the spell comes out of the hand */
	bool bCastAnimated = false;

	/** True while the ability waits for a channeled spell to end */
	bool bWaitingForChannel = false;
};
