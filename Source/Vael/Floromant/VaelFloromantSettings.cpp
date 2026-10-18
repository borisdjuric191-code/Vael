// Copyright Epic Games, Inc. All Rights Reserved.

#include "Floromant/VaelFloromantSettings.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

#define LOCTEXT_NAMESPACE "VaelFloromant"

namespace
{
	/** Materials made by Scripts/CreateFloromantKit.py */
	const TCHAR* FloromantMaterials = TEXT("/Game/Vael/Characters/Floromant/Materialien");

	template<typename T>
	TSoftObjectPtr<T> AssetAt(const FString& Folder, const FString& Name)
	{
		return TSoftObjectPtr<T>(FSoftObjectPath(FString::Printf(TEXT("%s/%s.%s"), *Folder, *Name, *Name)));
	}
}

UVaelFloromantSettings::UVaelFloromantSettings()
{
	// Bodies from the creature factory (Tripo, humans pipeline), on the skeleton of the mage
	const FString Bodies = TEXT("/Game/Vael/Characters/Floromant");
	BodyMeshes[0] = AssetAt<USkeletalMesh>(Bodies / TEXT("FloromantMann"), TEXT("SK_FloromantMann_Koerper"));
	BodyMeshes[1] = AssetAt<USkeletalMesh>(Bodies / TEXT("FloromantinFrau"), TEXT("SK_FloromantinFrau_Koerper"));
	HeadMeshes[0] = AssetAt<USkeletalMesh>(Bodies / TEXT("FloromantMann"), TEXT("SK_FloromantMann_Kopf"));
	HeadMeshes[1] = AssetAt<USkeletalMesh>(Bodies / TEXT("FloromantinFrau"), TEXT("SK_FloromantinFrau_Kopf"));
	WingMaterial = AssetAt<UMaterialInterface>(FloromantMaterials, TEXT("MI_Floromant_Fluegel"));

	// The three test plants: the Tripo models of the props where they exist
	FVaelPlantLook& Thistle = Plants[static_cast<int32>(EVaelSeedKind::Thistle)];
	Thistle.DisplayName = LOCTEXT("Thistle", "Glutdistel");
	Thistle.Mesh = AssetAt<UStaticMesh>(TEXT("/Game/Vael/Requisiten/Aschenmark/Glutdistel"), TEXT("SM_Glutdistel"));
	Thistle.Material = AssetAt<UMaterialInterface>(FloromantMaterials, TEXT("MI_Floromant_Glutdistel"));
	Thistle.Height = 110.0f;
	Thistle.SaftCost = 35.0f;
	Thistle.Health = 70.0f;
	Thistle.Lifetime = 18.0f;
	Thistle.Color = FLinearColor(1.0f, 0.32f, 0.12f);

	FVaelPlantLook& Wall = Plants[static_cast<int32>(EVaelSeedKind::RootWall)];
	Wall.DisplayName = LOCTEXT("RootWall", "Wurzelwand");
	Wall.Material = AssetAt<UMaterialInterface>(FloromantMaterials, TEXT("MI_Floromant_Wurzel"));
	Wall.Height = 150.0f;
	Wall.SaftCost = 30.0f;
	Wall.Health = 160.0f;
	Wall.Lifetime = 12.0f;
	Wall.Color = FLinearColor(0.85f, 0.3f, 0.6f);

	// No Tränenkelch model yet: the Heilkelch of the Wurzelforst stands in for it
	FVaelPlantLook& Chalice = Plants[static_cast<int32>(EVaelSeedKind::Chalice)];
	Chalice.DisplayName = LOCTEXT("Chalice", "Tränenkelch");
	Chalice.Mesh = AssetAt<UStaticMesh>(TEXT("/Game/Vael/Requisiten/Wurzelforst/Heilkelch"), TEXT("SM_Heilkelch"));
	Chalice.Material = AssetAt<UMaterialInterface>(FloromantMaterials, TEXT("MI_Floromant_Traenenkelch"));
	Chalice.Height = 95.0f;
	Chalice.SaftCost = 40.0f;
	Chalice.Health = 60.0f;
	Chalice.Lifetime = 15.0f;
	Chalice.Color = FLinearColor(0.2f, 0.95f, 0.85f);
}

const FVaelPlantLook& UVaelFloromantSettings::GetPlant(EVaelSeedKind Kind) const
{
	return Plants[FMath::Clamp(static_cast<int32>(Kind), 0, static_cast<int32>(EVaelSeedKind::Count) - 1)];
}

#undef LOCTEXT_NAMESPACE
