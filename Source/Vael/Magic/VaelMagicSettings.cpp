// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelMagicSettings.h"

UVaelMagicSettings::UVaelMagicSettings()
{
	// Values of the browser prototype
	ManaCostByElementCount = { 8.0f, 15.0f, 24.0f };

	ElementColors.Add(EVaelElement::Fire, FLinearColor::FromSRGBColor(FColor(0xff, 0x7a, 0x2e)));
	ElementColors.Add(EVaelElement::Water, FLinearColor::FromSRGBColor(FColor(0x3f, 0x9d, 0xf0)));
	ElementColors.Add(EVaelElement::Earth, FLinearColor::FromSRGBColor(FColor(0xa8, 0x83, 0x4e)));
	ElementColors.Add(EVaelElement::Air, FLinearColor::FromSRGBColor(FColor(0xd6, 0xec, 0xff)));
	ElementColors.Add(EVaelElement::Mark, FLinearColor::FromSRGBColor(FColor(0xa2, 0x4d, 0xff)));
}

float UVaelMagicSettings::GetManaCost(int32 NumElements, int32 NumEnvironmentElements) const
{
	if (NumElements <= 0 || ManaCostByElementCount.IsEmpty())
	{
		return 0.0f;
	}

	// Longer formulas than the table knows cost as much as its last entry
	const float BaseCost = ManaCostByElementCount[FMath::Min(NumElements, ManaCostByElementCount.Num()) - 1];

	return FMath::Max(BaseCost - NumEnvironmentElements * EnvironmentCostReduction, MinManaCost);
}

FLinearColor UVaelMagicSettings::GetElementColor(EVaelElement Element) const
{
	const FLinearColor* Color = ElementColors.Find(Element);
	return Color != nullptr ? *Color : FLinearColor::White;
}
