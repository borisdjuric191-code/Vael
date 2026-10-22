// Copyright Epic Games, Inc. All Rights Reserved.

#include "Floromant/VaelFloromantSettings.h"
#include "Engine/SkeletalMesh.h"
#include "Floromant/VaelPlantArmData.h"
#include "Floromant/VaelPlantData.h"
#include "Materials/MaterialInterface.h"

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
	RootMaterial = AssetAt<UMaterialInterface>(FloromantMaterials, TEXT("MI_Floromant_Wurzel"));

	// A shooter, a healer, a wall and a helper on the belt to start with; every plant with a model can go there
	for (const TCHAR* Id : { TEXT("Glutdistel"), TEXT("Russrose"), TEXT("Schlotwinde"), TEXT("Glimmklee") })
	{
		SeedBelt.Add(AssetAt<UVaelPlantData>(PlantDataFolder, FString(TEXT("DA_Pflanze_")) + Id));
	}

	// The hands start with the Windhalm; DA_Pflanzenarm_<Id> are made by Scripts/CreatePflanzenarmKit.py
	HandBelt.Add(AssetAt<UVaelPlantArmData>(TEXT("/Game/Vael/Characters/Floromant/Pflanzenarm"), TEXT("DA_Pflanzenarm_Windhalm")));
}
