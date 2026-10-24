// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Magic/VaelElementTypes.h"
#include "VaelPlantArmData.generated.h"

class UMaterialInterface;
class UNiagaraSystem;
class UStaticMesh;
class UVaelElementSource;
class UVaelPlantData;

/**
 *  A plant the hands of a Floromant turn into (DA_Pflanzenarm_<Id>): the stalks that grow out of fingers, forearm and
 *  upper arm, and the shots the fingertips fire while the button of the hand is held.
 *  Every stalk mesh stands on +Z with its pivot at the base and carries a vertex colour ramp from 0 at the base to 1 at
 *  the tip; the arm bends it along a spline, the material M_Halm sways, spins and grows it.
 */
UCLASS(BlueprintType)
class UVaelPlantArmData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** The plant itself: name, shadow name and colours on the seed belt */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plant")
	TSoftObjectPtr<UVaelPlantData> Plant;

	// --- Look

	/** Stalks that grow out of the fingers, long, ending in the head of the plant; picked in turn per finger */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look")
	TArray<TSoftObjectPtr<UStaticMesh>> FingerMeshes;

	/** Stalks that wind around the forearm and grow wild from the upper arm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look")
	TArray<TSoftObjectPtr<UStaticMesh>> ArmMeshes;

	/** Optional head on the end of every finger stalk (like the ball of the Glutdistel); none for plants whose stalk ends in it already */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look")
	TSoftObjectPtr<UStaticMesh> TipMesh;

	/** Size of the head in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look", meta = (ClampMin = 0))
	float TipSize = 10.0f;

	/** Material of all stalks, made from M_Halm (Wind, Spin, Wuchs, Schatten, Aufleuchten) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look")
	TSoftObjectPtr<UMaterialInterface> Material;

	/** How far a finger stalk reaches beyond the fingertip, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look", meta = (ClampMin = 0))
	float FingerLength = 55.0f;

	/** Where the finger stalks come out of the arm: 0 at the wrist, 1 at the elbow */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look", meta = (ClampMin = 0, ClampMax = 1))
	float FingerRootReach = 0.85f;

	/** Thickness of a finger stalk, compared with the mesh */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look", meta = (ClampMin = 0))
	float FingerThickness = 0.45f;

	/** Thickness of the stalks of the arm (winding and wild), compared with their usual size */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look", meta = (ClampMin = 0))
	float ArmThickness = 1.0f;

	/** Stalks that wind around the forearm in half a turn */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look", meta = (ClampMin = 0, ClampMax = 8))
	int32 WrapCount = 4;

	/** Distance of the winding stalks from the bone of the forearm, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look", meta = (ClampMin = 0))
	float WrapRadius = 6.0f;

	/** Turns of a winding stalk from elbow to wrist */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look", meta = (ClampMin = 0))
	float WrapTurns = 0.5f;

	/** Stalks that grow from the upper arm over the elbow and end in nothing, so the arm looks overgrown */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look", meta = (ClampMin = 0, ClampMax = 8))
	int32 WildCount = 3;

	/** How far up the upper arm the plant grows: 0 at the elbow, 1 at the shoulder */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look", meta = (ClampMin = 0, ClampMax = 1))
	float UpperArmReach = 0.45f;

	/** Length of the wild stalks in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look", meta = (ClampMin = 0))
	float WildLength = 30.0f;

	/** Sway of the stalks at rest (material parameter Wind) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look", meta = (ClampMin = 0))
	float RestWind = 0.35f;

	/** Angle in degrees by which the finger stalks fan out while firing */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look", meta = (ClampMin = 0, ClampMax = 60))
	float FanAngle = 22.0f;

	/** Turns per second of the winding stalks after firing for SpinRampTime seconds; they start slowly */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look", meta = (ClampMin = 0))
	float SpinSpeed = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look", meta = (ClampMin = 0.01))
	float SpinRampTime = 2.0f;

	/** Seconds the hand needs to turn into the plant, and back */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look", meta = (ClampMin = 0.01))
	float GrowTime = 0.4f;

	// --- Overgrowth: while the hand aims, the plant overruns the whole arm like heavy armament

	/** Thick twisted vines (SM_<Id>_Ranke) that climb from the wrist over the elbow up to the shoulder */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Overgrowth")
	TArray<TSoftObjectPtr<UStaticMesh>> VineMeshes;

	/** Vines around the arm, and their thickness compared with the mesh */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Overgrowth", meta = (ClampMin = 0, ClampMax = 6))
	int32 VineCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Overgrowth", meta = (ClampMin = 0))
	float VineThickness = 1.0f;

	/** More wild stalks out of upper arm and shoulder, and their length in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Overgrowth", meta = (ClampMin = 0, ClampMax = 10))
	int32 OvergrowthWildCount = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Overgrowth", meta = (ClampMin = 0))
	float OvergrowthWildLength = 45.0f;

	/** Seconds the overgrowth needs to cover the arm when the hand starts to aim, and to fall back when it stops */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Overgrowth", meta = (ClampMin = 0.05))
	float OvergrowthTime = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Overgrowth", meta = (ClampMin = 0.05))
	float OvergrowthFadeTime = 0.8f;

	/** Burst while the hand turns; empty uses the burst of the element of the shots */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look")
	TSoftObjectPtr<UNiagaraSystem> GrowEffect;

	// --- Funnel leaves (Taublatt, Boris 2026-10-24): loose and open along the arm at rest, the cup facing the hand; while aiming
	// they lie against the arm and a pump wave runs through them from the shoulder to the hand; the heads of the fingers nest
	// into each other in front of the hand and form the barrel

	/** Leaf along the arm, a funnel with its foot at the pivot and its opening up (+Z); none for plants without */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Funnels")
	TSoftObjectPtr<UStaticMesh> LeafMesh;

	/** Leaves along forearm and upper arm, their height in cm, and how far they stand off the arm at rest in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Funnels", meta = (ClampMin = 0, ClampMax = 12))
	int32 LeafCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Funnels", meta = (ClampMin = 1))
	float LeafSize = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Funnels", meta = (ClampMin = 0, ClampMax = 90))
	float LeafOpenAngle = 55.0f;

	/** While aiming the finger heads gather in front of the hand into a barrel, each one nested into the one before it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Funnels")
	bool bFingersFormBarrel = false;

	/** Distance between two funnels of the barrel, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Funnels", meta = (ClampMin = 0))
	float BarrelSpacing = 7.0f;

	/** Seconds the pump wave takes from the shoulder to the hand; a tapped shot leaves the barrel when the wave arrives */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Funnels", meta = (ClampMin = 0.05))
	float PumpWaveTime = 0.22f;

	/** How much a leaf swells while the wave passes it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Funnels", meta = (ClampMin = 0))
	float PumpSwell = 0.35f;

	// --- Water: the shots are Tauperlen of the source's water; holding the button turns them into a jet that pushes enemies back.
	// The water heals no one; it waters plants (Boris 2026-10-24)

	/** Water of the shots; set, the shots fly as balls of real water of this source and burst into its puddles */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water")
	TSoftObjectPtr<UVaelElementSource> WaterSource;

	/** Condition the shots and the jet leave, and its seconds (Wet: lightning of the mage hits twice as hard) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water")
	EVaelStatus ShotStatus = EVaelStatus::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water", meta = (ClampMin = 0))
	float ShotStatusDuration = 4.0f;

	/** Radius in cm in which a burst shot waters plants, and how much (1 = a full Tauperle); 0 for plants whose shots don't water */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water", meta = (ClampMin = 0))
	float WaterRadius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water", meta = (ClampMin = 0))
	float WaterPerShot = 1.0f;

	/** Seconds the button has to be held before the shots turn into the jet; 0 = no jet */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Jet", meta = (ClampMin = 0))
	float JetHoldTime = 0.0f;

	/** Damage per second, reach and half width of the jet in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Jet", meta = (ClampMin = 0))
	float JetDamagePerSecond = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Jet", meta = (ClampMin = 50))
	float JetRange = 850.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Jet", meta = (ClampMin = 5))
	float JetHalfWidth = 45.0f;

	/** Speed in cm/s at which the jet pushes enemies away, again and again while they stand in it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Jet", meta = (ClampMin = 0))
	float JetPush = 450.0f;

	/** Shots of the plant bar and Saft the jet uses per second */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Jet", meta = (ClampMin = 0))
	float JetShotsPerSecond = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Jet", meta = (ClampMin = 0))
	float JetSaftPerSecond = 3.0f;

	/** Water the jet gives the plants it touches per second (1 = a Tauperle) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Jet", meta = (ClampMin = 0))
	float JetWaterPerSecond = 2.0f;

	/** Radius of the jet at the barrel and where it ends, in cm, and turns of its spiral per metre */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Jet", meta = (ClampMin = 1))
	float JetStartRadius = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Jet", meta = (ClampMin = 1))
	float JetEndRadius = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Jet", meta = (ClampMin = 0))
	float JetSpiralTurns = 0.8f;

	// --- Shots

	/** Element of the shots; it picks the look of the projectile from the effects of the mage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shots")
	EVaelElement ShotElement = EVaelElement::Air;

	/** Seconds between two shots of one hand; the fingers fire in turn */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shots", meta = (ClampMin = 0.02))
	float FireInterval = 0.12f;

	/** Damage of a shot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shots", meta = (ClampMin = 0))
	float ShotDamage = 4.0f;

	/** Speed (cm/s), size (cm) and reach (cm) of a shot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shots", meta = (ClampMin = 1))
	float ShotSpeed = 2200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shots", meta = (ClampMin = 1))
	float ShotRadius = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shots", meta = (ClampMin = 1))
	float ShotRange = 1100.0f;

	/** Random spread of the shots in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shots", meta = (ClampMin = 0))
	float ShotSpread = 3.0f;

	/** Shots that burst where they land, like the seeds of the Glutdistel in the garden: radius in cm (0 = no burst) and damage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shots", meta = (ClampMin = 0))
	float BurstRadius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shots", meta = (ClampMin = 0))
	float BurstDamage = 0.0f;

	/** Saft every shot takes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shots", meta = (ClampMin = 0))
	float SaftPerShot = 0.8f;

	/** Shots in the plant bar of a hand; Keimpelz fills it again */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shots", meta = (ClampMin = 1))
	int32 BarSize = 40;

	/** Shots per second the bar regains by itself while the hand doesn't fire, after a short pause; 0 (Boris: the bar only fills with Keimpelz) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shots", meta = (ClampMin = 0))
	float BarRefillPerSecond = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shots", meta = (ClampMin = 0))
	float BarRefillDelay = 1.0f;

	/** Name and colours, taken from the plant; the shadow forms on the way of shadow */
	FText GetDisplayName(bool bShadow) const;
	FLinearColor GetColor(bool bShadow) const;
};
