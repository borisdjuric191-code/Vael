// Copyright Epic Games, Inc. All Rights Reserved.

#include "VaelScriptStatics.h"
#include "Engine/FontFace.h"
#include "Fonts/FontFaceInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Vael.h"

FGameplayTagContainer UVaelScriptStatics::MakeTagContainer(const TArray<FName>& TagNames)
{
	FGameplayTagContainer Container;

	for (const FName TagName : TagNames)
	{
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TagName, false);
		if (Tag.IsValid())
		{
			Container.AddTag(Tag);
		}
		else
		{
			UE_LOG(LogVael, Warning, TEXT("MakeTagContainer: unknown tag %s"), *TagName.ToString());
		}
	}

	return Container;
}

#if WITH_EDITOR
UFontFace* UVaelScriptStatics::CreateFontFaceFromFile(const FString& FontFile, const FString& AssetPath)
{
	TArray<uint8> FontData;
	if (!FFileHelper::LoadFileToArray(FontData, *FontFile))
	{
		UE_LOG(LogVael, Warning, TEXT("CreateFontFaceFromFile: %s can't be read"), *FontFile);
		return nullptr;
	}

	UPackage* Package = CreatePackage(*AssetPath);
	UFontFace* Face = NewObject<UFontFace>(Package, FName(*FPackageName::GetShortName(AssetPath)), RF_Public | RF_Standalone);
	static_cast<IFontFaceInterface*>(Face)->InitializeFromBulkData(FontFile, EFontHinting::Default, FontData.GetData(), FontData.Num());
	Face->LoadingPolicy = EFontLoadingPolicy::Inline;
	Package->MarkPackageDirty();
	return Face;
}

void UVaelScriptStatics::MakeFontAssets()
{
	TArray<FString> Files;
	const FString Folder = FPaths::ProjectDir() / TEXT("Scripts/Data/Fonts");
	IFileManager::Get().FindFiles(Files, *(Folder / TEXT("*.ttf")), true, false);

	for (const FString& File : Files)
	{
		const FString AssetPath = TEXT("/Game/Vael/UI/Fonts/") + FPaths::GetBaseFilename(File);
		const FString PackageFile = FPackageName::LongPackageNameToFilename(AssetPath, FPackageName::GetAssetPackageExtension());
		if (IFileManager::Get().FileExists(*PackageFile))
		{
			continue;
		}

		UFontFace* Face = CreateFontFaceFromFile(Folder / File, AssetPath);
		if (Face == nullptr)
		{
			continue;
		}

		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		const bool bSaved = UPackage::SavePackage(Face->GetPackage(), Face, *PackageFile, Args);
		UE_LOG(LogVael, Display, TEXT("Font asset %s %s"), *AssetPath, bSaved ? TEXT("saved") : TEXT("NOT saved"));
	}
}
#endif
