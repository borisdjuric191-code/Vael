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

	/** A formula of a shot (goal 14.11.): Hülle + Kern + Verstärker, one of each part per batch of ten */
	FVaelGear MakeFormula(FName Id, FText Name, FText Text, FLinearColor Color, FName Hull, FName Core, TArray<FName> Boosters, EVaelAmmoEffect Effect, float DamageScale)
	{
		TArray<FVaelGearCost> Costs = { { Hull, 1 } };
		const auto AddPart = [&Costs](FName Part)
		{
			if (FVaelGearCost* Same = Costs.FindByPredicate([Part](const FVaelGearCost& Cost) { return Cost.Material == Part; }))
			{
				++Same->Count;
			}
			else
			{
				Costs.Add({ Part, 1 });
			}
		};
		AddPart(Core);
		for (const FName Booster : Boosters)
		{
			AddPart(Booster);
		}
		FVaelGear Gear = MakeAmmo(Id, Name, Text, Color, MoveTemp(Costs), FName(), Effect, DamageScale);
		Gear.Hull = Hull;
		Gear.Core = Core;
		Gear.Boosters = MoveTemp(Boosters);
		return Gear;
	}

	/** A Werkstoff: refined at the forge, the grindstone or the field box into another material */
	FVaelGear MakeRefine(FName Output, int32 OutputCount, FText Name, FText Text, FLinearColor Color, TArray<FVaelGearCost> Costs)
	{
		FVaelGear Gear;
		Gear.Id = FName(*(TEXT("Veredeln_") + Output.ToString()));
		Gear.Kind = EVaelGearKind::Refine;
		Gear.Name = Name;
		Gear.Text = Text;
		Gear.Color = Color;
		Gear.Costs = MoveTemp(Costs);
		Gear.bField = true;
		Gear.Output = Output;
		Gear.OutputCount = OutputCount;
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
			LOCTEXT("SchleuderText", "Eine Zwille: eine Astgabel aus Narbenholz, ein Band aus Spannfaden, eine Lederschale f\u00FCr den Stein. Das erste, was er sich gebaut hat. Je weiter er das Band spannt, desto weiter fliegt der Stein, bis 24 Meter. Steine liegen \u00FCberall."),
			GearRgb(150, 118, 84), {}, None, 9.0f, 0.55f, 2400.0f, 2600.0f, 70.0f, false, LOCTEXT("Steine", "Steine")));
		All.Last().SweetMin = 600.0f;
		All.Last().SweetMax = 1800.0f;
		All.Add(MakeWeapon(VaelEngineerGear::Bow, LOCTEXT("Bogen", "Bogen aus lebendem Holz"),
			LOCTEXT("BogenText", "Sein Meisterstück: Narbenholz, das noch wächst, mit Spannfaden bespannt. In ihn schlägt er sein Zeichen, und ab dann trägt alles, was er baut, dieses Zeichen. Weit und hart."),
			GearRgb(120, 160, 90), { { TEXT("Narbenholz"), 4 }, { TEXT("Spannfaden"), 3 }, { TEXT("Rankenfaser"), 2 } }, VaelEngineerGear::Sling,
			14.0f, 0.85f, 4500.0f, 4800.0f, 40.0f, false, LOCTEXT("Holzpfeile", "Holzpfeile")));
		All.Last().SweetMin = 1500.0f;
		All.Last().SweetMax = 3500.0f;
		All.Add(MakeWeapon(VaelEngineerGear::Crossbow, LOCTEXT("Armbrust", "Armbrust"),
			LOCTEXT("ArmbrustText", "Rollpanzer als Bogen, Sichelklingen als Abzug. Langsam zu spannen, aber der Bolzen geht durch alles, was in einer Reihe steht."),
			GearRgb(160, 160, 176), { { TEXT("Rollpanzer"), 3 }, { TEXT("Sichelklinge"), 2 }, { TEXT("Rollsehne"), 2 }, { TEXT("Schlackeneisen"), 3 } }, VaelEngineerGear::Bow,
			30.0f, 1.3f, 1700.0f, 4400.0f, 6.0f, true, LOCTEXT("Bolzen", "Eisenbolzen")));
		All.Last().SweetMin = 500.0f;
		All.Last().SweetMax = 1200.0f;

		All.Last().bNeedsPlan = true;

		// --- The trees of the weapons: two branches of tier 1, then the master piece of tier 2 (it keeps the branch)
		const FName SlingId = VaelEngineerGear::Sling, BowId = VaelEngineerGear::Bow, CrossbowId = VaelEngineerGear::Crossbow;
		All.Add(MakeUpgrade(TEXT("Glutschleuder"), LOCTEXT("Glutschleuder", "Glutschleuder"), LOCTEXT("GlutschleuderText", "Eine Glutdr\u00FCse im Leder: jeder Stein brennt."),
			SlingId, 1, EVaelAmmoEffect::Fire, 1.0f, GearRgb(240, 120, 50), { { TEXT("Glutstein"), 3 }, { TEXT("Glutdruese"), 1 }, { TEXT("Puppengarn"), 1 } }));
		All.Add(MakeUpgrade(TEXT("Spannschleuder"), LOCTEXT("Spannschleuder", "Spannschleuder"), LOCTEXT("SpannschleuderText", "Ein Band aus Latex mit Rollsehne als Feder: 20 % schneller."),
			SlingId, 1, EVaelAmmoEffect::None, 0.8f, GearRgb(200, 190, 160), { { TEXT("Latex"), 2 }, { TEXT("Rollsehne"), 1 }, { TEXT("Schlackeneisen"), 1 } }));
		All.Add(MakeUpgrade(TEXT("Meisterschleuder"), LOCTEXT("Meisterschleuder", "Meisterschleuder"), LOCTEXT("MeisterschleuderText", "Mit dem Glutkiefer eines Glutkriechers beschwert: zweite Stufe, der Zweig bleibt."),
			SlingId, 2, EVaelAmmoEffect::None, 1.0f, GearRgb(230, 200, 120), { { TEXT("Glutkiefer"), 1 }, { TEXT("Rollpanzer"), 2 } }));
		All.Add(MakeUpgrade(TEXT("Dornenbogen"), LOCTEXT("Dornenbogen", "Dornenbogen"), LOCTEXT("DornenbogenText", "Das Holz treibt Dornen: jeder Pfeil vergiftet."),
			BowId, 1, EVaelAmmoEffect::Poison, 1.0f, GearRgb(140, 200, 80), { { TEXT("Rankenfaser"), 3 }, { TEXT("Sichelklinge"), 2 } }));
		All.Add(MakeUpgrade(TEXT("Sehnenbogen"), LOCTEXT("Sehnenbogen", "Sehnenbogen"), LOCTEXT("SehnenbogenText", "Ru\u00DFfedern an den Wurfarmen: er zieht 15 % schneller."),
			BowId, 1, EVaelAmmoEffect::None, 0.85f, GearRgb(80, 76, 84), { { TEXT("Spannfaden"), 4 }, { TEXT("Russfeder"), 2 } }));
		All.Add(MakeUpgrade(TEXT("Meisterbogen"), LOCTEXT("Meisterbogen", "Meisterbogen"), LOCTEXT("MeisterbogenText", "Harpyienschn\u00E4bel in den Wurfarmen: zweite Stufe."),
			BowId, 2, EVaelAmmoEffect::None, 1.0f, GearRgb(230, 200, 120), { { TEXT("Harpyienschnabel"), 2 }, { TEXT("Narbenholz"), 3 } }));
		All.Add(MakeUpgrade(TEXT("SchwereArmbrust"), LOCTEXT("SchwereArmbrust", "Schwere Armbrust"), LOCTEXT("SchwereArmbrustText", "Schlacke-Bolzen: jeder zerspringt im Ziel."),
			CrossbowId, 1, EVaelAmmoEffect::Shards, 1.0f, GearRgb(160, 160, 176), { { TEXT("Rollpanzer"), 3 }, { TEXT("Schlacke"), 3 } }));
		All.Add(MakeUpgrade(TEXT("Meisterarmbrust"), LOCTEXT("Meisterarmbrust", "Meisterarmbrust"), LOCTEXT("MeisterarmbrustText", "Der Legestachel der Glutk\u00F6nigin als Abzug, ihre Glutkrone als Spannkopf: zweite Stufe."),
			CrossbowId, 2, EVaelAmmoEffect::None, 1.0f, GearRgb(230, 200, 120), { { TEXT("Legestachel"), 1 }, { TEXT("Glutkrone"), 1 }, { TEXT("Messingblech"), 2 } }));

		// --- Tools: the aiming goggles of the first ore he finds, then lenses for them
		All.Add(MakeTool(VaelEngineerGear::Goggles, LOCTEXT("Brille", "Zielhilfebrille"),
			LOCTEXT("BrilleText", "Zwei Scheiben Ru\u00DFglas in Schlackeneisen gefasst, ein Band aus Silberhalm, mit Latex abgedichtet: sein erstes Werkst\u00FCck. Er sieht, wo ein Tier weich ist: Schwachstellen nehmen 1,5-fachen statt 1,3-fachen Schaden, und jeder zehnte Schuss mehr trifft kritisch."),
			GearRgb(90, 86, 96), { { TEXT("Russglas"), 2 }, { TEXT("Schlackeneisen"), 1 }, { TEXT("Silberhalm"), 2 }, { TEXT("Latex"), 1 } }, None));
		All.Add(MakeTool(VaelEngineerGear::PouchArmor, LOCTEXT("Panzerbeutel", "Panzerbeutel"),
			LOCTEXT("PanzerbeutelText", "Beutel mit Rollpanzer-Deckeln am G\u00FCrtel: 30 Ladungen jeder Art und 3 Vorrichtungen griffbereit statt 20 und 2."),
			GearRgb(150, 140, 120), { { TEXT("Rollpanzer"), 2 }, { TEXT("Spannfaden"), 2 } }, None));
		All.Add(MakeTool(VaelEngineerGear::PouchSilk, LOCTEXT("Seidenbeutel", "Seidenbeutel"),
			LOCTEXT("SeidenbeutelText", "Kokonseide, mit einem Bruchst\u00FCck verschlossen: 40 Ladungen und 4 Vorrichtungen am G\u00FCrtel."),
			GearRgb(230, 226, 210), { { TEXT("Kokonseide"), 3 }, { TEXT("Bruchstueck"), 1 } }, VaelEngineerGear::PouchArmor));
		All.Last().bNeedsPlan = true;
		All.Add(MakeTool(VaelEngineerGear::LensFog, LOCTEXT("LinseGlimm", "Glimmaugen-Linse"),
			LOCTEXT("LinseGlimmText", "Ein Glimmauge vor dem Quarz: der Nebel wird d\u00FCnn, seine Sch\u00FCsse reichen 15 % weiter."),
			GearRgb(150, 200, 220), { { TEXT("Glimmauge"), 2 }, { TEXT("Russglas"), 1 }, { TEXT("Messingblech"), 1 } }, VaelEngineerGear::Goggles));
		All.Add(MakeTool(VaelEngineerGear::LensMark, LOCTEXT("LinseNid", "Nidkristall-Linse"),
			LOCTEXT("LinseNidText", "Ein Splitter Nid im Glas: was er trifft, ist gezeichnet und nimmt 10 % mehr Schaden von ihm. Das Nid fordert seinen Preis nicht \u2013 es ist gebunden."),
			GearRgb(150, 90, 200), { { TEXT("Nidkristall"), 1 }, { TEXT("Russglas"), 1 }, { TEXT("Messingblech"), 1 } }, VaelEngineerGear::Goggles));
		All.Add(MakeTool(VaelEngineerGear::LensBreak, LOCTEXT("LinseStern", "Sternmilch-Linse"),
			LOCTEXT("LinseSternText", "Sternmilch zwischen zwei Scheiben: er sieht die Risse im Panzer. Teile brechen 25 % fr\u00FCher."),
			GearRgb(220, 220, 240), { { TEXT("Sternmilch"), 1 }, { TEXT("Russglas"), 1 }, { TEXT("Messingblech"), 1 } }, VaelEngineerGear::Goggles));
		All.Add(MakeTool(VaelEngineerGear::Spyglass, LOCTEXT("Fernglas", "Fernglas"),
			LOCTEXT("FernglasText", "Zwei Scheiben Rußglas in einem Rohr aus Narbenholz, mit Messing gefasst: sein Fernrohr reicht doppelt so weit, und was er darin beobachtet, ist gezeichnet und nimmt mehr Schaden."),
			GearRgb(180, 150, 90), { { TEXT("Russglas"), 2 }, { TEXT("Narbenholz"), 1 }, { TEXT("Messingblech"), 1 } }, None));

		// --- Shots as formulas (goal 14.11., Docs/Ingenieur_Aschenmark_Materialien.md section 6): Hülle + Kern + up to two
		// Verstärker. Hüllen: Schlacke (hard, hits one target), Blasenhaut (bursts into a cloud), Latex (splashes like a water
		// balloon), Schlackenglas (shatters, for fire and acid), Hohlkiel (a needle that stays in the target). Ids of the shots
		// that were in the game before are kept, so saved pouches stay valid. Every formula is learned by mixing.
		const FName Schlacke = TEXT("Schlacke"), Blase = TEXT("Blasenhaut"), Latex = TEXT("Latex"), Glas = TEXT("Schlackenglas"), Kiel = TEXT("Hohlkiel");
		const auto Tune = [&All](float Seconds, float Radius, float Delay = 0.0f, float ShadowPull = 0.0f)
		{
			All.Last().Seconds = Seconds;
			All.Last().Radius = Radius;
			All.Last().Delay = Delay;
			All.Last().ShadowPull = ShadowPull;
		};

		// Feuer (Glutstein)
		All.Add(MakeFormula(TEXT("Brand"), LOCTEXT("Glutladung", "Glutladung"),
			LOCTEXT("GlutladungText", "Ein Glutstein in Schlacke gefasst: setzt das Ziel in Brand und zündet Minen mit Feuer."),
			GearRgb(240, 120, 50), Schlacke, TEXT("Glutstein"), {}, EVaelAmmoEffect::Fire, 1.0f));
		Tune(6.0f, 0.0f);
		All.Add(MakeFormula(TEXT("Knister"), LOCTEXT("Stichflamme", "Stichflamme"),
			LOCTEXT("StichflammeText", "Knistersamen am Glutstein: beim Aufschlag schießt ein greller Feuerstoß im Kegel nach vorn."),
			GearRgb(255, 150, 50), Schlacke, TEXT("Glutstein"), { TEXT("Knistersamen") }, EVaelAmmoEffect::Flame, 0.9f));
		Tune(3.0f, 400.0f);
		All.Add(MakeFormula(TEXT("GrosseStichflamme"), LOCTEXT("GrosseStichflamme", "Große Stichflamme"),
			LOCTEXT("GrosseStichflammeText", "Dazu Distelflaum, der sofort Feuer fängt: der Stoß reicht weiter und brennt heißer."),
			GearRgb(255, 175, 70), Schlacke, TEXT("Glutstein"), { TEXT("Knistersamen"), TEXT("Distelflaum") }, EVaelAmmoEffect::Flame, 1.1f));
		Tune(4.0f, 600.0f);
		All.Add(MakeFormula(TEXT("Feuerstoss"), LOCTEXT("Feuerstoss", "Feuerstoß"),
			LOCTEXT("FeuerstossText", "Statt Flaum eine Glutdrüse: die Stichflamme explodiert und wirft zurück, was davor steht."),
			GearRgb(255, 110, 40), Schlacke, TEXT("Glutstein"), { TEXT("Knistersamen"), TEXT("Glutdruese") }, EVaelAmmoEffect::Blast, 1.2f));
		Tune(3.0f, 380.0f);
		All.Last().bAlsoBurn = true;
		All.Add(MakeFormula(TEXT("Brandsatz"), LOCTEXT("Brandsatz", "Brandsatz"),
			LOCTEXT("BrandsatzText", "Glutstein und Glutsamen in Schlackenglas: es zerspringt, und der Boden brennt."),
			GearRgb(230, 90, 30), Glas, TEXT("Glutstein"), { TEXT("Glutsamen") }, EVaelAmmoEffect::FireField, 0.8f));
		Tune(5.0f, 300.0f);
		All.Add(MakeFormula(TEXT("Schwarzfeuer"), LOCTEXT("Schwarzfeuer", "Schwarzfeuer"),
			LOCTEXT("SchwarzfeuerText", "Schwarzfunken am Glutstein: violettes Feuer, das kein Feuerschutz aufhält. Es zieht an der Waage."),
			GearRgb(120, 50, 160), Glas, TEXT("Glutstein"), { TEXT("Schwarzfunken") }, EVaelAmmoEffect::BlackFire, 1.1f));
		Tune(6.0f, 0.0f, 0.0f, 1.0f);
		All.Add(MakeFormula(TEXT("Glutnadel"), LOCTEXT("Glutnadel", "Glutnadel"),
			LOCTEXT("GlutnadelText", "Eine Glutdrüse im Hohlkiel: steckt im Ziel und explodiert zwei Sekunden später."),
			GearRgb(255, 130, 60), Kiel, TEXT("Glutdruese"), {}, EVaelAmmoEffect::Blast, 0.6f));
		Tune(0.0f, 300.0f, 2.0f);

		// Wasser (Blatttau)
		All.Add(MakeFormula(TEXT("Tau"), LOCTEXT("Wasserkapsel", "Wasserkapsel"),
			LOCTEXT("WasserkapselText", "Blatttau in einer Latexkapsel: platzt wie ein Wasserballon. Nass: Feuer geht aus, Blitze treffen doppelt."),
			GearRgb(110, 170, 220), Latex, TEXT("Blatttau"), {}, EVaelAmmoEffect::Dew, 0.5f));
		Tune(8.0f, 200.0f);
		All.Add(MakeFormula(TEXT("GrosseWasserkapsel"), LOCTEXT("GrosseWasserkapsel", "Große Wasserkapsel"),
			LOCTEXT("GrosseWasserkapselText", "Dazu Pelzwasser: der Schwall macht alles in vier Metern nass."),
			GearRgb(90, 160, 230), Latex, TEXT("Blatttau"), { TEXT("Pelzwasser") }, EVaelAmmoEffect::Dew, 0.5f));
		Tune(8.0f, 400.0f);
		All.Add(MakeFormula(TEXT("Dampfkapsel"), LOCTEXT("Dampfkapsel", "Dampfkapsel"),
			LOCTEXT("DampfkapselText", "Blatttau auf Glutstein in Schlackenglas: heißer Dampf verbrüht und blendet. Dampf ist kein Feuer, er trifft auch Feuerfeste."),
			GearRgb(220, 220, 225), Glas, TEXT("Blatttau"), { TEXT("Glutstein") }, EVaelAmmoEffect::Steam, 0.8f));
		Tune(3.0f, 250.0f);
		All.Add(MakeFormula(TEXT("Dampfwolke"), LOCTEXT("Dampfwolke", "Dampfwolke"),
			LOCTEXT("DampfwolkeText", "Dazu Pelzwasser: eine Wolke aus Dampf, die fünf Sekunden stehen bleibt."),
			GearRgb(235, 235, 240), Glas, TEXT("Blatttau"), { TEXT("Glutstein"), TEXT("Pelzwasser") }, EVaelAmmoEffect::Steam, 0.6f));
		Tune(5.0f, 400.0f);

		// Rauch (Rauchstaub, Material Rauchblase)
		All.Add(MakeFormula(TEXT("Rauch"), LOCTEXT("Rauch", "Rauchladung"),
			LOCTEXT("RauchText", "Rauchstaub in einer Blasenhaut: eine Wolke, in der Gegner das Ziel verlieren und taumeln."),
			GearRgb(110, 106, 104), Blase, TEXT("Rauchblase"), {}, EVaelAmmoEffect::Smoke, 0.3f));
		Tune(4.0f, 340.0f);
		All.Add(MakeFormula(TEXT("Schlaf"), LOCTEXT("Schlafwolke", "Schlafwolke"),
			LOCTEXT("SchlafwolkeText", "Schwarzmohn im Rauch: schwache Gegner in der Wolke schlafen ein, starke werden träge."),
			GearRgb(120, 70, 140), Blase, TEXT("Rauchblase"), { TEXT("Schwarzmohnblatt") }, EVaelAmmoEffect::Sleep, 0.3f));
		Tune(5.0f, 340.0f);
		All.Add(MakeFormula(TEXT("Wirrwolke"), LOCTEXT("Wirrwolke", "Wirrwolke"),
			LOCTEXT("WirrwolkeText", "Raunsamen im Rauch: wer in der Wolke steht, hört Stimmen und geht drei Sekunden auf die eigenen Leute los."),
			GearRgb(140, 120, 170), Blase, TEXT("Rauchblase"), { TEXT("Raunsamen") }, EVaelAmmoEffect::Frenzy, 0.3f));
		Tune(3.0f, 340.0f);
		All.Add(MakeFormula(TEXT("Glutrauch"), LOCTEXT("Glutrauch", "Glutrauch"),
			LOCTEXT("GlutrauchText", "Glutsamen und Distelflaum im Rauch: die Wolke glüht von innen, wer darin steht, brennt und verliert das Ziel."),
			GearRgb(160, 90, 60), Blase, TEXT("Rauchblase"), { TEXT("Glutsamen"), TEXT("Distelflaum") }, EVaelAmmoEffect::Smoke, 0.5f));
		Tune(4.0f, 340.0f);
		All.Last().bAlsoBurn = true;

		// Gift (Faultau, Schatten)
		All.Add(MakeFormula(TEXT("Gift"), LOCTEXT("Gift", "Giftladung"),
			LOCTEXT("GiftText", "Faultau in einer Latexkapsel: vergiftet, das Ziel taumelt. Schatten: zieht leicht an der Waage."),
			GearRgb(140, 200, 80), Latex, TEXT("Faultau"), {}, EVaelAmmoEffect::Poison, 0.8f));
		Tune(6.0f, 0.0f, 0.0f, 0.5f);
		All.Add(MakeFormula(TEXT("Zehrgift"), LOCTEXT("Zehrgift", "Zehrgift"),
			LOCTEXT("ZehrgiftText", "Dazu Gallpollen: das Gift wirkt zehn Sekunden lang."),
			GearRgb(110, 170, 70), Latex, TEXT("Faultau"), { TEXT("Gallpollen") }, EVaelAmmoEffect::Poison, 0.8f));
		Tune(10.0f, 0.0f, 0.0f, 0.5f);
		All.Add(MakeFormula(TEXT("Dorn"), LOCTEXT("Dorngift", "Dorngift"),
			LOCTEXT("DorngiftText", "Dazu Dornpollen: die Dornen bleiben in der Wunde, das Ziel ist vergiftet und blutet."),
			GearRgb(150, 40, 40), Latex, TEXT("Faultau"), { TEXT("Dornpollen") }, EVaelAmmoEffect::Poison, 0.9f));
		Tune(6.0f, 0.0f, 0.0f, 0.5f);
		All.Last().bAlsoBleed = true;
		All.Add(MakeFormula(TEXT("Giftnadel"), LOCTEXT("Giftnadel", "Giftnadel"),
			LOCTEXT("GiftnadelText", "Faultau und Gallpollen im Hohlkiel: die Nadel steckt und gibt acht Sekunden lang Gift ab, stärker als die Kapsel."),
			GearRgb(90, 150, 60), Kiel, TEXT("Faultau"), { TEXT("Gallpollen") }, EVaelAmmoEffect::Poison, 0.7f));
		Tune(8.0f, 0.0f, 0.0f, 0.5f);

		// Säure (Verdauungssäure, Schatten): frisst Latex, braucht Glas oder Kiel
		All.Add(MakeFormula(TEXT("Saeure"), LOCTEXT("Saeure", "Säureladung"),
			LOCTEXT("SaeureText", "Verdauungssäure in Schlackenglas: frisst den Panzer, das Ziel nimmt mehr Schaden von allem."),
			GearRgb(200, 220, 70), Glas, TEXT("Verdauungssaeure"), {}, EVaelAmmoEffect::Acid, 0.8f));
		Tune(8.0f, 0.0f, 0.0f, 0.5f);
		All.Add(MakeFormula(TEXT("Klebsaeure"), LOCTEXT("Klebsaeure", "Klebsäure"),
			LOCTEXT("KlebsaeureText", "Dazu Kelchschleim: die Säure haftet, frisst doppelt so lange und macht das Ziel langsam."),
			GearRgb(180, 200, 60), Glas, TEXT("Verdauungssaeure"), { TEXT("Kelchschleim") }, EVaelAmmoEffect::Acid, 0.7f));
		Tune(16.0f, 0.0f, 0.0f, 0.5f);
		All.Last().bAlsoSlow = true;
		All.Add(MakeFormula(TEXT("Saeurenadel"), LOCTEXT("Saeurenadel", "Säurenadel"),
			LOCTEXT("SaeurenadelText", "Verdauungssäure im Hohlkiel: frisst genau eine Stelle, Teile brechen viel schneller."),
			GearRgb(170, 210, 50), Kiel, TEXT("Verdauungssaeure"), {}, EVaelAmmoEffect::Acid, 0.8f));
		Tune(8.0f, 0.0f, 0.0f, 0.5f);
		All.Last().PartBreak = 1.6f;

		// Heilung (Kelchtau) und Licht (Glockenglut)
		All.Add(MakeFormula(TEXT("Heilkapsel"), LOCTEXT("Heilkapsel", "Heilkapsel"),
			LOCTEXT("HeilkapselText", "Kelchtau in einer Latexkapsel: platzt bei den Gefährten und heilt sie, verletzt niemanden."),
			GearRgb(200, 230, 255), Latex, TEXT("Kelchtau"), {}, EVaelAmmoEffect::Heal, 0.0f));
		Tune(0.0f, 320.0f);
		All.Add(MakeFormula(TEXT("Heil"), LOCTEXT("StarkeHeilkapsel", "Starke Heilkapsel"),
			LOCTEXT("StarkeHeilkapselText", "Dazu ein Lichttropfen: heilt deutlich mehr."),
			GearRgb(250, 226, 150), Latex, TEXT("Kelchtau"), { TEXT("Lichttropfen") }, EVaelAmmoEffect::Heal, 0.0f));
		Tune(0.0f, 320.0f);
		All.Add(MakeFormula(TEXT("Heiltau"), LOCTEXT("Heiltau", "Heiltau"),
			LOCTEXT("HeiltauText", "Dazu Rosenherz-Nektar: der Tau bleibt liegen und heilt die Gefährten acht Sekunden lang."),
			GearRgb(160, 240, 170), Latex, TEXT("Kelchtau"), { TEXT("RosenherzNektar") }, EVaelAmmoEffect::HealTime, 0.0f));
		Tune(8.0f, 320.0f);
		All.Add(MakeFormula(TEXT("Leucht"), LOCTEXT("Leucht", "Leuchtladung"),
			LOCTEXT("LeuchtText", "Glockenglut in einer Blasenhaut: das Ziel leuchtet, auch nachts, und nimmt mehr Schaden von allen."),
			GearRgb(255, 220, 150), Blase, TEXT("Glockenglut"), {}, EVaelAmmoEffect::Glow, 0.5f));
		Tune(10.0f, 0.0f);
		All.Add(MakeFormula(TEXT("Blendladung"), LOCTEXT("Blendladung", "Blendladung"),
			LOCTEXT("BlendladungText", "Dazu Glimmstroh: ein greller Lichtblitz, alle Gegner in drei Metern sind zwei Sekunden geblendet."),
			GearRgb(255, 245, 210), Blase, TEXT("Glockenglut"), { TEXT("Glimmstroh") }, EVaelAmmoEffect::Blind, 0.3f));
		Tune(2.0f, 300.0f);
		All.Add(MakeFormula(TEXT("Sonnenladung"), LOCTEXT("Sonnenladung", "Sonnenladung"),
			LOCTEXT("SonnenladungText", "Dazu ein Lichttropfen: blendet drei Sekunden und zeichnet alle im Blitz, sie nehmen mehr Schaden."),
			GearRgb(255, 240, 170), Blase, TEXT("Glockenglut"), { TEXT("Lichttropfen") }, EVaelAmmoEffect::Blind, 0.5f));
		Tune(3.0f, 300.0f);

		// Wucht und Splitter
		All.Add(MakeFormula(TEXT("Splitter"), LOCTEXT("Splitter", "Splitterladung"),
			LOCTEXT("SplitterText", "Eine Sichelspitze in Schlacke: zerspringt beim Aufschlag und trifft alles rundherum."),
			GearRgb(196, 186, 170), Schlacke, TEXT("Sichelklinge"), {}, EVaelAmmoEffect::Shards, 1.0f));
		All.Add(MakeFormula(TEXT("Schwer"), LOCTEXT("Schwer", "Schwerer Stein"),
			LOCTEXT("SchwerText", "Schlacke um ein Stück Rollpanzer: wirft das Ziel zurück und betäubt es kurz."),
			GearRgb(120, 116, 112), Schlacke, TEXT("Rollpanzer"), {}, EVaelAmmoEffect::Heavy, 1.3f));
		All.Add(MakeFormula(TEXT("Krustenbolzen"), LOCTEXT("Krustenbolzen", "Krustenbolzen"),
			LOCTEXT("KrustenbolzenText", "Eine Krustenplatte mit Schlackeneisen-Spitze: geht durch Panzer, die gepanzerte Flanke zählt wie der Rücken."),
			GearRgb(70, 60, 58), Schlacke, TEXT("Krustenplatte"), { TEXT("Schlackeneisen") }, EVaelAmmoEffect::Pierce, 1.2f));
		All.Add(MakeFormula(TEXT("Klebe"), LOCTEXT("Klebe", "Klebekapsel"),
			LOCTEXT("KlebeText", "Kelchschleim in einer Latexkapsel: das Ziel klebt fest und bewegt sich sechs Sekunden halb so schnell."),
			GearRgb(210, 196, 120), Latex, TEXT("Kelchschleim"), {}, EVaelAmmoEffect::Sticky, 0.6f));
		Tune(6.0f, 0.0f);

		// Nid: the strongest shot, needs a blueprint
		All.Add(MakeFormula(TEXT("Nid"), LOCTEXT("Nid", "Nidladung"),
			LOCTEXT("NidText", "Ein Nidkristall mit Schwarzfunken in Schlackenglas: trifft doppelt so hart, aber jeder Schuss zieht seine Waage zum Schatten."),
			GearRgb(110, 40, 120), Glas, TEXT("Nidkristall"), { TEXT("Schwarzfunken") }, EVaelAmmoEffect::Nid, 2.0f));
		Tune(0.0f, 0.0f, 0.0f, 1.5f);
		All.Last().bNeedsPlan = true;

		// --- Werkstoffe (goal 14.11.): refined at the workshop or by a campfire (field box), never on the way
		All.Add(MakeRefine(TEXT("Schlackeneisen"), 1, LOCTEXT("Schlackeneisen", "Schlackeneisen"),
			LOCTEXT("SchlackeneisenText", "Drei Brocken Schlacke mit Glutstein ausgeschmolzen: das Grundmetall für Fassungen, Nieten und Spitzen."),
			GearRgb(110, 104, 110), { { TEXT("Schlacke"), 3 }, { TEXT("Glutstein"), 1 } }));
		All.Add(MakeRefine(TEXT("Schlackenglas"), 2, LOCTEXT("Schlackenglas", "Schlackenglas"),
			LOCTEXT("SchlackenglasText", "Schlacke geschmolzen und zu dünnwandigen Kugeln geblasen: zerspringt beim Aufschlag. Hülle für Feuer und Säure."),
			GearRgb(70, 76, 70), { { TEXT("Schlacke"), 2 }, { TEXT("Glutstein"), 1 } }));
		All.Add(MakeRefine(TEXT("Russglas"), 1, LOCTEXT("Russglas", "Rußglas"),
			LOCTEXT("RussglasText", "Rußquarz, mit Aschenstäubling am Schleifstein klar geschliffen: die Linse der Brille."),
			GearRgb(150, 148, 160), { { TEXT("Russquarz"), 2 }, { TEXT("Staeublingspulver"), 1 } }));
		All.Add(MakeRefine(TEXT("Latex"), 2, LOCTEXT("Latex", "Latex"),
			LOCTEXT("LatexText", "Sternmilch, eingedickt und gezogen: dehnbare Haut für Kapseln, Dichtungen und Bänder."),
			GearRgb(240, 236, 220), { { TEXT("Sternmilch"), 2 } }));
		All.Add(MakeRefine(TEXT("SchwarzerLatex"), 2, LOCTEXT("SchwarzerLatex", "Schwarzer Latex"),
			LOCTEXT("SchwarzerLatexText", "Aus Gallmilch: zäher als heller Latex. Schatten."),
			GearRgb(50, 40, 56), { { TEXT("Gallmilch"), 2 } }));
		All.Add(MakeRefine(TEXT("Blasenhaut"), 1, LOCTEXT("Blasenhaut", "Blasenhaut"),
			LOCTEXT("BlasenhautText", "Eine Aschblase, vorsichtig angestochen und leer gedrückt: eine dünne Haut, die beim Aufschlag weich platzt. Hülle für Rauch und Licht."),
			GearRgb(170, 176, 180), { { TEXT("Rauchblase"), 1 } }));
		All.Add(MakeRefine(TEXT("GehaertetePlatte"), 1, LOCTEXT("GehaertetePlatte", "Gehärtete Platte"),
			LOCTEXT("GehaertetePlatteText", "Zwei Krustenplatten in der Glut gehärtet: der Kern schwerer Rüstung."),
			GearRgb(40, 34, 32), { { TEXT("Krustenplatte"), 2 }, { TEXT("Glutstein"), 1 } }));
		All.Add(MakeRefine(TEXT("Messingblech"), 1, LOCTEXT("Messingblech", "Messingblech"),
			LOCTEXT("MessingblechText", "Der gestohlene Tand der Harpyien, eingeschmolzen und gehämmert: Messing für Linsenringe und edle Fassungen."),
			GearRgb(214, 164, 82), { { TEXT("Harpyientand"), 2 }, { TEXT("Schlacke"), 1 } }));

		// --- Devices: laid out before the fight, set off with a shot (lore: like the mercenary, placing and igniting)
		All.Add(MakeDevice(TEXT("Fangeisen"), LOCTEXT("Fangeisen", "Fangeisen"),
			LOCTEXT("FangeisenText", "Eine Sichelklinge auf der Rollsehne eines Aschenrollers als Feder: schnappt zu, wenn ein Feind darauf tritt, hält ihn fest und verletzt ihn."),
			GearRgb(170, 170, 180), { { TEXT("Sichelklinge"), 1 }, { TEXT("Rollsehne"), 1 }, { TEXT("Schlackeneisen"), 1 } }, None, EVaelDeviceType::Trap, 2));
		All.Add(MakeDevice(TEXT("Knallmine"), LOCTEXT("Knallmine", "Knallmine"),
			LOCTEXT("KnallmineText", "Eine Rauchblase voll Glutstaub: wartet, bis ein Schuss sie trifft, und platzt mit der Wirkung dieses Schusses."),
			GearRgb(230, 90, 60), { { TEXT("Rauchblase"), 1 }, { TEXT("Glutstein"), 1 } }, None, EVaelDeviceType::Mine, 2));
		All.Add(MakeDevice(TEXT("Schleuderturm"), LOCTEXT("Schleuderturm", "Schleuderturm"),
			LOCTEXT("SchleuderturmText", "Ein Gestell aus Narbenholz mit Rollpanzer-Schild: schießt eine Weile Steine auf den nächsten Feind. Ein besonderer Schuss hinein lädt ihn mit dieser Ladung."),
			GearRgb(150, 120, 80), { { TEXT("Narbenholz"), 2 }, { TEXT("Rollpanzer"), 2 }, { TEXT("Spannfaden"), 2 } }, TEXT("Knallmine"), EVaelDeviceType::Turret, 1));

		// --- Fallen-Paket (25.11.): lure them in with the whistle, yank the strays with the hook, let them step on the buried
		// mine, and hide in the smoke. Whistle and hook are kept and used again after a while.
		All.Add(MakeDevice(TEXT("Grabmine"), LOCTEXT("Grabmine", "Grabmine"),
			LOCTEXT("GrabmineText", "Eine Glutdrüse unter einer Krustenplatte, mit Distelflaum gepolstert: gräbt sich ein und ist nicht mehr zu sehen. Wer darauf tritt, fliegt in die Luft. Ein Schuss oder die Zündung lässt sie auch hochgehen, und mit anderen Minen geht sie in einer Kette."),
			GearRgb(200, 80, 40), { { TEXT("Glutdruese"), 1 }, { TEXT("Krustenplatte"), 1 }, { TEXT("Distelflaum"), 1 } }, None, EVaelDeviceType::BuriedMine, 2));
		All.Add(MakeDevice(TEXT("Rauchtopf"), LOCTEXT("Rauchtopf", "Rauchtopf"),
			LOCTEXT("RauchtopfText", "Zwei Rauchblasen in Siegelwachs, die beim Aufschlag platzen: acht Sekunden eine Wand aus Rauch. Wer darin steht, taumelt und verliert sein Ziel."),
			GearRgb(120, 116, 110), { { TEXT("Rauchblase"), 2 }, { TEXT("Siegelwachs"), 1 } }, TEXT("Knallmine"), EVaelDeviceType::Smoke, 2));
		All.Add(MakeDevice(TEXT("Lockpfeife"), LOCTEXT("Lockpfeife", "Lockpfeife"),
			LOCTEXT("LockpfeifeText", "Ein Kreischbalg im Hohlkiel, Pfeifensporen als Zunge: ein Pfiff, und die Feinde im Umkreis laufen sechs Sekunden zu der Stelle, auf die er zeigt – am besten auf eine Grabmine. Große Tiere folgen nur kurz, Bosse gar nicht. Bleibt bei ihm, wieder bereit nach 12 Sekunden."),
			GearRgb(220, 190, 120), { { TEXT("Kreischbalg"), 1 }, { TEXT("Hohlkiel"), 1 }, { TEXT("Pfeifensporen"), 1 } }, None, EVaelDeviceType::Whistle, 1));
		All.Last().Cooldown = 12.0f;
		All.Add(MakeDevice(TEXT("Wurfhaken"), LOCTEXT("Wurfhaken", "Wurfhaken"),
			LOCTEXT("WurfhakenText", "Greifklaue und Klammerfinger an einem Seil aus Rankenfaser: hakt sich in den ersten Feind auf dem Weg. Kleine reißt er zu sich her, an schwere zieht er sich selbst heran. Bleibt bei ihm, wieder bereit nach 6 Sekunden."),
			GearRgb(160, 130, 90), { { TEXT("Greifklaue"), 1 }, { TEXT("Klammerfinger"), 1 }, { TEXT("Rankenfaser"), 3 } }, None, EVaelDeviceType::Hook, 1));
		All.Last().Cooldown = 6.0f;

		// --- Umwelt (25.11.): devices that use what lies round them. Mines and smoke read their surroundings too (fire, wet
		// ground, wind), see AVaelEngineerDevice.
		All.Add(MakeDevice(TEXT("Glutzapfer"), LOCTEXT("Glutzapfer", "Glutzapfer"),
			LOCTEXT("GlutzapferText", "Ein Schlot aus Schlackeneisen, der Glutkiefer als Düse, mit Kriechersehne verspannt: nur an ein Feuer gesetzt – einen Brandherd, ein Glutfeld, Glutdisteln. Er zapft die Glut an und spuckt 15 Sekunden lang Flammen auf den nächsten Feind. Ein Brandherd brennt dabei herunter."),
			GearRgb(240, 120, 40), { { TEXT("Glutkiefer"), 1 }, { TEXT("Schlackeneisen"), 2 }, { TEXT("Kriechersehne"), 1 } }, None, EVaelDeviceType::Tapper, 1));
		All.Add(MakeDevice(TEXT("Saeurefass"), LOCTEXT("Saeurefass", "Säurefass"),
			LOCTEXT("SaeurefassText", "Verdauungssäure des Fleischkelchs, mit Kelchschleim gebunden und in Siegelwachs gegossen: platzt, wenn ein Feind darauf tritt oder ein Schuss es trifft. Die Pfütze frisst acht Sekunden lang den Panzer – wer darin steht, nimmt mehr Schaden. Im Regen läuft sie breiter."),
			GearRgb(140, 200, 60), { { TEXT("Verdauungssaeure"), 2 }, { TEXT("Kelchschleim"), 1 }, { TEXT("Siegelwachs"), 1 } }, None, EVaelDeviceType::Acid, 2));
		All.Add(MakeDevice(TEXT("Loeschpulver"), LOCTEXT("Loeschpulver", "Löschpulver"),
			LOCTEXT("LoeschpulverText", "Stäublingspulver in einem Wachsbeutel: platzt, wo er landet. Entfachte Brände gehen aus, was brennt, kühlt ab, und Feinde bleiben eine Weile nass – auch ein Glutkriecher zündet dann schwer."),
			GearRgb(200, 196, 186), { { TEXT("Staeublingspulver"), 2 }, { TEXT("Siegelwachs"), 1 } }, None, EVaelDeviceType::Douse, 2));

		// --- Five more devices (25.11.)
		All.Add(MakeDevice(TEXT("Sichelkreisel"), LOCTEXT("Sichelkreisel", "Sichelkreisel"),
			LOCTEXT("SichelkreiselText", "Zwei Sichelklingen auf einem Kreisel aus Puppengarn, mit glimmendem Stroh gepolstert: wirbelt vier Sekunden wie die Sichelpuppe. Wer zu nahe kommt, wird geschnitten, weggeschleudert und fängt Feuer."),
			GearRgb(200, 170, 110), { { TEXT("Sichelklinge"), 2 }, { TEXT("Puppengarn"), 1 }, { TEXT("Glimmstroh"), 1 } }, None, EVaelDeviceType::Whirl, 2));
		All.Add(MakeDevice(TEXT("Wuergnetz"), LOCTEXT("Wuergnetz", "Würgnetz"),
			LOCTEXT("WuergnetzText", "Würgranke und Rankenfaser, mit Dornpollen bestäubt: schnellt hoch, wenn ein Feind darauf tritt, und hält alle in der Nähe zweieinhalb Sekunden fest. Wer sich losreißt, blutet. Eine Schattenpflanze: Sie zieht dich ein wenig zum Schatten."),
			GearRgb(90, 110, 70), { { TEXT("Wuergranke"), 2 }, { TEXT("Rankenfaser"), 2 }, { TEXT("Dornpollen"), 1 } }, None, EVaelDeviceType::Net, 2));
		All.Add(MakeDevice(TEXT("Blendlaterne"), LOCTEXT("Blendlaterne", "Blendlaterne"),
			LOCTEXT("BlendlaterneText", "Glockenglut hinter Rußglas, ein Glimmauge als Linse, in Schlackeneisen gefasst: leuchtet 30 Sekunden hell und blitzt alle drei Sekunden. Wer im Licht steht, ist geblendet und taumelt. Nachts reicht das Licht weiter."),
			GearRgb(250, 210, 120), { { TEXT("Glockenglut"), 1 }, { TEXT("Russglas"), 1 }, { TEXT("Schlackeneisen"), 1 }, { TEXT("Glimmauge"), 1 } }, None, EVaelDeviceType::Lantern, 1));
		All.Add(MakeDevice(TEXT("Sturmwarner"), LOCTEXT("Sturmwarner", "Sturmwarner"),
			LOCTEXT("SturmwarnerText", "Pfeifensporen in einem Messingrohr, eine Windfahne aus Silberhalm: Die Pfeifenmorchel pfeift, bevor es stürmt, und sie pfeift auch, wenn Feinde kommen. Wer sich bis 12 m nähert, ist gezeichnet. Bläst Wind, fängt die Fahne ihn und stößt nahe Feinde weg. Steht eine Minute."),
			GearRgb(200, 180, 120), { { TEXT("Pfeifensporen"), 1 }, { TEXT("Messingblech"), 1 }, { TEXT("Silberhalm"), 1 } }, None, EVaelDeviceType::Warner, 1));
		All.Add(MakeDevice(TEXT("Rollwall"), LOCTEXT("Rollwall", "Rollwall"),
			LOCTEXT("RollwallText", "Rückenpanzer eines Aschenrollers, mit seiner Rollsehne zur Kugel gespannt: rollt von dir bis zu 10 m zur Stelle, rempelt Feinde beiseite und schnellt dort zu einer Wand auf, die zehn Sekunden steht."),
			GearRgb(150, 140, 120), { { TEXT("Rollpanzer"), 2 }, { TEXT("Rollsehne"), 1 }, { TEXT("Schlackeneisen"), 1 } }, None, EVaelDeviceType::Rollwall, 1));

		// --- Rammschild (Aschenmark abgeschlossen 25.11.) and the craft of the Kobold-Sprengmeister (Eisenschlund)
		All.Add(MakeDevice(TEXT("Rammschild"), LOCTEXT("Rammschild", "Rammschild"),
			LOCTEXT("RammschildText", "Drei Reihen Rollerpanzer, schräg gestellt und mit Schlackeneisen gespickt, von einer Rollsehne gehalten: steht 25 Sekunden quer im Weg. Wer dagegen stürmt oder rollt, prallt verletzt und benommen ab, ein Aschenroller liegt danach auf dem Rücken. Wer sich anlehnt, sticht sich."),
			GearRgb(140, 130, 115), { { TEXT("Rollpanzer"), 3 }, { TEXT("Rollsehne"), 1 }, { TEXT("Schlackeneisen"), 2 } }, None, EVaelDeviceType::Ram, 1));
		All.Add(MakeDevice(TEXT("Stolperdraht"), LOCTEXT("Stolperdraht", "Stolperdraht"),
			LOCTEXT("StolperdrahtText", "Abgeschaut beim Kobold-Sprengmeister: ein Draht aus Drahtsilber zwischen zwei Pflöcken, vier Meter quer zu deinem Blick, mit Funkenerz-Ladungen behängt. Wer ihn kreuzt, löst sie dort aus, wo er hängen bleibt."),
			GearRgb(230, 110, 60), { { TEXT("Funkenerz"), 1 }, { TEXT("Drahtsilber"), 1 }, { TEXT("Schlackeneisen"), 1 } }, None, EVaelDeviceType::Tripwire, 2));
		All.Add(MakeDevice(TEXT("Funkenladung"), LOCTEXT("Funkenladung", "Funkenladung"),
			LOCTEXT("FunkenladungText", "Funkenerz, mit Kobaltblüte verstopft wie die Ladungen der Kobolde: geworfen leuchtet der Boden auf, wo sie liegt, nach eineinhalb Sekunden platzt sie und setzt alles in Brand."),
			GearRgb(255, 90, 15), { { TEXT("Funkenerz"), 2 }, { TEXT("Kobaltbluete"), 1 } }, None, EVaelDeviceType::OreCharge, 3));
		All.Last().Effect = EVaelAmmoEffect::Fire;
		All.Add(MakeDevice(TEXT("Salzladung"), LOCTEXT("Salzladung", "Salzladung"),
			LOCTEXT("SalzladungText", "Salzspat unter einem Kobaltstopfen: platzt zu einer Wolke aus nassem Salz. Wer darin steht, ist durchnässt, und Blitze treffen ihn härter."),
			GearRgb(255, 140, 165), { { TEXT("Salzspat"), 2 }, { TEXT("Kobaltbluete"), 1 } }, None, EVaelDeviceType::OreCharge, 3));
		All.Last().Effect = EVaelAmmoEffect::Dew;
		All.Add(MakeDevice(TEXT("Gruenspanladung"), LOCTEXT("Gruenspanladung", "Grünspanladung"),
			LOCTEXT("GruenspanladungText", "Grünspan unter einem Kobaltstopfen: der knisternde Staub legt sich auf alles in der Nähe, sie werden langsam."),
			GearRgb(25, 230, 130), { { TEXT("Gruenspan"), 2 }, { TEXT("Kobaltbluete"), 1 } }, None, EVaelDeviceType::OreCharge, 3));
		All.Last().Effect = EVaelAmmoEffect::Sticky;

		// --- Nid-Verlockungen aus dem Orden der Narbe (25.11.): the Order seals the Nid to hoard it, he learns their craft
		All.Add(MakeAmmo(TEXT("Siegelkapsel"), LOCTEXT("Siegelkapsel", "Siegelkapsel"),
			LOCTEXT("SiegelkapselText", "Ein Splitter Nid in Siegelwachs, mit dem Ordenssiegel verschlossen: das Siegel hält, das Nid gibt nur seine Hitze ab. Der Schuss brennt hart und zieht kaum an der Waage."),
			GearRgb(200, 140, 90), { { TEXT("Ordenssiegel"), 1 }, { TEXT("Siegelwachs"), 1 }, { TEXT("Nidkristall"), 1 } }, None, EVaelAmmoEffect::Fire, 1.6f));
		All.Last().Batch = 5;
		All.Last().ShadowPull = 0.05f;
		All.Add(MakeAmmo(TEXT("GebrochenesSiegel"), LOCTEXT("GebrochenesSiegel", "Gebrochenes Siegel"),
			LOCTEXT("GebrochenesSiegelText", "Dieselbe Kapsel, das Siegel absichtlich angebrochen: das Nid bricht frei als violettes Feuer, das kein Feuerschutz aufhält. Doppelter Schaden, aber jeder Schuss zieht dich zum Schatten."),
			GearRgb(130, 50, 170), { { TEXT("Ordenssiegel"), 1 }, { TEXT("Nidkristall"), 2 } }, TEXT("Siegelkapsel"), EVaelAmmoEffect::BlackFire, 2.0f));
		All.Last().Batch = 5;
		All.Last().ShadowPull = 0.4f;
		All.Add(MakeDevice(TEXT("Bannkreis"), LOCTEXT("Bannkreis", "Bannkreis"),
			LOCTEXT("BannkreisText", "Eine Kette aus Ordenssiegeln, auf Ordenstuch genäht, als Ring auf den Boden gelegt: zehn Sekunden lang hält er jedes Nidwesen darin fest (gezeichnete Kreaturen, Sichelpuppen, Prediger). Was der Orden einsperrt, sperrst jetzt du ein."),
			GearRgb(230, 200, 140), { { TEXT("Ordenssiegel"), 1 }, { TEXT("Ordenstuch"), 1 }, { TEXT("Schlackeneisen"), 1 } }, None, EVaelDeviceType::Ward, 1));
		All.Add(MakeDevice(TEXT("Saugkreis"), LOCTEXT("Saugkreis", "Saugkreis"),
			LOCTEXT("SaugkreisText", "Derselbe Ring, die Siegel nach innen gedreht, ein Nidkristall in der Mitte: er saugt den Nidwesen darin das Nid aus, das schmerzt sie, und füllt dich mit Nidladungen für den Überdruck. Jede Ladung zieht dich zum Schatten."),
			GearRgb(140, 60, 180), { { TEXT("Ordenssiegel"), 1 }, { TEXT("Nidkristall"), 1 }, { TEXT("Schlackeneisen"), 1 } }, TEXT("Bannkreis"), EVaelDeviceType::Drain, 1));
		All.Add(MakeDevice(TEXT("Ueberdruck"), LOCTEXT("Ueberdruck", "Überdruck"),
			LOCTEXT("UeberdruckText", "Ein Ventil aus Messing mit einem Nidkristall: verbraucht eine Nidladung, und die nächste Sprengvorrichtung, die du legst (Mine, Grabmine, Stolperdraht, Kristallladung), knallt doppelt so hart und weiter. Jede fünfte birst schon beim Legen."),
			GearRgb(170, 80, 200), { { TEXT("Nidkristall"), 1 }, { TEXT("Messingblech"), 2 }, { TEXT("Rollsehne"), 1 } }, TEXT("Saugkreis"), EVaelDeviceType::Overcharge, 1));
		All.Last().Cooldown = 3.0f;

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
		// Boris 17.11.: only what has a model stays in the game, that is the Schwarzglut set; these four creature sets wait,
		// switched off here, until they get models of their own
		constexpr bool bCreatureSets = false;
		for (const FSetPieces& Set : Sets)
		{
			if constexpr (!bCreatureSets)
			{
				break;
			}
			for (int32 Place = 0; Place < 4; ++Place)
			{
				const FName Id(*FString::Printf(TEXT("%s_%d"), *Set.Set.ToString(), Place));
				All.Add(MakeArmor(Id, FText::Format(PlaceNames[Place], Set.Prefix), Set.Set, static_cast<EVaelArmorPlace>(Place), Set.Color,
					{ { Set.Main, MainCounts[Place] }, { Set.Second, 1 } }, None, (Place == 1 ? 8.0f : 4.0f) * Set.ArmorScale, Place == 1 ? 20.0f : 10.0f));
				All.Last().bNeedsPlan = Set.Set != FName(TEXT("Rollpanzer"));
			}
		}

		// --- Schwarzglut-Rüstung (goal 14.11., Docs/Ingenieur_Aschenmark_Materialien.md 6b): a mixed set as a formula of roles,
		// Platte = Gehärtete Platte of the Glutkriecher, Gelenk = Rollpanzer, Fassung = Schlackeneisen, Kern = Glutstein in the
		// chest, Polster = Ordenstuch; and as a fifth piece the folding shield of Aschenroller bands
		{
			const FName Set(TEXT("Schwarzglut"));
			const FLinearColor Black = GearRgb(52, 38, 34);
			const auto Piece = [&All, Set, Black](int32 Place, FText Name, FText Text, TArray<FVaelGearCost> Costs, float Armor, float Health)
			{
				All.Add(MakeArmor(FName(*FString::Printf(TEXT("Schwarzglut_%d"), Place)), Name, Set, static_cast<EVaelArmorPlace>(Place), Black, MoveTemp(Costs), NAME_None, Armor, Health));
				All.Last().Text = Text;
			};
			Piece(0, LOCTEXT("SchwarzglutHelm", "Schwarzglut-Helm"),
				LOCTEXT("SchwarzglutHelmText", "Zwei gehärtete Krustenplatten über dem Schädel, wie beim Glutkriecher überlappend, in einem Rand aus Schlackeneisen, darunter Ordenstuch."),
				{ { TEXT("GehaertetePlatte"), 2 }, { TEXT("Schlackeneisen"), 1 }, { TEXT("Ordenstuch"), 1 } }, 6.0f, 12.0f);
			Piece(1, LOCTEXT("SchwarzglutBrust", "Schwarzglut-Brustpanzer"),
				LOCTEXT("SchwarzglutBrustText", "Drei Krustenplatten vorn und hinten, ein Rollpanzer-Gürtel um die Mitte. In der Brust sitzt ein Glutstein: er lässt die Risse der Platten glimmen."),
				{ { TEXT("GehaertetePlatte"), 3 }, { TEXT("Rollpanzer"), 1 }, { TEXT("Glutstein"), 1 }, { TEXT("Schlackeneisen"), 2 }, { TEXT("Ordenstuch"), 1 } }, 12.0f, 25.0f);
			Piece(2, LOCTEXT("SchwarzglutArme", "Schwarzglut-Armschienen"),
				LOCTEXT("SchwarzglutArmeText", "Rollpanzer-Bänder um den Unterarm, die sich mit ihm biegen, außen eine Krustenplatte."),
				{ { TEXT("GehaertetePlatte"), 1 }, { TEXT("Rollpanzer"), 2 }, { TEXT("Schlackeneisen"), 1 } }, 6.0f, 12.0f);
			Piece(3, LOCTEXT("SchwarzglutBeine", "Schwarzglut-Beinschienen"),
				LOCTEXT("SchwarzglutBeineText", "Zwei Krustenplatten über dem Schienbein, ein Rollpanzer-Band über dem Knie, gebunden mit Kriechersehne."),
				{ { TEXT("GehaertetePlatte"), 2 }, { TEXT("Rollpanzer"), 1 }, { TEXT("Schlackeneisen"), 1 }, { TEXT("Kriechersehne"), 1 } }, 6.0f, 12.0f);
			Piece(4, LOCTEXT("Klappschild", "Klappschild"),
				LOCTEXT("KlappschildText", "Fast der ganze Rückenpanzer eines Aschenrollers, in fünf Bandreihen geteilt und mit Schlackeneisen gefasst. Zusammengeschoben liegt er auf dem linken Unterarm; L2 oder die rechte Maustaste gehalten, und die Rollsehne schiebt ihn zum Turmschild auf. Er hält, was von vorn kommt, im rechten Augenblick alles."),
				{ { TEXT("Rollpanzer"), 3 }, { TEXT("Rollsehne"), 1 }, { TEXT("Schlackeneisen"), 1 }, { TEXT("GehaertetePlatte"), 1 } }, 0.0f, 0.0f);
		}

		// --- Aschenjäger (Boris 25.11., Tripo model 26.11., Docs/Ingenieur_Aschenmark_Materialien.md 6d): the light set of
		// Sichelpuppe, Aschharpyie and Schleieraffe. Straw and doll's twine bind the body, soot feathers lie on collar and
		// forearms, the monkey's veil makes the hood and its glow eyes the glasses. Little armour, he is fast instead
		{
			const FName Set(TEXT("Aschenjaeger"));
			const FLinearColor Ash = GearRgb(70, 62, 54);
			const auto Piece = [&All, Set, Ash](int32 Place, FText Name, FText Text, TArray<FVaelGearCost> Costs, float Armor, float Health)
			{
				All.Add(MakeArmor(FName(*FString::Printf(TEXT("Aschenjaeger_%d"), Place)), Name, Set, static_cast<EVaelArmorPlace>(Place), Ash, MoveTemp(Costs), NAME_None, Armor, Health));
				All.Last().Text = Text;
			};
			Piece(0, LOCTEXT("AschenjaegerKopf", "Aschenjäger-Kapuze"),
				LOCTEXT("AschenjaegerKopfText", "Eine tiefe Kapuze aus dem Schleierpelz des Affen, die Maske aus Puppengarn gewickelt. Zwei Glimmaugen sind als Gläser eingesetzt: durch sie sieht er im Rauch."),
				{ { TEXT("Schleierpelz"), 2 }, { TEXT("Glimmauge"), 2 }, { TEXT("Puppengarn"), 1 }, { TEXT("Russfeder"), 1 } }, 3.0f, 8.0f);
			Piece(1, LOCTEXT("AschenjaegerBrust", "Aschenjäger-Wams"),
				LOCTEXT("AschenjaegerBrustText", "Der Leib kreuzweise in Glimmstroh und Puppengarn gebunden wie eine Sichelpuppe, darüber ein Kragen aus Rußfedern mit rostroten Spitzen."),
				{ { TEXT("Puppengarn"), 3 }, { TEXT("Glimmstroh"), 2 }, { TEXT("Russfeder"), 2 }, { TEXT("Schleierpelz"), 1 } }, 6.0f, 15.0f);
			Piece(2, LOCTEXT("AschenjaegerArme", "Aschenjäger-Armwicklungen"),
				LOCTEXT("AschenjaegerArmeText", "Bandagen aus Puppengarn, außen Reihen von Rußfedern wie kleine Schwingen. In den Handwicklungen stecken Klammerfinger: was er wirft, fängt er sicher."),
				{ { TEXT("Puppengarn"), 2 }, { TEXT("Russfeder"), 2 }, { TEXT("Klammerfinger"), 1 } }, 3.0f, 8.0f);
			Piece(3, LOCTEXT("AschenjaegerBeine", "Aschenjäger-Beinwicklungen"),
				LOCTEXT("AschenjaegerBeineText", "Gewickelte Beinlinge mit Strohringen an Knie und Schienbein, ein Strohrock am Gürtel, an den Stiefeln Greifklauen der Harpyie."),
				{ { TEXT("Puppengarn"), 2 }, { TEXT("Glimmstroh"), 2 }, { TEXT("Greifklaue"), 2 } }, 3.0f, 8.0f);
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
			{ TEXT("Schwarzglut"), LOCTEXT("SetNameSchwarzglut", "Schwarzglut-Rüstung (Glutkriecher und Aschenroller)"),
				LOCTEXT("Schwarzglut2", "2 Teile: 10 % weniger Schaden, 15 % weniger Feuerschaden. Schwer: jedes Rüstungsteil macht 3 % langsamer"),
				LOCTEXT("Schwarzglut4", "4 Teile: Ausweichen rollt ihn zur Panzerkugel ein; gehalten rollt sie bis zu 3 Sekunden weiter, lenkbar und bergab schneller. Eingerollt 60 % weniger Schaden, wer ihn trifft, fängt Feuer; Gegner im Weg fliegen brennend beiseite. Gegen eine Wand: kurz benommen"),
				LOCTEXT("SchwarzglutSchild", "Mit Klappschild: ein Block im rechten Augenblick wirft den Angreifer zurück und setzt ihn in Brand") },
			{ TEXT("Aschenjaeger"), LOCTEXT("SetNameAschenjaeger", "Aschenjäger (Sichelpuppe, Aschharpyie und Schleieraffe)"),
				LOCTEXT("Aschenjaeger2", "2 Teile: 10 % schneller zu Fuß"),
				LOCTEXT("Aschenjaeger4", "4 Teile: die Doppelsichel ersetzt die Schleuder und kehrt immer zurück. Antippen wirft gerade, Halten im Bogen um den Zielpunkt; sie schneidet auf Hin- und Rückweg. Kurz vor der Rückkehr gedrückt fängt er sie, der nächste Wurf ist 30 % schneller und härter. Rauchschritt folgt") },
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
	case EVaelGearKind::Refine: return LOCTEXT("KindRefine", "Veredeln");
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
	case EVaelArmorPlace::Shield: return LOCTEXT("PlaceShield", "Schild");
	default: return LOCTEXT("PlaceLegs", "Beine");
	}
}

FText VaelEngineerGear::GetTabName(int32 Tab)
{
	return Tab == VaelCraftTabMix ? LOCTEXT("TabMix", "Mischen") : Tab == VaelCraftTabIdeas ? LOCTEXT("TabIdeas", "Ideen") : GetKindName(static_cast<EVaelGearKind>(Tab));
}

FString VaelEngineerGear::GetMixKey(const TArray<FName>& Parts)
{
	TArray<FString> Names;
	for (const FName Part : Parts)
	{
		Names.Add(Part.ToString());
	}
	Names.Sort();
	return FString::Join(Names, TEXT("+"));
}

const FVaelGear* VaelEngineerGear::FindFormula(const TArray<FName>& Parts)
{
	const FString Key = GetMixKey(Parts);
	for (const FVaelGear& Gear : GetAll())
	{
		if (!Gear.IsFormula())
		{
			continue;
		}
		TArray<FName> Own = { Gear.Hull, Gear.Core };
		Own.Append(Gear.Boosters);
		if (GetMixKey(Own) == Key)
		{
			return &Gear;
		}
	}
	return nullptr;
}

const TArray<FName>& VaelEngineerGear::GetMixMaterials()
{
	static const TArray<FName> Materials = []()
	{
		TArray<FName> Hulls, Cores, Boosters;
		for (const FVaelGear& Gear : GetAll())
		{
			if (Gear.IsFormula())
			{
				Hulls.AddUnique(Gear.Hull);
				Cores.AddUnique(Gear.Core);
				for (const FName Booster : Gear.Boosters)
				{
					Boosters.AddUnique(Booster);
				}
			}
		}
		TArray<FName> All = Hulls;
		for (const FName Core : Cores)
		{
			All.AddUnique(Core);
		}
		for (const FName Booster : Boosters)
		{
			All.AddUnique(Booster);
		}
		return All;
	}();
	return Materials;
}

FText VaelEngineerGear::GetMixRole(FName Material)
{
	bool bHull = false, bCore = false, bBooster = false;
	for (const FVaelGear& Gear : GetAll())
	{
		if (Gear.IsFormula())
		{
			bHull |= Gear.Hull == Material;
			bCore |= Gear.Core == Material;
			bBooster |= Gear.Boosters.Contains(Material);
		}
	}
	return bHull ? LOCTEXT("RoleHull", "Hülle") : bCore && bBooster ? LOCTEXT("RoleBoth", "Kern, Verstärker") : bCore ? LOCTEXT("RoleCore", "Kern")
		: bBooster ? LOCTEXT("RoleBooster", "Verstärker") : FText::GetEmpty();
}

FText VaelEngineerGear::GetMixHint(const TArray<FName>& Parts)
{
	// What the parts are: hulls, cores of formulas
	TArray<FName> Hulls, Cores;
	for (const FName Part : Parts)
	{
		for (const FVaelGear& Gear : GetAll())
		{
			if (Gear.IsFormula() && Gear.Hull == Part)
			{
				Hulls.AddUnique(Part);
			}
			if (Gear.IsFormula() && Gear.Core == Part)
			{
				Cores.AddUnique(Part);
			}
		}
	}
	if (Hulls.IsEmpty())
	{
		return LOCTEXT("HintNoHull", "Es fehlt eine Hülle: Schlacke, Blasenhaut, Latex, Schlackenglas oder ein Hohlkiel.");
	}
	if (Hulls.Num() > 1)
	{
		return LOCTEXT("HintTwoHulls", "Zwei Hüllen passen nicht ineinander.");
	}
	const FName Hull = Hulls[0];
	const bool bFire = Parts.Contains(FName(TEXT("Glutstein"))) || Parts.Contains(FName(TEXT("Glutdruese"))) || Parts.Contains(FName(TEXT("Schwarzfunken")));
	const bool bAcid = Parts.Contains(FName(TEXT("Verdauungssaeure")));
	const bool bWet = Parts.Contains(FName(TEXT("Blatttau"))) || Parts.Contains(FName(TEXT("Faultau"))) || Parts.Contains(FName(TEXT("Kelchtau")))
		|| Parts.Contains(FName(TEXT("Kelchschleim")));
	if (Hull == FName(TEXT("Latex")) && bFire)
	{
		return LOCTEXT("HintLatexFire", "Feuer verbrennt Latex. Glut braucht Schlacke oder Schlackenglas.");
	}
	if (Hull == FName(TEXT("Latex")) && bAcid)
	{
		return LOCTEXT("HintLatexAcid", "Die Säure frisst sich durch den Latex. Sie braucht Glas oder einen Kiel.");
	}
	if (Hull == FName(TEXT("Blasenhaut")) && bFire)
	{
		return LOCTEXT("HintBubbleFire", "Die Blasenhaut verbrennt, bevor sie fliegt. Feuer braucht eine harte Hülle.");
	}
	if (Hull == FName(TEXT("Schlacke")) && (bWet || bAcid))
	{
		return LOCTEXT("HintSlagWet", "Flüssiges läuft aus der Schlacke heraus. Es braucht eine Kapsel.");
	}
	if (Cores.IsEmpty())
	{
		return LOCTEXT("HintNoCore", "Es fehlt ein Kern, etwas, das wirkt: Glut, Wasser, Rauch, Gift, Säure, Heilung, Licht oder Wucht.");
	}
	if (Parts.Num() > 4)
	{
		return LOCTEXT("HintTooMany", "Zu viel auf einmal: höchstens vier Teile.");
	}
	return LOCTEXT("HintNothing", "Die Teile passen nicht zusammen. Vielleicht ein anderer Verstärker, oder einer weniger.");
}

FText VaelEngineerGear::GetEffectText(const FVaelGear& Gear)
{
	const FText Secs = FText::AsNumber(FMath::RoundToInt(Gear.Seconds));
	const FText Meters = FText::AsNumber(FMath::RoundToInt(Gear.Radius / 100.0f));
	FText Base;
	switch (Gear.Effect)
	{
	case EVaelAmmoEffect::Shards: Base = LOCTEXT("EffShards", "Splitter treffen alles im Umkreis (60 %)"); break;
	case EVaelAmmoEffect::Fire: Base = FText::Format(LOCTEXT("EffFire", "Brand: {0} Sekunden Feuer"), Secs); break;
	case EVaelAmmoEffect::Poison: Base = FText::Format(LOCTEXT("EffPoison", "Gift: {0} Sekunden Schaden, das Ziel taumelt"), Secs); break;
	case EVaelAmmoEffect::Acid: Base = FText::Format(LOCTEXT("EffAcid", "Säure: {0} Sekunden mehr Schaden von allem"), Secs); break;
	case EVaelAmmoEffect::Heal: Base = LOCTEXT("EffHeal", "Heilt die Gefährten am Aufschlag"); break;
	case EVaelAmmoEffect::HealTime: Base = FText::Format(LOCTEXT("EffHealTime", "Heilt die Gefährten {0} Sekunden lang"), Secs); break;
	case EVaelAmmoEffect::Heavy: Base = LOCTEXT("EffHeavy", "Wirft zurück und betäubt 0,8 Sekunden"); break;
	case EVaelAmmoEffect::Sticky: Base = FText::Format(LOCTEXT("EffSticky", "{0} Sekunden halb so schnell"), Secs); break;
	case EVaelAmmoEffect::Smoke: Base = FText::Format(LOCTEXT("EffSmoke", "Rauchwolke {0} m: Gegner darin taumeln"), Meters); break;
	case EVaelAmmoEffect::Sleep: Base = LOCTEXT("EffSleep", "Schwache Gegner in der Wolke schlafen, starke werden träge"); break;
	case EVaelAmmoEffect::Glow: Base = FText::Format(LOCTEXT("EffGlow", "Leuchtet {0} Sekunden, nimmt mehr Schaden"), Secs); break;
	case EVaelAmmoEffect::Dew: Base = FText::Format(LOCTEXT("EffDew", "Nass in {0} m: Feuer aus, Blitz trifft doppelt"), Meters); break;
	case EVaelAmmoEffect::Nid: Base = LOCTEXT("EffNid", "Doppelter Schaden"); break;
	case EVaelAmmoEffect::Flame: Base = FText::Format(LOCTEXT("EffFlame", "Stichflamme {0} m im Kegel, alles darin brennt"), Meters); break;
	case EVaelAmmoEffect::Blast: Base = Gear.Delay > 0.0f ? FText::Format(LOCTEXT("EffDelayBlast", "Explodiert nach {0} s im Umkreis von {1} m"), FText::AsNumber(Gear.Delay), Meters)
		: FText::Format(LOCTEXT("EffBlast", "Explosion {0} m, wirft zurück"), Meters); break;
	case EVaelAmmoEffect::FireField: Base = FText::Format(LOCTEXT("EffField", "Brennender Boden {0} m, {1} Sekunden"), Meters, Secs); break;
	case EVaelAmmoEffect::BlackFire: Base = FText::Format(LOCTEXT("EffBlack", "Schwarzfeuer {0} Sekunden, übergeht Feuerschutz"), Secs); break;
	case EVaelAmmoEffect::Steam: Base = Gear.Seconds >= 4.0f ? FText::Format(LOCTEXT("EffSteamCloud", "Dampfwolke {0} m, {1} Sekunden: verbrüht und blendet"), Meters, Secs)
		: FText::Format(LOCTEXT("EffSteam", "Dampf {0} m: verbrüht, blendet {1} Sekunden"), Meters, Secs); break;
	case EVaelAmmoEffect::Frenzy: Base = FText::Format(LOCTEXT("EffFrenzy", "Gegner in {0} m gehen {1} Sekunden aufeinander los"), Meters, Secs); break;
	case EVaelAmmoEffect::Blind: Base = FText::Format(LOCTEXT("EffBlind", "Lichtblitz {0} m: blendet {1} Sekunden"), Meters, Secs); break;
	case EVaelAmmoEffect::Pierce: Base = LOCTEXT("EffPierce", "Geht durch Panzer"); break;
	default: Base = FText::GetEmpty(); break;
	}
	TArray<FString> Extra;
	if (Gear.bAlsoBurn) { Extra.Add(LOCTEXT("ExtraBurn", "brennt").ToString()); }
	if (Gear.bAlsoBleed) { Extra.Add(LOCTEXT("ExtraBleed", "blutet").ToString()); }
	if (Gear.bAlsoSlow) { Extra.Add(LOCTEXT("ExtraSlow", "verlangsamt").ToString()); }
	if (Gear.PartBreak > 1.0f) { Extra.Add(LOCTEXT("ExtraBreak", "Teile brechen schneller").ToString()); }
	if (Gear.ShadowPull > 0.0f) { Extra.Add(FText::Format(LOCTEXT("ExtraShadow", "zieht {0} zum Schatten"), FText::AsNumber(Gear.ShadowPull)).ToString()); }
	return Extra.IsEmpty() ? Base : FText::FromString(Base.ToString() + TEXT(" · ") + FString::Join(Extra, TEXT(" · ")));
}

#undef LOCTEXT_NAMESPACE
