// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Items/VaelItemTypes.h"
#include "Items/VaelMaterial.h"
#include "Magic/VaelElementTypes.h"
#include "VaelCreatureData.generated.h"

class AVaelCreature;

/**
 *  Values of a kind of creature that every creature has.
 *  The defaults of each data class are the values of the browser prototype (1 tile = 140 cm),
 *  so a creature without a data asset still behaves like the prototype.
 */
UCLASS(Abstract, BlueprintType)
class UVaelCreatureData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** Name shown to the players */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Creature")
	FText DisplayName;

	/** Class spawned for this kind of creature by spawners and console commands */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Creature")
	TSubclassOf<AVaelCreature> CreatureClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attributes", meta = (ClampMin = 1))
	float MaxHealth = 100.0f;

	/** Normal movement speed in cm/s */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attributes", meta = (ClampMin = 0))
	float MoveSpeed = 300.0f;

	/** Radius of the collision capsule in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attributes", meta = (ClampMin = 10))
	float CollisionRadius = 50.0f;

	/** Half height of the collision capsule in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attributes", meta = (ClampMin = 10))
	float CollisionHalfHeight = 60.0f;

	/** Damage of an element is multiplied by this. Above 1 is a weakness, below 1 a resistance, missing elements count as 1. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attributes")
	TMap<EVaelElement, float> ElementMultipliers;

	/** Knockback the creature takes is multiplied by this */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attributes", meta = (ClampMin = 0))
	float KnockbackMultiplier = 1.0f;

	/** Stagger after hits lasts this many times as long; bosses barely stagger */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attributes", meta = (ClampMin = 0))
	float StaggerMultiplier = 1.0f;

	/** Stuns by spells like the earthquake last this many times as long; bosses shake them off quickly */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attributes", meta = (ClampMin = 0))
	float StunMultiplier = 1.0f;

	/** Whirlwinds drag the creature in at this share of their pull; bosses stand firm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attributes", meta = (ClampMin = 0))
	float PullMultiplier = 1.0f;

	/** False for bosses: a corrupted region never sends a marked, stronger version */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attributes")
	bool bCanBeMarked = true;

	/** False for bosses: spells can't freeze them */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attributes")
	bool bCanBeFrozen = true;

	/** Materials the creature drops. Each roll happens once, every player gets their own copy. Marked creatures drop Markkristalle on top. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot")
	TArray<FVaelLootEntry> Loot;

	/** Chance from 0 to 1 that a health orb drops (prototype: 0.3) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot", meta = (ClampMin = 0, ClampMax = 1))
	float HealthOrbChance = 0.3f;

	/** Chance from 0 to 1 that a mana orb drops (prototype: 0.32) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot", meta = (ClampMin = 0, ClampMax = 1))
	float ManaOrbChance = 0.32f;

	/** Health orbs that always drop, like after a boss */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot", meta = (ClampMin = 0))
	int32 GuaranteedHealthOrbs = 0;

	/** Chance from 0 to 1, rolled for every player, that an item drops for them. Creatures drop parts, not gear; humans and bosses do. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot", meta = (ClampMin = 0, ClampMax = 1))
	float ItemDropChance = 0.0f;

	/** Items every player gets for sure, like after a boss */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot", meta = (ClampMin = 0))
	int32 GuaranteedItems = 0;

	/** Dropped items are at least this rare */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot")
	EVaelRarity MinItemRarity = EVaelRarity::Common;

	/** Players closer than this are noticed, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Behavior", meta = (ClampMin = 0))
	float AggroRange = 1540.0f;

	/** Elements of the formula the players learn when they kill the creature, empty for none */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Formulas")
	TArray<EVaelElement> FormulaOnDeath;

	/** Placeholder color of the body */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Appearance")
	FLinearColor BodyColor = FLinearColor::Gray;
};

/**
 *  Glutkriecher: burrows through the ash towards the players, surfaces next to them and explodes.
 *  Water puts the fuse out; a doused crawler keeps fighting with its claws.
 */
UCLASS(BlueprintType)
class UVaelEmberCrawlerData : public UVaelCreatureData
{
	GENERATED_BODY()

public:

	UVaelEmberCrawlerData();

	/** Damage taken while burrowed is multiplied by this, except for earth */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Burrow", meta = (ClampMin = 0))
	float BurrowedDamageMultiplier = 0.5f;

	/** Earth damage taken while burrowed is multiplied by this */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Burrow", meta = (ClampMin = 0))
	float BurrowedEarthMultiplier = 1.4f;

	/** Share of the move speed while burrowing towards a player */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Burrow", meta = (ClampMin = 0))
	float BurrowChaseSpeedScale = 0.9f;

	/** Share of the move speed while burrowing back home */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Burrow", meta = (ClampMin = 0))
	float BurrowReturnSpeedScale = 0.5f;

	/** The crawler surfaces when a player is this close, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Burrow", meta = (ClampMin = 0))
	float SurfaceDistance = 364.0f;

	/** Seconds from surfacing until the fuse is lit */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Explosion", meta = (ClampMin = 0))
	float SurfaceDuration = 0.45f;

	/** Seconds the fuse burns before the explosion */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Explosion", meta = (ClampMin = 0))
	float FuseDuration = 1.35f;

	/** Share of the move speed while the fuse burns */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Explosion", meta = (ClampMin = 0))
	float FuseChaseSpeedScale = 0.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Explosion", meta = (ClampMin = 0))
	float ExplosionRadius = 266.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Explosion", meta = (ClampMin = 0))
	float ExplosionDamage = 22.0f;

	/** Fire damage of the explosion to other creatures */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Explosion", meta = (ClampMin = 0))
	float ExplosionDamageToCreatures = 20.0f;

	/** Speed at which the explosion pushes other creatures away, in cm/s */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Explosion", meta = (ClampMin = 0))
	float ExplosionKnockback = 560.0f;

	/** Radius of the fire the explosion leaves behind, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Explosion", meta = (ClampMin = 0))
	float FireRadius = 182.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Explosion", meta = (ClampMin = 0))
	float FireLifetime = 4.0f;

	/** Damage per second of the fire to players */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Explosion", meta = (ClampMin = 0))
	float FireDamagePerSecond = 5.0f;

	/** Players this close to an explosion learn the formula below, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Formulas", meta = (ClampMin = 0))
	float WitnessDistance = 644.0f;

	/** Formula learned by surviving an explosion from close by */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Formulas")
	TArray<EVaelElement> FormulaOnWitnessedExplosion;

	/** Seconds a doused crawler needs before it crawls on */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Doused", meta = (ClampMin = 0))
	float DousedDuration = 1.6f;

	/** Share of the move speed of a doused crawler */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Doused", meta = (ClampMin = 0))
	float CrawlSpeedScale = 1.3f;

	/** Distance between the centers at which a doused crawler claws, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Doused", meta = (ClampMin = 0))
	float ClawRange = 126.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Doused", meta = (ClampMin = 0))
	float ClawDamage = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Doused", meta = (ClampMin = 0))
	float ClawInterval = 1.1f;
};

/**
 *  Aschharpyie: circles the players in the air and dives at them after a short warning.
 *  Wind throws it out of the air.
 */
UCLASS(BlueprintType)
class UVaelAshHarpyData : public UVaelCreatureData
{
	GENERATED_BODY()

public:

	UVaelAshHarpyData();

	/** Height of the body above the collision capsule while circling, in cm. The capsule stays low so spells still hit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flight", meta = (ClampMin = 0))
	float FlightHeight = 140.0f;

	/** Radius of the circle around the home while no player is near, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flight", meta = (ClampMin = 0))
	float IdleCircleRadius = 350.0f;

	/** Radians per second along the idle circle */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flight", meta = (ClampMin = 0))
	float IdleCircleSpeed = 0.8f;

	/** Radius of the circle around the target player, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flight", meta = (ClampMin = 0))
	float CombatCircleRadius = 560.0f;

	/** Radians per second along the circle around the target */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flight", meta = (ClampMin = 0))
	float CombatCircleSpeed = 1.1f;

	/** The harpy gives up when its target is farther than this, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flight", meta = (ClampMin = 0))
	float LoseInterestRange = 2100.0f;

	/** Seconds of warning before a dive */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dive", meta = (ClampMin = 0))
	float DiveWarningDuration = 0.6f;

	/** Speed of a dive in cm/s */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dive", meta = (ClampMin = 0))
	float DiveSpeed = 1750.0f;

	/** Seconds a dive lasts */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dive", meta = (ClampMin = 0))
	float DiveDuration = 0.6f;

	/** Damage to every player the dive touches */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dive", meta = (ClampMin = 0))
	float DiveDamage = 11.0f;

	/** Seconds the harpy needs to climb back up after a dive */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dive", meta = (ClampMin = 0))
	float RecoverDuration = 0.9f;

	/** Seconds between noticing a player and the first dive, picked at random in this range */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dive")
	FVector2D FirstDiveDelay = FVector2D(1.0f, 2.5f);

	/** Seconds between two dives, picked at random in this range */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dive")
	FVector2D DiveInterval = FVector2D(2.2f, 4.0f);

	/** Players this close to a dive learn the formula below, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Formulas", meta = (ClampMin = 0))
	float WitnessDistance = 364.0f;

	/** Formula learned by watching a dive from close by */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Formulas")
	TArray<EVaelElement> FormulaOnWitnessedDive;

	/** Knockback by air is multiplied by this */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wind", meta = (ClampMin = 0))
	float AirKnockbackMultiplier = 1.8f;

	/** Seconds the harpy tumbles after a hit by air */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wind", meta = (ClampMin = 0))
	float AirStunDuration = 0.6f;

	/** True if the harpy calls more harpies with a screech */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Screech")
	bool bCanScreech = false;

	/** Seconds from noticing a player until the first screech */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Screech", meta = (ClampMin = 0, EditCondition = "bCanScreech"))
	float FirstScreechDelay = 6.0f;

	/** Seconds between two screeches */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Screech", meta = (ClampMin = 0, EditCondition = "bCanScreech"))
	float ScreechInterval = 9.0f;

	/** Harpies called by one screech */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Screech", meta = (ClampMin = 0, EditCondition = "bCanScreech"))
	int32 ScreechSpawnCount = 2;

	/** No harpies are called while this many are already near */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Screech", meta = (ClampMin = 0, EditCondition = "bCanScreech"))
	int32 ScreechMaxNearby = 5;

	/** Range in which harpies count as near, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Screech", meta = (ClampMin = 0, EditCondition = "bCanScreech"))
	float ScreechRange = 1260.0f;

	/** Kind of harpy called by the screech, the default harpy if empty */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Screech", meta = (EditCondition = "bCanScreech"))
	TObjectPtr<UVaelCreatureData> ScreechSpawnData;
};

/**
 *  Harpyien-Aelteste: a big, tough harpy that calls more harpies and carries a formula fragment.
 *  Only a different set of default values for the harpy.
 */
UCLASS(BlueprintType)
class UVaelHarpyElderData : public UVaelAshHarpyData
{
	GENERATED_BODY()

public:

	UVaelHarpyElderData();
};

/**
 *  Prediger der Narbe: keeps its distance, shoots Mark bolts and calls bone spikes out of the ground in a line.
 */
UCLASS(BlueprintType)
class UVaelPreacherData : public UVaelCreatureData
{
	GENERATED_BODY()

public:

	UVaelPreacherData();

	/** The preacher backs away from players closer than this, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Distance", meta = (ClampMin = 0))
	float MinDistance = 630.0f;

	/** The preacher walks towards players farther than this, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Distance", meta = (ClampMin = 0))
	float MaxDistance = 980.0f;

	/** Share of the move speed used to walk sideways */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Distance", meta = (ClampMin = 0))
	float StrafeScale = 0.6f;

	/** Share of the move speed while walking back home */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Distance", meta = (ClampMin = 0))
	float ReturnSpeedScale = 0.6f;

	/** Seconds between two bolts, picked at random in this range */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bolt")
	FVector2D BoltInterval = FVector2D(1.8f, 2.6f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bolt", meta = (ClampMin = 0))
	float BoltDamage = 11.0f;

	/** Speed of a bolt in cm/s */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bolt", meta = (ClampMin = 0))
	float BoltSpeed = 1050.0f;

	/** Radius of a bolt in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bolt", meta = (ClampMin = 1))
	float BoltRadius = 35.0f;

	/** Seconds a bolt flies */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bolt", meta = (ClampMin = 0))
	float BoltLifetime = 1.8f;

	/** Seconds until the first spike line, picked at random in this range */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spikes")
	FVector2D FirstSpikeDelay = FVector2D(4.0f, 7.0f);

	/** Seconds between two spike lines, picked at random in this range */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spikes")
	FVector2D SpikeInterval = FVector2D(6.0f, 8.0f);

	/** Spike lines are only called at players closer than this, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spikes", meta = (ClampMin = 0))
	float SpikeRange = 1120.0f;

	/** Seconds the preacher stands still and calls before the first spike breaks out */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spikes", meta = (ClampMin = 0))
	float SpikeWindup = 1.05f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spikes", meta = (ClampMin = 1))
	int32 SpikeCount = 9;

	/** Distance between two spikes in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spikes", meta = (ClampMin = 1))
	float SpikeSpacing = 126.0f;

	/** Seconds between two spikes breaking out */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spikes", meta = (ClampMin = 0))
	float SpikeStagger = 0.05f;

	/** Reach of a single spike in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spikes", meta = (ClampMin = 1))
	float SpikeRadius = 98.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spikes", meta = (ClampMin = 0))
	float SpikeDamage = 16.0f;

	/** Players this close to the line learn the formula below, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Formulas", meta = (ClampMin = 0))
	float WitnessDistance = 252.0f;

	/** Formula learned by watching a spike line from close by */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Formulas")
	TArray<EVaelElement> FormulaOnWitnessedSpikes;
};

/**
 *  Glutkoenigin: mother of all ember crawlers, the boss of the Aschenmark.
 *  Sleeps until the players come close, bites, spits fire and calls her brood.
 *  Below a share of her health she also sends out rings of flame and charges.
 */
UCLASS(BlueprintType)
class UVaelEmberQueenData : public UVaelCreatureData
{
	GENERATED_BODY()

public:

	UVaelEmberQueenData();

	/** Health with one player is MaxHealth times this ... */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attributes", meta = (ClampMin = 0))
	float HealthScaleBase = 0.7f;

	/** ... plus this for every player in the game */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attributes", meta = (ClampMin = 0))
	float HealthScalePerPlayer = 0.3f;

	/** Seconds between waking up and attacking */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Behavior", meta = (ClampMin = 0))
	float WakeDuration = 1.6f;

	/** The second phase starts below this share of the health */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Behavior", meta = (ClampMin = 0, ClampMax = 1))
	float SecondPhaseHealthShare = 0.55f;

	/** Share of the move speed while wet */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Behavior", meta = (ClampMin = 0))
	float WetSpeedScale = 0.7f;

	/** Distance between the centers at which she stops and bites, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bite", meta = (ClampMin = 0))
	float BiteRange = 336.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bite", meta = (ClampMin = 0))
	float BiteDamage = 16.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bite", meta = (ClampMin = 0))
	float BiteInterval = 1.2f;

	/** Seconds between two volleys of fire spit */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spit", meta = (ClampMin = 0))
	float SpitInterval = 2.3f;

	/** Seconds between two volleys while she is wet */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spit", meta = (ClampMin = 0))
	float SpitIntervalWet = 3.2f;

	/** Seconds taken off the interval in the second phase */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spit", meta = (ClampMin = 0))
	float SpitIntervalReductionSecondPhase = 0.5f;

	/** Seconds from the start of the fight until the first volley */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spit", meta = (ClampMin = 0))
	float FirstSpitDelay = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spit", meta = (ClampMin = 1))
	int32 SpitCount = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spit", meta = (ClampMin = 1))
	int32 SpitCountSecondPhase = 5;

	/** Angle between two globs of a volley in degrees */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spit", meta = (ClampMin = 0))
	float SpitSpreadDegrees = 16.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spit", meta = (ClampMin = 0))
	float SpitDamage = 12.0f;

	/** Speed of a glob in cm/s */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spit", meta = (ClampMin = 0))
	float SpitSpeed = 980.0f;

	/** Radius of a glob in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spit", meta = (ClampMin = 1))
	float SpitRadius = 49.0f;

	/** Seconds a glob flies */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spit", meta = (ClampMin = 0))
	float SpitLifetime = 1.6f;

	/** Radius of the fire a glob leaves where it lands, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spit", meta = (ClampMin = 0))
	float SpitFireRadius = 126.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spit", meta = (ClampMin = 0))
	float SpitFireLifetime = 2.5f;

	/** Damage per second of that fire to players */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spit", meta = (ClampMin = 0))
	float SpitFireDamagePerSecond = 5.0f;

	/** Kind of creature she calls, the default ember crawler if empty */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Brood")
	TObjectPtr<UVaelCreatureData> BroodData;

	/** Seconds from the start of the fight until she first calls her brood */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Brood", meta = (ClampMin = 0))
	float FirstBroodDelay = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Brood", meta = (ClampMin = 0))
	float BroodInterval = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Brood", meta = (ClampMin = 0))
	float BroodIntervalSecondPhase = 8.0f;

	/** Crawlers called at once */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Brood", meta = (ClampMin = 0))
	int32 BroodCount = 3;

	/** No brood is called while this many crawlers are near */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Brood", meta = (ClampMin = 0))
	int32 BroodMaxNearby = 6;

	/** Range in which crawlers count as near, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Brood", meta = (ClampMin = 0))
	float BroodNearbyRange = 1680.0f;

	/** Distance from her center at which the brood appears, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Brood", meta = (ClampMin = 0))
	float BroodSpawnDistance = 420.0f;

	/** Seconds from the start of the second phase until the first ring of flame */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flame Ring", meta = (ClampMin = 0))
	float FirstRingDelay = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flame Ring", meta = (ClampMin = 0))
	float RingInterval = 6.5f;

	/** Speed at which the ring grows, in cm/s */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flame Ring", meta = (ClampMin = 0))
	float RingSpeed = 728.0f;

	/** Radius at which the ring fades, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flame Ring", meta = (ClampMin = 0))
	float RingMaxRadius = 1540.0f;

	/** Players this close to the ring line are hit, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flame Ring", meta = (ClampMin = 0))
	float RingHalfWidth = 63.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flame Ring", meta = (ClampMin = 0))
	float RingDamage = 20.0f;

	/** Seconds from the start of the second phase until the first charge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charge", meta = (ClampMin = 0))
	float FirstChargeDelay = 4.0f;

	/** Seconds between two charges, picked at random in this range */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charge")
	FVector2D ChargeInterval = FVector2D(7.0f, 9.0f);

	/** She only charges at players closer than this, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charge", meta = (ClampMin = 0))
	float ChargeRange = 1260.0f;

	/** Seconds of warning before she charges */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charge", meta = (ClampMin = 0))
	float ChargeWindup = 0.7f;

	/** Seconds the charge lasts */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charge", meta = (ClampMin = 0))
	float ChargeDuration = 0.8f;

	/** Speed of the charge in cm/s */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charge", meta = (ClampMin = 0))
	float ChargeSpeed = 1260.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charge", meta = (ClampMin = 0))
	float ChargeDamage = 24.0f;
};
