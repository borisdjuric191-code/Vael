// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelGameplayTags.h"

namespace VaelTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Element_Fire, "Element.Fire", "Fire damage");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Element_Water, "Element.Water", "Water damage");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Element_Earth, "Element.Earth", "Earth damage");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Element_Air, "Element.Air", "Air damage");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Element_Mark, "Element.Mark", "Mark damage");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Damage, "SetByCaller.Damage", "Amount of damage of a damage effect");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Mana, "SetByCaller.Mana", "Amount of mana an effect adds, negative for costs");

	FGameplayTag GetElementTag(EVaelElement Element)
	{
		switch (Element)
		{
		case EVaelElement::Fire:	return Element_Fire;
		case EVaelElement::Water:	return Element_Water;
		case EVaelElement::Earth:	return Element_Earth;
		case EVaelElement::Air:		return Element_Air;
		case EVaelElement::Mark:	return Element_Mark;
		default:					return FGameplayTag();
		}
	}
}
