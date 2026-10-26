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
	Dampfstoss
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

	/**
	 *  Lets the pair act once from the origin towards the direction (flat over the ground). Power multiplies the damage
	 *  (rooted hands, giant form), Charge (0 to 1) makes a held cross of the hands bigger.
	 */
	void Fire(APawn* Instigator, EVaelSymbiosis Kind, const UVaelPlantArmData* A, const UVaelPlantArmData* B, const FVector& Origin, const FVector& Direction, float Power, float Charge, bool bShadow);

	/** One shot of a plant as its hand fires it; the scales make it bigger or stronger. Returns false if it could not be spawned */
	bool FireShot(APawn* Instigator, const UVaelPlantArmData* Plant, const FVector& Muzzle, const FVector& Direction, float Power, bool bShadow, float SizeScale = 1.0f, float DamageScale = 1.0f);

	/** The living enemy of the instigator closest to the location within the range, null if there is none */
	AActor* FindTarget(APawn* Instigator, const FVector& Location, float Range);
}
