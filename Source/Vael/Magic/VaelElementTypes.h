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
	Cone,
	/** Strikes the first enemy in the aim direction and jumps on to others nearby */
	Chain,
	/** A projectile that bursts where its flight ends and may leave a patch on the ground */
	Explosion,
	/** Places a patch on the ground at the aimed point */
	GroundArea,
	/** The caster rushes in the aim direction and bursts where the dash ends */
	Dash,
	/** Raises a wall of rock blocks across the aim direction at the aimed point */
	Wall,
	/** A channeled beam from the hands of the caster that burns everything along it for some seconds */
	Beam,
	/** Hits everyone around the caster at once */
	Nova,
	/** A slow whirlwind that wanders in the aim direction, pulls enemies in and turns into a fire whirl over fire */
	Vortex,
	/** A storm around the caster for some seconds that grinds and blinds everyone close by */
	Aura,
	/** A line of spikes breaks out of the ground in the aim direction, one after the other, until a wall */
	SpikeLine,
	/** A single strike at the aimed point that lands after a warning, like a meteor or a geyser, and may leave a patch on the ground */
	Strike
};

/** Look of a flying spell while it has no effect of its own */
UENUM(BlueprintType)
enum class EVaelProjectileLook : uint8
{
	/** A plain ball in the color of the element */
	Sphere,
	/** A slim, glowing streak that keeps shedding little embers in flight, like the Funke */
	Spark,
	/** A round, softly wobbling ball of water that bursts under pressure where it hits, like the Wassergeschoss */
	WaterOrb,
	/** A pointed stone that spins in its fast flight and loses sand and pebbles, like the Steinbrocken; it shatters where it hits */
	RockShard
};

/** What a patch on the ground does to the enemies of its creator standing in it */
UENUM(BlueprintType)
enum class EVaelGroundEffect : uint8
{
	/** Only its element and damage; mages can draw the element from it */
	None,
	/** Steam: enemies inside can't see and don't start attacks */
	Blind,
	/** Mud: walking enemies are slowed, flying ones aren't */
	Slow
};

/** Conditions a hit can leave on its target */
UENUM(BlueprintType)
enum class EVaelStatus : uint8
{
	None,
	/** Doubles lightning damage, weakens fire */
	Wet,
	/** Takes fire damage over time, water puts it out */
	Burning,
	/** Can't move, earth shatters it for extra damage */
	Frozen,
	/** Moves at a share of its speed, walking or flying */
	Slowed
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
	/** The formula exists but is sealed: it has to be found in the world first */
	UnknownFormula,
	/** Experimenting with an undiscovered formula went wrong and hurt the caster */
	UnstableDischarge,
	NotEnoughMana,
	/** The formula uses the Mark, which still sleeps (until Act III) */
	MarkAsleep,
	/** The quick slot still has to cool down */
	OnCooldown,
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
