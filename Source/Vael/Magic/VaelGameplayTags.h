// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"
#include "Magic/VaelElementTypes.h"

namespace VaelTags
{
	/** Element of a damage effect */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Element_Fire);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Element_Water);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Element_Earth);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Element_Air);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Element_Mark);

	/** Conditions of a character, granted by status effects while they last */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Wet);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Burning);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Frozen);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Slowed);

	/** Granted while a character channels a spell: no other spell can be cast and the character walks slower */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Channeling);

	/** Granted while a character plays the animation of a spell, from the start until it blends out */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Casting);

	/** Sent by the cast point notify of a cast animation: the moment the spell leaves the hand */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_CastPoint);

	/** Granted by gear while it is worn */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Gear_WeatherWard);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Gear_WeatherAttunement);

	/** Magnitudes handed to gameplay effects by their caller */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Damage);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Mana);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Duration);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Multiplier);

	/** Returns the tag of an element */
	FGameplayTag GetElementTag(EVaelElement Element);

	/** Returns the tag of a status, invalid for None */
	FGameplayTag GetStatusTag(EVaelStatus Status);
}
