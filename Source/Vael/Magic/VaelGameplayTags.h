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

	/** Granted by gear while it is worn */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Gear_WeatherWard);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Gear_WeatherAttunement);

	/** Magnitudes handed to gameplay effects by their caller */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Damage);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Mana);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Duration);

	/** Returns the tag of an element */
	FGameplayTag GetElementTag(EVaelElement Element);

	/** Returns the tag of a status, invalid for None */
	FGameplayTag GetStatusTag(EVaelStatus Status);
}
