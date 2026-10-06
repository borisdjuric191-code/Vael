// Copyright Epic Games, Inc. All Rights Reserved.

#include "VaelAssets.h"
#include "Misc/PackageName.h"

UObject* VaelAssets::LoadOptional(const FSoftObjectPath& Path)
{
	if (Path.IsNull())
	{
		return nullptr;
	}

	// Already loaded assets need no look on the disk
	if (UObject* Loaded = Path.ResolveObject())
	{
		return Loaded;
	}

	if (!FPackageName::DoesPackageExist(Path.GetLongPackageName()))
	{
		return nullptr;
	}

	return Path.TryLoad();
}
