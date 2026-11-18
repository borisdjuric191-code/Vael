// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UVaelMaterial;

/** What kind of thing a material of the bag is, for its colour behind the picture and, later, for sorting (Boris 17.11.:
 *  "verschiedene Farben für Itemkategorien, zum späteren Sortieren"). Finer than EVaelMaterialCategory: it tells an ore from
 *  a part of a creature from what the engineer made of them. */
enum class EVaelItemGroup : uint8
{
	Ore,		// ores and stones: Funkenerz, Rosteisen, Glutstein, Schlacke
	Crystal,	// crystals of the Nid and of the moon
	Creature,	// what creatures leave: plates, sinews, feathers, claws, glands, eyes, hides
	Workstuff,	// what is made of them: Schlackeneisen, Messingblech, Latex, Ordenstuch, Russglas
	Plant,		// leaves, petals, fibres, flesh and seeds of plants as ingredients
	Liquid,		// saps, dews, milks, nectars
	Dust,		// pollen, dust, glitter
	Fungus,		// the catalysts of the witch's kitchen
	Seed,		// seeds to sow
	Potion,		// brewed potions
	Armor,		// the engineer's armour pieces
	Count
};

namespace VaelItemGroups
{
	EVaelItemGroup GetGroup(const UVaelMaterial* Material);
	FLinearColor GetColor(EVaelItemGroup Group);
	FText GetName(EVaelItemGroup Group);
}
