// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Items/VaelItemTypes.h"
#include "Items/VaelMaterial.h"
#include "Magic/VaelElementTypes.h"
#include "Nature/VaelRainBloom.h"
#include "VaelCreatureData.generated.h"

class AVaelCreature;
class UAnimSequence;
class UMaterialInterface;
class USkeletalMesh;
class UStaticMesh;
class UVaelElementSource;

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

	/** Entry of the compendium this kind of creature is researched under, like "Glutkriecher"; none for people, who are no subject of the bestiary */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Creature")
	FName CompendiumId;

	/** Kind of creature quests count, like "Glutkriecher" or "Prediger"; people have one too */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Creature")
	FName KindId;

	/** There is only one of this kind in the story, like the Glutkönigin: once defeated, spawners leave it out for good */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Creature")
	bool bUnique = false;

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

	/** How far it notices players by day and in the deep night, as a share of its ranges (Boris 2026-10-26: animals react to the time of day) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Behavior|Time of Day", meta = (ClampMin = 0))
	float DayAwareness = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Behavior|Time of Day", meta = (ClampMin = 0))
	float NightAwareness = 1.0f;

	/** Elements of the formula the players learn when they kill the creature, empty for none */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Formulas")
	TArray<EVaelElement> FormulaOnDeath;

	/** Placeholder color of the body */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Appearance")
	FLinearColor BodyColor = FLinearColor::Gray;

	/** Mesh of the creature factory (Scripts in Tools/Kreaturenfabrik), facing +Y like the Unreal mannequin. Set: it replaces the placeholder look,
	 *  and creatures of the family Vielbeiner walk on procedural legs. Empty: the placeholder or code look of the creature. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Appearance")
	TSoftObjectPtr<USkeletalMesh> Mesh;

	/** Turn of the mesh around the up axis, so it faces the front of the creature */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Appearance")
	float MeshYaw = -90.0f;

	/** Glow of the ember parts of the mesh (orange in its colors) at rest, 0 for none */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Appearance", meta = (ClampMin = 0))
	float MeshGlow = 0.0f;
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

	/** Whose water the bolts are: with a source the preacher shoots balls of water of the water kit that make wet. Empty for bolts of Mark. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bolt")
	TSoftObjectPtr<UVaelElementSource> BoltWaterSource;

	/** Seconds a ball of water keeps its target wet */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bolt", meta = (ClampMin = 0))
	float BoltWetDuration = 5.0f;
};

/**
 *  Test variant of the preacher for the water kit: shoots balls of black-violet Mark water instead of bolts,
 *  so they can be compared with the water of the mages. Spawned with the cheat VaelSpawn Markprediger.
 */
UCLASS(BlueprintType)
class UVaelMarkWaterPreacherData : public UVaelPreacherData
{
	GENERATED_BODY()

public:

	UVaelMarkWaterPreacherData();
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

/**
 *  Quellwaechter: the guardian of a Mark source, rising after the offering of Markkristalle.
 *  A hybrid of the creatures around the source, fused by the Mark into one being with a swarm consciousness:
 *  it bursts like an ember crawler, dives like a harpy and calls its swarm out of the source.
 *  At shares of its health it screams with the pain and rage of the Sleeper, and the swarm grows denser.
 */
UCLASS(BlueprintType)
class UVaelSourceGuardianData : public UVaelCreatureData
{
	GENERATED_BODY()

public:

	UVaelSourceGuardianData();

	/** Health with one player is MaxHealth times this ... */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attributes", meta = (ClampMin = 0))
	float HealthScaleBase = 0.7f;

	/** ... plus this for every player in the game */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attributes", meta = (ClampMin = 0))
	float HealthScalePerPlayer = 0.3f;

	/** Seconds it takes to rise out of the source before it attacks */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Behavior", meta = (ClampMin = 0))
	float RiseDuration = 2.0f;

	/** It screams each time its health falls below one of these shares; every scream makes the swarm denser */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Scream")
	TArray<float> ScreamHealthShares;

	/** Speed at which a scream throws players back, in cm/s */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Scream", meta = (ClampMin = 0))
	float ScreamKnockback = 900.0f;

	/** Reach of the scream in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Scream", meta = (ClampMin = 0))
	float ScreamRadius = 700.0f;

	/** Distance at which it stops and strikes, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Strike", meta = (ClampMin = 0))
	float StrikeRange = 260.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Strike", meta = (ClampMin = 0))
	float StrikeDamage = 14.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Strike", meta = (ClampMin = 0))
	float StrikeInterval = 1.3f;

	/** Seconds between two ember bursts around it, like a crawler exploding */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ember Burst", meta = (ClampMin = 0))
	float BurstInterval = 7.0f;

	/** Warning time before the burst breaks out */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ember Burst", meta = (ClampMin = 0))
	float BurstWarning = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ember Burst", meta = (ClampMin = 0))
	float BurstRadius = 350.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ember Burst", meta = (ClampMin = 0))
	float BurstDamage = 24.0f;

	/** Seconds between two dives, like a harpy, picked between X and Y */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dive")
	FVector2D DiveInterval = FVector2D(6.0f, 9.0f);

	/** Players farther away than this are not dived at, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dive", meta = (ClampMin = 0))
	float DiveRange = 1400.0f;

	/** Seconds it shows the line of the dive before it rushes */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dive", meta = (ClampMin = 0))
	float DiveWarning = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dive", meta = (ClampMin = 0))
	float DiveSpeed = 1800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dive", meta = (ClampMin = 0))
	float DiveDuration = 0.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dive", meta = (ClampMin = 0))
	float DiveDamage = 18.0f;

	/** Creatures of the swarm, called in turns; empty: ember crawlers and ash harpies */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Swarm")
	TArray<TObjectPtr<UVaelCreatureData>> SwarmData;

	/** Seconds between two calls of the swarm, before the first scream */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Swarm", meta = (ClampMin = 0))
	float SwarmInterval = 10.0f;

	/** Seconds each scream takes off the interval */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Swarm", meta = (ClampMin = 0))
	float SwarmIntervalReductionPerScream = 2.0f;

	/** Creatures per call, before the first scream */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Swarm", meta = (ClampMin = 1))
	int32 SwarmCount = 2;

	/** Creatures each scream adds to a call */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Swarm", meta = (ClampMin = 0))
	int32 SwarmCountPerScream = 1;

	/** No new swarm while this many of its creatures still live */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Swarm", meta = (ClampMin = 1))
	int32 SwarmMaxAlive = 8;
};

/**
 *  Spannhornkaefer (Wurzelforst, body family Vielbeiner): an artillery beetle. Keeps its distance, spreads its antler-like
 *  horns with a ratcheting sound until the thread between their tips locks into the middle horn with a "Klack", then lobs
 *  a shot in an arc at where the player stood, marked on the ground. Fire on the tensed thread burns it through; the beetle
 *  is defenseless for a while. First creature of the creature factory: a Tripo mesh with procedural legs.
 */
UCLASS(BlueprintType)
class UVaelHornBeetleData : public UVaelCreatureData
{
	GENERATED_BODY()

public:

	UVaelHornBeetleData();

	/** Tips of the side horns in the reference pose, component space of the mesh in cm (the thread runs between them) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Appearance")
	FVector HornTipFirst = FVector(-60.6f, 9.2f, 72.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Appearance")
	FVector HornTipSecond = FVector(60.8f, 5.4f, 73.8f);

	/** Tip of the middle horn, where the shot leaves */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Appearance")
	FVector MiddleHornTip = FVector(0.2f, 58.7f, 60.8f);

	/** Degrees each side horn swings outwards when fully spread */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Appearance", meta = (ClampMin = 0, ClampMax = 60))
	float HornSpread = 28.0f;

	/** The beetle backs away from players closer than this, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Distance", meta = (ClampMin = 0))
	float MinDistance = 550.0f;

	/** The beetle walks towards players farther than this, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Distance", meta = (ClampMin = 0))
	float MaxDistance = 900.0f;

	/** Degrees per second the beetle turns */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Distance", meta = (ClampMin = 1))
	float TurnSpeed = 220.0f;

	/** Seconds until the first shot, picked at random in this range */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot")
	FVector2D FirstShotDelay = FVector2D(1.0f, 2.2f);

	/** Seconds between two shots, picked at random in this range */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot")
	FVector2D ShotInterval = FVector2D(2.6f, 3.6f);

	/** Shots only go at players closer than this, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot", meta = (ClampMin = 0))
	float ShotRange = 1250.0f;

	/** Seconds of ratcheting while the horns spread */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot", meta = (ClampMin = 0.1))
	float TensionTime = 1.3f;

	/** Seconds between two clicks of the ratchet */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot", meta = (ClampMin = 0.02))
	float ClickInterval = 0.12f;

	/** Seconds between the "Klack" and the shot */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot", meta = (ClampMin = 0))
	float LockTime = 0.4f;

	/** Seconds the shot flies; the mark on the ground warns for as long */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot", meta = (ClampMin = 0.1))
	float FlightTime = 0.6f;

	/** Height of the arc above the higher end, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot", meta = (ClampMin = 0))
	float ShotApex = 60.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot", meta = (ClampMin = 0))
	float ShotDamage = 20.0f;

	/** Radius of the impact in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot", meta = (ClampMin = 1))
	float ShotRadius = 130.0f;

	/** Seconds the beetle is defenseless after fire burned its thread */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Thread", meta = (ClampMin = 0))
	float DefenselessTime = 4.0f;

	/** Damage it takes while defenseless is multiplied by this */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Thread", meta = (ClampMin = 0))
	float DefenselessDamageMultiplier = 1.5f;
};

/**
 *  Aschenroller (Aschenmark): an armadillo in a shell of ash crust, a defender. Curls up, shows the lane it will take and
 *  rolls down it through everything alive. Its shell shrugs off most hits while it is curled. If the roll ends at a rock
 *  wall or any other solid thing, it lies stunned on its back and shows its soft belly.
 */
UCLASS(BlueprintType)
class UVaelAshRollerData : public UVaelCreatureData
{
	GENERATED_BODY()

public:

	UVaelAshRollerData();

	/** Model of the roller curled up into a ball (creature factory, mode requisite), shown while it curls and rolls instead of the walking mesh; empty: the walking mesh turns as a whole */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Appearance")
	TSoftObjectPtr<UStaticMesh> BallMesh;

	/** Diameter the ball is shown at, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Appearance", meta = (ClampMin = 1))
	float BallDiameter = 120.0f;

	/** The roller walks towards players farther than this, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Distance", meta = (ClampMin = 0))
	float PreferredDistance = 650.0f;

	/** Share of the move speed while walking back home */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Distance", meta = (ClampMin = 0))
	float ReturnSpeedScale = 0.6f;

	/** Degrees per second the roller turns while walking */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Distance", meta = (ClampMin = 1))
	float TurnSpeed = 200.0f;

	/** Seconds until the first roll, picked at random in this range */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Roll")
	FVector2D FirstRollDelay = FVector2D(0.8f, 1.8f);

	/** Seconds between two rolls, picked at random in this range */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Roll")
	FVector2D RollInterval = FVector2D(2.2f, 3.4f);

	/** It only rolls at players closer than this, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Roll", meta = (ClampMin = 0))
	float RollRange = 1300.0f;

	/** Seconds it curls up and shows its lane before it rolls */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Roll", meta = (ClampMin = 0.1))
	float CurlTime = 0.9f;

	/** Speed of the roll in cm/s */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Roll", meta = (ClampMin = 1))
	float RollSpeed = 1250.0f;

	/** Length of the lane in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Roll", meta = (ClampMin = 1))
	float RollLength = 1500.0f;

	/** Damage to every player the roll runs over */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Roll", meta = (ClampMin = 0))
	float RollDamage = 18.0f;

	/** Speed at which the roll throws players aside, in cm/s */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Roll", meta = (ClampMin = 0))
	float RollKnockback = 800.0f;

	/** Seconds it needs to uncurl after a roll */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Roll", meta = (ClampMin = 0))
	float UncurlTime = 0.6f;

	/** Damage it takes while curled up or rolling is multiplied by this */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shell", meta = (ClampMin = 0))
	float CurledDamageMultiplier = 0.35f;

	/** Seconds it lies on its back after rolling into something solid */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shell", meta = (ClampMin = 0))
	float OnBackTime = 4.0f;

	/** Damage it takes while on its back is multiplied by this */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shell", meta = (ClampMin = 0))
	float OnBackDamageMultiplier = 1.75f;
};

/**
 *  Sichelpuppe (Aschenmark, fields): a charred harvest doll of straw, woken by the Mark. Jerks towards the players, whirls
 *  like a top with its sickles out and is dazed for a moment afterwards. Fire-proof; a hit of water lets it collapse into wet straw.
 */
UCLASS(BlueprintType)
class UVaelSickleDollData : public UVaelCreatureData
{
	GENERATED_BODY()

public:

	UVaelSickleDollData();

	/** Seconds it moves, and seconds it stands still between two jerks */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Jerks", meta = (ClampMin = 0.05))
	float StepTime = 0.32f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Jerks", meta = (ClampMin = 0))
	float PauseTime = 0.22f;

	/** It starts to whirl at players closer than this, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Whirl", meta = (ClampMin = 0))
	float WhirlRange = 420.0f;

	/** Seconds between two whirls, picked at random in this range */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Whirl")
	FVector2D WhirlInterval = FVector2D(1.2f, 2.0f);

	/** Seconds it raises its sickles before it whirls */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Whirl", meta = (ClampMin = 0))
	float WindupTime = 0.55f;

	/** Seconds a whirl lasts */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Whirl", meta = (ClampMin = 0.1))
	float WhirlTime = 2.2f;

	/** Speed while whirling in cm/s */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Whirl", meta = (ClampMin = 0))
	float WhirlSpeed = 420.0f;

	/** Reach of the sickles from its middle, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Whirl", meta = (ClampMin = 1))
	float WhirlRadius = 150.0f;

	/** Damage of each cut, and seconds between two cuts */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Whirl", meta = (ClampMin = 0))
	float CutDamage = 9.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Whirl", meta = (ClampMin = 0.05))
	float CutInterval = 0.45f;

	/** Seconds it is dazed after a whirl, and what damage it takes meanwhile is multiplied by */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dazed", meta = (ClampMin = 0))
	float DazedTime = 1.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dazed", meta = (ClampMin = 0))
	float DazedDamageMultiplier = 1.5f;

	/** Seconds it lies collapsed after a hit of water, and what damage it takes meanwhile is multiplied by */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Soaked", meta = (ClampMin = 0))
	float SoakedTime = 3.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Soaked", meta = (ClampMin = 0))
	float SoakedDamageMultiplier = 1.5f;
};

/**
 *  Schleieraffe (working title; idea of Boris, 2026-10-12): a shy nocturnal monkey with huge glowing eyes, wrapped in rags.
 *  Lives everywhere, most of all in the Hain. It keeps a rock, trunk or wall between itself and the players; only its eyes show,
 *  and it points at the player with the least life (the aye-aye as an omen of death). In healthy land it only watches and flees.
 *  The more corrupted the region, the more of them come out, the wilder they look, and they leap onto players' backs (Aufhocker):
 *  clinging on, slowing them and stealing materials. A dodge roll throws it off; fire and lightning blind its big eyes.
 */
UCLASS(BlueprintType)
class UVaelVeilMonkeyData : public UVaelCreatureData
{
	GENERATED_BODY()

public:

	UVaelVeilMonkeyData();

	/** Below this corruption of the region it only watches; at and above the second value it is as wild as it gets */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Corruption", meta = (ClampMin = 0, ClampMax = 100))
	float CalmCorruption = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Corruption", meta = (ClampMin = 0, ClampMax = 100))
	float WildCorruption = 70.0f;

	/** Share of the monkeys of a place that show themselves in healthy land; the rest come out as the corruption rises */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Corruption", meta = (ClampMin = 0, ClampMax = 1))
	float PresenceInHealthyLand = 0.34f;

	/** It looks for cover this far around itself, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hiding", meta = (ClampMin = 0))
	float CoverSearchRadius = 1100.0f;

	/** Things this small to this large (half their width in cm) count as cover */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hiding")
	FVector2D CoverSize = FVector2D(35.0f, 450.0f);

	/** A calm monkey flees from players closer than this, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hiding", meta = (ClampMin = 0))
	float FleeDistance = 420.0f;

	/** It shuts its eyes while a player this close looks its way, within this many degrees of where they face */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hiding", meta = (ClampMin = 0))
	float WatchedDistance = 1300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hiding", meta = (ClampMin = 0, ClampMax = 180))
	float WatchedAngle = 28.0f;

	/** It climbs over things this low to this high that stand in its way, in cm; lower ones it steps onto, higher ones it walks around */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Climbing")
	FVector2D ClimbHeight = FVector2D(35.0f, 230.0f);

	/** Things deeper than this it doesn't climb over, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Climbing", meta = (ClampMin = 0))
	float ClimbMaxDepth = 450.0f;

	/** Speed of the climb in cm/s */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Climbing", meta = (ClampMin = 1))
	float ClimbSpeed = 420.0f;

	/** It neither hides nor leaps this close to a lantern or fire light, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Light", meta = (ClampMin = 0))
	float LightAvoidRadius = 500.0f;

	/** A wild monkey leaps at players closer than this, in cm; in between it grows with the corruption */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pounce", meta = (ClampMin = 0))
	float PounceRange = 430.0f;

	/** Speed of the leap in cm/s, and the longest it lasts in seconds */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pounce", meta = (ClampMin = 0))
	float PounceSpeed = 1000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pounce", meta = (ClampMin = 0.1))
	float PounceTime = 0.7f;

	/** Seconds between two leaps, picked at random in this range */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pounce")
	FVector2D PounceInterval = FVector2D(4.0f, 7.0f);

	/** Seconds it clings on at the calmest and at the wildest */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cling")
	FVector2D ClingTime = FVector2D(2.5f, 5.0f);

	/** The player it clings to walks at this share of the speed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cling", meta = (ClampMin = 0, ClampMax = 1))
	float ClingSpeedMultiplier = 0.55f;

	/** Seconds between two thefts while it clings, and how many of a material it takes at once */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cling", meta = (ClampMin = 0.1))
	float StealInterval = 1.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cling", meta = (ClampMin = 1))
	int32 StealCount = 2;

	/** Damage of the bite when it lets go by itself */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cling", meta = (ClampMin = 0))
	float BiteDamage = 6.0f;

	/** Seconds fire or lightning blinds it, and what damage it takes meanwhile is multiplied by */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Light", meta = (ClampMin = 0))
	float BlindedTime = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Light", meta = (ClampMin = 0))
	float BlindedDamageMultiplier = 1.6f;
};

/**
 *  Rankenluchs: a lynx grown through with plants, hunting in packs of three in the Wurzelforst. It lies in ambush flat on
 *  the ground, sunk into the undergrowth so only its back and the swaying leaf crest show, and can't be aimed at then.
 *  A player coming close or a hit wakes it, and its pack wakes a moment later. It hunts like any cat: runs in from the
 *  flanks, pounces, bites and backs off to circle again. No magic (rope-like vines are kept for the pack leader).
 *  In a region at or above ShadowCorruption it shows its shadow form (ash grey, black violet, glowing violet veins).
 *  Its look are the baked animations of SK_Rankenluchs (Desktop/Vael_Rankenluchs scripts), not the creature factory.
 */
UCLASS(BlueprintType)
class UVaelRankenluchsData : public UVaelCreatureData
{
	GENERATED_BODY()

public:

	UVaelRankenluchsData();

	/** Its skeletal mesh with the material slots body, Kamm and Augen */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Look")
	TSoftObjectPtr<USkeletalMesh> LuchsMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Look")
	TSoftObjectPtr<UAnimSequence> IdleAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Look")
	TSoftObjectPtr<UAnimSequence> CamouflageAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Look")
	TSoftObjectPtr<UAnimSequence> WakeAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Look")
	TSoftObjectPtr<UAnimSequence> PounceAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Look")
	TSoftObjectPtr<UAnimSequence> WalkAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Look")
	TSoftObjectPtr<UAnimSequence> RunAnimation;

	/** Size of the lynx compared with the model (0.70 m at the shoulder). Boris 2026-10-28: too small to read its crest and vines,
	 *  so 1.57 makes it 1.1 m at the shoulder and about 2 m long; steps, ambush sinking and the bite grow with it */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Look", meta = (ClampMin = 0.2))
	float BodyScale = 1.57f;

	/** Speeds in cm/s at which the walk and the run animation play at their own pace (at the model's size; a bigger lynx takes longer steps) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Look", meta = (ClampMin = 1))
	float WalkAnimationSpeed = 140.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Look", meta = (ClampMin = 1))
	float RunAnimationSpeed = 520.0f;

	/** How deep the lying lynx sinks into the undergrowth while in ambush, in cm: only the back and the crest stay up */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ambush", meta = (ClampMin = 0))
	float AmbushSink = 26.0f;

	/** A player closer than this wakes it, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ambush", meta = (ClampMin = 0))
	float WakeRange = 700.0f;

	/** Lynxes this close to one that wakes belong to its pack and wake too, after a delay in seconds picked in this range */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ambush", meta = (ClampMin = 0))
	float PackRange = 1600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ambush")
	FVector2D PackWakeDelay = FVector2D(0.3f, 0.8f);

	/** Without a target this long it goes home and lies in ambush again, in seconds */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ambush", meta = (ClampMin = 0))
	float LoseInterestTime = 6.0f;

	/** Walking speed (stalking, circling, going home) in cm/s; the run is MoveSpeed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hunt", meta = (ClampMin = 0))
	float WalkSpeed = 230.0f;

	/** Pack members come in from the side, this many degrees apart around the prey */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hunt", meta = (ClampMin = 0, ClampMax = 120))
	float FlankAngle = 55.0f;

	/** It pounces on prey between these distances, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hunt")
	FVector2D PounceRange = FVector2D(260.0f, 700.0f);

	/** Speed of the leap in cm/s; the leap is the middle part of the pounce animation */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hunt", meta = (ClampMin = 0))
	float PounceSpeed = 1150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hunt", meta = (ClampMin = 0))
	float PounceDamage = 13.0f;

	/** After landing it bites prey this close, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hunt", meta = (ClampMin = 0))
	float BiteRange = 160.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hunt", meta = (ClampMin = 0))
	float BiteDamage = 6.0f;

	/** Seconds between two pounces, picked in this range */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hunt")
	FVector2D PounceInterval = FVector2D(2.5f, 4.0f);

	/** After an attack it backs off this far while circling, for seconds picked in the range */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hunt", meta = (ClampMin = 0))
	float RetreatDistance = 480.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hunt")
	FVector2D RetreatTime = FVector2D(1.0f, 1.7f);

	/** The last of its pack flees below this share of its health and hides in ambush: for CaptureWindow seconds the Tamer
	 *  can bind it there (capture condition "Letzter des Rudels", Boris 2026-10-21) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Capture", meta = (ClampMin = 0, ClampMax = 1))
	float FleeHealthShare = 0.3f;

	/** How far it flees before it hides, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Capture", meta = (ClampMin = 0))
	float FleeDistance = 900.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Capture", meta = (ClampMin = 0))
	float CaptureWindow = 8.0f;

	/** Materials in the light form and in the shadow form (the Loot of the creature data is not used) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot")
	TArray<FVaelLootEntry> LightLoot;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot")
	TArray<FVaelLootEntry> ShadowLoot;

	/** At this corruption of its region (percent) and above it shows its shadow form */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Corruption", meta = (ClampMin = 0, ClampMax = 100))
	float ShadowCorruption = 55.0f;

	/** Seconds the change between light and shadow form takes */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Corruption", meta = (ClampMin = 0))
	float FormChangeTime = 1.5f;
};

/**
 *  Draculaupe: a slow caterpillar that does no harm itself. The moment it touches a player it spins itself into a cocoon,
 *  and its alarm calls every other caterpillar near to that player. A moth hatches from the cocoon after a few seconds
 *  unless the cocoon is destroyed first, which takes a lot of damage.
 */
UCLASS()
class UVaelDraculaupeData : public UVaelCreatureData
{
	GENERATED_BODY()

public:

	UVaelDraculaupeData();

	/** True if it crawls towards players it notices; false: it only wanders around its home */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Behavior")
	bool bSeeksPlayers = true;

	/** It touches a player closer than this beyond the two bodies, in cm; a player in a dodge roll can't be touched */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Touch", meta = (ClampMin = 0))
	float TouchReach = 18.0f;

	/** True if its touch calls the other caterpillars */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Touch")
	bool bAlarms = true;

	/** The alarm reaches caterpillars within this distance, in cm, and they crawl this much faster */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Touch", meta = (ClampMin = 0))
	float AlarmRadius = 2000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Touch", meta = (ClampMin = 1))
	float AlarmSpeedMultiplier = 1.5f;

	/** Seconds from the touch until the moth hatches */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cocoon", meta = (ClampMin = 0.5))
	float CocoonTime = 10.0f;

	/** Damage the cocoon takes is multiplied by this: it needs a lot */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cocoon", meta = (ClampMin = 0))
	float CocoonDamageMultiplier = 0.22f;

	/** Model of the cocoon (creature factory, mode requisite) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cocoon")
	TSoftObjectPtr<UStaticMesh> CocoonMesh;

	/** Color of the threads it spins */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cocoon")
	FLinearColor ThreadColor = FLinearColor(0.72f, 0.68f, 0.6f);

	/** What a destroyed cocoon gives besides the loot of the caterpillar */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cocoon")
	TArray<FVaelLootEntry> CocoonLoot;

	/** The moth that hatches */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cocoon")
	TSubclassOf<UVaelCreatureData> MothData;
};

/**
 *  Tauraupe: the healed Draculaupe of a pure land. It wanders instead of seeking players and calls no others;
 *  touched, it spins a white cocoon from which a Taufalter hatches.
 *  Only a different set of default values for the caterpillar.
 */
UCLASS()
class UVaelTauraupeData : public UVaelDraculaupeData
{
	GENERATED_BODY()

public:

	UVaelTauraupeData();
};

/**
 *  Nosferalter: the moth of the Draculaupe. It rises above the player its caterpillar touched, follows them and circles,
 *  and for a while single drops of blood fall around it and hurt whoever they hit. Where drops soak the earth, flowers grow now and then.
 *  It can be fought in the air, but takes far more damage than the cocoon. When its rain is spent it flies off.
 */
UCLASS()
class UVaelBloodMothData : public UVaelCreatureData
{
	GENERATED_BODY()

public:

	UVaelBloodMothData();

	/** Height of the body above the ground, in cm; the collision stays low so spells still hit */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flight", meta = (ClampMin = 0))
	float FlightHeight = 260.0f;

	/** Radius of its circle above the player in cm, and its speed in radians per second */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flight", meta = (ClampMin = 0))
	float CircleRadius = 230.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flight")
	float CircleSpeed = 1.0f;

	/** Wing beats per second and how far the wings swing, in degrees */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flight", meta = (ClampMin = 0))
	float FlapsPerSecond = 2.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flight", meta = (ClampMin = 0))
	float FlapAngle = 40.0f;

	/** Seconds its rain lasts, picked at random in this range */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain")
	FVector2D RainTime = FVector2D(30.0f, 60.0f);

	/** Drops fall within this distance around the moth, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain", meta = (ClampMin = 0))
	float RainRadius = 380.0f;

	/** Seconds between two drops, picked at random in this range */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain")
	FVector2D DropInterval = FVector2D(0.3f, 0.7f);

	/** Seconds a drop falls, and how close to where it lands it hits, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain", meta = (ClampMin = 0.1))
	float DropFallTime = 0.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain", meta = (ClampMin = 0))
	float DropRadius = 90.0f;

	/** Damage of a drop; with healing rain the health it gives instead */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain", meta = (ClampMin = 0))
	float DropAmount = 6.0f;

	/** True for the rain of the healed moth: its drops heal */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain")
	bool bHealingRain = false;

	/** Color of the drops */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain")
	FLinearColor DropColor = FLinearColor(0.55f, 0.01f, 0.02f);

	/** Chance from 0 to 1 that a flower grows where a drop lands, and how many one rain lets grow at most */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Blooms", meta = (ClampMin = 0, ClampMax = 1))
	float BloomChance = 0.09f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Blooms", meta = (ClampMin = 0))
	int32 MaxBlooms = 5;

	/** Seconds a flower stands before it wilts, picked at random in this range */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Blooms")
	FVector2D BloomLifetime = FVector2D(120.0f, 180.0f);

	/** Flowers keep at least this far from each other, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Blooms", meta = (ClampMin = 0))
	float BloomSpacing = 260.0f;

	/** The kinds of flowers its rain lets grow, weighed by their chance */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Blooms")
	TArray<FVaelRainBloomKind> Blooms;
};

/**
 *  Taufalter: the moth of the Tauraupe. Its rain is water that heals, and only the good flowers grow under it.
 *  Only a different set of default values for the moth.
 */
UCLASS()
class UVaelDewMothData : public UVaelBloodMothData
{
	GENERATED_BODY()

public:

	UVaelDewMothData();
};
