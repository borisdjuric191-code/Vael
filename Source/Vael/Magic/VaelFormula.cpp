// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelFormula.h"
#include "Magic/VaelFormulaAbility.h"
#include "Magic/VaelSpellProjectile.h"

const FPrimaryAssetType UVaelFormula::PrimaryAssetType(TEXT("VaelFormula"));

UVaelFormula::UVaelFormula()
{
	AbilityClass = UVaelFormulaAbility::StaticClass();
	ProjectileClass = AVaelSpellProjectile::StaticClass();
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

	return Hit;
}

FVaelSpellHit UVaelFormula::MakeExplosionHit(float Power) const
{
	FVaelSpellHit Hit = MakeSpellHit(Power);
	Hit.Damage = ExplosionDamage * Power;
	Hit.Knockback = ExplosionKnockback;

	return Hit;
}

FPrimaryAssetId UVaelFormula::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(PrimaryAssetType, GetFName());
}
