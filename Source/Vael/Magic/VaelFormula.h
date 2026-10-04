// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Combat/VaelCombatStatics.h"
#include "Magic/VaelElementTypes.h"
#include "VaelFormula.generated.h"

class AVaelSpellProjectile;
class UVaelFormulaAbility;

/**
 *  A formula of the mage: the combination of elements that triggers it and what it does.
 *  The order of the elements doesn't matter, Fire + Earth is the same formula as Earth + Fire.
 */
UCLASS(BlueprintType)
class UVaelFormula : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** Asset manager type of all formulas */
	static const FPrimaryAssetType PrimaryAssetType;

	/** Constructor */
	UVaelFormula();

	//~Begin UObject
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	//~End UObject

	/** Order-independent key of the element combination */
	uint32 GetComboKey() const { return VaelElements::MakeComboKey(Elements); }

	/** What the formula does to a single target at the given power */
	FVaelSpellHit MakeSpellHit(float Power) const;

	/** Name shown in the grimoire */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Formula")
	FText DisplayName;

	/** What the formula does, shown in the grimoire */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Formula", meta = (MultiLine = true))
	FText Description;

	/** Elements that have to be combined, in any order */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Formula")
	TArray<EVaelElement> Elements;

	/** How the formula gets into the grimoire */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Formula")
	EVaelFormulaSource Source = EVaelFormulaSource::Free;

	/** Hint of the echo where a sealed formula can be found */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Formula", meta = (MultiLine = true))
	FText Hint;

	/** Ability that performs the formula. The default handles projectiles and cones, special formulas get their own class. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Effect")
	TSubclassOf<UVaelFormulaAbility> AbilityClass;

	/** How the formula reaches its targets */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Effect")
	EVaelSpellDelivery Delivery = EVaelSpellDelivery::Projectile;

	/** Element of the damage, decides weaknesses and reactions */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Effect")
	EVaelElement DamageElement = EVaelElement::Fire;

	/** Damage per target */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Effect", meta = (ClampMin = 0))
	float Damage = 0.0f;

	/** Speed at which targets are pushed away, in cm/s */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Effect", meta = (ClampMin = 0))
	float Knockback = 0.0f;

	/** True for lightning, which hits wet targets twice as hard */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Effect")
	bool bLightning = false;

	/** Condition the formula leaves on its targets */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Status")
	EVaelStatus AppliedStatus = EVaelStatus::None;

	/** Seconds the condition lasts */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Status", meta = (ClampMin = 0, EditCondition = "AppliedStatus != EVaelStatus::None"))
	float StatusDuration = 0.0f;

	/** Damage per second while burning */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Status", meta = (ClampMin = 0, EditCondition = "AppliedStatus == EVaelStatus::Burning"))
	float StatusDamagePerSecond = 0.0f;

	/** Projectile actor to spawn */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile", meta = (EditCondition = "Delivery == EVaelSpellDelivery::Projectile"))
	TSubclassOf<AVaelSpellProjectile> ProjectileClass;

	/** Flight speed in cm/s */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::Projectile"))
	float ProjectileSpeed = 1500.0f;

	/** Collision radius in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile", meta = (ClampMin = 1, EditCondition = "Delivery == EVaelSpellDelivery::Projectile"))
	float ProjectileRadius = 30.0f;

	/** Seconds until the projectile fizzles out */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile", meta = (ClampMin = 0.1, EditCondition = "Delivery == EVaelSpellDelivery::Projectile"))
	float ProjectileLifetime = 1.1f;

	/** Number of additional enemies the projectile flies through after its first hit */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::Projectile"))
	int32 ProjectilePierce = 0;

	/** How far away the first target of the chain may be, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Chain", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::Chain"))
	float ChainRange = 1330.0f;

	/** How far the chain jumps from one target to the next, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Chain", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::Chain"))
	float ChainJumpRange = 560.0f;

	/** Highest number of targets the chain hits */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Chain", meta = (ClampMin = 1, EditCondition = "Delivery == EVaelSpellDelivery::Chain"))
	int32 ChainMaxTargets = 5;

	/** The first target has to be within this angle of the aim direction, in degrees */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Chain", meta = (ClampMin = 1, ClampMax = 180, EditCondition = "Delivery == EVaelSpellDelivery::Chain"))
	float ChainAimHalfAngle = 29.0f;

	/** Reach of the cone in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cone", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::Cone"))
	float ConeRange = 450.0f;

	/** Half opening angle of the cone in degrees */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cone", meta = (ClampMin = 1, ClampMax = 180, EditCondition = "Delivery == EVaelSpellDelivery::Cone"))
	float ConeHalfAngle = 45.0f;
};
