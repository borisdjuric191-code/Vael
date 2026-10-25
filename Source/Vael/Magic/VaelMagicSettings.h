// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Magic/VaelElementTypes.h"
#include "Magic/VaelSpellEffects.h"
#include "Magic/VaelDebrisWhirl.h"
#include "VaelMagicSettings.generated.h"

class UAnimMontage;
class UMaterialInterface;
class UNiagaraSystem;
class UStaticMesh;
class UVaelElementSource;
class AVaelWaterOrb;

/**
 *  Rules of the magic system that apply to every formula.
 *  Edited under Project Settings > Game > Vael Magic, stored in DefaultGame.ini.
 */
UCLASS(config=Game, defaultconfig, meta = (DisplayName = "Vael Magic"))
class UVaelMagicSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	/** Constructor */
	UVaelMagicSettings();

	/** Returns the settings object */
	static const UVaelMagicSettings* Get() { return GetDefault<UVaelMagicSettings>(); }

	/** Mana cost of a hand-cast formula made of the given number of elements, some of them drawn from the environment */
	float GetManaCost(int32 NumElements, int32 NumEnvironmentElements) const;

	/** Returns the placeholder color of an element */
	FLinearColor GetElementColor(EVaelElement Element) const;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** Mana cost by number of elements in the formula; the first entry is for one element */
	UPROPERTY(config, EditAnywhere, Category="Mana")
	TArray<float> ManaCostByElementCount;

	/** Mana saved for every element drawn from the environment */
	UPROPERTY(config, EditAnywhere, Category="Mana", meta = (ClampMin = 0))
	float EnvironmentCostReduction = 4.0f;

	/** No formula gets cheaper than this */
	UPROPERTY(config, EditAnywhere, Category="Mana", meta = (ClampMin = 0))
	float MinManaCost = 2.0f;

	/** Formulas cast from a quick slot cost this much more mana than by hand */
	UPROPERTY(config, EditAnywhere, Category="Quick Slots", meta = (ClampMin = 0))
	float QuickManaCostMultiplier = 1.5f;

	/** Formulas cast from a quick slot hit with this share of their power: combos by hand are stronger */
	UPROPERTY(config, EditAnywhere, Category="Quick Slots", meta = (ClampMin = 0))
	float QuickPowerMultiplier = 0.85f;

	/** Lightning damage against wet targets is multiplied by this */
	UPROPERTY(config, EditAnywhere, Category="Reactions", meta = (ClampMin = 0))
	float LightningOnWetMultiplier = 2.0f;

	/** Earth damage against frozen targets is multiplied by this, the target thaws */
	UPROPERTY(config, EditAnywhere, Category="Reactions", meta = (ClampMin = 0))
	float EarthOnFrozenMultiplier = 2.5f;

	/** Fire damage against wet targets is multiplied by this, the target dries */
	UPROPERTY(config, EditAnywhere, Category="Reactions", meta = (ClampMin = 0))
	float FireOnWetMultiplier = 0.6f;

	/** Seconds a water hit leaves its target wet if the formula doesn't say otherwise */
	UPROPERTY(config, EditAnywhere, Category="Reactions", meta = (ClampMin = 0))
	float DefaultWetDuration = 3.0f;

	/** How far a gust of wind carries a fire, in cm */
	UPROPERTY(config, EditAnywhere, Category="Reactions", meta = (ClampMin = 0))
	float FireSpreadDistance = 308.0f;

	/** Seconds a fire spread by wind burns */
	UPROPERTY(config, EditAnywhere, Category="Reactions", meta = (ClampMin = 0))
	float FireSpreadLifetime = 4.5f;

	/** Damage per second of a fire spread by wind if the original fire deals none */
	UPROPERTY(config, EditAnywhere, Category="Reactions", meta = (ClampMin = 0))
	float FireSpreadDamagePerSecond = 12.0f;

	/** Highest number of fires a single gust can spread */
	UPROPERTY(config, EditAnywhere, Category="Reactions", meta = (ClampMin = 0))
	int32 MaxFireSpreads = 3;

	/** Extra power per element drawn from the environment while the caster is free of Mark */
	UPROPERTY(config, EditAnywhere, Category="Environment", meta = (ClampMin = 0))
	float EnvironmentPowerBonusPure = 0.45f;

	/** Extra power per element drawn from the environment once the caster is corrupted by Mark */
	UPROPERTY(config, EditAnywhere, Category="Environment", meta = (ClampMin = 0))
	float EnvironmentPowerBonusCorrupted = 0.22f;

	/** A fire provides the fire element up to this distance beyond its edge, in cm */
	UPROPERTY(config, EditAnywhere, Category="Environment", meta = (ClampMin = 0))
	float FireSourceMargin = 168.0f;

	/** Rock or walls within this distance provide the earth element, in cm */
	UPROPERTY(config, EditAnywhere, Category="Environment", meta = (ClampMin = 0))
	float EarthSourceDistance = 170.0f;

	/** Chance to discover a free formula when its elements are combined for the first time */
	UPROPERTY(config, EditAnywhere, Category="Discovery", meta = (ClampMin = 0, ClampMax = 1))
	float DiscoveryChance = 0.7f;

	/** Damage the caster takes when experimenting fails */
	UPROPERTY(config, EditAnywhere, Category="Discovery", meta = (ClampMin = 0))
	float UnstableSelfDamage = 5.0f;

	/** Damage to enemies around the caster when experimenting fails */
	UPROPERTY(config, EditAnywhere, Category="Discovery", meta = (ClampMin = 0))
	float UnstableDamage = 10.0f;

	/** Reach of the discharge around the caster when experimenting fails, in cm */
	UPROPERTY(config, EditAnywhere, Category="Discovery", meta = (ClampMin = 0))
	float UnstableRadius = 250.0f;

	/** Speed at which the discharge pushes enemies away, in cm/s */
	UPROPERTY(config, EditAnywhere, Category="Discovery", meta = (ClampMin = 0))
	float UnstableKnockback = 420.0f;

	/** Walking enemies in mud move at this share of their speed */
	UPROPERTY(config, EditAnywhere, Category="Ground Areas", meta = (ClampMin = 0, ClampMax = 1))
	float MudSpeedMultiplier = 0.35f;

	/** Slowed enemies, walking or flying, move at this share of their speed */
	UPROPERTY(config, EditAnywhere, Category="Conditions", meta = (ClampMin = 0, ClampMax = 1))
	float SlowedSpeedMultiplier = 0.5f;

	/** A mage channeling a spell like the fire beam walks at this share of their speed */
	UPROPERTY(config, EditAnywhere, Category="Channeling", meta = (ClampMin = 0, ClampMax = 1))
	float ChannelMoveSpeedMultiplier = 0.35f;

	/** Seconds an enemy stays blind after leaving steam */
	UPROPERTY(config, EditAnywhere, Category="Ground Areas", meta = (ClampMin = 0))
	float SteamBlindLinger = 0.3f;

	/** Enemies closer than this to a player hidden in mist, in cm, still notice them; those further away lose them */
	UPROPERTY(config, EditAnywhere, Category="Ground Areas", meta = (ClampMin = 0))
	float MistSenseRange = 280.0f;

	/** Damage taken by those marked by the Mark, as multiplier */
	UPROPERTY(config, EditAnywhere, Category="Conditions", meta = (ClampMin = 1))
	float MarkedDamageMultiplier = 1.35f;

	/** Placeholder color of steam */
	UPROPERTY(config, EditAnywhere, Category="Appearance")
	FLinearColor SteamColor = FLinearColor(0.8f, 0.82f, 0.85f);

	/** Placeholder color of the mist that hides allies */
	UPROPERTY(config, EditAnywhere, Category="Appearance")
	FLinearColor MistColor = FLinearColor(0.62f, 0.7f, 0.8f);

	/** Placeholder color per element */
	UPROPERTY(config, EditAnywhere, Category="Appearance")
	TMap<EVaelElement, FLinearColor> ElementColors;

	/**
	 *  Cast animation per kind of formula, for formulas without their own montage.
	 *  Montages that don't exist yet are skipped, the formula then uses the default cast montage.
	 */
	UPROPERTY(config, EditAnywhere, Category="Animation")
	TMap<EVaelSpellDelivery, TSoftObjectPtr<UAnimMontage>> CastMontagesByDelivery;

	/** Cast animation of every formula that has no other. Without it spells appear at once, without animation. */
	UPROPERTY(config, EditAnywhere, Category="Animation")
	TSoftObjectPtr<UAnimMontage> DefaultCastMontage;

	/** Short hand gesture when an element is chosen, played if no spell is being cast */
	UPROPERTY(config, EditAnywhere, Category="Animation")
	TSoftObjectPtr<UAnimMontage> ElementSelectMontage;

	/** Socket or bone of the character from which projectiles leave after a cast animation */
	UPROPERTY(config, EditAnywhere, Category="Animation")
	FName CastSocketName = TEXT("hand_r");

	/** Returns the cast montage the formula uses by its kind, null if none exists */
	UAnimMontage* FindCastMontage(EVaelSpellDelivery Delivery) const;

	/** Effects every spell uses unless its element or formula has its own. Tinted in the color of the element. */
	UPROPERTY(config, EditAnywhere, Category="Effects")
	FVaelSpellEffects DefaultEffects;

	/** Effects per element; empty entries fall back to the default effects */
	UPROPERTY(config, EditAnywhere, Category="Effects")
	TMap<EVaelElement, FVaelSpellEffects> ElementEffects;

	/** Look of steam on the ground; without it steam uses the ground effect of water, tinted pale */
	UPROPERTY(config, EditAnywhere, Category="Effects")
	TSoftObjectPtr<UNiagaraSystem> SteamEffect;

	/** Look of mud on the ground; without it mud uses the ground effect of earth */
	UPROPERTY(config, EditAnywhere, Category="Effects")
	TSoftObjectPtr<UNiagaraSystem> MudEffect;

	/** Pieces the debris whirl of sand storms and dust devils carries, and how many a funnel and a storm has */
	UPROPERTY(config, EditAnywhere, Category="Effects")
	TArray<FVaelWhirlPiece> WhirlPieces;

	UPROPERTY(config, EditAnywhere, Category="Effects", meta = (ClampMin = 1))
	int32 WhirlFunnelPieces = 170;

	UPROPERTY(config, EditAnywhere, Category="Effects", meta = (ClampMin = 1))
	int32 WhirlStormPieces = 500;

	/** Body of dust of a whirl: seconds between two dust clouds (the dust burst of a rock wall) in a funnel and in a storm, their size in whirl radii */
	UPROPERTY(config, EditAnywhere, Category="Effects", meta = (ClampMin = 0.01))
	float WhirlFunnelDustInterval = 0.07f;

	UPROPERTY(config, EditAnywhere, Category="Effects", meta = (ClampMin = 0.01))
	float WhirlStormDustInterval = 0.035f;

	UPROPERTY(config, EditAnywhere, Category="Effects", meta = (ClampMin = 0.05))
	float WhirlDustSize = 0.35f;

	/** Mist veil (Nebelschleier): balls of steam in its cloud, its turn in degrees per second, seconds between two shreds tearing off and their drift (pack wind speed) */
	UPROPERTY(config, EditAnywhere, Category="Effects", meta = (ClampMin = 2))
	int32 VeilBalls = 12;

	UPROPERTY(config, EditAnywhere, Category="Effects")
	float VeilTurnSpeed = 14.0f;

	UPROPERTY(config, EditAnywhere, Category="Effects", meta = (ClampMin = 0.1))
	float VeilShredInterval = 0.9f;

	UPROPERTY(config, EditAnywhere, Category="Effects")
	float VeilShredDrift = 3.0f;

	/** Dust thrown up where something heavy hits the ground (rock shards, stamps, rocks breaking out); gets the dust color and the spread */
	UPROPERTY(config, EditAnywhere, Category="Effects")
	TSoftObjectPtr<UNiagaraSystem> DustBurstEffect;

	/** Small puff of dust or ash behind rolling and crawling things; without it they puff spheres of the glow material */
	UPROPERTY(config, EditAnywhere, Category="Effects")
	TSoftObjectPtr<UNiagaraSystem> DustPuffEffect;

	/** How effects of bought packs are fitted to Vael (color, size, emitters off), wherever they are used */
	UPROPERTY(config, EditAnywhere, Category="Effects")
	TArray<FVaelPackTuning> PackTunings;

	/** Beam between two points, for the fire beam and the jumps of the chain lightning. Gets the user parameter BeamEnd (a position). */
	UPROPERTY(config, EditAnywhere, Category="Effects")
	TSoftObjectPtr<UNiagaraSystem> BeamEffect;

	/** Glow at the hand while elements are chosen; gets Color, and Intensity 1 or 2 for elements from the environment */
	UPROPERTY(config, EditAnywhere, Category="Effects")
	TSoftObjectPtr<UNiagaraSystem> HandEffect;

	/** Small splash where a wave of water passes, played once at many places */
	UPROPERTY(config, EditAnywhere, Category="Effects")
	TSoftObjectPtr<UNiagaraSystem> SplashEffect;

	/** Solid, glowing material of the element orbs above the head; gets Color and Glow. Without it they use the plain engine material. */
	UPROPERTY(config, EditAnywhere, Category="Effects")
	TSoftObjectPtr<UMaterialInterface> ElementOrbCoreMaterial;

	/** See-through glow of the element orbs: halos, flames, wind; gets Color, Glow and Rim (0 bright in the middle, 1 bright at the edge) */
	UPROPERTY(config, EditAnywhere, Category="Effects")
	TSoftObjectPtr<UMaterialInterface> ElementOrbGlowMaterial;

	/** Rock of the earth orb. Without it the orb is a brown sphere. */
	UPROPERTY(config, EditAnywhere, Category="Effects")
	TSoftObjectPtr<UStaticMesh> ElementOrbRockMesh;

	/** Ball of water from the water kit that water projectiles fly as (BP_WaterOrb). Without it they keep their look of glowing spheres. */
	UPROPERTY(config, EditAnywhere, Category="Water Kit")
	TSoftClassPtr<AVaelWaterOrb> WaterOrbClass;

	/** Shot of water like a jet from a water gun that water projectiles fly as instead of the ball; empty: they fly as the ball */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Bolt")
	TSoftClassPtr<AVaelWaterOrb> WaterBoltClass;

	/** Length of the jet behind its round nose, in radii of the ball of the same size */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Bolt", meta = (ClampMin = 0.5))
	float WaterBoltTailLength = 6.5f;

	/** Thickness of the jet against the round ball of the same size: lower is slimmer and looks like more pressure */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Bolt", meta = (ClampMin = 0.2, ClampMax = 1))
	float WaterBoltThickness = 0.5f;

	/** Turns of the white spiral of spray that winds around the jet and widens behind it; 0 for none */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Bolt", meta = (ClampMin = 0))
	float WaterBoltSpiralTurns = 0.0f;

	/** Degrees per second the spiral turns around the jet, so it seems to drill forward */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Bolt")
	float WaterBoltSpiralSpin = 720.0f;

	/** White spray on the rim of the bolt, the FoamAmount of the water material */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Bolt", meta = (ClampMin = 0))
	float WaterBoltFoam = 1.2f;

	/** How wide the bright rim of the bolt is, the FresnelPower of the water material: lower is wider */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Bolt", meta = (ClampMin = 0.1))
	float WaterBoltFresnelPower = 3.5f;

	/** At least this much of the deep color of the water shows in the body of the bolt, so it reads as water and not as a pale ball; clear sources get deeper, murky ones keep theirs */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Bolt", meta = (ClampMin = 0, ClampMax = 1))
	float WaterBoltMinMurkiness = 0.95f;

	/** Most glow the body of the bolt gets from its source; the magic water glows strongly on the floating ball, which makes a fast bolt pale */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Bolt", meta = (ClampMin = 0))
	float WaterBoltMaxEmissive = 0.15f;

	/** Effect around the head of the bolt, gets User.WaterColor */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Bolt")
	TSoftObjectPtr<UNiagaraSystem> WaterBoltHeadEffect;

	/** Spray the bolt leaves behind along its flight (world space), gets User.WaterColor; empty for none */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Bolt")
	TSoftObjectPtr<UNiagaraSystem> WaterBoltTrailEffect;

	/** Burst where a ball of water ends (NS_Water_OrbImpact); gets the particle color of the source as User.WaterColor */
	UPROPERTY(config, EditAnywhere, Category="Water Kit")
	TSoftObjectPtr<UNiagaraSystem> WaterOrbImpactEffect;

	/** Whose water the mages cast */
	UPROPERTY(config, EditAnywhere, Category="Water Kit")
	TSoftObjectPtr<UVaelElementSource> MageWaterSource;

	/** Size of the burst compared to the ball: 1 is made for a ball of 1 m and shrinks and grows with it */
	UPROPERTY(config, EditAnywhere, Category="Water Kit", meta = (ClampMin = 0))
	float WaterOrbImpactScale = 0.6f;

	/** Diameter in cm of a ball of water that splashes with User.WaterAmount 1 (the mage's Wassergeschoss is about 57 cm); bigger balls splash more by their cross section */
	UPROPERTY(config, EditAnywhere, Category="Water Kit", meta = (ClampMin = 1, Units = "cm"))
	float WaterAmountReferenceDiameter = 57.0f;

	/** Most water one burst may get, so a huge ball cannot flood the screen with particles */
	UPROPERTY(config, EditAnywhere, Category="Water Kit", meta = (ClampMin = 0))
	float MaxWaterAmount = 8.0f;

	/** Radius in cm of the puddle one Wassergeschoss leaves; more water covers more ground */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Puddle", meta = (ClampMin = 1, Units = "cm"))
	float PuddleRadiusPerWater = 55.0f;

	/** Water a puddle needs before mages can draw from it and it wets those standing in it; 1 is one Wassergeschoss */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Puddle", meta = (ClampMin = 0))
	float PuddleWaterToCount = 2.5f;

	/** Most water one puddle holds */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Puddle", meta = (ClampMin = 0))
	float PuddleMaxWater = 15.0f;

	/** Water a puddle loses per second; rain keeps it */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Puddle", meta = (ClampMin = 0))
	float PuddleDryPerSecond = 0.04f;

	/** How much faster puddles dry in a drought */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Puddle", meta = (ClampMin = 1))
	float PuddleDroughtDryMultiplier = 2.5f;

	/** Water landing this close to the edge of a puddle flows into it, in cm */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Puddle", meta = (ClampMin = 0, Units = "cm"))
	float PuddleMergeDistance = 40.0f;

	/** Seconds those who step out of a deep puddle stay wet */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Puddle", meta = (ClampMin = 0, Units = "s"))
	float PuddleWetDuration = 2.0f;

	/** Seconds after the burst until the first water reaches the ground */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Puddle", meta = (ClampMin = 0, Units = "s"))
	float PuddleFirstDropDelay = 0.25f;

	/** Seconds after the burst until the last drops thrown out have landed */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Puddle", meta = (ClampMin = 0, Units = "s"))
	float PuddleLastDropDelay = 0.75f;

	/** Material of the water surface of a puddle (M_Water_Puddle); it takes the look of the source's water and is cut out by the painted outline */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Puddle")
	TSoftObjectPtr<UMaterialInterface> PuddleMaterial;

	/** How far around the caster a wave of water (Welle, Flutwelle) draws water from puddles, ponds and water plants, in cm */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Wave Swell", meta = (ClampMin = 0))
	float WaveSwellRadius = 800.0f;

	/** Water around the caster at which a wave reaches its full size and damage, in User.WaterAmount units (1 is one Wassergeschoss) */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Wave Swell", meta = (ClampMin = 0.1))
	float WaveSwellFullWater = 6.0f;

	/** Size of a wave with all the water it can use, against one without: reach and width of the Welle, width of the Flutwelle */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Wave Swell", meta = (ClampMin = 1))
	float WaveSwellMaxSize = 2.0f;

	/** Damage of a wave with all the water it can use, against one without */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Wave Swell", meta = (ClampMin = 1))
	float WaveSwellMaxDamage = 1.75f;

	/** Water rain adds around the caster */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Wave Swell", meta = (ClampMin = 0))
	float WaveSwellRainWater = 3.0f;

	/** Water every square meter of a pond or other patch of water on the ground adds; ponds are never used up */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Wave Swell", meta = (ClampMin = 0))
	float WaveSwellPondWaterPerSquareMeter = 0.4f;

	/** Water every water plant (like the Traenenkelch) adds */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Wave Swell", meta = (ClampMin = 0))
	float WaveSwellPlantWater = 1.5f;

	/** Water rain gathers into puddles on the ground; nobody made it, so it wets everyone standing in it. Empty: rain leaves no puddles */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Rain Puddles")
	TSoftObjectPtr<UVaelElementSource> RainWaterSource;

	/** Seconds between two drops of rain water gathering around every player */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Rain Puddles", meta = (ClampMin = 0.05, Units = "s"))
	float RainPuddleInterval = 0.8f;

	/** Water of one such drop, in User.WaterAmount units (1 is one Wassergeschoss) */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Rain Puddles", meta = (ClampMin = 0))
	float RainPuddleWater = 0.5f;

	/** How far around a player rain water gathers, in cm */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Rain Puddles", meta = (ClampMin = 0, Units = "cm"))
	float RainPuddleReach = 1500.0f;

	/** Most rain puddles around one player; once there are this many, rain only makes them bigger */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Rain Puddles", meta = (ClampMin = 1))
	int32 RainPuddleMaxNearby = 5;

	/** Share of the drops that land on a rain puddle already lying there instead of starting a new one */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Rain Puddles", meta = (ClampMin = 0, ClampMax = 1))
	float RainPuddleGrowShare = 0.75f;

	/** Seconds the strand of water stays that runs from a pond, puddle or water plant up to a water orb drawn from it */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Draw Strand", meta = (ClampMin = 0.1, Units = "s"))
	float WaterDrawStrandTime = 1.0f;

	/** Color of the strand where it leaves a pond or water plant: dark pond water that turns into bright spell water on its way up */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Draw Strand")
	FLinearColor WaterDrawPondColor = FLinearColor(0.012f, 0.035f, 0.04f);

	/** How far from the feet the strand leaves the water when the mage stands in it, in cm, so it bends visibly up to the orb */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Draw Strand", meta = (ClampMin = 0, Units = "cm"))
	float WaterDrawFootOffset = 170.0f;

	/** Material of water in motion that is built as a mesh: waves, the drawn strand, their spray and drops (M_Water_Wave and its instances) */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Wave")
	TSoftObjectPtr<UMaterialInterface> WaveMaterial;

	/** Seconds the Welle needs to roll to the end of its cone; it breaks on the way and runs out as foam afterwards */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Wave", meta = (ClampMin = 0.2, Units = "s"))
	float WaveRollTime = 0.85f;

	/** Height of the Welle at its full size, as a share of its reach */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Wave", meta = (ClampMin = 0.05))
	float WaveHeightShare = 0.2f;

	/** Height of the Flutwelle compared to its width (radius of the spell) */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Wave", meta = (ClampMin = 0.1))
	float TidalHeightShare = 1.15f;

	/** How wild the Welle is: 0 smooth and even, 1 torn crest and much spray */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Wave", meta = (ClampMin = 0, ClampMax = 1.5))
	float WaveViolence = 0.6f;

	/** How wild the Flutwelle is, see WaveViolence */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Wave", meta = (ClampMin = 0, ClampMax = 1.5))
	float TidalViolence = 1.0f;

	/** Material of waves of Mark blood (MI_Water_Wave_Mark), used by cone waves of the Mark like the Blutflut; without it they roll as spell water */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Wave")
	TSoftObjectPtr<UMaterialInterface> MarkWaveMaterial;

	/** The Wassergeschoss is built as real water with the wave material (lumpy jet, foaming nose, torn tail, falling drops); off: the smooth jet in the source material */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Bolt")
	bool bWaterBoltRealWater = true;

	/** Drops the real-water bolt sheds per second from its tail; they fall in arcs behind it */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Bolt", meta = (ClampMin = 0))
	float WaterBoltDropRate = 110.0f;

	/** How lumpy and torn the real-water bolt is: 0 a smooth jet, 1 boiling and breaking up behind the nose */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Bolt", meta = (ClampMin = 0, ClampMax = 1.5))
	float WaterBoltViolence = 0.7f;

	/** Height of the column of the Geysir, in cm */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Geyser", meta = (ClampMin = 50, Units = "cm"))
	float GeyserHeight = 520.0f;

	/** Seconds from the eruption until the column has fallen back and its water has run out */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Geyser", meta = (ClampMin = 0.3, Units = "s"))
	float GeyserTime = 1.7f;

	/** How wild the Geysir is: 0 a smooth column, 1 a boiling, torn column with much spray */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Geyser", meta = (ClampMin = 0, ClampMax = 1.5))
	float GeyserViolence = 1.0f;

	/** Water the Geysir leaves on the ground as a puddle, in User.WaterAmount units (1 is one Wassergeschoss); 0 for none */
	UPROPERTY(config, EditAnywhere, Category="Water Kit|Geyser", meta = (ClampMin = 0))
	float GeyserPuddleWater = 3.0f;
};
