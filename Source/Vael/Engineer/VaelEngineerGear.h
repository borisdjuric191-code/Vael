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
	Heal,
	/** Schwerer Stein: knocks the target back and stuns it for a moment */
	Heavy,
	/** Klebe: glue holds the target, it moves slowly for a while */
	Sticky,
	/** Rauch: a cloud where it lands, enemies in it lose their target and stagger */
	Smoke,
	/** Knister: crackling seeds burst in a small chain of fire round the target and set off his mines nearby */
	Crackle,
	/** Schlaf: weak enemies fall asleep, strong ones grow sluggish */
	Sleep,
	/** Leucht: the target glows and takes more damage from everyone */
	Glow,
	/** Dorn: thorns stay in the wound, it bleeds */
	Thorn,
	/** Tau: water, the target is wet (lightning hits harder, fire goes out) */
	Dew,
	/** Nid: very hard, but every shot pulls Die Waage of the shooter towards shadow */
	Nid
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
	// Weapon upgrade (the tree of the workbench): the weapon it improves, its tier (1.18^tier damage); a branch of tier 1 also
	// gives the endless shot an effect and changes the pace of the weapon
	FName UpgradeOf;
	int32 Tier = 0;
	EVaelAmmoEffect StandardEffect = EVaelAmmoEffect::None;
	float IntervalScale = 1.0f;

	/** Needs a blueprint first (found on creatures, from broken parts, on bosses), as the lore says: he finds plans, not gear */
	bool bNeedsPlan = false;

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
	inline const FName PouchArmor = TEXT("Panzerbeutel");
	inline const FName PouchSilk = TEXT("Seidenbeutel");
}
