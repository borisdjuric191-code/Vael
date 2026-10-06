// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelCreatureData.h"
#include "Creatures/VaelAshHarpy.h"
#include "Creatures/VaelEmberCrawler.h"
#include "Creatures/VaelEmberQueen.h"
#include "Creatures/VaelPreacher.h"

#define LOCTEXT_NAMESPACE "VaelCreatures"

UVaelEmberCrawlerData::UVaelEmberCrawlerData()
{
	DisplayName = LOCTEXT("EmberCrawler", "Glutkriecher");
	CreatureClass = AVaelEmberCrawler::StaticClass();

	MaxHealth = 34.0f;
	MoveSpeed = 238.0f;
	CollisionRadius = 59.0f;
	CollisionHalfHeight = 59.0f;
	ElementMultipliers.Add(EVaelElement::Water, 1.5f);
	ElementMultipliers.Add(EVaelElement::Fire, 0.5f);
	AggroRange = 1540.0f;
	FormulaOnWitnessedExplosion = { EVaelElement::Fire, EVaelElement::Fire, EVaelElement::Fire };
	BodyColor = FLinearColor(1.0f, 0.25f, 0.05f);
}

UVaelAshHarpyData::UVaelAshHarpyData()
{
	DisplayName = LOCTEXT("AshHarpy", "Aschharpyie");
	CreatureClass = AVaelAshHarpy::StaticClass();

	MaxHealth = 26.0f;
	MoveSpeed = 616.0f;
	CollisionRadius = 50.0f;
	CollisionHalfHeight = 60.0f;
	ElementMultipliers.Add(EVaelElement::Earth, 1.4f);
	ElementMultipliers.Add(EVaelElement::Air, 1.2f);
	AggroRange = 1400.0f;
	FormulaOnWitnessedDive = { EVaelElement::Earth, EVaelElement::Air };
	BodyColor = FLinearColor(0.15f, 0.13f, 0.12f);
}

UVaelHarpyElderData::UVaelHarpyElderData()
{
	DisplayName = LOCTEXT("HarpyElder", "Harpyien-\u00C4lteste");

	MaxHealth = 180.0f;
	MoveSpeed = 532.0f;
	CollisionRadius = 81.0f;
	CollisionHalfHeight = 81.0f;
	ElementMultipliers.Reset();
	ElementMultipliers.Add(EVaelElement::Earth, 1.3f);
	KnockbackMultiplier = 0.5f;
	StaggerMultiplier = 0.5f;
	FormulaOnDeath = { EVaelElement::Air, EVaelElement::Air, EVaelElement::Air };
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
	BodyColor = FLinearColor(0.27f, 0.05f, 0.1f);
}

UVaelEmberQueenData::UVaelEmberQueenData()
{
	DisplayName = LOCTEXT("EmberQueen", "Glutk\u00F6nigin");
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
	AggroRange = 1400.0f;
	BodyColor = FLinearColor(1.0f, 0.15f, 0.01f);
}

#undef LOCTEXT_NAMESPACE
