// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelFormula.h"
#include "Animation/AnimMontage.h"
#include "Magic/VaelClayGolem.h"
#include "Magic/VaelFormulaAbility.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelSpellProjectile.h"
#include "VaelAssets.h"

const FPrimaryAssetType UVaelFormula::PrimaryAssetType(TEXT("VaelFormula"));

UVaelFormula::UVaelFormula()
{
	AbilityClass = UVaelFormulaAbility::StaticClass();
	ProjectileClass = AVaelSpellProjectile::StaticClass();
	SummonClass = AVaelClayGolem::StaticClass();
}

FVaelSpellHit UVaelFormula::MakeSpellHit(float Power) const
{
	FVaelSpellHit Hit;
	Hit.Damage = Damage * Power;
	Hit.Element = DamageElement;
	Hit.Knockback = Knockback;
	Hit.bLightning = bLightning;
	Hit.Status = AppliedStatus;
	Hit.StatusDuration = StatusDuration;
	Hit.StatusDamagePerSecond = StatusDamagePerSecond;
	Hit.StunDuration = StunDuration;
	Hit.LifeSteal = LifeSteal;
	Hit.Charge = Charge;
	Hit.ChargeTime = ChargeTime;
	Hit.ChargeDamage = ChargeDamage * Power;
	Hit.ChargeRadius = ChargeRadius;

	return Hit;
}

FVaelSpellHit UVaelFormula::MakeExplosionHit(float Power) const
{
	FVaelSpellHit Hit = MakeSpellHit(Power);
	Hit.Damage = ExplosionDamage * Power;
	Hit.Knockback = ExplosionKnockback;

	return Hit;
}

UAnimMontage* UVaelFormula::FindCastMontage() const
{
	if (UAnimMontage* OwnMontage = VaelAssets::LoadOptional(CastMontage))
	{
		return OwnMontage;
	}

	return UVaelMagicSettings::Get()->FindCastMontage(Delivery);
}

FVaelLoadedEffects UVaelFormula::LoadEffects() const
{
	return VaelEffects::Load(DamageElement, &Effects);
}

FPrimaryAssetId UVaelFormula::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(PrimaryAssetType, GetFName());
}
