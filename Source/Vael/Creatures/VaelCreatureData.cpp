// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelCreatureData.h"
#include "Creatures/VaelAshHarpy.h"
#include "Creatures/VaelEmberCrawler.h"
#include "Creatures/VaelEmberQueen.h"
#include "Creatures/VaelPreacher.h"
#include "Creatures/VaelHornBeetle.h"
#include "Creatures/VaelSourceGuardian.h"

#define LOCTEXT_NAMESPACE "VaelCreatures"

UVaelEmberCrawlerData::UVaelEmberCrawlerData()
{
	DisplayName = LOCTEXT("EmberCrawler", "Glutkriecher");
	KindId = TEXT("Glutkriecher");
	CompendiumId = TEXT("Glutkriecher");
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
	FormulaOnWitnessedDive = { EVaelElement::Earth, EVaelElement::Air };
	Loot.Add(FVaelLootEntry(TEXT("/Game/Vael/Items/Materials/DA_Material_Russfeder.DA_Material_Russfeder"), 0.5f));
	BodyColor = FLinearColor(0.15f, 0.13f, 0.12f);
}

UVaelHarpyElderData::UVaelHarpyElderData()
{
	DisplayName = LOCTEXT("HarpyElder", "Harpyien-\u00C4lteste");
	KindId = TEXT("HarpyienAelteste");
	CompendiumId = TEXT("HarpyienAelteste");

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
	CompendiumId = TEXT("Glutkoenigin");
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

#undef LOCTEXT_NAMESPACE
