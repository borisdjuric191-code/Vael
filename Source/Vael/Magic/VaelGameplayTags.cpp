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
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Duration, "SetByCaller.Duration", "Seconds a status effect lasts");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Multiplier, "SetByCaller.Multiplier", "Factor a reaction applied to the damage, for the damage number");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Wet, "Status.Wet", "Wet: lightning hits twice as hard, fire is weakened");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Burning, "Status.Burning", "Burning: takes fire damage over time");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Frozen, "Status.Frozen", "Frozen: can't move, earth shatters it");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Slowed, "Status.Slowed", "Slowed: moves at a share of its speed");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Marked, "Status.Marked", "Marked by the Mark: takes more damage from everything");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Feared, "Status.Feared", "Feared: flees from the players and doesn't attack");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Fevered, "Status.Fevered", "Fevered: goes for other creatures, who can hurt each other");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Channeling, "State.Channeling", "Channels a spell: casts nothing else and walks slower");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Casting, "State.Casting", "Plays the animation of a spell");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_CastPoint, "Event.CastPoint", "The moment in a cast animation at which the spell leaves the hand");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Gear_WeatherWard, "Gear.WeatherWard", "Gear like the Sturmmantel: rain doesn't wet its wearer, lightning strikes and the backlash of lightning cast while wet don't hurt");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Gear_WeatherAttunement, "Gear.WeatherAttunement", "Gear like a storm cloak: what the weather gives to spells counts twice");

	FGameplayTag GetStatusTag(EVaelStatus Status)
	{
		switch (Status)
		{
		case EVaelStatus::Wet:		return Status_Wet;
		case EVaelStatus::Burning:	return Status_Burning;
		case EVaelStatus::Frozen:	return Status_Frozen;
		case EVaelStatus::Slowed:	return Status_Slowed;
		case EVaelStatus::Marked:	return Status_Marked;
		case EVaelStatus::Feared:	return Status_Feared;
		case EVaelStatus::Fevered:	return Status_Fevered;
		default:					return FGameplayTag();
		}
	}

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
