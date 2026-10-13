// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelCreatureData.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Creatures/VaelAshHarpy.h"
#include "Creatures/VaelAshRoller.h"
#include "Creatures/VaelBloodMoth.h"
#include "Creatures/VaelDraculaupe.h"
#include "Creatures/VaelEmberCrawler.h"
#include "Creatures/VaelEmberQueen.h"
#include "Creatures/VaelPreacher.h"
#include "Creatures/VaelSickleDoll.h"
#include "Creatures/VaelHornBeetle.h"
#include "Creatures/VaelSourceGuardian.h"
#include "Creatures/VaelVeilMonkey.h"

#define LOCTEXT_NAMESPACE "VaelCreatures"

UVaelEmberCrawlerData::UVaelEmberCrawlerData()
{
	DisplayName = LOCTEXT("EmberCrawler", "Glutkriecher");
	KindId = TEXT("Glutkriecher");
	CompendiumId = TEXT("Glutkriecher");
	Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Vael/Kreaturen/Aschenmark/Glutkriecher/SK_Glutkriecher.SK_Glutkriecher")));
	MeshGlow = 1.5f;
	CreatureClass = AVaelEmberCrawler::StaticClass();

	MaxHealth = 34.0f;
	MoveSpeed = 238.0f;
	CollisionRadius = 59.0f;
	CollisionHalfHeight = 59.0f;
	ElementMultipliers.Add(EVaelElement::Water, 1.5f);
	ElementMultipliers.Add(EVaelElement::Fire, 0.5f);
	AggroRange = 1540.0f;
	FormulaOnWitnessedExplosion = { EVaelElement::Fire, EVaelElement::Fire, EVaelElement::Fire };
	Loot.Add(FVaelLootEntry(TEXT("/Game/Vael/Items/Materials/DA_Material_Glutdruese.DA_Material_Glutdruese"), 0.55f));
	BodyColor = FLinearColor(1.0f, 0.25f, 0.05f);
}

UVaelAshHarpyData::UVaelAshHarpyData()
{
	DisplayName = LOCTEXT("AshHarpy", "Aschharpyie");
	KindId = TEXT("Aschharpyie");
	CompendiumId = TEXT("Aschharpyie");
	CreatureClass = AVaelAshHarpy::StaticClass();

	MaxHealth = 26.0f;
	MoveSpeed = 616.0f;
	CollisionRadius = 50.0f;
	CollisionHalfHeight = 60.0f;
	ElementMultipliers.Add(EVaelElement::Earth, 1.4f);
	ElementMultipliers.Add(EVaelElement::Air, 1.2f);
	AggroRange = 1400.0f;
	Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Vael/Kreaturen/Aschenmark/Aschharpyie/SK_Aschharpyie.SK_Aschharpyie")));
	FormulaOnWitnessedDive = { EVaelElement::Earth, EVaelElement::Air };
	Loot.Add(FVaelLootEntry(TEXT("/Game/Vael/Items/Materials/DA_Material_Russfeder.DA_Material_Russfeder"), 0.5f));
	BodyColor = FLinearColor(0.15f, 0.13f, 0.12f);
}

UVaelHarpyElderData::UVaelHarpyElderData()
{
	DisplayName = LOCTEXT("HarpyElder", "Harpyien-\u00C4lteste");
	KindId = TEXT("HarpyienAelteste");
	CompendiumId = TEXT("HarpyienAelteste");
	Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Vael/Kreaturen/Aschenmark/HarpyienAelteste/SK_HarpyienAelteste.SK_HarpyienAelteste")));

	MaxHealth = 180.0f;
	MoveSpeed = 532.0f;
	CollisionRadius = 81.0f;
	CollisionHalfHeight = 81.0f;
	ElementMultipliers.Reset();
	ElementMultipliers.Add(EVaelElement::Earth, 1.3f);
	KnockbackMultiplier = 0.5f;
	StaggerMultiplier = 0.5f;
	FormulaOnDeath = { EVaelElement::Air, EVaelElement::Air, EVaelElement::Air };
	Loot.Reset();
	Loot.Add(FVaelLootEntry(TEXT("/Game/Vael/Items/Materials/DA_Material_Aeltestenschwinge.DA_Material_Aeltestenschwinge"), 1.0f));
	BodyColor = FLinearColor(0.3f, 0.26f, 0.2f);

	FlightHeight = 180.0f;
	CombatCircleRadius = 700.0f;
	CombatCircleSpeed = 0.7f;
	DiveWarningDuration = 0.8f;
	DiveSpeed = 1820.0f;
	DiveDuration = 0.75f;
	DiveDamage = 18.0f;

	bCanScreech = true;
}

UVaelPreacherData::UVaelPreacherData()
{
	DisplayName = LOCTEXT("Preacher", "Prediger der Narbe");
	KindId = TEXT("Prediger");
	CreatureClass = AVaelPreacher::StaticClass();

	MaxHealth = 85.0f;
	MoveSpeed = 364.0f;
	CollisionRadius = 50.0f;
	CollisionHalfHeight = 90.0f;
	ElementMultipliers.Add(EVaelElement::Mark, 0.6f);
	ElementMultipliers.Add(EVaelElement::Air, 1.1f);
	AggroRange = 1680.0f;
	FormulaOnDeath = { EVaelElement::Mark, EVaelElement::Fire };
	FormulaOnWitnessedSpikes = { EVaelElement::Mark, EVaelElement::Earth };
	Loot.Add(FVaelLootEntry(TEXT("/Game/Vael/Items/Materials/DA_Material_Ordenssiegel.DA_Material_Ordenssiegel"), 1.0f));
	ItemDropChance = 0.4f;
	BodyColor = FLinearColor(0.27f, 0.05f, 0.1f);
}

UVaelEmberQueenData::UVaelEmberQueenData()
{
	DisplayName = LOCTEXT("EmberQueen", "Glutk\u00F6nigin");
	KindId = TEXT("Glutkoenigin");
	bUnique = true;
	CompendiumId = TEXT("Glutkoenigin");
	Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Vael/Kreaturen/Aschenmark/Glutkoenigin/SK_Glutkoenigin.SK_Glutkoenigin")));
	MeshGlow = 1.2f;
	CreatureClass = AVaelEmberQueen::StaticClass();

	MaxHealth = 1300.0f;
	MoveSpeed = 175.0f;
	CollisionRadius = 203.0f;
	CollisionHalfHeight = 203.0f;
	ElementMultipliers.Add(EVaelElement::Water, 1.5f);
	ElementMultipliers.Add(EVaelElement::Fire, 0.35f);
	KnockbackMultiplier = 0.15f;
	bCanBeFrozen = false;
	bCanBeMarked = false;
	StaggerMultiplier = 0.2f;
	StunMultiplier = 0.2f;
	PullMultiplier = 0.0f;
	Loot.Add(FVaelLootEntry(TEXT("/Game/Vael/Items/Materials/DA_Material_HerzDerGlut.DA_Material_HerzDerGlut"), 1.0f));
	GuaranteedHealthOrbs = 6;
	GuaranteedItems = 1;
	MinItemRarity = EVaelRarity::Rare;
	AggroRange = 1400.0f;
	BodyColor = FLinearColor(1.0f, 0.15f, 0.01f);
}

UVaelSourceGuardianData::UVaelSourceGuardianData()
{
	DisplayName = LOCTEXT("SourceGuardian", "Quellwächter");
	KindId = TEXT("Quellwaechter");
	CompendiumId = TEXT("Quellwaechter");
	CreatureClass = AVaelSourceGuardian::StaticClass();

	// Between the harpy elder and the ember queen; it shares the weaknesses of the crawlers and harpies it is made of
	MaxHealth = 650.0f;
	MoveSpeed = 260.0f;
	CollisionRadius = 120.0f;
	CollisionHalfHeight = 140.0f;
	ElementMultipliers.Add(EVaelElement::Water, 1.25f);
	ElementMultipliers.Add(EVaelElement::Earth, 1.25f);
	ElementMultipliers.Add(EVaelElement::Mark, 0.5f);
	KnockbackMultiplier = 0.2f;
	StaggerMultiplier = 0.3f;
	StunMultiplier = 0.3f;
	PullMultiplier = 0.0f;
	bCanBeMarked = false;
	bCanBeFrozen = false;
	AggroRange = 100000.0f;
	BodyColor = FLinearColor(0.35f, 0.08f, 0.5f);

	GuaranteedHealthOrbs = 4;
	GuaranteedItems = 1;
	MinItemRarity = EVaelRarity::Rare;

	ScreamHealthShares = { 0.66f, 0.33f };
}

UVaelHornBeetleData::UVaelHornBeetleData()
{
	DisplayName = LOCTEXT("HornBeetle", "Spannhornkäfer");
	CompendiumId = TEXT("Spannhornkaefer");
	KindId = TEXT("Spannhornkaefer");
	CreatureClass = AVaelHornBeetle::StaticClass();
	Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Vael/Kreaturen/Wurzelforst/Spannhornkaefer/SK_Spannhornkaefer.SK_Spannhornkaefer")));

	MaxHealth = 70.0f;
	MoveSpeed = 260.0f;
	CollisionRadius = 55.0f;
	CollisionHalfHeight = 55.0f;
	ElementMultipliers.Add(EVaelElement::Fire, 1.5f);
	KnockbackMultiplier = 0.6f;
	AggroRange = 1500.0f;
	Loot.Add(FVaelLootEntry(TEXT("/Game/Vael/Items/Materials/DA_Material_Spannfaden.DA_Material_Spannfaden"), 0.8f));
	BodyColor = FLinearColor(0.72f, 0.62f, 0.45f);
}

UVaelAshRollerData::UVaelAshRollerData()
{
	DisplayName = LOCTEXT("AshRoller", "Aschenroller");
	CompendiumId = TEXT("Aschenroller");
	KindId = TEXT("Aschenroller");
	CreatureClass = AVaelAshRoller::StaticClass();
	Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Vael/Kreaturen/Aschenmark/Aschenroller/SK_Aschenroller.SK_Aschenroller")));
	BallMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Vael/Requisiten/Aschenmark/AschenrollerKugel/SM_AschenrollerKugel.SM_AschenrollerKugel")));
	MeshGlow = 1.0f;

	// A defender: tougher than the other creatures of the Aschenmark, and at home in the embers
	MaxHealth = 90.0f;
	MoveSpeed = 210.0f;
	CollisionRadius = 60.0f;
	CollisionHalfHeight = 60.0f;
	ElementMultipliers.Add(EVaelElement::Fire, 0.5f);
	KnockbackMultiplier = 0.5f;
	AggroRange = 1540.0f;
	Loot.Add(FVaelLootEntry(TEXT("/Game/Vael/Items/Materials/DA_Material_Rollpanzer.DA_Material_Rollpanzer"), 0.6f));
	BodyColor = FLinearColor(0.17f, 0.14f, 0.12f);
}

UVaelSickleDollData::UVaelSickleDollData()
{
	DisplayName = LOCTEXT("SickleDoll", "Sichelpuppe");
	CompendiumId = TEXT("Sichelpuppe");
	KindId = TEXT("Sichelpuppe");
	CreatureClass = AVaelSickleDoll::StaticClass();
	Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Vael/Kreaturen/Aschenmark/Sichelpuppe/SK_Sichelpuppe.SK_Sichelpuppe")));
	MeshGlow = 1.2f;

	// Light and quick; charred straw doesn't burn twice, but it drinks water
	MaxHealth = 55.0f;
	MoveSpeed = 330.0f;
	CollisionRadius = 42.0f;
	CollisionHalfHeight = 80.0f;
	ElementMultipliers.Add(EVaelElement::Fire, 0.1f);
	ElementMultipliers.Add(EVaelElement::Water, 1.5f);
	AggroRange = 1400.0f;
	Loot.Add(FVaelLootEntry(TEXT("/Game/Vael/Items/Materials/DA_Material_Glimmstroh.DA_Material_Glimmstroh"), 0.6f));
	Loot.Add(FVaelLootEntry(TEXT("/Game/Vael/Items/Materials/DA_Material_Sichelklinge.DA_Material_Sichelklinge"), 0.25f));
	BodyColor = FLinearColor(0.13f, 0.1f, 0.07f);
}

UVaelVeilMonkeyData::UVaelVeilMonkeyData()
{
	DisplayName = LOCTEXT("VeilMonkey", "Schleieraffe");
	CompendiumId = TEXT("Schleieraffe");
	KindId = TEXT("Schleieraffe");
	CreatureClass = AVaelVeilMonkey::StaticClass();
	Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Vael/Kreaturen/Aschenmark/Schleieraffe/SK_Schleieraffe.SK_Schleieraffe")));

	// Small, quick and frail; it lives on not being found
	MaxHealth = 30.0f;
	MoveSpeed = 520.0f;
	CollisionRadius = 34.0f;
	CollisionHalfHeight = 45.0f;
	ElementMultipliers.Add(EVaelElement::Fire, 1.3f);
	AggroRange = 1800.0f;
	bCanBeMarked = false;
	Loot.Add(FVaelLootEntry(TEXT("/Game/Vael/Items/Materials/DA_Material_Schleierpelz.DA_Material_Schleierpelz"), 0.6f));
	Loot.Add(FVaelLootEntry(TEXT("/Game/Vael/Items/Materials/DA_Material_Glimmauge.DA_Material_Glimmauge"), 0.3f));
	HealthOrbChance = 0.0f;
	ManaOrbChance = 0.0f;
	BodyColor = FLinearColor(0.03f, 0.03f, 0.035f);
}

UVaelDraculaupeData::UVaelDraculaupeData()
{
	DisplayName = LOCTEXT("Draculaupe", "Draculaupe");
	CompendiumId = TEXT("Draculaupe");
	KindId = TEXT("Draculaupe");
	CreatureClass = AVaelDraculaupe::StaticClass();
	Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Vael/Kreaturen/Wurzelforst/Draculaupe/SK_Draculaupe.SK_Draculaupe")));
	CocoonMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Vael/Requisiten/Wurzelforst/DraculaupeKokon/SM_DraculaupeKokon.SM_DraculaupeKokon")));
	MothData = UVaelBloodMothData::StaticClass();

	// Slow and frail; its cocoon is what takes the damage
	MaxHealth = 45.0f;
	MoveSpeed = 150.0f;
	CollisionRadius = 42.0f;
	CollisionHalfHeight = 42.0f;
	ElementMultipliers.Add(EVaelElement::Fire, 1.4f);
	AggroRange = 1100.0f;
	Loot.Add(FVaelLootEntry(TEXT("/Game/Vael/Items/Materials/DA_Material_Rotnoppe.DA_Material_Rotnoppe"), 0.6f));
	CocoonLoot.Add(FVaelLootEntry(TEXT("/Game/Vael/Items/Materials/DA_Material_Kokonseide.DA_Material_Kokonseide"), 1.0f, 1, 2));
	HealthOrbChance = 0.0f;
	ManaOrbChance = 0.0f;
	BodyColor = FLinearColor(0.35f, 0.6f, 0.5f);
}

UVaelTauraupeData::UVaelTauraupeData()
{
	DisplayName = LOCTEXT("Tauraupe", "Tauraupe");
	CompendiumId = TEXT("Tauraupe");
	KindId = TEXT("Tauraupe");
	Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Vael/Kreaturen/Wurzelforst/Tauraupe/SK_Tauraupe.SK_Tauraupe")));
	CocoonMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Vael/Requisiten/Wurzelforst/TauraupeKokon/SM_TauraupeKokon.SM_TauraupeKokon")));
	MothData = UVaelDewMothData::StaticClass();

	// Harmless: it wanders on its hind end, slower still, and calls nobody
	MoveSpeed = 90.0f;
	bSeeksPlayers = false;
	bAlarms = false;
	ThreadColor = FLinearColor(0.95f, 0.97f, 1.0f);
	Loot.Reset();
	bCanBeMarked = false;
}

UVaelBloodMothData::UVaelBloodMothData()
{
	DisplayName = LOCTEXT("BloodMoth", "Nosferalter");
	CompendiumId = TEXT("Nosferalter");
	KindId = TEXT("Nosferalter");
	CreatureClass = AVaelBloodMoth::StaticClass();
	Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Vael/Kreaturen/Wurzelforst/Nosferalter/SK_Nosferalter.SK_Nosferalter")));

	// Far tougher than the cocoon it came from
	MaxHealth = 320.0f;
	MoveSpeed = 520.0f;
	CollisionRadius = 55.0f;
	CollisionHalfHeight = 60.0f;
	ElementMultipliers.Add(EVaelElement::Air, 1.3f);
	ElementMultipliers.Add(EVaelElement::Fire, 1.2f);
	KnockbackMultiplier = 0.6f;
	Loot.Add(FVaelLootEntry(TEXT("/Game/Vael/Items/Materials/DA_Material_Blutschuppen.DA_Material_Blutschuppen"), 1.0f, 1, 2));
	HealthOrbChance = 0.0f;
	ManaOrbChance = 0.0f;
	BodyColor = FLinearColor(0.5f, 0.05f, 0.08f);

	const auto AddBloom = [this](EVaelBloomKind Kind, const FText& Name, const TCHAR* MeshPath, float Chance)
	{
		FVaelRainBloomKind& Bloom = Blooms.AddDefaulted_GetRef();
		Bloom.Kind = Kind;
		Bloom.DisplayName = Name;
		Bloom.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(MeshPath));
		Bloom.Chance = Chance;
		return &Bloom;
	};

	// The bad kinds are the common ones
	FVaelRainBloomKind* Thorn = AddBloom(EVaelBloomKind::Thorn, LOCTEXT("ThornBloom", "Dornkelch"), TEXT("/Game/Vael/Requisiten/Wurzelforst/Dornkelch/SM_Dornkelch.SM_Dornkelch"), 0.40f);
	Thorn->Radius = 170.0f;
	Thorn->Amount = 4.0f;
	Thorn->Interval = 1.0f;

	FVaelRainBloomKind* Slumber = AddBloom(EVaelBloomKind::Slumber, LOCTEXT("SlumberBloom", "Schlummerkelch"), TEXT("/Game/Vael/Requisiten/Wurzelforst/Schlummerkelch/SM_Schlummerkelch.SM_Schlummerkelch"), 0.30f);
	Slumber->Radius = 210.0f;
	Slumber->Amount = 0.3f;

	FVaelRainBloomKind* Heal = AddBloom(EVaelBloomKind::Heal, LOCTEXT("HealBloom", "Heilkelch"), TEXT("/Game/Vael/Requisiten/Wurzelforst/Heilkelch/SM_Heilkelch.SM_Heilkelch"), 0.18f);
	Heal->Amount = 25.0f;
	Heal->GlowColor = FLinearColor(1.0f, 0.95f, 0.75f);

	FVaelRainBloomKind* Harvest = AddBloom(EVaelBloomKind::Harvest, LOCTEXT("HarvestBloom", "Erntekelch"), TEXT("/Game/Vael/Requisiten/Wurzelforst/Erntekelch/SM_Erntekelch.SM_Erntekelch"), 0.12f);
	Harvest->Loot.Add(FVaelLootEntry(TEXT("/Game/Vael/Items/Materials/DA_Material_Kelchkapsel.DA_Material_Kelchkapsel"), 1.0f, 1, 2));
}

UVaelDewMothData::UVaelDewMothData()
{
	DisplayName = LOCTEXT("DewMoth", "Taufalter");
	CompendiumId = TEXT("Taufalter");
	KindId = TEXT("Taufalter");
	Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Vael/Kreaturen/Wurzelforst/Taufalter/SK_Taufalter.SK_Taufalter")));

	// Its rain heals, and only the good flowers grow under it
	bHealingRain = true;
	DropAmount = 4.0f;
	DropColor = FLinearColor(0.55f, 0.85f, 1.0f);
	Loot.Reset();
	bCanBeMarked = false;
	BodyColor = FLinearColor(0.4f, 0.75f, 0.7f);

	Blooms.RemoveAll([](const FVaelRainBloomKind& Bloom) { return Bloom.Kind == EVaelBloomKind::Thorn || Bloom.Kind == EVaelBloomKind::Slumber; });
	for (FVaelRainBloomKind& Bloom : Blooms)
	{
		Bloom.Chance = Bloom.Kind == EVaelBloomKind::Heal ? 0.6f : 0.4f;
	}
}

#undef LOCTEXT_NAMESPACE
