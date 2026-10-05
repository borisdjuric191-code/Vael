// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "VaelAttributeSet.generated.h"

#define VAEL_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 *  Core attributes of everything that can fight: players and creatures.
 */
UCLASS()
class UVaelAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:

	/** Keeps health and mana inside their limits */
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	/** Turns incoming damage into lost health */
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	UPROPERTY(BlueprintReadOnly, Category="Attributes")
	FGameplayAttributeData Health;
	VAEL_ATTRIBUTE_ACCESSORS(UVaelAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, Category="Attributes")
	FGameplayAttributeData MaxHealth;
	VAEL_ATTRIBUTE_ACCESSORS(UVaelAttributeSet, MaxHealth)

	UPROPERTY(BlueprintReadOnly, Category="Attributes")
	FGameplayAttributeData Mana;
	VAEL_ATTRIBUTE_ACCESSORS(UVaelAttributeSet, Mana)

	UPROPERTY(BlueprintReadOnly, Category="Attributes")
	FGameplayAttributeData MaxMana;
	VAEL_ATTRIBUTE_ACCESSORS(UVaelAttributeSet, MaxMana)

	/** Corruption by the Mark, 0 (pure) to 100. Grows with every Mark element cast. */
	UPROPERTY(BlueprintReadOnly, Category="Attributes")
	FGameplayAttributeData Corruption;
	VAEL_ATTRIBUTE_ACCESSORS(UVaelAttributeSet, Corruption)

	/** Meta attribute: damage effects write here, the value is moved to Health right away and never kept */
	UPROPERTY(BlueprintReadOnly, Category="Attributes")
	FGameplayAttributeData IncomingDamage;
	VAEL_ATTRIBUTE_ACCESSORS(UVaelAttributeSet, IncomingDamage)
};
