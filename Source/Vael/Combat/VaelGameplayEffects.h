// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "VaelGameplayEffects.generated.h"

/**
 *  Instant damage. The amount is set by the caller with the tag SetByCaller.Damage,
 *  the element is added to the effect as a dynamic asset tag.
 */
UCLASS()
class UVaelGE_Damage : public UGameplayEffect
{
	GENERATED_BODY()

public:

	UVaelGE_Damage();
};

/**
 *  A status without an effect of its own, like wet or frozen. Lasts for SetByCaller.Duration seconds.
 *  The status tag is added to the effect as a dynamically granted tag; what the status means is decided by whoever reads the tag.
 */
UCLASS()
class UVaelGE_Status : public UGameplayEffect
{
	GENERATED_BODY()

public:

	UVaelGE_Status();
};

/**
 *  Burning: deals SetByCaller.Damage at every step for SetByCaller.Duration seconds.
 *  The tag Status.Burning is added to the effect as a dynamically granted tag.
 */
UCLASS()
class UVaelGE_Burning : public UGameplayEffect
{
	GENERATED_BODY()

public:

	/** Time between two damage steps in seconds */
	static constexpr float StepInterval = 0.5f;

	UVaelGE_Burning();
};

/**
 *  Instant change of mana, used for the cost of formulas.
 *  The amount is set by the caller with the tag SetByCaller.Mana and is negative for costs.
 */
UCLASS()
class UVaelGE_ManaCost : public UGameplayEffect
{
	GENERATED_BODY()

public:

	UVaelGE_ManaCost();
};

/**
 *  Endless mana regeneration in small steps.
 *  The mana per step is set by the caller with the tag SetByCaller.Mana.
 */
UCLASS()
class UVaelGE_ManaRegen : public UGameplayEffect
{
	GENERATED_BODY()

public:

	/** Time between two regeneration steps in seconds */
	static constexpr float StepInterval = 0.1f;

	UVaelGE_ManaRegen();
};
