// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Magic/VaelElementTypes.h"
#include "VaelPlantArmData.generated.h"

class UMaterialInterface;
class UNiagaraSystem;
class UStaticMesh;
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
