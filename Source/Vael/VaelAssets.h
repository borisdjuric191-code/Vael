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
