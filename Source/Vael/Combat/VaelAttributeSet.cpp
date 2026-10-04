// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VaelAttributeSet.h"
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
		const float Damage = GetIncomingDamage();
		SetIncomingDamage(0.0f);

		if (Damage > 0.0f)
		{
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
