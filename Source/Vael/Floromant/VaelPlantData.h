// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VaelPlantData.generated.h"

class UMaterialInterface;
class UStaticMesh;

/** What a plant does while the Ruf des Gartens makes it act */
UENUM(BlueprintType)
enum class EVaelPlantEffect : uint8
{
	None,
	// Light forms
	/** Glimmklee: blinds enemies around it and gives Saft to the Floromant */
	BlindAndSaft,
	/** Taublatt: makes enemies around it wet and slow */
	WetAndSlow,
	/** Windhalm: shoots pollen darts at enemies */
	PollenDarts,
	/** Schlotwinde: raises a wall of vines towards the enemies */
	VineWall,
	/** Rußrose: heals allies around it */
	HealAllies,
	/** Milchstern: cleanses allies of burning, wet and frost (poison and curses once they exist) */
	Cleanse,
	/** Funkenmohn: bursts in fire and leaves a burning patch, then withers */
	FireBurst,
	/** Glutdistel: shoots seeds of fire that burst */
	FireSeeds,
	// Shadow forms
	/** Schwelklee: smoke that blinds enemies inside */
	Smoke,
	/** Faulblatt: a pool of poison that hurts enemies standing in it */
	PoisonPool,
	/** Flüsterhalm: confuses enemies, they go for each other */
	Confuse,
	/** Würgwinde: holds enemies around it fast */
	Hold,
	/** Blutrose: drinks the life of enemies around it for the Floromant */
	LifeSteal,
	/** Gallstern: weakens enemies, they take more damage */
	Weaken,
	/** Schwarzmohn: sleeping pollen, enemies stand still */
	Sleep
};

/**
 *  One plant of the Floromant: its light form, its shadow form and what both do when the Ruf des Gartens makes them act.
 *  Same names as the seed plants of the garden (PlantId), so plants growing in the world answer the call as well.
 *  One asset per plant: DA_Pflanze_<Id>.
 */
UCLASS(BlueprintType)
class UVaelPlantData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** Name in the garden and in the compendium, like "Glimmklee" */
	UPROPERTY(EditAnywhere, Category="Plant")
	FName PlantId;

	UPROPERTY(EditAnywhere, Category="Plant")
	FText LightName;

	UPROPERTY(EditAnywhere, Category="Plant")
	FText ShadowName;

	UPROPERTY(EditAnywhere, Category="Plant")
	EVaelPlantEffect LightEffect = EVaelPlantEffect::None;

	UPROPERTY(EditAnywhere, Category="Plant")
	EVaelPlantEffect ShadowEffect = EVaelPlantEffect::None;

	/** Model of the plant; the shadow form shows the same model under the shadow overlay. Empty: a placeholder shape */
	UPROPERTY(EditAnywhere, Category="Look")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** Material, an instance of M_Floromant_Pflanze, so it grows in, sways and flashes */
	UPROPERTY(EditAnywhere, Category="Look")
	TSoftObjectPtr<UMaterialInterface> Material;

	/** Height of the grown plant in cm */
	UPROPERTY(EditAnywhere, Category="Look", meta = (ClampMin = 10))
	float Height = 100.0f;

	/** Colour of the seed, of the glow while it grows and of the flash before it acts */
	UPROPERTY(EditAnywhere, Category="Look")
	FLinearColor Color = FLinearColor(1.0f, 0.35f, 0.65f);

	UPROPERTY(EditAnywhere, Category="Look")
	FLinearColor ShadowColor = FLinearColor(0.61f, 0.36f, 0.9f);

	/** Saft sowing costs, and Saft for every call that makes it act */
	UPROPERTY(EditAnywhere, Category="Values", meta = (ClampMin = 0))
	float SowCost = 25.0f;

	UPROPERTY(EditAnywhere, Category="Values", meta = (ClampMin = 0))
	float CallCost = 6.0f;

	/** Health of the plant; only the fire of enemies hurts it */
	UPROPERTY(EditAnywhere, Category="Values", meta = (ClampMin = 1))
	float Health = 70.0f;

	/** Seconds a sown plant lives before it withers */
	UPROPERTY(EditAnywhere, Category="Values", meta = (ClampMin = 1))
	float Lifetime = 30.0f;

	/** Strength of its effect: damage, healing or Saft per pulse; how far it reaches in cm */
	UPROPERTY(EditAnywhere, Category="Values", meta = (ClampMin = 0))
	float Power = 8.0f;

	UPROPERTY(EditAnywhere, Category="Values", meta = (ClampMin = 0))
	float Radius = 400.0f;

	/** Name and effect of a form */
	const FText& GetName(bool bShadow) const { return bShadow && !ShadowName.IsEmpty() ? ShadowName : LightName; }
	EVaelPlantEffect GetEffect(bool bShadow) const { return bShadow && ShadowEffect != EVaelPlantEffect::None ? ShadowEffect : LightEffect; }
};
