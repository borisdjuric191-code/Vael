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
	Tool,
	/** Werkstoffe (goal 14.11.): raw materials refined at the forge, the grindstone or the field box into a better material
	 *  (Schlackeneisen, Rußglas, Latex …); what it makes goes into the material bag */
	Refine
};

/** The craft tab where he mixes materials to find new formulas of shots (goal 14.11.: formulas are learned by trying) */
inline constexpr int32 VaelCraftTabMix = 6;

/** The sketchbook of his ideas (goal 16.11.): what he has thought of, and riddles of what he still may */
inline constexpr int32 VaelCraftTabIdeas = 7;
inline constexpr int32 VaelCraftTabCount = 8;

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
	Nid,
	// --- Formulas of the Aschenmark (goal 14.11.)
	/** Stichflamme: a blast of fire in a cone ahead of the shot (Radius = its length), everything in it burns */
	Flame,
	/** Glutstoß: an explosion round the spot (Radius), knocks back; with Delay it goes off later (Glutnadel) */
	Blast,
	/** Brandsatz: a burning field on the ground (Radius, Seconds) that sets alight what stands in it */
	FireField,
	/** Schwarzfeuer: violet fire of the Nid that no resistance to fire holds back */
	BlackFire,
	/** Dampf: scalds and blinds (Radius); with Seconds of 4 or more a cloud that stays */
	Steam,
	/** Wirrwolke: enemies in it go for each other */
	Frenzy,
	/** Blendladung: a flash of light, enemies around are blinded */
	Blind,
	/** Heiltau: heals the companions round the spot over Seconds */
	HealTime,
	/** Krustenbolzen: goes through armour, the armoured flank counts as plain */
	Pierce
};

/** What a device does */
enum class EVaelDeviceType : uint8
{
	/** Snaps shut on the first enemy that steps on it: holds it fast and hurts it */
	Trap,
	/** Waits until a shot of the engineer hits it (or the remote ignition), then bursts; it carries the effect of that shot */
	Mine,
	/** Fires stones at the nearest enemy for a while; a special shot into it loads it with that ammo */
	Turret,
	/** Grabmine (Fallen-Paket 25.11.): digs itself in, goes off when an enemy steps on it; shots and the ignition set it off too */
	BuriedMine,
	/** Rauchtopf: a wall of smoke for a while, enemies inside stagger and lose the target */
	Smoke,
	/** Lockpfeife (kept, used again after its cooldown): enemies around come to the spot he whistles to */
	Whistle,
	/** Wurfhaken (kept): snags the first enemy on the way; small ones are yanked to him, heavy ones pull him to them */
	Hook
};

/** True for both kinds of mine: shots and the ignition set them off, and they go off together in a chain */
inline bool VaelIsMine(EVaelDeviceType Type)
{
	return Type == EVaelDeviceType::Mine || Type == EVaelDeviceType::BuriedMine;
}

/** Armour places of the engineer's own pieces */
enum class EVaelArmorPlace : uint8
{
	Head,
	Chest,
	Arms,
	Legs,
	/** The folding shield on the left forearm (goal 14.11.): held up with L2 / right mouse, no armour, it blocks from ahead */
	Shield
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

	/** Above 0 the device is a tool he keeps (Lockpfeife, Wurfhaken): built once, used again after this many seconds */
	float Cooldown = 0.0f;
	bool IsKeptDevice() const { return Kind == EVaelGearKind::Device && Cooldown > 0.0f; }

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

	// Formula of a shot (goal 14.11.): Hülle + Kern + up to two Verstärker; the costs are exactly these parts, one of each.
	// A formula must be found by mixing before it can be built.
	FName Hull;
	FName Core;
	TArray<FName> Boosters;

	/** Tuning of the effect: seconds it lasts (0 = its default), radius in cm round the spot (0 = only the target),
	 *  delay before it goes off, pull of Die Waage towards shadow per hit, and side effects */
	float Seconds = 0.0f;
	float Radius = 0.0f;
	float Delay = 0.0f;
	float ShadowPull = 0.0f;
	bool bAlsoBurn = false;
	bool bAlsoBleed = false;
	bool bAlsoSlow = false;
	float PartBreak = 1.0f;

	/** Werkstoff: the material it makes and how many */
	FName Output;
	int32 OutputCount = 1;

	bool IsFormula() const { return !Hull.IsNone(); }
};

/** What came of a mix at the workbench */
enum class EVaelMixResult : uint8
{
	/** A new formula: learned and built at once */
	Learned,
	/** A formula he knew already: built */
	Built,
	/** The parts fit no formula; nothing is lost, a hint says why */
	Nothing,
	/** He tried exactly this before */
	Tried,
	/** He picked fewer than two parts, or doesn't have them */
	Missing
};

/** The set bonus of a creature set, in the creature's way of fighting */
struct FVaelGearSet
{
	FName Id;
	FText Name;
	FText TwoPieces;
	FText FourPieces;
	/** With its shield worn too (only sets that have one) */
	FText WithShield;
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

	/** The name of a craft tab (the kinds, then "Mischen") */
	FText GetTabName(int32 Tab);

	/** The formula made of exactly these parts (order doesn't matter), or null */
	const FVaelGear* FindFormula(const TArray<FName>& Parts);

	/** Every material that is part of some formula, in a fixed order: hulls, cores, boosters */
	const TArray<FName>& GetMixMaterials();

	/** What a material is in the formulas: "Hülle", "Kern", "Verstärker" or empty */
	FText GetMixRole(FName Material);

	/** Why these parts make nothing (a hint for the next try) */
	FText GetMixHint(const TArray<FName>& Parts);

	/** A key for a set of parts, the same for every order */
	FString GetMixKey(const TArray<FName>& Parts);

	/** The text of what a shot does, with its seconds and reach */
	FText GetEffectText(const FVaelGear& Gear);

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
