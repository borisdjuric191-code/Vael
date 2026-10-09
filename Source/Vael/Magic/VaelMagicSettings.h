// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Magic/VaelElementTypes.h"
#include "Magic/VaelSpellEffects.h"
#include "VaelMagicSettings.generated.h"

class UAnimMontage;
class UMaterialInterface;
class UNiagaraSystem;
class UStaticMesh;

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

	/** Beam between two points, for the fire beam and the jumps of the chain lightning. Gets the user parameter BeamEnd (a position). */
	UPROPERTY(config, EditAnywhere, Category="Effects")
	TSoftObjectPtr<UNiagaraSystem> BeamEffect;

	/** Glow at the hand while elements are chosen; gets Color, and Intensity 1 or 2 for elements from the environment */
	UPROPERTY(config, EditAnywhere, Category="Effects")
	TSoftObjectPtr<UNiagaraSystem> HandEffect;

	/** Solid, glowing material of the element orbs above the head; gets Color and Glow. Without it they use the plain engine material. */
	UPROPERTY(config, EditAnywhere, Category="Effects")
	TSoftObjectPtr<UMaterialInterface> ElementOrbCoreMaterial;

	/** See-through glow of the element orbs: halos, flames, wind; gets Color, Glow and Rim (0 bright in the middle, 1 bright at the edge) */
	UPROPERTY(config, EditAnywhere, Category="Effects")
	TSoftObjectPtr<UMaterialInterface> ElementOrbGlowMaterial;

	/** Rock of the earth orb. Without it the orb is a brown sphere. */
	UPROPERTY(config, EditAnywhere, Category="Effects")
	TSoftObjectPtr<UStaticMesh> ElementOrbRockMesh;
};
