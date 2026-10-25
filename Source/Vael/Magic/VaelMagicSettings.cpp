// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelMagicSettings.h"
#include "Animation/AnimMontage.h"
#include "Engine/StaticMesh.h"
#include "Magic/VaelElementSource.h"
#include "Magic/VaelWaterOrb.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "VaelAssets.h"
#include "Misc/Paths.h"

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
	CastMontagesByDelivery.Add(EVaelSpellDelivery::Summon, PushMontage);
	CastMontagesByDelivery.Add(EVaelSpellDelivery::Orbit, PushMontage);
	CastMontagesByDelivery.Add(EVaelSpellDelivery::Teleport, PushMontage);
	CastMontagesByDelivery.Add(EVaelSpellDelivery::Raise, PushMontage);
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

	// Until an element gets its own, its impacts use the default Vael impact, tinted in the colour of the element.

	MudEffect = Effect(TEXT("NS_Vael_Mud"));

	// Effects of bought packs (Boris 2026-10-25: only what fits our idea of the spells), each fitted by a tuning
	const auto Pack = [](const TCHAR* Path)
	{
		const FString Name = FPaths::GetBaseFilename(Path);
		return TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(FString::Printf(TEXT("/Game/%s.%s"), Path, *Name)));
	};
	const auto Tune = [this](const TSoftObjectPtr<UNiagaraSystem>& System, float ReferenceRadius, TArray<FName> ColorParameters, TArray<FName> EmittersOff)
	{
		FVaelPackTuning& Tuning = PackTunings.AddDefaulted_GetRef();
		Tuning.System = System;
		Tuning.ReferenceRadius = ReferenceRadius;
		Tuning.ColorParameters = MoveTemp(ColorParameters);
		Tuning.EmittersOff = MoveTemp(EmittersOff);
		return &Tuning;
	};

	// Fire (the systems are set in the fire settings). All of them simulate in world space and size their sprites by their own
	// parameters, which are multipliers of 1; "Fire overlay color" is the brightness of the flames (2 in the pack, darker for Vael)
	const TMap<FName, float> FireSizes = { { TEXT("Emitter Size Common"), 1.0f }, { TEXT("Fire Size Common"), 1.0f }, { TEXT("Sprite Size Common"), 1.0f }, { TEXT("Smoke Size Common"), 1.0f } };
	FVaelPackTuning* GroundFire = Tune(Pack(TEXT("Fire_Solution_FX/Fire_Common/NS_FireCommon")), 60.0f, {}, {});
	GroundFire->ScaleParameters = FireSizes;
	GroundFire->Colors.Add(TEXT("Fire overlay color"), FLinearColor(1.25f, 1.15f, 1.05f, 1.0f));
	GroundFire->Colors.Add(TEXT("Smoke color"), FLinearColor(0.35f, 0.33f, 0.32f, 0.55f));
	// A little smaller and calmer: slower flames, fewer of them (Boris 2026-10-25, the campfire)
	GroundFire->ReferenceRadius = 80.0f;
	GroundFire->Floats = { { TEXT("Velocity Common"), 0.7f }, { TEXT("Spawn Rate Common"), 0.8f }, { TEXT("Drag Common"), 1.3f } };
	FVaelPackTuning* BallFire = Tune(Pack(TEXT("Fire_Solution_FX/Fire_Small/NS_FireSmall_var1")), 25.0f, {}, {});
	BallFire->ScaleParameters = FireSizes;
	// Balls of fire a little redder at the edge
	BallFire->Colors.Add(TEXT("Fire overlay color"), FLinearColor(1.15f, 0.72f, 0.55f, 1.0f));

	// Burst where a Funke hits: a copy of the small fire, short and thrown outwards
	FVaelPackTuning* SparkHit = Tune(Pack(TEXT("Vael/Effects/Paket/NS_Vael_Funke_Treffer")), 20.0f, {}, {});
	SparkHit->ScaleParameters = FireSizes;
	SparkHit->Floats = { { TEXT("LifeTime Common"), 0.45f }, { TEXT("Velocity Common"), 2.6f }, { TEXT("Spawn Rate Common"), 4.0f } };

	// Mushroom cloud of the Feuerball: a copy of the common fire, its smoke in the color the mushroom gives, thick and long
	FVaelPackTuning* Mushroom = Tune(Pack(TEXT("Vael/Effects/Paket/NS_Vael_Feuerpilz_Rauch")), 60.0f, { TEXT("Smoke color") }, {});
	Mushroom->ScaleParameters = FireSizes;
	// It shoots up fast and lives long, so the moment it emits towers into a rearing cloud
	Mushroom->Floats = { { TEXT("LifeTime Smoke"), 3.2f }, { TEXT("LifeTime Fire"), 1.0f }, { TEXT("Spawn Rate Common"), 3.0f }, { TEXT("Velocity Common"), 3.0f }, { TEXT("Drag Common"), 0.6f } };
	FVaelPackTuning* TrailFire = Tune(Pack(TEXT("Fire_Solution_FX/Fire_Windy/NS_FireWindy_Common")), 30.0f, {}, {});
	TrailFire->ScaleParameters = FireSizes;
	FVaelPackTuning* BigFire = Tune(Pack(TEXT("Fire_Solution_FX/Fire_Big/NS_FireBig")), 120.0f, {}, {});
	BigFire->ScaleParameters = FireSizes;

	// Steam: the grey-scale smoke of the fire pack without its flames, in the pale steam color
	// The smoke plume of NiagaraExamples: soft light puffs in any color (the smoke of the fire pack is made for fire and
	// stays dark and orange-lit). Sized by its sprite size in cm: scaling the component only spreads world-space puffs thin
	SteamEffect = Pack(TEXT("NiagaraExamples/FX_Smoke/NS_Smoke_Plume"));
	FVaelPackTuning* Steam = Tune(SteamEffect, 100.0f, { TEXT("Smoke Color") }, {});
	Steam->ScaleParameters = { { TEXT("Sprite Size"), 110.0f } };
	Steam->ScaleRange = FVector2D(0.5f, 3.5f);
	Steam->Floats = { { TEXT("SpawnRate"), 16.0f } };

	// Dust: the dirt explosion without its fire flash and sparks, in the color of the debris; puffs of the gravel footstep without stones and print
	DustBurstEffect = Pack(TEXT("NiagaraExamples/FX_Explosions/NS_Dirt_Explosion_Small"));
	Tune(DustBurstEffect, 100.0f, { TEXT("Dust Color"), TEXT("Dirt Color") },
		{ TEXT("Explosion"), TEXT("SparkDebris"), TEXT("NE_PostProcess"), TEXT("Decal_Light_Flash") });
	DustPuffEffect = Pack(TEXT("NiagaraExamples/FX_Footstep/NS_Footstep_Gravel"));
	Tune(DustPuffEffect, 30.0f, {}, { TEXT("Rocks"), TEXT("FootprintBase") });

	// Mist and steam of Vael's own mist kit (Scripts/CreateMistSandKit.py)
	MistMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Vael/Effects/Nebel/M_Vael_Nebel.M_Vael_Nebel")));
	SteamBallMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Vael/Effects/Nebel/MI_Vael_Dampfkugel.MI_Vael_Dampfkugel")));
	WhirlMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Vael/Effects/Nebel/M_Vael_Sandwirbel.M_Vael_Sandwirbel")));

	// Debris of sand storms and dust devils (Boris 2026-10-25: many grains, small stones and twigs of the Megascans packs, dark and small)
	const auto Piece = [this](const TCHAR* Path, float MinSize, float MaxSize, float Share)
	{
		FVaelWhirlPiece& Kind = WhirlPieces.AddDefaulted_GetRef();
		const FString Name = FPaths::GetBaseFilename(Path);
		Kind.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(FString::Printf(TEXT("/Game/%s.%s"), Path, *Name)));
		Kind.MinSize = MinSize;
		Kind.MaxSize = MaxSize;
		Kind.Share = Share;
	};
	Piece(TEXT("Megascans/3D_Assets/CoalA/SM_CoalA"), 0.8f, 1.8f, 0.3f);
	Piece(TEXT("Megascans/3D_Assets/CoalB/SM_CoalB"), 1.5f, 3.5f, 0.22f);
	Piece(TEXT("Megascans/3D_Assets/HovsBeachRock/SM_HovsBeachRock"), 2.5f, 6.0f, 0.16f);
	Piece(TEXT("Rock_Collection_04/Meshes/Rock_03/StaticMeshes/SM_Rock_03"), 3.0f, 7.0f, 0.1f);
	Piece(TEXT("Megascans/3D_Assets/Branch/SM_Branch_00"), 10.0f, 22.0f, 0.06f);
	Piece(TEXT("Megascans/3D_Assets/Branch/SM_Branch_01"), 8.0f, 18.0f, 0.06f);
	Piece(TEXT("Megascans/3D_Assets/WoodBranchA/SM_WoodBranchA"), 12.0f, 26.0f, 0.04f);
	Piece(TEXT("Megascans/3D_Assets/Branch/SM_Branch_02"), 6.0f, 14.0f, 0.06f);

	// Storms and whirlwinds (Sandsturm, Staubteufel): the air aura, its smoke, spiral and wave in the color of the spell
	FVaelPackTuning* Storm = Tune(Pack(TEXT("Free_Spells/VFX_Niagara/NS_Free_Spells_Aura_Air")), 150.0f,
		{ TEXT("Color_Smoke"), TEXT("Color_Spiral1"), TEXT("Color_Wave"), TEXT("Color_Sparks1") }, {});
	Storm->ScaleParameters = { { TEXT("Scale_All"), 1.0f } };
	BeamEffect = Effect(TEXT("NS_Vael_Beam"));
	HandEffect = Effect(TEXT("NS_Vael_Hand"));

	ElementOrbCoreMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Vael/Effects/M_Vael_Kugel.M_Vael_Kugel")));
	ElementOrbGlowMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Vael/Effects/M_Vael_Schein.M_Vael_Schein")));
	ElementOrbRockMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Rock_Collection_04/Meshes/Rock_03/StaticMeshes/SM_Rock_03.SM_Rock_03")));

	// The water kit under Content/VFX/Water
	WaterOrbClass = TSoftClassPtr<AVaelWaterOrb>(FSoftObjectPath(TEXT("/Game/VFX/Water/Blueprints/BP_WaterOrb.BP_WaterOrb_C")));
	PuddleMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/VFX/Water/Materials/M_Water_Puddle.M_Water_Puddle")));
	WaterBoltClass = TSoftClassPtr<AVaelWaterOrb>(FSoftObjectPath(TEXT("/Script/Vael.VaelWaterBolt")));
	WaterBoltHeadEffect = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/VFX/Water/Niagara/NS_Water_Orb.NS_Water_Orb")));
	WaterOrbImpactEffect = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/VFX/Water/Niagara/NS_Water_OrbImpact.NS_Water_OrbImpact")));
	MageWaterSource = TSoftObjectPtr<UVaelElementSource>(FSoftObjectPath(TEXT("/Game/VFX/Water/Sources/DA_ElementSource_Water_Mage.DA_ElementSource_Water_Mage")));
	RainWaterSource = TSoftObjectPtr<UVaelElementSource>(FSoftObjectPath(TEXT("/Game/VFX/Water/Sources/DA_ElementSource_Water_Rain.DA_ElementSource_Water_Rain")));
	WaveMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/VFX/Water/Materials/MI_Water_Wave_Mage.MI_Water_Wave_Mage")));
	MarkWaveMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/VFX/Water/Materials/MI_Water_Wave_Mark.MI_Water_Wave_Mark")));
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
