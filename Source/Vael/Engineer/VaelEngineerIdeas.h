// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Compendium/VaelCompendiumEntry.h"

/**
 *  Ideas of the engineer (goal 16.11.): he doesn't find finished plans for his weapons and armour, he gets ideas from what he
 *  meets. Seeing a creature, watching it, beating it, studying it with Albrun (the stages of the compendium) each can give
 *  him an idea: how its plates hold heat, how its tendon springs back. An idea unlocks fixed recipes at the workbench and the
 *  forge. A recipe that has an idea can be built once he has that idea (or found its blueprint); the formulas of shots are
 *  still found by mixing.
 */
struct FVaelEngineerIdea
{
	FName Id;

	/** The compendium entry and the stage that gives the idea */
	FName Subject;
	EVaelResearchStage Stage = EVaelResearchStage::Sighted;

	/** The recipes it unlocks */
	TArray<FName> Gear;

	/** Its name in his sketchbook, the thought in his own words, and a riddle of where it hides while he doesn't have it */
	FText Title;
	FText Thought;
	FText Hint;
};

namespace VaelEngineerIdeas
{
	/** Every idea, in the order of his sketchbook */
	const TArray<FVaelEngineerIdea>& GetAll();

	const FVaelEngineerIdea* Find(FName Id);

	/** The idea that unlocks a recipe, null if the recipe needs none */
	const FVaelEngineerIdea* FindForGear(FName GearId);
}
