// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/VaelWorldMap.h"

#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "UI/VaelUISettings.h"
#include "VaelAssets.h"

const UVaelWorldMap* UVaelWorldMap::ForWorld(const UWorld* World)
{
	if (World == nullptr)
	{
		return nullptr;
	}
	// Set in the project settings, otherwise by name: L_Vael shows /Game/Vael/UI/Karten/DA_Karte_Vael
	const FString LevelName = UWorld::RemovePIEPrefix(World->GetMapName());
	if (const TSoftObjectPtr<UVaelWorldMap>* Entry = UVaelUISettings::Get()->LevelMaps.Find(FName(*LevelName)))
	{
		return VaelAssets::LoadOptional(*Entry);
	}
	FString Short = LevelName;
	Short.RemoveFromStart(TEXT("L_"));
	const TSoftObjectPtr<UVaelWorldMap> ByName(FSoftObjectPath(FString::Printf(TEXT("/Game/Vael/UI/Karten/DA_Karte_%s.DA_Karte_%s"), *Short, *Short)));
	return VaelAssets::LoadOptional(ByName);
}
