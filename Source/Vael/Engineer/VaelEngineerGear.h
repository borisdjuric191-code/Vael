// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AVaelCharacter;
class UVaelMaterial;

/** What the engineer builds (Lore-Bibel, Ingenieur): every weapon, every shot, every device and every piece of armour he carries
 *  he made himself from what he found. Only the sling and its stones he has from the start. */
enum class EVaelGearKind : uint8
{
	Weapon,
	Ammo,
	Device,
	Armor,
	/** Tools that stay with him: the aiming goggles of Rußquarz and their lenses, later pouches and the backpack */
	Tool
};

/** What a shot does where it hits, besides its damage */
enum class EVaelAmmoEffect : uint8
{
	None,
	/** Bursts into shards that hit the enemies around */
	Shards,
	/** Sets the target on fire */
	Fire,
	/** Poisons: damage over a few seconds, the target staggers */
	Poison,
	/** Eats the armour: the target takes more from everything */
	Acid,
	/** Heals the players around where it lands, hurts nobody */
	Heal
};

/** What a device does */
enum class EVaelDeviceType : uint8
{
	/** Snaps shut on the first enemy that steps on it: holds it fast and hurts it */
	Trap,
	/** Waits until a shot of the engineer hits it (or the remote ignition), then bursts; it carries the effect of that shot */
	Mine,
	/** Fires stones at the nearest enemy for a while; a special shot into it loads it with that ammo */
	Turret
};

/** Armour places of the engineer's own pieces */
enum class EVaelArmorPlace : uint8
{
	Head,
	Chest,
	Arms,
	Legs
};

/** A material and how many of it a piece costs */
struct FVaelGearCost
{
	FName Material;
	int32 Count = 1;
};

/** One thing the engineer can build */
struct FVaelGear
{
	FName Id;
	EVaelGearKind Kind = EVaelGearKind::Weapon;
	FText Name;
	FText Text;
	FLinearColor Color = FLinearColor::White;
	TArray<FVaelGearCost> Costs;

	/** Needs this piece built first (the tree of the workbench) */
	FName Requires;

	/** Can be built at the field box by a campfire too; weapons and armour need the workbench of the refugium */
	bool bField = false;

	/** How many one build gives (shots of ammo, devices) */
	int32 Batch = 1;

	// Weapon: damage of a shot, seconds between shots, reach, speed of the shot, height of its arc, its endless standard shot
	float Damage = 0.0f;
	float Interval = 1.0f;
	float Range = 1000.0f;
	float Speed = 2000.0f;
	float Arc = 0.0f;
	bool bPierce = false;
	FText StandardShot;

	/** Where the weapon hits best (cm): inside this band x1.15, closer or farther it falls off to x0.7 at the ends */
	float SweetMin = 300.0f;
	float SweetMax = 800.0f;

	// Ammo
	EVaelAmmoEffect Effect = EVaelAmmoEffect::None;
	float DamageScale = 1.0f;

	// Device
	EVaelDeviceType Device = EVaelDeviceType::Trap;

	// Armour: place, armour, life, and the creature set it belongs to (two and four pieces of one set give its bonus)
	EVaelArmorPlace Place = EVaelArmorPlace::Head;
	float Armor = 0.0f;
	float Health = 0.0f;
	FName Set;
};

/** The set bonus of a creature set, in the creature's way of fighting */
struct FVaelGearSet
{
	FName Id;
	FText Name;
	FText TwoPieces;
	FText FourPieces;
};

namespace VaelEngineerGear
{
	/** Every piece, in the order of the workbench */
	const TArray<FVaelGear>& GetAll();
	const TArray<FVaelGearSet>& GetSets();

	const FVaelGear* Find(FName Id);
	const FVaelGearSet* FindSet(FName Id);

	/** The material asset of a cost (DA_Material_<Name> in /Game/Vael/Items/Materials) */
	const UVaelMaterial* LoadMaterial(FName Material);

	/** Short text of the costs: "2 Rollpanzer, 1 Spannfaden" */
	FText GetCostText(const FVaelGear& Gear);

	FText GetKindName(EVaelGearKind Kind);
	FText GetPlaceName(EVaelArmorPlace Place);

	/** The standard pieces the engineer starts with */
	inline const FName Sling = TEXT("Schleuder");
	inline const FName Bow = TEXT("Bogen");
	inline const FName Crossbow = TEXT("Armbrust");
	inline const FName Standard = TEXT("Standard");
	inline const FName Goggles = TEXT("Brille");
	inline const FName LensFog = TEXT("LinseGlimm");
	inline const FName LensMark = TEXT("LinseNid");
	inline const FName LensBreak = TEXT("LinseStern");
}
