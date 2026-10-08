// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelMagicSettings.h"
#include "Animation/AnimMontage.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "VaelAssets.h"

UVaelMagicSettings::UVaelMagicSettings()
{
	// Values of the browser prototype
	ManaCostByElementCount = { 8.0f, 15.0f, 24.0f };

	ElementColors.Add(EVaelElement::Fire, FLinearColor::FromSRGBColor(FColor(0xff, 0x7a, 0x2e)));
	ElementColors.Add(EVaelElement::Water, FLinearColor::FromSRGBColor(FColor(0x3f, 0x9d, 0xf0)));
	ElementColors.Add(EVaelElement::Earth, FLinearColor::FromSRGBColor(FColor(0xa8, 0x83, 0x4e)));
	ElementColors.Add(EVaelElement::Air, FLinearColor::FromSRGBColor(FColor(0xd6, 0xec, 0xff)));
	ElementColors.Add(EVaelElement::Mark, FLinearColor::FromSRGBColor(FColor(0xa2, 0x4d, 0xff)));

	// Cast animations the mage gets once they are made in the editor: a one-handed throw for everything that flies,
	// a two-handed push for cones, walls, patches and bursts, a loop while channeling
	const TSoftObjectPtr<UAnimMontage> PushMontage(FSoftObjectPath(TEXT("/Game/Vael/Characters/Mage/AM_Mage_Push.AM_Mage_Push")));

	DefaultCastMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Vael/Characters/Mage/AM_Mage_Cast.AM_Mage_Cast")));
	CastMontagesByDelivery.Add(EVaelSpellDelivery::Cone, PushMontage);
	CastMontagesByDelivery.Add(EVaelSpellDelivery::Wall, PushMontage);
	CastMontagesByDelivery.Add(EVaelSpellDelivery::GroundArea, PushMontage);
	CastMontagesByDelivery.Add(EVaelSpellDelivery::Nova, PushMontage);
	CastMontagesByDelivery.Add(EVaelSpellDelivery::Aura, PushMontage);
	CastMontagesByDelivery.Add(EVaelSpellDelivery::SpikeLine, PushMontage);
	CastMontagesByDelivery.Add(EVaelSpellDelivery::Strike, PushMontage);
	CastMontagesByDelivery.Add(EVaelSpellDelivery::Beam, TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Vael/Characters/Mage/AM_Mage_Channel.AM_Mage_Channel"))));
	ElementSelectMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Vael/Characters/Mage/AM_Mage_Select.AM_Mage_Select")));

	// General effects the spells use once they are made in the editor; they are tinted in the color of each element
	const auto Effect = [](const TCHAR* Name)
	{
		return TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(FString::Printf(TEXT("/Game/Vael/Effects/%s.%s"), Name, Name)));
	};
	const auto Sound = [](const TCHAR* Name)
	{
		return TSoftObjectPtr<USoundBase>(FSoftObjectPath(FString::Printf(TEXT("/Game/Vael/Audio/%s.%s"), Name, Name)));
	};

	DefaultEffects.CastEffect = Effect(TEXT("NS_Vael_Cast"));
	DefaultEffects.TrailEffect = Effect(TEXT("NS_Vael_Trail"));
	DefaultEffects.ImpactEffect = Effect(TEXT("NS_Vael_Impact"));
	DefaultEffects.GroundEffect = Effect(TEXT("NS_Vael_Ground"));
	DefaultEffects.CastSound = Sound(TEXT("SFX_Vael_Cast"));
	DefaultEffects.ImpactSound = Sound(TEXT("SFX_Vael_Impact"));

	SteamEffect = Effect(TEXT("NS_Vael_Steam"));
	MudEffect = Effect(TEXT("NS_Vael_Mud"));
	BeamEffect = Effect(TEXT("NS_Vael_Beam"));
	HandEffect = Effect(TEXT("NS_Vael_Hand"));
}

UAnimMontage* UVaelMagicSettings::FindCastMontage(EVaelSpellDelivery Delivery) const
{
	// Dashes have their own montage on the character
	if (Delivery == EVaelSpellDelivery::Dash)
	{
		return nullptr;
	}

	if (const TSoftObjectPtr<UAnimMontage>* ByDelivery = CastMontagesByDelivery.Find(Delivery))
	{
		if (UAnimMontage* Montage = VaelAssets::LoadOptional(*ByDelivery))
		{
			return Montage;
		}
	}

	return VaelAssets::LoadOptional(DefaultCastMontage);
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
