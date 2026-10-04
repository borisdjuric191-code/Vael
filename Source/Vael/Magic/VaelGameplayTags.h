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

	/** Magnitudes handed to gameplay effects by their caller */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Damage);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Mana);

	/** Returns the tag of an element */
	FGameplayTag GetElementTag(EVaelElement Element);
}
