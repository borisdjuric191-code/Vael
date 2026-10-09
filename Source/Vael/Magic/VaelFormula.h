// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Combat/VaelCombatStatics.h"
#include "Magic/VaelElementTypes.h"
#include "Magic/VaelSpellEffects.h"
#include "VaelFormula.generated.h"

class AVaelSpellProjectile;
class UAnimMontage;
class UVaelFormulaAbility;

/**
 *  A formula of the mage: the combination of elements that triggers it and what it does.
 *  The order of the elements doesn't matter, Fire + Earth is the same formula as Earth + Fire.
 */
UCLASS(BlueprintType)
class UVaelFormula : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** Asset manager type of all formulas */
	static const FPrimaryAssetType PrimaryAssetType;

	/** Constructor */
	UVaelFormula();

	//~Begin UObject
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	//~End UObject

	/** Order-independent key of the element combination */
	uint32 GetComboKey() const { return VaelElements::MakeComboKey(Elements); }

	/** What the formula does to a single target at the given power */
	FVaelSpellHit MakeSpellHit(float Power) const;

	/** Name shown in the grimoire */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Formula")
	FText DisplayName;

	/** What the formula does, shown in the grimoire */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Formula", meta = (MultiLine = true))
	FText Description;

	/** Elements that have to be combined, in any order */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Formula")
	TArray<EVaelElement> Elements;

	/** How the formula gets into the grimoire */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Formula")
	EVaelFormulaSource Source = EVaelFormulaSource::Free;

	/** Hint of the echo where a sealed formula can be found */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Formula", meta = (MultiLine = true))
	FText Hint;

	/** Health the caster pays on top of the mana, like the Markstoss. Never brings them below 1. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Formula", meta = (ClampMin = 0))
	float HealthCost = 0.0f;

	/** Seconds a quick slot needs after casting this formula, before it can be cast from there again */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Formula", meta = (ClampMin = 0))
	float QuickCooldown = 4.0f;

	/** Ability that performs the formula. The default handles projectiles and cones, special formulas get their own class. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Effect")
	TSubclassOf<UVaelFormulaAbility> AbilityClass;

	/**
	 *  Animation of the cast, played on the upper body. The spell appears at its "Vael Cast Point" notify, or when it ends without one.
	 *  Empty: the cast montage of this kind of formula from the magic settings.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Animation")
	TSoftObjectPtr<UAnimMontage> CastMontage;

	/** Speed of the cast animation, above 1 is faster */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Animation", meta = (ClampMin = 0.1))
	float CastMontagePlayRate = 1.0f;

	/** Returns the cast animation of the formula, its own or the one of its kind, null if none exists yet */
	UAnimMontage* FindCastMontage() const;

	/** Own effects and sounds of the formula. Empty entries use those of its damage element from the magic settings. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Effects")
	FVaelSpellEffects Effects;

	/** Loads the effects of the formula, tinted in the color of its damage element */
	FVaelLoadedEffects LoadEffects() const;

	/** How the formula reaches its targets */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Effect")
	EVaelSpellDelivery Delivery = EVaelSpellDelivery::Projectile;

	/** Element of the damage, decides weaknesses and reactions */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Effect")
	EVaelElement DamageElement = EVaelElement::Fire;

	/** Damage per target. For beams, whirlwinds and storms: damage per second to everyone inside. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Effect", meta = (ClampMin = 0))
	float Damage = 0.0f;

	/** Speed at which targets are pushed away, in cm/s */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Effect", meta = (ClampMin = 0))
	float Knockback = 0.0f;

	/** True for lightning, which hits wet targets twice as hard */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Effect")
	bool bLightning = false;

	/** Condition the formula leaves on its targets */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Status")
	EVaelStatus AppliedStatus = EVaelStatus::None;

	/** Seconds the condition lasts */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Status", meta = (ClampMin = 0, EditCondition = "AppliedStatus != EVaelStatus::None"))
	float StatusDuration = 0.0f;

	/** Damage per second while burning */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Status", meta = (ClampMin = 0, EditCondition = "AppliedStatus == EVaelStatus::Burning"))
	float StatusDamagePerSecond = 0.0f;

	/** Seconds a hit enemy can't act, 0 for none. Bosses shake it off faster. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Status", meta = (ClampMin = 0))
	float StunDuration = 0.0f;

	/** Projectile actor to spawn */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile", meta = (EditCondition = "Delivery == EVaelSpellDelivery::Projectile || Delivery == EVaelSpellDelivery::Explosion"))
	TSubclassOf<AVaelSpellProjectile> ProjectileClass;

	/** Own look of the projectile, shown together with its trail effect */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile", meta = (EditCondition = "Delivery == EVaelSpellDelivery::Projectile || Delivery == EVaelSpellDelivery::Explosion"))
	EVaelProjectileLook ProjectileLook = EVaelProjectileLook::Sphere;

	/** Flight speed in cm/s */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::Projectile || Delivery == EVaelSpellDelivery::Explosion"))
	float ProjectileSpeed = 1500.0f;

	/** Collision radius in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile", meta = (ClampMin = 1, EditCondition = "Delivery == EVaelSpellDelivery::Projectile || Delivery == EVaelSpellDelivery::Explosion"))
	float ProjectileRadius = 30.0f;

	/** Seconds until the projectile fizzles out */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile", meta = (ClampMin = 0.1, EditCondition = "Delivery == EVaelSpellDelivery::Projectile || Delivery == EVaelSpellDelivery::Explosion"))
	float ProjectileLifetime = 1.1f;

	/** Number of additional enemies the projectile flies through after its first hit */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::Projectile || Delivery == EVaelSpellDelivery::Explosion"))
	int32 ProjectilePierce = 0;

	/** True if walls, rocks and rock walls don't stop the projectile */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile", meta = (EditCondition = "Delivery == EVaelSpellDelivery::Projectile || Delivery == EVaelSpellDelivery::Explosion"))
	bool bProjectilePassesWalls = false;

	/** How far away the first target of the chain may be, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Chain", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::Chain"))
	float ChainRange = 1330.0f;

	/** How far the chain jumps from one target to the next, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Chain", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::Chain"))
	float ChainJumpRange = 560.0f;

	/** Highest number of targets the chain hits */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Chain", meta = (ClampMin = 1, EditCondition = "Delivery == EVaelSpellDelivery::Chain"))
	int32 ChainMaxTargets = 5;

	/** The first target has to be within this angle of the aim direction, in degrees */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Chain", meta = (ClampMin = 1, ClampMax = 180, EditCondition = "Delivery == EVaelSpellDelivery::Chain"))
	float ChainAimHalfAngle = 29.0f;

	/** Reach of the cone in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cone", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::Cone"))
	float ConeRange = 450.0f;

	/** Half opening angle of the cone in degrees */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cone", meta = (ClampMin = 1, ClampMax = 180, EditCondition = "Delivery == EVaelSpellDelivery::Cone"))
	float ConeHalfAngle = 45.0f;

	/** Own look of the cone, shown together with its cast effect */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cone", meta = (EditCondition = "Delivery == EVaelSpellDelivery::Cone"))
	EVaelConeLook ConeLook = EVaelConeLook::Plain;

	/** Radius of the burst where the projectile ends, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Explosion", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::Explosion"))
	float ExplosionRadius = 266.0f;

	/** Damage of the burst to everyone in it, on top of the damage of a direct hit. The burst leaves the same condition as a direct hit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Explosion", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::Explosion"))
	float ExplosionDamage = 0.0f;

	/** Speed at which the burst pushes targets away from its center, in cm/s */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Explosion", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::Explosion"))
	float ExplosionKnockback = 560.0f;

	/** How far away the patch, the strike or a standing whirl can be placed, in cm. Walls stop it earlier. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ground Area", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::GroundArea || Delivery == EVaelSpellDelivery::Strike || Delivery == EVaelSpellDelivery::Vortex"))
	float AreaRange = 980.0f;

	/** Radius of the patch on the ground in cm. Explosions, strikes and spike lines leave no patch while this is 0. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ground Area", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::GroundArea || Delivery == EVaelSpellDelivery::Explosion || Delivery == EVaelSpellDelivery::Beam || Delivery == EVaelSpellDelivery::Vortex || Delivery == EVaelSpellDelivery::Strike || Delivery == EVaelSpellDelivery::SpikeLine"))
	float AreaRadius = 0.0f;

	/** Seconds the patch lasts */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ground Area", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::GroundArea || Delivery == EVaelSpellDelivery::Explosion || Delivery == EVaelSpellDelivery::Beam || Delivery == EVaelSpellDelivery::Vortex || Delivery == EVaelSpellDelivery::Strike || Delivery == EVaelSpellDelivery::SpikeLine"))
	float AreaLifetime = 5.0f;

	/** Damage per second of the patch to the enemies of the caster, in the damage element of the formula */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ground Area", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::GroundArea || Delivery == EVaelSpellDelivery::Explosion || Delivery == EVaelSpellDelivery::Beam || Delivery == EVaelSpellDelivery::Vortex || Delivery == EVaelSpellDelivery::Strike || Delivery == EVaelSpellDelivery::SpikeLine"))
	float AreaDamagePerSecond = 0.0f;

	/** What the patch does to enemies standing in it */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ground Area", meta = (EditCondition = "Delivery == EVaelSpellDelivery::GroundArea || Delivery == EVaelSpellDelivery::Explosion || Delivery == EVaelSpellDelivery::Strike"))
	EVaelGroundEffect AreaEffect = EVaelGroundEffect::None;

	/** Seconds between the warning on the ground and the strike landing */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Strike", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::Strike"))
	float StrikeDelay = 1.0f;

	/** Reach of the strike in cm; it deals the damage, knockback and condition of the formula to everyone inside */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Strike", meta = (ClampMin = 1, EditCondition = "Delivery == EVaelSpellDelivery::Strike"))
	float StrikeRadius = 266.0f;

	/** Strength of the camera shake when the strike lands */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Strike", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::Strike"))
	float StrikeShake = 0.0f;

	/** Number of strikes. More than one makes a storm: they land one after the other, scattered around the aimed point. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Strike", meta = (ClampMin = 1, EditCondition = "Delivery == EVaelSpellDelivery::Strike"))
	int32 StrikeCount = 1;

	/** Seconds between two strikes of a storm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Strike", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::Strike && StrikeCount > 1"))
	float StrikeInterval = 0.3f;

	/** How far from the aimed point the strikes of a storm land at most, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Strike", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::Strike && StrikeCount > 1"))
	float StrikeScatter = 0.0f;

	/** Speed of the dash in cm/s */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dash", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::Dash"))
	float DashSpeed = 2100.0f;

	/** Seconds the dash lasts */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dash", meta = (ClampMin = 0.05, EditCondition = "Delivery == EVaelSpellDelivery::Dash"))
	float DashDuration = 0.28f;

	/** Seconds nothing can hurt the caster from the start of the dash; the dash itself always protects */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dash", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::Dash"))
	float DashInvulnerability = 0.32f;

	/** Radius of the burst at the end of the dash in cm; it deals the damage and knockback of the formula */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dash", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::Dash"))
	float DashBurstRadius = 322.0f;

	/** How far away the wall can be raised, in cm. Walls of the level stop it earlier. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wall", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::Wall"))
	float WallRange = 476.0f;

	/** Number of rock blocks in a row across the aim direction */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wall", meta = (ClampMin = 1, EditCondition = "Delivery == EVaelSpellDelivery::Wall"))
	int32 WallBlocks = 5;

	/** Distance between the centers of two blocks in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wall", meta = (ClampMin = 1, EditCondition = "Delivery == EVaelSpellDelivery::Wall"))
	float WallBlockSpacing = 133.0f;

	/** Width and depth of a block in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wall", meta = (ClampMin = 1, EditCondition = "Delivery == EVaelSpellDelivery::Wall"))
	float WallBlockSize = 118.0f;

	/** Height of a block in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wall", meta = (ClampMin = 1, EditCondition = "Delivery == EVaelSpellDelivery::Wall"))
	float WallHeight = 120.0f;

	/** Seconds until the wall crumbles */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wall", meta = (ClampMin = 0.1, EditCondition = "Delivery == EVaelSpellDelivery::Wall"))
	float WallLifetime = 8.0f;

	/** Seconds the beam is channeled. The caster walks slower meanwhile and casts nothing else. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Beam", meta = (ClampMin = 0.1, EditCondition = "Delivery == EVaelSpellDelivery::Beam"))
	float BeamDuration = 1.7f;

	/** Reach of the beam in cm. Walls stop it earlier. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Beam", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::Beam"))
	float BeamLength = 1050.0f;

	/** Enemies this far from the middle line of the beam are hit, plus half their own radius, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Beam", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::Beam"))
	float BeamHalfWidth = 77.0f;

	/** Damage collects on each enemy and is dealt once it reaches this much, so the numbers stay readable. Also used by whirlwinds and storms. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Beam", meta = (ClampMin = 0.1, EditCondition = "Delivery == EVaelSpellDelivery::Beam || Delivery == EVaelSpellDelivery::Vortex || Delivery == EVaelSpellDelivery::Aura"))
	float BeamDamageStep = 8.0f;

	/** Seconds between two fires the beam leaves where it ends; the fires use the ground area values. 0 for none. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Beam", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::Beam"))
	float BeamFireInterval = 0.35f;

	/** Reach of the burst around the caster in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Nova", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::Nova"))
	float NovaRadius = 644.0f;

	/** Strength of the camera shake of the burst */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Nova", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::Nova"))
	float NovaShake = 0.7f;

	/** Wandering speed of the whirlwind in cm/s. At 0 it stands still and is placed at the aimed point, like a maw in a swamp. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Vortex", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::Vortex"))
	float VortexSpeed = 476.0f;

	/** Enemies inside the whirlwind can't see for this long, renewed as long as they stay inside. 0 for none. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Vortex", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::Vortex"))
	float VortexBlindDuration = 0.0f;

	/** Radius of the whirlwind in cm; enemies touching it are pulled in and hurt */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Vortex", meta = (ClampMin = 1, EditCondition = "Delivery == EVaelSpellDelivery::Vortex"))
	float VortexRadius = 224.0f;

	/** Seconds until the whirlwind dies down */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Vortex", meta = (ClampMin = 0.1, EditCondition = "Delivery == EVaelSpellDelivery::Vortex"))
	float VortexLifetime = 4.2f;

	/** Speed at which enemies inside are dragged towards the middle, in cm/s. Bosses resist through their pull multiplier. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Vortex", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::Vortex"))
	float VortexPullSpeed = 500.0f;

	/** Extra damage per second once the whirlwind has passed over fire and become a fire whirl; it then deals fire damage */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Vortex", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::Vortex"))
	float VortexFireDamageBonus = 14.0f;

	/** Seconds a fire whirl sets enemies on fire */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Vortex", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::Vortex"))
	float VortexBurnDuration = 2.0f;

	/** Damage per second while burning from a fire whirl */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Vortex", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::Vortex"))
	float VortexBurnDamagePerSecond = 6.0f;

	/** Seconds between two fires a fire whirl leaves behind; the fires use the ground area values. 0 for none. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Vortex", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::Vortex"))
	float VortexFireInterval = 0.5f;

	/** Number of spikes in the line */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spike Line", meta = (ClampMin = 1, EditCondition = "Delivery == EVaelSpellDelivery::SpikeLine"))
	int32 SpikeCount = 9;

	/** Distance between two spikes in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spike Line", meta = (ClampMin = 1, EditCondition = "Delivery == EVaelSpellDelivery::SpikeLine"))
	float SpikeSpacing = 126.0f;

	/** Seconds between two spikes breaking out */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spike Line", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::SpikeLine"))
	float SpikeStagger = 0.055f;

	/** Reach of each spike in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spike Line", meta = (ClampMin = 1, EditCondition = "Delivery == EVaelSpellDelivery::SpikeLine"))
	float SpikeRadius = 105.0f;

	/** Reach of the storm around the caster in cm, plus the radius of the enemy */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Aura", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::Aura"))
	float AuraRadius = 420.0f;

	/** Seconds the storm lasts */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Aura", meta = (ClampMin = 0.1, EditCondition = "Delivery == EVaelSpellDelivery::Aura"))
	float AuraDuration = 4.2f;

	/** Enemies inside the storm can't see for this long, renewed as long as they stay inside */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Aura", meta = (ClampMin = 0, EditCondition = "Delivery == EVaelSpellDelivery::Aura"))
	float AuraBlindDuration = 0.4f;

	/** What the burst of an explosion does to a single target at the given power */
	FVaelSpellHit MakeExplosionHit(float Power) const;
};
