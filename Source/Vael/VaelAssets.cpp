// Copyright Epic Games, Inc. All Rights Reserved.

#include "VaelAssets.h"
#include "NiagaraSystem.h"
#include "Creatures/VaelCreatureData.h"
#include "Engine/AssetManager.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "UObject/UObjectIterator.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Vael.h"
#include "Misc/PackageName.h"
#include "UObject/GCObject.h"

namespace
{
	/**
	 *  In the game every optional asset is looked up once (Boris 2026-11-27: the game stutters in fights). Before, a missing asset
	 *  asked the disk on every spell and debris burst, and an asset nothing else held was dropped by the garbage collection and
	 *  loaded again from the disk at the next hit, in the middle of the fight. The cache keeps what it loaded and remembers what
	 *  is missing. Only in the game: in the editor assets are made, renamed and deleted while it runs.
	 */
	class FVaelAssetCache : public FGCObject
	{
	public:
		TMap<FSoftObjectPath, TObjectPtr<UObject>> Loaded;
		TSet<FSoftObjectPath> Missing;
		/** Paths already in one of the preload lists */
		TSet<FString> Remembered;
		TSharedPtr<FStreamableHandle> Background;
		bool bPreloadStarted = false;

		virtual void AddReferencedObjects(FReferenceCollector& Collector) override
		{
			Collector.AddReferencedObjects(Loaded);
		}

		virtual FString GetReferencerName() const override
		{
			return TEXT("FVaelAssetCache");
		}
	};

	FVaelAssetCache& Cache()
	{
		static FVaelAssetCache Instance;
		return Instance;
	}

	FString SavedList()
	{
		return FPaths::ProjectSavedDir() / TEXT("VaelPreload.txt");
	}

	FString ProjectList()
	{
		return FPaths::ProjectConfigDir() / TEXT("VaelPreload.txt");
	}
}

static void CollectGameSoftReferences(TSet<FSoftObjectPath>& Paths);

void VaelAssets::PreloadRemembered()
{
	FVaelAssetCache& Assets = Cache();
	if (!FApp::IsGame() || Assets.bPreloadStarted)
	{
		return;
	}
	Assets.bPreloadStarted = true;
	const double Start = FPlatformTime::Seconds();
	int32 Count = 0;
	// what every creature kind and the settings name by soft reference (the first spider or harpy of a fight loaded its body from
	// the disk), loaded with the level, before play
	TSet<FSoftObjectPath> GameReferences;
	CollectGameSoftReferences(GameReferences);
	for (const FSoftObjectPath& Path : GameReferences)
	{
		if (Path.ResolveObject() == nullptr && FPackageName::DoesPackageExist(Path.GetLongPackageName()))
		{
			if (UObject* Found = Path.TryLoad())
			{
				Assets.Loaded.Add(Path, Found);
				++Count;
			}
		}
	}
	for (const FString& File : { ProjectList(), SavedList() })
	{
		TArray<FString> Lines;
		FFileHelper::LoadFileToStringArray(Lines, *File);
		for (FString& Line : Lines)
		{
			Line.TrimStartAndEndInline();
			if (Line.IsEmpty() || Assets.Remembered.Contains(Line))
			{
				continue;
			}
			Assets.Remembered.Add(Line);
			const FSoftObjectPath Path(Line);
			if (Path.IsNull())
			{
				continue;
			}
			// loaded now, while the level comes up, instead of in the middle of the first fight
			UObject* Found = Path.ResolveObject();
			if (Found == nullptr && FPackageName::DoesPackageExist(Path.GetLongPackageName()))
			{
				Found = Path.TryLoad();
				++Count;
			}
			if (Found != nullptr)
			{
				Assets.Loaded.Add(Path, Found);
			}
		}
	}

#if WITH_EDITORONLY_DATA
	// In the editor build a Niagara system is digested again on its first activation in every run (80-160 ms each, the
	// worst hitches of a fight, trace 2026-11-27); done here for every loaded system, all at once
	TArray<UNiagaraSystem*> Compiling;
	for (TObjectIterator<UNiagaraSystem> It; It; ++It)
	{
		UNiagaraSystem* System = *It;
		if (System != nullptr && !System->HasAnyFlags(RF_ClassDefaultObject) && System->NeedsRequestCompile())
		{
			System->RequestCompile(false);
			Compiling.Add(System);
		}
	}
	for (UNiagaraSystem* System : Compiling)
	{
		System->WaitForCompilationComplete(false, false);
	}
	UE_LOG(LogVael, Log, TEXT("VaelAssets: %d assets loaded and %d Niagara systems prepared before play (%.1f s)"), Count, Compiling.Num(), FPlatformTime::Seconds() - Start);
#else
	UE_LOG(LogVael, Log, TEXT("VaelAssets: %d assets loaded before play (%.1f s)"), Count, FPlatformTime::Seconds() - Start);
#endif
}

namespace
{
	/** Soft references in a property, its arrays and structs (a few levels deep); never whole levels */
	void CollectSoft(const FProperty* Property, const void* Container, int32 Depth, TSet<FSoftObjectPath>& Out)
	{
		if (Depth > 3 || Property == nullptr || Container == nullptr)
		{
			return;
		}
		for (int32 Index = 0; Index < Property->ArrayDim; ++Index)
		{
			const void* Value = Property->ContainerPtrToValuePtr<void>(Container, Index);
			if (const FSoftObjectProperty* Soft = CastField<FSoftObjectProperty>(Property))
			{
				const UClass* Kind = Soft->PropertyClass;
				if (Kind != nullptr && !Kind->IsChildOf(UWorld::StaticClass()))
				{
					const FSoftObjectPath Path = Soft->GetPropertyValue(Value).ToSoftObjectPath();
					if (!Path.IsNull())
					{
						Out.Add(Path);
					}
				}
			}
			else if (const FArrayProperty* Array = CastField<FArrayProperty>(Property))
			{
				FScriptArrayHelper Helper(Array, Value);
				for (int32 Element = 0; Element < Helper.Num() && Element < 256; ++Element)
				{
					// the element sits at its pointer; CollectSoft reads it as a container of the inner property at offset 0
					CollectSoft(Array->Inner, Helper.GetRawPtr(Element) - Array->Inner->GetOffset_ForInternal(), Depth + 1, Out);
				}
			}
			else if (const FStructProperty* Struct = CastField<FStructProperty>(Property))
			{
				for (TFieldIterator<FProperty> It(Struct->Struct); It; ++It)
				{
					CollectSoft(*It, Value, Depth + 1, Out);
				}
			}
		}
	}
}

static void CollectGameSoftReferences(TSet<FSoftObjectPath>& Paths)
{
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) || !Class->GetName().StartsWith(TEXT("Vael")))
		{
			continue;
		}
		if (!Class->IsChildOf(UVaelCreatureData::StaticClass()) && !Class->IsChildOf(UDeveloperSettings::StaticClass()))
		{
			continue;
		}
		const UObject* Defaults = Class->GetDefaultObject();
		for (TFieldIterator<FProperty> Field(Class); Field; ++Field)
		{
			CollectSoft(*Field, Defaults, 0, Paths);
		}
	}
	// the creature kinds made as data assets in the editor
	for (TObjectIterator<UVaelCreatureData> It; It; ++It)
	{
		if (!It->HasAnyFlags(RF_ClassDefaultObject))
		{
			for (TFieldIterator<FProperty> Field(It->GetClass()); Field; ++Field)
			{
				CollectSoft(*Field, *It, 0, Paths);
			}
		}
	}
}

void VaelAssets::PreloadInBackground()
{
	// loading during play made hitches of its own (post-load work on the game thread, up to 670 ms, 2026-11-28):
	// PreloadRemembered loads the same before play
}

UObject* VaelAssets::LoadOptional(const FSoftObjectPath& Path)
{
	if (Path.IsNull())
	{
		return nullptr;
	}

	const bool bGame = FApp::IsGame();
	if (bGame)
	{
		FVaelAssetCache& Assets = Cache();
		if (const TObjectPtr<UObject>* Known = Assets.Loaded.Find(Path); Known != nullptr && *Known != nullptr)
		{
			return *Known;
		}
		if (Assets.Missing.Contains(Path))
		{
			return nullptr;
		}
	}

	// Already loaded assets need no look on the disk
	UObject* Found = Path.ResolveObject();
	if (Found == nullptr)
	{
		if (!FPackageName::DoesPackageExist(Path.GetLongPackageName()))
		{
			if (bGame)
			{
				Cache().Missing.Add(Path);
			}
			return nullptr;
		}
		const double Before = FPlatformTime::Seconds();
		Found = Path.TryLoad();
		// Loaded in the middle of play: remembered, so the next game loads it in the background at the start
		if (bGame && Found != nullptr)
		{
			FVaelAssetCache& Assets = Cache();
			const FString Line = Path.ToString();
			UE_LOG(LogVael, Log, TEXT("VaelAssets: '%s' loaded during play (%.1f ms)"), *Line, (FPlatformTime::Seconds() - Before) * 1000.0);
			if (!Assets.Remembered.Contains(Line))
			{
				Assets.Remembered.Add(Line);
				FFileHelper::SaveStringToFile(Line + LINE_TERMINATOR, *SavedList(), FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
			}
		}
	}

	if (bGame)
	{
		if (Found != nullptr)
		{
			Cache().Loaded.Add(Path, Found);
		}
		else
		{
			Cache().Missing.Add(Path);
		}
	}
	return Found;
}
