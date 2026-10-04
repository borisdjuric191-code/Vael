// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VaelElementTypes.generated.h"

/** The primal elements a mage combines into formulas */
UENUM(BlueprintType)
enum class EVaelElement : uint8
{
	Fire,
	Water,
	Earth,
	Air,
	Mark
};

/** How a formula gets into the grimoire */
UENUM(BlueprintType)
enum class EVaelFormulaSource : uint8
{
	/** Known from the beginning */
	Start,
	/** Can be discovered by experimenting */
	Free,
	/** Has to be found in the world, trying it only produces an echo */
	Sealed,
	/** Available once the Mark has awakened */
	Mark
};

/** How a formula reaches its targets */
UENUM(BlueprintType)
enum class EVaelSpellDelivery : uint8
{
	Projectile,
	Cone
};

/** Outcome of trying to cast the queued elements */
UENUM(BlueprintType)
enum class EVaelCastResult : uint8
{
	Success,
	/** No elements were queued */
	EmptyQueue,
	/** No formula exists for this combination */
	NoFormula,
	/** The formula exists but has not been learned yet */
	UnknownFormula,
	NotEnoughMana,
	/** The ability system refused to activate the formula */
	Blocked
};

namespace VaelElements
{
	/** Highest number of elements a formula can be made of */
	inline constexpr int32 MaxQueueSlots = 5;

	/** Key for a combination of elements that doesn't depend on their order: four bits hold the count of each element */
	inline uint32 MakeComboKey(TConstArrayView<EVaelElement> Elements)
	{
		uint32 Key = 0;
		for (const EVaelElement Element : Elements)
		{
			Key += 1u << (4 * static_cast<uint32>(Element));
		}
		return Key;
	}
}
