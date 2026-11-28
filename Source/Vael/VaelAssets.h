// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"

/**
 *  Loading of assets that are made in the editor by hand and may not exist yet.
 *  The code names them by a fixed path and picks them up as soon as they are there.
 */
namespace VaelAssets
{
	/** Loads, while the level starts, every optional asset an earlier game had to load in the middle of play
	 *  (Config/VaelPreload.txt, kept in the project, and Saved/VaelPreload.txt, written while playing), and in the editor build
	 *  prepares every loaded Niagara system. Once per run. */
	void PreloadRemembered();

	/** Starts loading, in the background, what the creatures and the settings of the game name by soft reference (meshes,
	 *  animations, materials, effects of every creature kind): the first of a kind that appears then needs no load from the disk. */
	void PreloadInBackground();

	/** Loads the asset at the path. Returns null without a warning if it doesn't exist. */
	UObject* LoadOptional(const FSoftObjectPath& Path);

	/** Loads the asset a soft pointer refers to. Returns null without a warning if it doesn't exist. */
	template<typename T>
	T* LoadOptional(const TSoftObjectPtr<T>& Asset)
	{
		return Cast<T>(LoadOptional(Asset.ToSoftObjectPath()));
	}

	/** Loads the class a soft class pointer refers to, like an animation blueprint. Returns null without a warning if it doesn't exist. */
	template<typename T>
	UClass* LoadOptionalClass(const TSoftClassPtr<T>& Class)
	{
		UClass* Loaded = Cast<UClass>(LoadOptional(Class.ToSoftObjectPath()));
		return Loaded != nullptr && Loaded->IsChildOf(T::StaticClass()) ? Loaded : nullptr;
	}
}
