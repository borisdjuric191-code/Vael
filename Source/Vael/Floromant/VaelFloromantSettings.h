// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "VaelFloromantSettings.generated.h"

class UMaterialInterface;
class USkeletalMesh;
class UStaticMesh;
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

	// --- Flight

	/** Degrees the body tips into the way it flies at full speed, and how far more while it speeds up, brakes or turns */
	UPROPERTY(config, EditAnywhere, Category="Flight", meta = (ClampMin = 0, ClampMax = 45))
	float FlightLean = 14.0f;

	UPROPERTY(config, EditAnywhere, Category="Flight", meta = (ClampMin = 0, ClampMax = 45))
	float FlightAccelLean = 10.0f;

	/** Change of speed in cm/s per second that counts as hard speeding up or braking */
	UPROPERTY(config, EditAnywhere, Category="Flight", meta = (ClampMin = 1))
	float FlightAccelReference = 1800.0f;

	/** Most degrees the body ever tips, and how quickly it follows (higher = quicker) */
	UPROPERTY(config, EditAnywhere, Category="Flight", meta = (ClampMin = 0, ClampMax = 60))
	float FlightMaxLean = 26.0f;

	UPROPERTY(config, EditAnywhere, Category="Flight", meta = (ClampMin = 0.1))
	float FlightLeanSpeed = 6.0f;

	/** Extra height in cm at full speed: she rises a little when she flies fast and sinks back when she stops */
	UPROPERTY(config, EditAnywhere, Category="Flight", meta = (ClampMin = 0))
	float FlightRise = 18.0f;

	/** Degrees the dangling legs trail behind at full speed */
	UPROPERTY(config, EditAnywhere, Category="Flight", meta = (ClampMin = 0, ClampMax = 60))
	float FlightLegTrail = 14.0f;

	/** How the legs swing after when the body tips: spring stiffness and damping (low damping = more swinging) */
	UPROPERTY(config, EditAnywhere, Category="Flight", meta = (ClampMin = 1))
	float FlightLegStiffness = 40.0f;

	UPROPERTY(config, EditAnywhere, Category="Flight", meta = (ClampMin = 0))
	float FlightLegDamping = 5.0f;

	/** Slow paddling of the hanging legs in degrees, the bend of the knees and how far the toes point down */
	UPROPERTY(config, EditAnywhere, Category="Flight", meta = (ClampMin = 0, ClampMax = 45))
	float FlightLegKick = 6.0f;

	UPROPERTY(config, EditAnywhere, Category="Flight", meta = (ClampMin = 0, ClampMax = 90))
	float FlightKneeBend = 18.0f;

	UPROPERTY(config, EditAnywhere, Category="Flight", meta = (ClampMin = 0, ClampMax = 90))
	float FlightFootPoint = 28.0f;

	/** Seconds of steady fast flight before the wings stop beating and she glides, and seconds between the beats that keep her up */
	UPROPERTY(config, EditAnywhere, Category="Flight", meta = (ClampMin = 0))
	float GlideDelay = 0.8f;

	UPROPERTY(config, EditAnywhere, Category="Flight")
	FVector2D GlidePumpInterval = FVector2D(1.6f, 3.2f);

	// --- Rising, rooting, wind and rain

	/** R3 (Left Ctrl): she rises high above the ground, out of reach of everything on it (fire, water, fields); true = a press switches, false = only while held */
	UPROPERTY(config, EditAnywhere, Category="Flight|Rise")
	bool bRiseToggles = true;

	/** Extra height in cm up high, seconds to get there, Saft per second it costs and share of the speed she flies with up there */
	UPROPERTY(config, EditAnywhere, Category="Flight|Rise", meta = (ClampMin = 0))
	float RiseHeight = 130.0f;

	UPROPERTY(config, EditAnywhere, Category="Flight|Rise", meta = (ClampMin = 0.05))
	float RiseTime = 0.5f;

	UPROPERTY(config, EditAnywhere, Category="Flight|Rise", meta = (ClampMin = 0))
	float RiseSaftPerSecond = 3.0f;

	UPROPERTY(config, EditAnywhere, Category="Flight|Rise", meta = (ClampMin = 0))
	float RiseMoveSpeedScale = 1.15f;

	/** Seconds she must stand firing on the spot before her feet take root, and how long the roots take to grow and to let go */
	UPROPERTY(config, EditAnywhere, Category="Flight|Rooting", meta = (ClampMin = 0))
	float RootDelay = 1.2f;

	UPROPERTY(config, EditAnywhere, Category="Flight|Rooting", meta = (ClampMin = 0.05))
	float RootGrowTime = 0.5f;

	UPROPERTY(config, EditAnywhere, Category="Flight|Rooting", meta = (ClampMin = 0))
	float UprootTime = 0.3f;

	/** Rooted: her hands hit this much harder, the earth gives Saft per second, and nothing pushes her away */
	UPROPERTY(config, EditAnywhere, Category="Flight|Rooting", meta = (ClampMin = 1))
	float RootedHandPower = 1.25f;

	UPROPERTY(config, EditAnywhere, Category="Flight|Rooting", meta = (ClampMin = 0))
	float RootedSaftPerSecond = 5.0f;

	/** Roots that grow out of each foot, and how far they reach in cm */
	UPROPERTY(config, EditAnywhere, Category="Flight|Rooting", meta = (ClampMin = 1, ClampMax = 8))
	int32 RootsPerFoot = 4;

	UPROPERTY(config, EditAnywhere, Category="Flight|Rooting", meta = (ClampMin = 5))
	float RootLength = 45.0f;

	/** Seconds in rain (or wet from a spell) until her wings are soaked, and until they are dry again */
	UPROPERTY(config, EditAnywhere, Category="Flight|Weather", meta = (ClampMin = 0.1))
	float WingSoakTime = 5.0f;

	UPROPERTY(config, EditAnywhere, Category="Flight|Weather", meta = (ClampMin = 0.1))
	float WingDryTime = 12.0f;

	/** Soaked wings carry her this much lower (share of the height), and she cannot glide with them */
	UPROPERTY(config, EditAnywhere, Category="Flight|Weather", meta = (ClampMin = 0, ClampMax = 1))
	float SoakedHeightLoss = 0.5f;

	/** Share of the wind that pushes her while she hovers (twice that up high, none while she stands), and degrees she leans into a full wind */
	UPROPERTY(config, EditAnywhere, Category="Flight|Weather", meta = (ClampMin = 0, ClampMax = 1))
	float WindDrift = 0.18f;

	UPROPERTY(config, EditAnywhere, Category="Flight|Weather", meta = (ClampMin = 0, ClampMax = 30))
	float WindLean = 8.0f;

	/** Wind in cm/s that counts as full wind for leaning and the flutter of the wings */
	UPROPERTY(config, EditAnywhere, Category="Flight|Weather", meta = (ClampMin = 1))
	float WindReference = 500.0f;

	/** Soft shadow under her that tells how high she flies: decal M_Floromant_Schatten, radius in cm on the ground and darkness */
	UPROPERTY(config, EditAnywhere, Category="Flight")
	TSoftObjectPtr<UMaterialInterface> ShadowMaterial;

	UPROPERTY(config, EditAnywhere, Category="Flight", meta = (ClampMin = 0))
	float ShadowRadius = 34.0f;

	UPROPERTY(config, EditAnywhere, Category="Flight", meta = (ClampMin = 0, ClampMax = 1))
	float ShadowStrength = 0.55f;

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

	/** Mortar hand (Funkenmohn, Boris 2026-10-26): degrees the stretched arm aims up above the ground, and how far it draws back
	 *  to the shoulder when fully charged (0 = stays stretched, 1 = the hand at the shoulder) */
	UPROPERTY(config, EditAnywhere, Category="Plant Arms|Mortar", meta = (ClampMin = 0, ClampMax = 80))
	float MortarArmPitch = 42.0f;

	UPROPERTY(config, EditAnywhere, Category="Plant Arms|Mortar", meta = (ClampMin = 0, ClampMax = 1))
	float MortarDrawBack = 0.85f;

	/** Dots of the ring that shows where the capsule will land, and their size in cm */
	UPROPERTY(config, EditAnywhere, Category="Plant Arms|Mortar", meta = (ClampMin = 0, ClampMax = 32))
	int32 MortarRingDots = 14;

	UPROPERTY(config, EditAnywhere, Category="Plant Arms|Mortar", meta = (ClampMin = 1))
	float MortarRingDotSize = 6.0f;

	/** Seconds L2 and R2 (Q and E) wait for the other one, so pressing both doesn't set a single tower first */
	UPROPERTY(config, EditAnywhere, Category="Plant Arms", meta = (ClampMin = 0, ClampMax = 0.5))
	float TowerChordDelay = 0.1f;

	/** Seconds the d-pad down is held to drink a healing potion instead of paging the belt */
	UPROPERTY(config, EditAnywhere, Category="Plant Arms", meta = (ClampMin = 0.1, ClampMax = 2))
	float PotionHoldTime = 0.4f;

	// --- Towers (L2 / R2, Boris 2026-10-26): the arm plant is stuck into the ground and grows into a tower that fires on its
	// own; the arm grows back before it is a plant hand again. Two towers close together fuse into a symbiosis

	/** Height of a tower in cm, how far ahead of her it is planted, seconds it stands and Saft it costs */
	UPROPERTY(config, EditAnywhere, Category="Towers", meta = (ClampMin = 50))
	float TowerHeight = 200.0f;

	UPROPERTY(config, EditAnywhere, Category="Towers", meta = (ClampMin = 0))
	float TowerPlantDistance = 110.0f;

	UPROPERTY(config, EditAnywhere, Category="Towers", meta = (ClampMin = 1))
	float TowerLifetime = 20.0f;

	UPROPERTY(config, EditAnywhere, Category="Towers", meta = (ClampMin = 0))
	float TowerSaftCost = 15.0f;

	/** Seconds the arm needs to grow back after it went into the ground */
	UPROPERTY(config, EditAnywhere, Category="Towers", meta = (ClampMin = 0))
	float TowerRegrowTime = 6.0f;

	/** Towers a player can have standing at once; a new one makes the oldest wither */
	UPROPERTY(config, EditAnywhere, Category="Towers", meta = (ClampMin = 1, ClampMax = 6))
	int32 MaxTowers = 2;

	/** A tower fires the shot of its plant this many times slower than the hand, and a little bigger */
	UPROPERTY(config, EditAnywhere, Category="Towers", meta = (ClampMin = 1))
	float TowerFireIntervalScale = 3.0f;

	UPROPERTY(config, EditAnywhere, Category="Towers", meta = (ClampMin = 0.1))
	float TowerShotScale = 1.25f;

	/** Two towers closer than this (cm, from foot to foot) grow together into a symbiosis; small, so towers can stand apart on purpose */
	UPROPERTY(config, EditAnywhere, Category="Towers|Symbiosis", meta = (ClampMin = 50))
	float SymbiosisRadius = 300.0f;

	/** Seconds the fused tower stands longer, and seconds the vines need to grow together */
	UPROPERTY(config, EditAnywhere, Category="Towers|Symbiosis", meta = (ClampMin = 0))
	float SymbiosisLifetimeBonus = 8.0f;

	UPROPERTY(config, EditAnywhere, Category="Towers|Symbiosis", meta = (ClampMin = 0.05))
	float SymbiosisGrowTime = 1.8f;

	/** Crossing the hands (L1 + R1 + Cross): Saft it costs, more for a held cross, and shots it takes from each bar */
	UPROPERTY(config, EditAnywhere, Category="Towers|Symbiosis", meta = (ClampMin = 0))
	float CrossSaftCost = 12.0f;

	UPROPERTY(config, EditAnywhere, Category="Towers|Symbiosis", meta = (ClampMin = 0))
	float CrossChargedSaftCost = 20.0f;

	UPROPERTY(config, EditAnywhere, Category="Towers|Symbiosis", meta = (ClampMin = 0))
	int32 CrossBarShots = 3;

	/** Seconds the knot of the crossed hands stays at the hands */
	UPROPERTY(config, EditAnywhere, Category="Towers|Symbiosis", meta = (ClampMin = 0.1))
	float CrossShowTime = 0.8f;

	/** Glass of the Glimmklee dome, M_Floromant_Glaskugel (made by Scripts/CreateFloromantKit.py) */
	UPROPERTY(config, EditAnywhere, Category="Towers|Symbiosis")
	TSoftObjectPtr<UMaterialInterface> GlassMaterial;

	/** Root net: rooted, the Ruf des Gartens runs as a wave through roots under the ground from her feet; its speed in cm/s
	 *  and how much farther it reaches than the call */
	UPROPERTY(config, EditAnywhere, Category="Towers|Root Net", meta = (ClampMin = 100))
	float RootWaveSpeed = 1400.0f;

	UPROPERTY(config, EditAnywhere, Category="Towers|Root Net", meta = (ClampMin = 1))
	float RootNetReach = 1.5f;

	/** A tower the wave reaches fires at once, this much stronger */
	UPROPERTY(config, EditAnywhere, Category="Towers|Root Net", meta = (ClampMin = 1))
	float RootWaveTowerPower = 1.5f;

	// --- Keimpelz: a moss pelt the Floromant carries; it grows back slowly, harvested Keimpelz fills it at once, reloading the hands takes from it

	/** Most Keimpelz the pelt holds, and how much it has at the start */
	UPROPERTY(config, EditAnywhere, Category="Keimpelz", meta = (ClampMin = 1))
	float KeimpelzMax = 100.0f;

	/** Keimpelz the pelt grows back per second */
	UPROPERTY(config, EditAnywhere, Category="Keimpelz", meta = (ClampMin = 0))
	float KeimpelzGrowPerSecond = 1.0f;

	/** Keimpelz one shot of a plant bar costs when reloading */
	UPROPERTY(config, EditAnywhere, Category="Keimpelz", meta = (ClampMin = 0))
	float KeimpelzPerShot = 1.0f;

	/** Keimpelz a Floromant gets from picking a Keimpelz in the world (after rain) */
	UPROPERTY(config, EditAnywhere, Category="Keimpelz", meta = (ClampMin = 0))
	float KeimpelzPerHarvest = 40.0f;

	/** Compendium id of the Keimpelz in the world (the former Rußmoos keeps its old id) */
	UPROPERTY(config, EditAnywhere, Category="Keimpelz")
	FName KeimpelzHarvestId = TEXT("Russmoos");

	/** Seconds a reload takes; the hands can't fire meanwhile */
	UPROPERTY(config, EditAnywhere, Category="Keimpelz", meta = (ClampMin = 0.05))
	float ReloadTime = 0.6f;

	/** The moss pad on the forearm that shows the pelt, and its size in cm when the pelt is full */
	UPROPERTY(config, EditAnywhere, Category="Keimpelz")
	TSoftObjectPtr<UStaticMesh> KeimpelzPadMesh;

	UPROPERTY(config, EditAnywhere, Category="Keimpelz", meta = (ClampMin = 1))
	float KeimpelzPadSize = 16.0f;

	/** Health a plant of the Floromant gets back from one Tauperle of water (a jet gives it bit by bit); the water heals no one else */
	UPROPERTY(config, EditAnywhere, Category="Watering", meta = (ClampMin = 0))
	float PlantWaterHealth = 6.0f;
};
