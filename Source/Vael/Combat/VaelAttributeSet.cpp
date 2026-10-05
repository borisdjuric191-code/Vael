// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VaelAttributeSet.h"
#include "Combat/VaelCharacterBase.h"
#include "GameplayEffectExtension.h"
#include "Vael.h"

void UVaelAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetManaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxMana());
	}
}

void UVaelAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	const FGameplayAttribute& Attribute = Data.EvaluatedData.Attribute;

	if (Attribute == GetIncomingDamageAttribute())
	{
		float Damage = GetIncomingDamage();
		SetIncomingDamage(0.0f);

		// Weaknesses and resistances of the target, for hits and damage over time alike
		AVaelCharacterBase* Target = Cast<AVaelCharacterBase>(GetOwningActor());
		FGameplayTagContainer DamageTags;

		if (Target != nullptr)
		{
			Data.EffectSpec.GetAllAssetTags(DamageTags);
			Damage *= Target->GetIncomingDamageMultiplier(DamageTags);
		}

		if (Damage > 0.0f)
		{
			if (Target != nullptr)
			{
				Target->OnDamageTaken(Damage, DamageTags);
			}

			SetHealth(FMath::Clamp(GetHealth() - Damage, 0.0f, GetMaxHealth()));

			UE_LOG(LogVael, VeryVerbose, TEXT("'%s' takes %.1f damage, health now %.1f"), *GetNameSafe(GetOwningActor()), Damage, GetHealth());
		}
	}
	else if (Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
	}
	else if (Attribute == GetManaAttribute())
	{
		SetMana(FMath::Clamp(GetMana(), 0.0f, GetMaxMana()));
	}
}
