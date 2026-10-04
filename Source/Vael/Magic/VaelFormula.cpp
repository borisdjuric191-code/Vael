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

FPrimaryAssetId UVaelFormula::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(PrimaryAssetType, GetFName());
}
