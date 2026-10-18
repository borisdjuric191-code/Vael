// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "VaelFloromantSettings.generated.h"

class UMaterialInterface;
class UStaticMesh;
class UTexture2D;

/** The two classes a player can play: the mage of the elements and the Floromant, a fairy who throws seeds that grow into plants */
UENUM(BlueprintType)
enum class EVaelHeroClass : uint8
{
	Mage,
	Floromant
};

/** The seeds of the Floromant; each grows into its own plant */
UENUM(BlueprintType)
enum class EVaelSeedKind : uint8
{
	/** Glutdistel: a shooter that spits seeds of fire which burst */
	Thistle,
	/** Wurzelwand: a wall of roots that holds enemies back */
	RootWall,
	/** Tränenkelch: a chalice that heals allies around it */
	Chalice,

	Count UMETA(Hidden)
};

static_assert(static_cast<int32>(EVaelSeedKind::Count) == 3, "UVaelFloromantSettings::Plants holds one plant per seed");

/** Look and values of one plant */
USTRUCT(BlueprintType)
struct FVaelPlantLook
{
	GENERATED_BODY()

	/** Name shown to the players */
	UPROPERTY(EditAnywhere, Category="Plant")
	FText DisplayName;

	/** Model of the plant; empty for the wall of roots, which is built from roots in code */
	UPROPERTY(EditAnywhere, Category="Plant")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** Material of the plant, an instance of M_Floromant_Pflanze, so it can grow in, sway and flash */
	UPROPERTY(EditAnywhere, Category="Plant")
	TSoftObjectPtr<UMaterialInterface> Material;

	/** Height of the grown plant in cm; the model is scaled to it */
	UPROPERTY(EditAnywhere, Category="Plant", meta = (ClampMin = 10))
	float Height = 120.0f;

	/** Saft a seed costs */
	UPROPERTY(EditAnywhere, Category="Plant", meta = (ClampMin = 0))
	float SaftCost = 30.0f;

	/** Health of the plant; it takes damage from the fire of enemies only */
	UPROPERTY(EditAnywhere, Category="Plant", meta = (ClampMin = 1))
	float Health = 80.0f;

	/** Seconds the plant lives before it withers */
	UPROPERTY(EditAnywhere, Category="Plant", meta = (ClampMin = 1))
	float Lifetime = 20.0f;

	/** Colour of the seed, the glow while it grows and the flash before it acts */
	UPROPERTY(EditAnywhere, Category="Plant")
	FLinearColor Color = FLinearColor(1.0f, 0.35f, 0.65f);
};

/**
 *  Values of the Floromant, the fifth class: Saft, seeds and the three test plants.
 *  Shown in the project settings under Vael Floromant.
 */
UCLASS(config=Game, defaultconfig, meta = (DisplayName = "Vael Floromant"))
class UVaelFloromantSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	/** Constructor */
	UVaelFloromantSettings();

	/** Returns the settings */
	static const UVaelFloromantSettings* Get() { return GetDefault<UVaelFloromantSettings>(); }

	/** Look and values of a plant */
	const FVaelPlantLook& GetPlant(EVaelSeedKind Kind) const;

	// --- Body

	/** Body and head of the Floromant, male and female, on the skeleton of the mage */
	UPROPERTY(config, EditAnywhere, Category="Body")
	TSoftObjectPtr<class USkeletalMesh> BodyMeshes[2];

	UPROPERTY(config, EditAnywhere, Category="Body")
	TSoftObjectPtr<class USkeletalMesh> HeadMeshes[2];

	/** Material of the wings, an instance of M_Floromant_Fluegel with the picture of one wing */
	UPROPERTY(config, EditAnywhere, Category="Body")
	TSoftObjectPtr<UMaterialInterface> WingMaterial;

	/** Size of one wing in cm: width and height */
	UPROPERTY(config, EditAnywhere, Category="Body")
	FVector2D WingSize = FVector2D(62.0f, 94.0f);

	/** Where the wings sit on the back, relative to the middle of the character: back, side, up */
	UPROPERTY(config, EditAnywhere, Category="Body")
	FVector WingRoot = FVector(-14.0f, 5.0f, 48.0f);

	/** Wing beats per second while standing and while walking */
	UPROPERTY(config, EditAnywhere, Category="Body", meta = (ClampMin = 0))
	float WingBeatIdle = 0.6f;

	UPROPERTY(config, EditAnywhere, Category="Body", meta = (ClampMin = 0))
	float WingBeatMoving = 1.6f;

	// --- Saft

	/** Most Saft the Floromant can hold */
	UPROPERTY(config, EditAnywhere, Category="Saft", meta = (ClampMin = 1))
	float MaxSaft = 100.0f;

	/** Saft that flows back per second; slower than the mana of the mage */
	UPROPERTY(config, EditAnywhere, Category="Saft", meta = (ClampMin = 0))
	float SaftPerSecond = 6.0f;

	// --- Seeds

	/** The plants, by seed: Glutdistel, Wurzelwand, Tränenkelch */
	UPROPERTY(config, EditAnywhere, Category="Seeds")
	FVaelPlantLook Plants[3];

	/** Most plants one Floromant can have at once; a new one makes the oldest wither */
	UPROPERTY(config, EditAnywhere, Category="Seeds", meta = (ClampMin = 1))
	int32 MaxPlants = 4;

	/** How far a seed is thrown at most, in cm */
	UPROPERTY(config, EditAnywhere, Category="Seeds", meta = (ClampMin = 100))
	float ThrowRange = 700.0f;

	/** Seconds a seed flies */
	UPROPERTY(config, EditAnywhere, Category="Seeds", meta = (ClampMin = 0.1))
	float SeedFlightTime = 0.45f;

	/** Seconds between two throws */
	UPROPERTY(config, EditAnywhere, Category="Seeds", meta = (ClampMin = 0))
	float ThrowCooldown = 0.5f;

	/** Seconds a plant needs to grow out of the seed */
	UPROPERTY(config, EditAnywhere, Category="Seeds", meta = (ClampMin = 0.1))
	float GrowTime = 1.4f;

	// --- Glutdistel

	/** Distance at which the thistle finds enemies, in cm */
	UPROPERTY(config, EditAnywhere, Category="Glutdistel", meta = (ClampMin = 0))
	float ThistleRange = 900.0f;

	/** Seconds between two shots */
	UPROPERTY(config, EditAnywhere, Category="Glutdistel", meta = (ClampMin = 0.1))
	float ThistleShotInterval = 1.6f;

	/** Seconds it glows and draws back before it shoots, so players see the shot coming */
	UPROPERTY(config, EditAnywhere, Category="Glutdistel", meta = (ClampMin = 0))
	float ThistleWindup = 0.35f;

	/** Damage of the seed of fire where it hits and of its burst around it */
	UPROPERTY(config, EditAnywhere, Category="Glutdistel", meta = (ClampMin = 0))
	float ThistleDamage = 9.0f;

	UPROPERTY(config, EditAnywhere, Category="Glutdistel", meta = (ClampMin = 0))
	float ThistleBurstDamage = 7.0f;

	UPROPERTY(config, EditAnywhere, Category="Glutdistel", meta = (ClampMin = 0))
	float ThistleBurstRadius = 160.0f;

	/** Speed of the seed of fire in cm/s */
	UPROPERTY(config, EditAnywhere, Category="Glutdistel", meta = (ClampMin = 1))
	float ThistleShotSpeed = 1300.0f;

	// --- Wurzelwand

	/** Length of the wall across the throw, its height and its thickness, in cm */
	UPROPERTY(config, EditAnywhere, Category="Wurzelwand", meta = (ClampMin = 50))
	float WallLength = 420.0f;

	UPROPERTY(config, EditAnywhere, Category="Wurzelwand", meta = (ClampMin = 20))
	float WallHeight = 150.0f;

	UPROPERTY(config, EditAnywhere, Category="Wurzelwand", meta = (ClampMin = 10))
	float WallThickness = 70.0f;

	/** Roots that make up the wall */
	UPROPERTY(config, EditAnywhere, Category="Wurzelwand", meta = (ClampMin = 2, ClampMax = 24))
	int32 WallRoots = 14;

	// --- Tränenkelch

	/** Radius of the healing around the chalice in cm */
	UPROPERTY(config, EditAnywhere, Category="Traenenkelch", meta = (ClampMin = 0))
	float ChaliceRadius = 380.0f;

	/** Health it gives every ally inside per second */
	UPROPERTY(config, EditAnywhere, Category="Traenenkelch", meta = (ClampMin = 0))
	float ChaliceHealPerSecond = 6.0f;

	/** Seconds between two pulses of healing */
	UPROPERTY(config, EditAnywhere, Category="Traenenkelch", meta = (ClampMin = 0.1))
	float ChalicePulseInterval = 1.0f;
};
