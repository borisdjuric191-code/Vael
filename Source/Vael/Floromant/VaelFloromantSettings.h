// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "VaelFloromantSettings.generated.h"

class UMaterialInterface;
class USkeletalMesh;
class UVaelPlantData;
class UVaelPlantArmData;

/** The two classes a player can play: the mage of the elements and the Floromant, a fairy who sows plants and calls them to act */
UENUM(BlueprintType)
enum class EVaelHeroClass : uint8
{
	Mage,
	Floromant
};

/** The way a hero has sworn to at the end of Act II; none before the oath */
UENUM(BlueprintType)
enum class EVaelPath : uint8
{
	None,
	Light,
	Shadow,
	Twilight
};

/**
 *  Values of the Floromant, the fifth class: body, Saft, seed belt, the Ruf des Gartens and the other abilities.
 *  Shown in the project settings under Vael Floromant. The plants themselves are data assets DA_Pflanze_<Id>.
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

	// --- Body

	/** Body and head of the Floromant, male and female, on the skeleton of the mage */
	UPROPERTY(config, EditAnywhere, Category="Body")
	TSoftObjectPtr<USkeletalMesh> BodyMeshes[2];

	UPROPERTY(config, EditAnywhere, Category="Body")
	TSoftObjectPtr<USkeletalMesh> HeadMeshes[2];

	/** Material of the wings, an instance of M_Floromant_Fluegel with the picture of one wing */
	UPROPERTY(config, EditAnywhere, Category="Body")
	TSoftObjectPtr<UMaterialInterface> WingMaterial;

	/** Size of one wing in cm: width and height */
	UPROPERTY(config, EditAnywhere, Category="Body")
	FVector2D WingSize = FVector2D(62.0f, 94.0f);

	/** Where the wings sit on the back, relative to the middle of the character: back, side, up */
	UPROPERTY(config, EditAnywhere, Category="Body")
	FVector WingRoot = FVector(-14.0f, 5.0f, 48.0f);

	/** Wing beats per second while standing and while moving */
	UPROPERTY(config, EditAnywhere, Category="Body", meta = (ClampMin = 0))
	float WingBeatIdle = 1.1f;

	UPROPERTY(config, EditAnywhere, Category="Body", meta = (ClampMin = 0))
	float WingBeatMoving = 2.4f;

	// --- Hovering

	/** The Floromant doesn't walk, it hovers: height above the ground in cm, how far it bobs and how fast */
	UPROPERTY(config, EditAnywhere, Category="Hovering", meta = (ClampMin = 0))
	float HoverHeight = 25.0f;

	UPROPERTY(config, EditAnywhere, Category="Hovering", meta = (ClampMin = 0))
	float HoverBob = 5.0f;

	UPROPERTY(config, EditAnywhere, Category="Hovering", meta = (ClampMin = 0))
	float HoverBobSpeed = 1.4f;

	// --- Saft

	/** Most Saft the Floromant can hold */
	UPROPERTY(config, EditAnywhere, Category="Saft", meta = (ClampMin = 1))
	float MaxSaft = 100.0f;

	/** Saft that flows back per second; slower than the mana of the mage */
	UPROPERTY(config, EditAnywhere, Category="Saft", meta = (ClampMin = 0))
	float SaftPerSecond = 6.0f;

	// --- Seeds

	/** The four seeds of the seed belt, picked with the d-pad and sown with Square */
	UPROPERTY(config, EditAnywhere, Category="Seeds")
	TArray<TSoftObjectPtr<UVaelPlantData>> SeedBelt;

	/** Folder of the plant data DA_Pflanze_<Id>; plants in the world answer the call by their id */
	UPROPERTY(config, EditAnywhere, Category="Seeds")
	FString PlantDataFolder = TEXT("/Game/Vael/Characters/Floromant/Pflanzen");

	/** Most plants one Floromant can have at once; a new one makes the oldest wither */
	UPROPERTY(config, EditAnywhere, Category="Seeds", meta = (ClampMin = 1))
	int32 MaxPlants = 4;

	/** How far a seed is thrown at most, how long it flies, the seconds between two throws, and how long a plant grows, in cm and seconds */
	UPROPERTY(config, EditAnywhere, Category="Seeds", meta = (ClampMin = 100))
	float ThrowRange = 700.0f;

	UPROPERTY(config, EditAnywhere, Category="Seeds", meta = (ClampMin = 0.1))
	float SeedFlightTime = 0.45f;

	UPROPERTY(config, EditAnywhere, Category="Seeds", meta = (ClampMin = 0))
	float ThrowCooldown = 0.5f;

	UPROPERTY(config, EditAnywhere, Category="Seeds", meta = (ClampMin = 0.1))
	float GrowTime = 1.4f;

	// --- Ruf des Gartens (Circle)

	/** Own and wild plants within this many cm act */
	UPROPERTY(config, EditAnywhere, Category="Ruf des Gartens", meta = (ClampMin = 0))
	float CallRadius = 800.0f;

	/** Seconds they act, and seconds until the next call */
	UPROPERTY(config, EditAnywhere, Category="Ruf des Gartens", meta = (ClampMin = 0.1))
	float CallDuration = 4.0f;

	UPROPERTY(config, EditAnywhere, Category="Ruf des Gartens", meta = (ClampMin = 0))
	float CallCooldown = 3.0f;

	/** Corruption of the region for each light form a Floromant of shadow spoils with the call */
	UPROPERTY(config, EditAnywhere, Category="Ruf des Gartens", meta = (ClampMin = 0))
	float SpoilCorruption = 0.5f;

	// --- Wurzelgriff (Cross)

	/** Saft, cooldown, radius at the aim point, seconds it holds, damage per second while it chokes */
	UPROPERTY(config, EditAnywhere, Category="Wurzelgriff", meta = (ClampMin = 0))
	float GripCost = 25.0f;

	UPROPERTY(config, EditAnywhere, Category="Wurzelgriff", meta = (ClampMin = 0))
	float GripCooldown = 6.0f;

	UPROPERTY(config, EditAnywhere, Category="Wurzelgriff", meta = (ClampMin = 0))
	float GripRadius = 230.0f;

	UPROPERTY(config, EditAnywhere, Category="Wurzelgriff", meta = (ClampMin = 0))
	float GripDuration = 2.5f;

	UPROPERTY(config, EditAnywhere, Category="Wurzelgriff", meta = (ClampMin = 0))
	float GripDamagePerSecond = 7.0f;

	// --- Wurzelsprung (Triangle)

	/** Saft, cooldown and how far the Floromant can leap to one of its plants */
	UPROPERTY(config, EditAnywhere, Category="Wurzelsprung", meta = (ClampMin = 0))
	float LeapCost = 15.0f;

	UPROPERTY(config, EditAnywhere, Category="Wurzelsprung", meta = (ClampMin = 0))
	float LeapCooldown = 5.0f;

	UPROPERTY(config, EditAnywhere, Category="Wurzelsprung", meta = (ClampMin = 0))
	float LeapRange = 1600.0f;

	// --- Wuchern (L1 + R1)

	/** Saft, cooldown, radius and seconds the plants around grow into their giant form and act */
	UPROPERTY(config, EditAnywhere, Category="Wuchern", meta = (ClampMin = 0))
	float OvergrowthCost = 40.0f;

	UPROPERTY(config, EditAnywhere, Category="Wuchern", meta = (ClampMin = 0))
	float OvergrowthCooldown = 30.0f;

	UPROPERTY(config, EditAnywhere, Category="Wuchern", meta = (ClampMin = 0))
	float OvergrowthRadius = 800.0f;

	UPROPERTY(config, EditAnywhere, Category="Wuchern", meta = (ClampMin = 0))
	float OvergrowthDuration = 10.0f;

	/** Size and strength of the giant form */
	UPROPERTY(config, EditAnywhere, Category="Wuchern", meta = (ClampMin = 1))
	float GiantScale = 1.8f;

	UPROPERTY(config, EditAnywhere, Category="Wuchern", meta = (ClampMin = 1))
	float GiantPower = 1.5f;

	// --- Dornenpeitsche (basic attack, R2)

	/** Reach, damage and seconds between two lashes */
	UPROPERTY(config, EditAnywhere, Category="Dornenpeitsche", meta = (ClampMin = 0))
	float WhipRange = 420.0f;

	UPROPERTY(config, EditAnywhere, Category="Dornenpeitsche", meta = (ClampMin = 0))
	float WhipDamage = 11.0f;

	UPROPERTY(config, EditAnywhere, Category="Dornenpeitsche", meta = (ClampMin = 0))
	float WhipCooldown = 0.55f;

	// --- Vine wall (Schlotwinde)

	/** Length of the wall across, its height and thickness, how many vines make it and how long it stands, in cm and seconds */
	UPROPERTY(config, EditAnywhere, Category="Vine Wall", meta = (ClampMin = 50))
	float WallLength = 420.0f;

	UPROPERTY(config, EditAnywhere, Category="Vine Wall", meta = (ClampMin = 20))
	float WallHeight = 150.0f;

	UPROPERTY(config, EditAnywhere, Category="Vine Wall", meta = (ClampMin = 10))
	float WallThickness = 70.0f;

	UPROPERTY(config, EditAnywhere, Category="Vine Wall", meta = (ClampMin = 2, ClampMax = 24))
	int32 WallRoots = 14;

	UPROPERTY(config, EditAnywhere, Category="Vine Wall", meta = (ClampMin = 1))
	float WallLifetime = 8.0f;

	/** Material of roots and vines (wall, grip, whip), an instance of M_Floromant_Pflanze */
	UPROPERTY(config, EditAnywhere, Category="Vine Wall")
	TSoftObjectPtr<UMaterialInterface> RootMaterial;

	// --- Plant arms

	/** The seed belt of the hands: plants from the repertoire that can be laid on a hand (d-pad up / down picks, left / right lays it on that hand) */
	UPROPERTY(config, EditAnywhere, Category="Plant Arms")
	TArray<TSoftObjectPtr<UVaelPlantArmData>> HandBelt;

	/** Walking speed while a hand fires: the Floromant lands and braces itself */
	UPROPERTY(config, EditAnywhere, Category="Plant Arms", meta = (ClampMin = 0, ClampMax = 1))
	float FiringMoveSpeedScale = 0.45f;

	/** Degrees the body leans back while firing, and how far the firing arm is stretched towards the aim (0 to 1) */
	UPROPERTY(config, EditAnywhere, Category="Plant Arms", meta = (ClampMin = 0, ClampMax = 40))
	float FiringLean = 14.0f;

	UPROPERTY(config, EditAnywhere, Category="Plant Arms", meta = (ClampMin = 0, ClampMax = 1))
	float FiringArmReach = 1.0f;

	/** Seconds L2 and R2 (Q and E) wait for the other one, so pressing both doesn't set a single tower first */
	UPROPERTY(config, EditAnywhere, Category="Plant Arms", meta = (ClampMin = 0, ClampMax = 0.5))
	float TowerChordDelay = 0.1f;

	/** Seconds the d-pad down is held to drink a healing potion instead of paging the belt */
	UPROPERTY(config, EditAnywhere, Category="Plant Arms", meta = (ClampMin = 0.1, ClampMax = 2))
	float PotionHoldTime = 0.4f;
};
