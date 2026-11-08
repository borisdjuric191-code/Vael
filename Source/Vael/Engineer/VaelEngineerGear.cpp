// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engineer/VaelEngineerGear.h"

#include "Items/VaelMaterial.h"

#define LOCTEXT_NAMESPACE "VaelEngineerGear"

namespace
{
	FLinearColor GearRgb(int32 R, int32 G, int32 B)
	{
		return FLinearColor(FColor(R, G, B));
	}

	FVaelGear MakeWeapon(FName Id, FText Name, FText Text, FLinearColor Color, TArray<FVaelGearCost> Costs, FName Requires,
		float Damage, float Interval, float Range, float Speed, float Arc, bool bPierce, FText StandardShot)
	{
		FVaelGear Gear;
		Gear.Id = Id;
		Gear.Kind = EVaelGearKind::Weapon;
		Gear.Name = Name;
		Gear.Text = Text;
		Gear.Color = Color;
		Gear.Costs = MoveTemp(Costs);
		Gear.Requires = Requires;
		Gear.Damage = Damage;
		Gear.Interval = Interval;
		Gear.Range = Range;
		Gear.Speed = Speed;
		Gear.Arc = Arc;
		Gear.bPierce = bPierce;
		Gear.StandardShot = StandardShot;
		return Gear;
	}

	FVaelGear MakeUpgrade(FName Id, FText Name, FText Text, FName Of, int32 Tier, EVaelAmmoEffect Effect, float IntervalScale, FLinearColor Color, TArray<FVaelGearCost> Costs)
	{
		FVaelGear Gear;
		Gear.Id = Id;
		Gear.Kind = EVaelGearKind::Weapon;
		Gear.Name = Name;
		Gear.Text = Text;
		Gear.Color = Color;
		Gear.Costs = MoveTemp(Costs);
		Gear.Requires = Of;
		Gear.UpgradeOf = Of;
		Gear.Tier = Tier;
		Gear.StandardEffect = Effect;
		Gear.IntervalScale = IntervalScale;
		Gear.bNeedsPlan = true;
		return Gear;
	}

	FVaelGear MakeTool(FName Id, FText Name, FText Text, FLinearColor Color, TArray<FVaelGearCost> Costs, FName Requires)
	{
		FVaelGear Gear;
		Gear.Id = Id;
		Gear.Kind = EVaelGearKind::Tool;
		Gear.Name = Name;
		Gear.Text = Text;
		Gear.Color = Color;
		Gear.Costs = MoveTemp(Costs);
		Gear.Requires = Requires;
		return Gear;
	}

	FVaelGear MakeAmmo(FName Id, FText Name, FText Text, FLinearColor Color, TArray<FVaelGearCost> Costs, FName Requires, EVaelAmmoEffect Effect, float DamageScale)
	{
		FVaelGear Gear;
		Gear.Id = Id;
		Gear.Kind = EVaelGearKind::Ammo;
		Gear.Name = Name;
		Gear.Text = Text;
		Gear.Color = Color;
		Gear.Costs = MoveTemp(Costs);
		Gear.Requires = Requires;
		Gear.bField = true;
		Gear.Batch = 10;
		Gear.Effect = Effect;
		Gear.DamageScale = DamageScale;
		return Gear;
	}

	FVaelGear MakeDevice(FName Id, FText Name, FText Text, FLinearColor Color, TArray<FVaelGearCost> Costs, FName Requires, EVaelDeviceType Device, int32 Batch)
	{
		FVaelGear Gear;
		Gear.Id = Id;
		Gear.Kind = EVaelGearKind::Device;
		Gear.Name = Name;
		Gear.Text = Text;
		Gear.Color = Color;
		Gear.Costs = MoveTemp(Costs);
		Gear.Requires = Requires;
		Gear.bField = true;
		Gear.Batch = Batch;
		Gear.Device = Device;
		return Gear;
	}

	FVaelGear MakeArmor(FName Id, FText Name, FName Set, EVaelArmorPlace Place, FLinearColor Color, TArray<FVaelGearCost> Costs, FName Requires, float Armor, float Health)
	{
		FVaelGear Gear;
		Gear.Id = Id;
		Gear.Kind = EVaelGearKind::Armor;
		Gear.Name = Name;
		Gear.Color = Color;
		Gear.Costs = MoveTemp(Costs);
		Gear.Requires = Requires;
		Gear.Place = Place;
		Gear.Armor = Armor;
		Gear.Health = Health;
		Gear.Set = Set;
		return Gear;
	}

	TArray<FVaelGear> BuildGear()
	{
		TArray<FVaelGear> All;
		const FName None;

		// --- Weapons: the sling from the start, the bow of living wood is his master piece (his mark is in it), the crossbow after it
		All.Add(MakeWeapon(VaelEngineerGear::Sling, LOCTEXT("Schleuder", "Schleuder"),
			LOCTEXT("SchleuderText", "Lederschlaufe und Riemen, das erste, was er sich gebaut hat. Schnell und nah, Steine liegen überall."),
			GearRgb(150, 118, 84), {}, None, 9.0f, 0.55f, 1100.0f, 2200.0f, 70.0f, false, LOCTEXT("Steine", "Steine")));
		All.Last().SweetMin = 300.0f;
		All.Last().SweetMax = 800.0f;
		All.Add(MakeWeapon(VaelEngineerGear::Bow, LOCTEXT("Bogen", "Bogen aus lebendem Holz"),
			LOCTEXT("BogenText", "Sein Meisterstück: Narbenholz, das noch wächst, mit Spannfaden bespannt. In ihn schlägt er sein Zeichen, und ab dann trägt alles, was er baut, dieses Zeichen. Weit und hart."),
			GearRgb(120, 160, 90), { { TEXT("Narbenholz"), 4 }, { TEXT("Spannfaden"), 3 }, { TEXT("Rankenfaser"), 2 } }, VaelEngineerGear::Sling,
			14.0f, 0.85f, 1900.0f, 3600.0f, 18.0f, false, LOCTEXT("Holzpfeile", "Holzpfeile")));
		All.Last().SweetMin = 800.0f;
		All.Last().SweetMax = 1500.0f;
		All.Add(MakeWeapon(VaelEngineerGear::Crossbow, LOCTEXT("Armbrust", "Armbrust"),
			LOCTEXT("ArmbrustText", "Rollpanzer als Bogen, Sichelklingen als Abzug. Langsam zu spannen, aber der Bolzen geht durch alles, was in einer Reihe steht."),
			GearRgb(160, 160, 176), { { TEXT("Rollpanzer"), 3 }, { TEXT("Sichelklinge"), 2 }, { TEXT("Spannfaden"), 4 }, { TEXT("Schlacke"), 3 } }, VaelEngineerGear::Bow,
			30.0f, 1.3f, 1700.0f, 4400.0f, 6.0f, true, LOCTEXT("Bolzen", "Eisenbolzen")));
		All.Last().SweetMin = 500.0f;
		All.Last().SweetMax = 1200.0f;

		All.Last().bNeedsPlan = true;

		// --- The trees of the weapons: two branches of tier 1, then the master piece of tier 2 (it keeps the branch)
		const FName SlingId = VaelEngineerGear::Sling, BowId = VaelEngineerGear::Bow, CrossbowId = VaelEngineerGear::Crossbow;
		All.Add(MakeUpgrade(TEXT("Glutschleuder"), LOCTEXT("Glutschleuder", "Glutschleuder"), LOCTEXT("GlutschleuderText", "Eine Glutdr\u00FCse im Leder: jeder Stein brennt."),
			SlingId, 1, EVaelAmmoEffect::Fire, 1.0f, GearRgb(240, 120, 50), { { TEXT("Glutstein"), 3 }, { TEXT("Glutdruese"), 1 } }));
		All.Add(MakeUpgrade(TEXT("Spannschleuder"), LOCTEXT("Spannschleuder", "Spannschleuder"), LOCTEXT("SpannschleuderText", "Doppeltes Band aus Spannfaden: 20 % schneller."),
			SlingId, 1, EVaelAmmoEffect::None, 0.8f, GearRgb(200, 190, 160), { { TEXT("Spannfaden"), 3 }, { TEXT("Sichelklinge"), 1 } }));
		All.Add(MakeUpgrade(TEXT("Meisterschleuder"), LOCTEXT("Meisterschleuder", "Meisterschleuder"), LOCTEXT("MeisterschleuderText", "Mit einem Bruchst\u00FCck beschwert: zweite Stufe, der Zweig bleibt."),
			SlingId, 2, EVaelAmmoEffect::None, 1.0f, GearRgb(230, 200, 120), { { TEXT("Bruchstueck"), 1 }, { TEXT("Rollpanzer"), 2 } }));
		All.Add(MakeUpgrade(TEXT("Dornenbogen"), LOCTEXT("Dornenbogen", "Dornenbogen"), LOCTEXT("DornenbogenText", "Das Holz treibt Dornen: jeder Pfeil vergiftet."),
			BowId, 1, EVaelAmmoEffect::Poison, 1.0f, GearRgb(140, 200, 80), { { TEXT("Rankenfaser"), 3 }, { TEXT("Sichelklinge"), 2 } }));
		All.Add(MakeUpgrade(TEXT("Sehnenbogen"), LOCTEXT("Sehnenbogen", "Sehnenbogen"), LOCTEXT("SehnenbogenText", "Ru\u00DFfedern an den Wurfarmen: er zieht 15 % schneller."),
			BowId, 1, EVaelAmmoEffect::None, 0.85f, GearRgb(80, 76, 84), { { TEXT("Spannfaden"), 4 }, { TEXT("Russfeder"), 2 } }));
		All.Add(MakeUpgrade(TEXT("Meisterbogen"), LOCTEXT("Meisterbogen", "Meisterbogen"), LOCTEXT("MeisterbogenText", "Bruchst\u00FCcke in den Wurfarmen: zweite Stufe."),
			BowId, 2, EVaelAmmoEffect::None, 1.0f, GearRgb(230, 200, 120), { { TEXT("Bruchstueck"), 2 }, { TEXT("Narbenholz"), 3 } }));
		All.Add(MakeUpgrade(TEXT("SchwereArmbrust"), LOCTEXT("SchwereArmbrust", "Schwere Armbrust"), LOCTEXT("SchwereArmbrustText", "Schlacke-Bolzen: jeder zerspringt im Ziel."),
			CrossbowId, 1, EVaelAmmoEffect::Shards, 1.0f, GearRgb(160, 160, 176), { { TEXT("Rollpanzer"), 3 }, { TEXT("Schlacke"), 3 } }));
		All.Add(MakeUpgrade(TEXT("Meisterarmbrust"), LOCTEXT("Meisterarmbrust", "Meisterarmbrust"), LOCTEXT("MeisterarmbrustText", "Bruchst\u00FCcke als Abzug: zweite Stufe."),
			CrossbowId, 2, EVaelAmmoEffect::None, 1.0f, GearRgb(230, 200, 120), { { TEXT("Bruchstueck"), 2 }, { TEXT("Sichelklinge"), 2 } }));

		// --- Tools: the aiming goggles of the first ore he finds, then lenses for them
		All.Add(MakeTool(VaelEngineerGear::Goggles, LOCTEXT("Brille", "Zielhilfebrille"),
			LOCTEXT("BrilleText", "Zwei geschliffene Scheiben Ru\u00DFquarz in einem Lederband, sein erstes Werkst\u00FCck. Er sieht, wo ein Tier weich ist: Schwachstellen nehmen 1,5-fachen statt 1,3-fachen Schaden, und jeder zehnte Schuss mehr trifft kritisch."),
			GearRgb(90, 86, 96), { { TEXT("Russquarz"), 2 }, { TEXT("Spannfaden"), 1 } }, None));
		All.Add(MakeTool(VaelEngineerGear::PouchArmor, LOCTEXT("Panzerbeutel", "Panzerbeutel"),
			LOCTEXT("PanzerbeutelText", "Beutel mit Rollpanzer-Deckeln am G\u00FCrtel: 30 Ladungen jeder Art und 3 Vorrichtungen griffbereit statt 20 und 2."),
			GearRgb(150, 140, 120), { { TEXT("Rollpanzer"), 2 }, { TEXT("Spannfaden"), 2 } }, None));
		All.Add(MakeTool(VaelEngineerGear::PouchSilk, LOCTEXT("Seidenbeutel", "Seidenbeutel"),
			LOCTEXT("SeidenbeutelText", "Kokonseide, mit einem Bruchst\u00FCck verschlossen: 40 Ladungen und 4 Vorrichtungen am G\u00FCrtel."),
			GearRgb(230, 226, 210), { { TEXT("Kokonseide"), 3 }, { TEXT("Bruchstueck"), 1 } }, VaelEngineerGear::PouchArmor));
		All.Last().bNeedsPlan = true;
		All.Add(MakeTool(VaelEngineerGear::LensFog, LOCTEXT("LinseGlimm", "Glimmaugen-Linse"),
			LOCTEXT("LinseGlimmText", "Ein Glimmauge vor dem Quarz: der Nebel wird d\u00FCnn, seine Sch\u00FCsse reichen 15 % weiter."),
			GearRgb(150, 200, 220), { { TEXT("Glimmauge"), 2 }, { TEXT("Russquarz"), 1 } }, VaelEngineerGear::Goggles));
		All.Add(MakeTool(VaelEngineerGear::LensMark, LOCTEXT("LinseNid", "Nidkristall-Linse"),
			LOCTEXT("LinseNidText", "Ein Splitter Nid im Glas: was er trifft, ist gezeichnet und nimmt 10 % mehr Schaden von ihm. Das Nid fordert seinen Preis nicht \u2013 es ist gebunden."),
			GearRgb(150, 90, 200), { { TEXT("Nidkristall"), 1 }, { TEXT("Russquarz"), 1 } }, VaelEngineerGear::Goggles));
		All.Add(MakeTool(VaelEngineerGear::LensBreak, LOCTEXT("LinseStern", "Sternmilch-Linse"),
			LOCTEXT("LinseSternText", "Sternmilch zwischen zwei Scheiben: er sieht die Risse im Panzer. Teile brechen 25 % fr\u00FCher."),
			GearRgb(220, 220, 240), { { TEXT("Sternmilch"), 1 }, { TEXT("Russquarz"), 1 } }, VaelEngineerGear::Goggles));

		// --- Ammo: heads for every weapon (on the stone, the arrow or the bolt), ten from one build; at the field box too
		All.Add(MakeAmmo(TEXT("Splitter"), LOCTEXT("Splitter", "Splitterladung"),
			LOCTEXT("SplitterText", "Schlacke um eine Sichelspitze: zerspringt beim Aufschlag und trifft alles rundherum."),
			GearRgb(196, 186, 170), { { TEXT("Schlacke"), 1 }, { TEXT("Sichelklinge"), 1 } }, None, EVaelAmmoEffect::Shards, 1.0f));
		All.Add(MakeAmmo(TEXT("Brand"), LOCTEXT("Brand", "Brandladung"),
			LOCTEXT("BrandText", "Glutstein in Rußmoos gewickelt: setzt das Ziel in Brand und zündet Minen mit Feuer."),
			GearRgb(240, 120, 50), { { TEXT("Glutstein"), 1 }, { TEXT("Russmoos"), 1 } }, None, EVaelAmmoEffect::Fire, 1.0f));
		All.Add(MakeAmmo(TEXT("Gift"), LOCTEXT("Gift", "Giftladung"),
			LOCTEXT("GiftText", "Faultau und Gallpollen in einer Hohlspitze: vergiftet, das Ziel taumelt."),
			GearRgb(140, 200, 80), { { TEXT("Faultau"), 1 }, { TEXT("Gallpollen"), 1 } }, None, EVaelAmmoEffect::Poison, 0.8f));
		All.Add(MakeAmmo(TEXT("Saeure"), LOCTEXT("Saeure", "Säureladung"),
			LOCTEXT("SaeureText", "Verdauungssäure in einer Wachskapsel: frisst den Panzer, das Ziel nimmt mehr Schaden von allem."),
			GearRgb(200, 220, 70), { { TEXT("Verdauungssaeure"), 1 }, { TEXT("Kokonseide"), 1 } }, TEXT("Gift"), EVaelAmmoEffect::Acid, 0.8f));
		All.Add(MakeAmmo(TEXT("Heil"), LOCTEXT("Heil", "Heilladung"),
			LOCTEXT("HeilText", "Lichttropfen und Blatttau in dünnem Glas: platzt bei den Gefährten und heilt sie, verletzt niemanden."),
			GearRgb(250, 226, 150), { { TEXT("Lichttropfen"), 1 }, { TEXT("Blatttau"), 1 } }, None, EVaelAmmoEffect::Heal, 0.0f));

		// --- Devices: laid out before the fight, set off with a shot (lore: like the mercenary, placing and igniting)
		All.Add(MakeDevice(TEXT("Fangeisen"), LOCTEXT("Fangeisen", "Fangeisen"),
			LOCTEXT("FangeisenText", "Zwei Sichelklingen auf einer Feder: schnappt zu, wenn ein Feind darauf tritt, hält ihn fest und verletzt ihn."),
			GearRgb(170, 170, 180), { { TEXT("Sichelklinge"), 1 }, { TEXT("Spannfaden"), 1 } }, None, EVaelDeviceType::Trap, 2));
		All.Add(MakeDevice(TEXT("Knallmine"), LOCTEXT("Knallmine", "Knallmine"),
			LOCTEXT("KnallmineText", "Eine Rauchblase voll Glutstaub: wartet, bis ein Schuss sie trifft, und platzt mit der Wirkung dieses Schusses."),
			GearRgb(230, 90, 60), { { TEXT("Rauchblase"), 1 }, { TEXT("Glutstein"), 1 } }, None, EVaelDeviceType::Mine, 2));
		All.Add(MakeDevice(TEXT("Schleuderturm"), LOCTEXT("Schleuderturm", "Schleuderturm"),
			LOCTEXT("SchleuderturmText", "Ein Gestell aus Narbenholz mit Rollpanzer-Schild: schießt eine Weile Steine auf den nächsten Feind. Ein besonderer Schuss hinein lädt ihn mit dieser Ladung."),
			GearRgb(150, 120, 80), { { TEXT("Narbenholz"), 2 }, { TEXT("Rollpanzer"), 2 }, { TEXT("Spannfaden"), 2 } }, TEXT("Knallmine"), EVaelDeviceType::Turret, 1));

		// --- Armour: four places, each set of one creature; two and four pieces of one set give its bonus
		struct FSetPieces { FName Set; FText Prefix; FName Main; FName Second; FLinearColor Color; float ArmorScale; };
		const FSetPieces Sets[] =
		{
			{ TEXT("Rollpanzer"), LOCTEXT("SetRollpanzer", "Panzer"), TEXT("Rollpanzer"), TEXT("Spannfaden"), GearRgb(150, 140, 120), 1.3f },
			{ TEXT("Sichelklinge"), LOCTEXT("SetSichel", "Sichel"), TEXT("Sichelklinge"), TEXT("Kokonseide"), GearRgb(190, 190, 200), 0.9f },
			{ TEXT("Russfeder"), LOCTEXT("SetRuss", "Ruß"), TEXT("Russfeder"), TEXT("Schleierpelz"), GearRgb(80, 76, 84), 0.8f },
			{ TEXT("Blutschuppen"), LOCTEXT("SetBlut", "Blutschuppen"), TEXT("Blutschuppen"), TEXT("Pelzwasser"), GearRgb(170, 60, 60), 1.0f },
		};
		const FText PlaceNames[] = { LOCTEXT("PieceHead", "{0}haube"), LOCTEXT("PieceChest", "{0}wams"), LOCTEXT("PieceArms", "{0}armschienen"), LOCTEXT("PieceLegs", "{0}beinlinge") };
		const int32 MainCounts[] = { 2, 4, 2, 3 };
		for (const FSetPieces& Set : Sets)
		{
			for (int32 Place = 0; Place < 4; ++Place)
			{
				const FName Id(*FString::Printf(TEXT("%s_%d"), *Set.Set.ToString(), Place));
				All.Add(MakeArmor(Id, FText::Format(PlaceNames[Place], Set.Prefix), Set.Set, static_cast<EVaelArmorPlace>(Place), Set.Color,
					{ { Set.Main, MainCounts[Place] }, { Set.Second, 1 } }, None, (Place == 1 ? 8.0f : 4.0f) * Set.ArmorScale, Place == 1 ? 20.0f : 10.0f));
				All.Last().bNeedsPlan = Set.Set != FName(TEXT("Rollpanzer"));
			}
		}
		return All;
	}

	TArray<FVaelGearSet> BuildSets()
	{
		return {
			{ TEXT("Rollpanzer"), LOCTEXT("SetNameRollpanzer", "Panzer-Satz (Rollpanzer)"),
				LOCTEXT("Roll2", "2 Teile: 10 % weniger Schaden"), LOCTEXT("Roll4", "4 Teile: nach dem Ausweichen eine Sekunde lang wie ein eingerollter Käfer, 60 % weniger Schaden") },
			{ TEXT("Sichelklinge"), LOCTEXT("SetNameSichel", "Sichel-Satz (Sichelklinge)"),
				LOCTEXT("Sichel2", "2 Teile: 15 % mehr Schaden"), LOCTEXT("Sichel4", "4 Teile: jeder Schuss geht durch das erste Ziel hindurch") },
			{ TEXT("Russfeder"), LOCTEXT("SetNameRuss", "Ruß-Satz (Rußfeder)"),
				LOCTEXT("Russ2", "2 Teile: 10 % schneller zu Fuß"), LOCTEXT("Russ4", "4 Teile: 20 % schneller nachladen") },
			{ TEXT("Blutschuppen"), LOCTEXT("SetNameBlut", "Blutschuppen-Satz"),
				LOCTEXT("Blut2", "2 Teile: 2 % des Schadens kommen als Leben zurück"), LOCTEXT("Blut4", "4 Teile: 6 % des Schadens kommen als Leben zurück") },
		};
	}
}

const TArray<FVaelGear>& VaelEngineerGear::GetAll()
{
	static const TArray<FVaelGear> All = BuildGear();
	return All;
}

const TArray<FVaelGearSet>& VaelEngineerGear::GetSets()
{
	static const TArray<FVaelGearSet> Sets = BuildSets();
	return Sets;
}

const FVaelGear* VaelEngineerGear::Find(FName Id)
{
	return GetAll().FindByPredicate([Id](const FVaelGear& Gear) { return Gear.Id == Id; });
}

const FVaelGearSet* VaelEngineerGear::FindSet(FName Id)
{
	return GetSets().FindByPredicate([Id](const FVaelGearSet& Set) { return Set.Id == Id; });
}

const UVaelMaterial* VaelEngineerGear::LoadMaterial(FName Material)
{
	static TMap<FName, TWeakObjectPtr<const UVaelMaterial>> Cache;
	if (const TWeakObjectPtr<const UVaelMaterial>* Found = Cache.Find(Material))
	{
		if (Found->IsValid())
		{
			return Found->Get();
		}
	}
	const FString Name = Material.ToString();
	const UVaelMaterial* Loaded = LoadObject<UVaelMaterial>(nullptr, *FString::Printf(TEXT("/Game/Vael/Items/Materials/DA_Material_%s.DA_Material_%s"), *Name, *Name));
	Cache.Add(Material, Loaded);
	return Loaded;
}

FText VaelEngineerGear::GetCostText(const FVaelGear& Gear)
{
	TArray<FString> Parts;
	for (const FVaelGearCost& Cost : Gear.Costs)
	{
		const UVaelMaterial* Material = LoadMaterial(Cost.Material);
		Parts.Add(FString::Printf(TEXT("%d %s"), Cost.Count, Material != nullptr ? *Material->DisplayName.ToString() : *Cost.Material.ToString()));
	}
	return Parts.IsEmpty() ? LOCTEXT("Free", "von Anfang an") : FText::FromString(FString::Join(Parts, TEXT(", ")));
}

FText VaelEngineerGear::GetKindName(EVaelGearKind Kind)
{
	switch (Kind)
	{
	case EVaelGearKind::Weapon: return LOCTEXT("KindWeapon", "Waffen");
	case EVaelGearKind::Ammo: return LOCTEXT("KindAmmo", "Munition");
	case EVaelGearKind::Device: return LOCTEXT("KindDevice", "Vorrichtungen");
	case EVaelGearKind::Tool: return LOCTEXT("KindTool", "Werkzeug");
	default: return LOCTEXT("KindArmor", "Rüstung");
	}
}

FText VaelEngineerGear::GetPlaceName(EVaelArmorPlace Place)
{
	switch (Place)
	{
	case EVaelArmorPlace::Head: return LOCTEXT("PlaceHead", "Kopf");
	case EVaelArmorPlace::Chest: return LOCTEXT("PlaceChest", "Brust");
	case EVaelArmorPlace::Arms: return LOCTEXT("PlaceArms", "Arme");
	default: return LOCTEXT("PlaceLegs", "Beine");
	}
}

#undef LOCTEXT_NAMESPACE
