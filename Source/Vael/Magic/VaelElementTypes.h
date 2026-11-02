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
	Strike,
	/** Calls a short-lived servant to the aimed point, like the Lehmgolem */
	Summon,
	/** Stones circle around the caster for some seconds, hit enemies they touch and stop enemy projectiles, like the Steinkreis */
	Orbit,
	/** The caster steps through the Mark to the aimed point, is briefly untouchable and leaves a trail of Mark, like the Schattensprung */
	Teleport,
	/** The enemy that fell last near the aimed point rises for some seconds and fights for the caster, like the Markgeburt */
	Raise
};

/** Own look of a flying spell, shown together with its trail effect */
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
	RockShard,
	/** A long crystal of ice with two points and little side spikes, loses frost and splinters on every enemy it pierces, like the Frostlanze */
	IceLance,
	/** A glowing core under tumbling plates of crust that drips lava and splashes it all around where it bursts, like the Lavaball */
	LavaBall,
	/** A crooked black splinter with a twitching violet edge whose motes rise instead of fall, like the Marksplitter */
	MarkShard,
	/** A wide rolling crest of water with foam on top that throws spray behind it and splashes along its foot, like the Flutwelle */
	TidalWave,
	/** A swirling ball of fire: a glowing core in a coat of flames that lick backwards, smoke and sparks behind it; it bursts in a blast of flames, like the Feuerball */
	Fireball,
	/** The seed head of the Glutdistel itself, tumbling in a small ball of flames, shedding glowing thistledown; it bursts in thorns and sparks */
	ThistleSeed,
	/** A sharp blade of the Windhalm flying like a dart and turning round its flight, a streak of ash behind it (embers when it burns),
	 *  the air shimmering at its point, pollen glittering off it; it ends in a small ring of wind */
	GrassDart,
	/** A wide wall of flame rolling forward across its whole breadth, embers before it, like the Feuerwalze */
	FireRoller,
	/** A black spore with thorns all round and a violet heart, tumbling and leaving motes, like the Sporenschwarm */
	Spore,
	/** A wobbling glob of boiling dark blood that throws drops, like the Blutsieden */
	BloodGlob,
	/** A ball of black fire: a void in tongues of violet flame, like the Schwarzes Feuer */
	BlackFire
};

/** Own look of a storm around the caster */
UENUM(BlueprintType)
enum class EVaelAuraLook : uint8
{
	/** The effect of its element, or the whirl of a sand storm */
	Plain,
	/** Splinters of bone racing round the caster in pale dust, like the Knochensturm */
	BoneStorm,
	/** A black sun with a burning corona over the head that rains embers, like the Schwarze Sonne */
	BlackSun
};

/** Gesture the mage makes with arm and fingers instead of a cast montage, posed in code */
UENUM(BlueprintType)
enum class EVaelCastGesture : uint8
{
	/** No gesture: the cast montage plays */
	None,
	/** Boris 2026-10-30: the right arm snaps forward, index and middle finger stretched out, the spell leaves their tips */
	TwoFingerPoint,
	/** Boris 2026-11-01: the Funke with more power: the left hand grips the right forearm, a longer wind-up before the shot */
	GripPoint,
	/** Boris 2026-11-01: gathers at the hip, thrusts the open palm forward with the forearm gripped, shakes while the beam burns, then hangs forward spent */
	PalmBeam,
	/** Boris 2026-11-01: scoop and flick: the hand scoops from the hip in a soft arc, the wrist snaps, the water flies off the fingers; as short as the Funke, soft where fire is hard */
	WaterFlick,
	/** Boris 2026-10-30: breathes in with the hands cupped before the chin, then blows the cloud out of the mouth, hands opening forward */
	BlowOut,
	/** Boris 2026-11-02: the scoop grown into a sweep (Welle): the hand skims low across the body in a wide arc, tips palm forward at its end and the wrist snaps */
	WaveSweep,
	/** Boris 2026-11-02: both hands lift the sea (Flutwelle): scoop low at the sides, heave up over the head, throw forward and down; spent afterwards */
	WaveHeave,
	/** Boris 2026-11-02: earth is hard where water is soft (Steinbrocken): the fist cocks at the far shoulder and lashes out back of the hand first, hard stop */
	Backfist,
	/** Boris 2026-11-02: Felswand: down in a crouch, both fists low with their backs forward, they climb up with the wall and hold a moment */
	WallRise,
	/** Boris 2026-11-02: Erdbeben: both fists together over the head, slammed down into the ground at once; crouched for a moment afterwards */
	GroundSlam
};

/** What rises out of the ground where a strike of a spell lands */
UENUM(BlueprintType)
enum class EVaelStrikeLook : uint8
{
	/** One plain spike in the color of the element */
	Spike,
	/** A cluster of pale, leaning bone spikes over a violet glow, shards and motes rise, like the Knochendornen */
	BoneSpikes,
	/** A cluster of dark rock spikes with glowing edges over molten ground, lava splashes up, like the Magmariss */
	MagmaSpikes,
	/** A column of boiling water bursts out of the ground, foams over at its top, rains down and runs out as a foaming ring, like the Geysir */
	Geyser,
	/** No spike: a dark storm cloud gathers above and lances of ice rain down, a volley shattering where each strike lands, like the Hagelsturm */
	Hail,
	/** No spike: a burning rock falls out of the sky along a slanted path while the warning fills, like the Meteor */
	Meteor,
	/** No spike: the ground tears open, every pulse throws black lightning out of the wound, like the Offene Wunde */
	OpenWound
};

/** Own look of a burst around the caster, shown together with its impact effect */
UENUM(BlueprintType)
enum class EVaelNovaLook : uint8
{
	/** Only the outline of the burst */
	Plain,
	/** The ground cracks open in a widening ring, rocks break out and sink back amid dust, like the Erdbeben */
	Quake,
	/** A violet dome of pressure swells out of a black hole at the caster's feet, dark shards rise, like the Markstoß */
	MarkPulse
};

/** Own look of a cone spell, shown together with its cast effect */
UENUM(BlueprintType)
enum class EVaelConeLook : uint8
{
	/** Only the outline of the cone */
	Plain,
	/** A curved wall of pressure rolls along the cone with streaks of wind and kicks up dust, like the Windstoß */
	WindGust,
	/** A curved wall of water with a foaming crest rolls along the cone, throws up drops and mist and splashes where it passes, like the Welle */
	WaterWave
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
	Slow,
	/** Mist: the caster and their allies inside are hidden from enemies further away, who lose them as a target */
	Veil,
	/** Hot spring: mages standing in it can draw both fire and water from it */
	HotSpring,
	/** Black water of the Mark: slows and marks enemies, mages can draw the Mark from it, it seeps corruption into the region */
	Blackwater,
	/** Fever mist: enemies inside fall into fever and go for each other */
	Fever
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
	Slowed,
	/** Marked by the Mark: takes more damage from everything */
	Marked,
	/** Flees from the players and doesn't attack */
	Feared,
	/** Mad with fever: goes for other creatures, and they can hurt each other */
	Fevered,
	/** Confused (Wachbeere): creatures stagger about and don't attack, a player's steps drift off course */
	Confused,
	/** Lured by a fairy of the Feenragwurz: creatures go after her and don't attack */
	Lured,
	/** Stuck in pitch (Harzkolben): moves at a share of its speed, fire on it burns twice as hard and uses the pitch up */
	Pitched
};

/** When a charge of Mark left in a target bursts */
UENUM(BlueprintType)
enum class EVaelChargeMode : uint8
{
	/** No charge */
	None,
	/** Bursts after its time, like the Kernsplitter */
	Delayed,
	/** Bursts only if the target dies within its time, and the burst charges those it hits again, like the Blutsieden */
	OnDeath
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
