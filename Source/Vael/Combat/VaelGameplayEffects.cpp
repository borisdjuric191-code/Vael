// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VaelGameplayEffects.h"
#include "Combat/VaelAttributeSet.h"
#include "Magic/VaelGameplayTags.h"

namespace
{
	/** Builds a magnitude that the caller of the effect sets through a tag */
	FGameplayEffectModifierMagnitude MakeSetByCallerMagnitude(const FGameplayTag& DataTag)
	{
		FSetByCallerFloat SetByCaller;
		SetByCaller.DataTag = DataTag;

		return FGameplayEffectModifierMagnitude(SetByCaller);
	}

	/** Builds a modifier that adds a caller-defined magnitude to an attribute */
	FGameplayModifierInfo MakeSetByCallerModifier(const FGameplayAttribute& Attribute, const FGameplayTag& DataTag)
	{
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = MakeSetByCallerMagnitude(DataTag);

		return Modifier;
	}
}

UVaelGE_Damage::UVaelGE_Damage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	Modifiers.Add(MakeSetByCallerModifier(UVaelAttributeSet::GetIncomingDamageAttribute(), VaelTags::SetByCaller_Damage));
}

UVaelGE_Status::UVaelGE_Status()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = MakeSetByCallerMagnitude(VaelTags::SetByCaller_Duration);
}

UVaelGE_Burning::UVaelGE_Burning()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = MakeSetByCallerMagnitude(VaelTags::SetByCaller_Duration);
	Period = FScalableFloat(StepInterval);
	bExecutePeriodicEffectOnApplication = false;
	Modifiers.Add(MakeSetByCallerModifier(UVaelAttributeSet::GetIncomingDamageAttribute(), VaelTags::SetByCaller_Damage));
}

UVaelGE_ManaCost::UVaelGE_ManaCost()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	Modifiers.Add(MakeSetByCallerModifier(UVaelAttributeSet::GetManaAttribute(), VaelTags::SetByCaller_Mana));
}

UVaelGE_ManaRegen::UVaelGE_ManaRegen()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Period = FScalableFloat(StepInterval);
	bExecutePeriodicEffectOnApplication = false;
	Modifiers.Add(MakeSetByCallerModifier(UVaelAttributeSet::GetManaAttribute(), VaelTags::SetByCaller_Mana));
}
