// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class APawn;
class UVaelPlantArmData;

/**
 *  Symbioses of the Floromant (Boris 2026-10-26): two plants grow together into one and act as a new plant. It happens in two
 *  places: two towers standing close together fuse (VaelPlantTower), and the two hands crossed while both fire grow into each
 *  other at the player (UVaelFloromantComponent::CrossHands). Both use the same effect of the pair.
 *  The first pairs are the three plants of the arms; every further plant gets its pairs once its list is agreed.
 */
enum class EVaelSymbiosis : uint8
{
	None,
	/** Windhalm + Windhalm: a fan of piercing pollen darts */
	Sturmgras,
	/** Glutdistel + Glutdistel: one big fire seed that bursts wide and leaves the ground burning */
	Glutbusch,
	/** Taublatt + Taublatt: a big Tauperle that soaks everything around where it bursts and waters the plants twice as far */
	Quellrose,
	/** Windhalm + Glutdistel: the wind carries embers, a fan of darts that set enemies on fire */
	Funkenflug,
	/** Windhalm + Taublatt: a spray of fine drops in a wide fan, everything gets wet, plants get watered */
	Spruehregen,
	/** Glutdistel + Taublatt: a ball of water hitting glowing seeds bursts into a cloud of scalding steam that blinds */
	Dampfstoss,
	// Glimmklee (Boris 2026-10-26): towers grow a dome of the pair, crossed hands act at once
	/** Glimmklee + Glimmklee: a big dome over both towers that flares and blinds / a flash that blinds everything around, Saft per blinded enemy */
	Lichtkuppel,
	/** Glimmklee + Glutdistel: a burning-glass dome, own fire much hotter and enemies inside burn / a short focused sun beam */
	Glutglas,
	/** Glimmklee + Windhalm: glittering pollen in the dome turns every enemy shot aside / a fan of pollen that blinds */
	Funkelwind,
	/** Glimmklee + Taublatt: a dew dome where enemy fire goes out, enemies get wet, plants are watered / a prism drop that blinds and soaks */
	Tauglocke,
	// Rußrose (Boris 2026-10-26): towers beat with healing, crossed hands care at once
	/** Rußrose + Rußrose: a strong heartbeat that heals far and helps the fallen up / both shoulder roses grow into one and shoot their heart */
	Rosenherz,
	/** Rußrose + Windhalm: the wind carries rose petals to every ally far around / a fan of petals that heals allies and cuts enemies */
	Bluetensturm,
	/** Rußrose + Glutdistel: the glowing seeds heal the allies near where they burn / burning seeds that heal allies where they burst */
	Aschenrose,
	/** Rußrose + Taublatt: dew that takes burning, wetness and frost off allies and heals plants (no player healing from water) */
	Rosentau,
	/** Rußrose + Glimmklee: the dome becomes a greenhouse where allies heal slowly / a heart shield around the most hurt ally */
	Glashaus,
	// Funkenmohn (Boris 2026-10-26): every pair throws its own seed capsule in an arc (VaelPoppyCapsule)
	/** Funkenmohn + Funkenmohn: a salvo of capsules / one giant capsule that bursts into many small ones like fireworks */
	Mohnfeuerwerk,
	/** Funkenmohn + Glutdistel: a row of poppies sprouts along the ground to the target and bursts one after the other */
	Zuendschnur,
	/** Funkenmohn + Windhalm: a whirl of burning petals that draws enemies in and burns them */
	Funkenwirbel,
	/** Funkenmohn + Taublatt: it soaks everything, then the sparks leap as lightning from one wet enemy to the next */
	Gewittermohn,
	/** Funkenmohn + Glimmklee: a glass bubble full of sparks shuts the enemies in, then bursts */
	Glutblase,
	/** Funkenmohn + Rußrose: a field of embers that burns out wounds: allies heal slowly, enemies burn */
	Brandsalbe
};

namespace VaelSymbiosis
{
	/** The pair two plants form, None if they form none (yet) */
	EVaelSymbiosis Find(const UVaelPlantArmData* A, const UVaelPlantArmData* B);

	/** Name shown to the players, like "Funkenflug" */
	FText GetName(EVaelSymbiosis Kind);

	/** Colour of the knot and of its glow */
	FLinearColor GetColor(EVaelSymbiosis Kind);

	/** Seconds between two attacks of a fused tower */
	float GetTowerInterval(EVaelSymbiosis Kind);

	/** True for the pairs with a Funkenmohn: they throw capsules in an arc */
	bool IsPoppy(EVaelSymbiosis Kind);

	/** True for the pairs with a Glimmklee: fused towers raise a dome of the pair */
	bool HasDome(EVaelSymbiosis Kind);

	/** True for the pairs with a Rußrose: fused towers beat with care every few seconds */
	bool HasRose(EVaelSymbiosis Kind);

	/** What a fused tower of a rose pair does at each beat, around the place; Power from the root net */
	void RoseBeat(APawn* Instigator, EVaelSymbiosis Kind, const FVector& Center, float Power, bool bShadow);

	/**
	 *  Lets the pair act once from the origin towards the direction (flat over the ground). Power multiplies the damage
	 *  (rooted hands, giant form), Charge (0 to 1) makes a held cross of the hands bigger. Reach is how far away the target is (a
	 *  tower knows it); 0 lets the pairs that throw look for a target ahead.
	 */
	void Fire(APawn* Instigator, EVaelSymbiosis Kind, const UVaelPlantArmData* A, const UVaelPlantArmData* B, const FVector& Origin, const FVector& Direction, float Power, float Charge, bool bShadow, float Reach = 0.0f);

	/** One shot of a plant as its hand fires it; the scales make it bigger or stronger. Returns false if it could not be spawned */
	bool FireShot(APawn* Instigator, const UVaelPlantArmData* Plant, const FVector& Muzzle, const FVector& Direction, float Power, bool bShadow, float SizeScale = 1.0f, float DamageScale = 1.0f, float Reach = 0.0f);

	/** Throws a seed capsule of the Funkenmohn from the muzzle to the ground at the target; Charge 0 tapped to 1 fully held makes it
	 *  fly further and burst bigger, Split breaks it up in the air (rooted), the scales make it bigger or stronger */
	void ThrowCapsule(APawn* Instigator, const UVaelPlantArmData* Plant, const FVector& Muzzle, const FVector& Target, float Charge, float Power, bool bShadow, int32 Split = 0, float SizeScale = 1.0f);

	/** Colour the parts of a plant glow in before they fire, and of the puff where its shot leaves (Boris 2026-10-26) */
	FLinearColor GetPlantColor(const UVaelPlantArmData* Plant, bool bShadow);

	/** A small glowing puff of the plant where one of its shots leaves: flakes of thistle down, pollen, drops or sparks */
	void MuzzlePuff(const UObject* WorldContext, const UVaelPlantArmData* Plant, const FVector& Location, bool bShadow, float Scale = 1.0f);

	/** The living enemy of the instigator closest to the location within the range, null if there is none */
	AActor* FindTarget(APawn* Instigator, const FVector& Location, float Range);
}
