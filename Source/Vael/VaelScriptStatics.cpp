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
#include "NiagaraSystem.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraEmitter.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraScript.h"
#include "Materials/MaterialInterface.h"

FString UVaelScriptStatics::DescribeNiagaraSystem(UNiagaraSystem* System)
{
	if (System == nullptr)
	{
		return TEXT("none");
	}

	// A value of a parameter store as text, for the plain types
	const auto ValueOf = [](const FNiagaraParameterStore& Store, const FNiagaraVariable& Variable) -> FString
	{
		const FNiagaraTypeDefinition& Type = Variable.GetType();
		const uint8* Data = Store.GetParameterData(Variable);
		if (Data == nullptr)
		{
			return TEXT("?");
		}
		if (Type == FNiagaraTypeDefinition::GetFloatDef())
		{
			return FString::SanitizeFloat(*reinterpret_cast<const float*>(Data));
		}
		if (Type == FNiagaraTypeDefinition::GetIntDef())
		{
			return FString::FromInt(*reinterpret_cast<const int32*>(Data));
		}
		if (Type == FNiagaraTypeDefinition::GetBoolDef())
		{
			return reinterpret_cast<const FNiagaraBool*>(Data)->GetValue() ? TEXT("true") : TEXT("false");
		}
		if (Type == FNiagaraTypeDefinition::GetVec3Def() || Type == FNiagaraTypeDefinition::GetPositionDef())
		{
			return reinterpret_cast<const FVector3f*>(Data)->ToString();
		}
		if (Type == FNiagaraTypeDefinition::GetVec2Def())
		{
			return reinterpret_cast<const FVector2f*>(Data)->ToString();
		}
		if (Type == FNiagaraTypeDefinition::GetColorDef() || Type == FNiagaraTypeDefinition::GetVec4Def())
		{
			return reinterpret_cast<const FLinearColor*>(Data)->ToString();
		}
		return Type.GetName();
	};

	FString Text;
	TArray<FNiagaraVariable> Parameters;
	System->GetExposedParameters().GetParameters(Parameters);
	for (const FNiagaraVariable& Parameter : Parameters)
	{
		Text += FString::Printf(TEXT("  param %s : %s = %s\n"), *Parameter.GetName().ToString(), *Parameter.GetType().GetName(), *ValueOf(System->GetExposedParameters(), Parameter));
	}
	for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		Text += FString::Printf(TEXT("  emitter %s%s\n"), *Handle.GetName().ToString(), Handle.GetIsEnabled() ? TEXT("") : TEXT(" (off)"));
		const FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData();
		if (Data == nullptr)
		{
			continue;
		}
		Text += FString::Printf(TEXT("    %s, %s space\n"), Data->SimTarget == ENiagaraSimTarget::GPUComputeSim ? TEXT("GPU") : TEXT("CPU"), Data->bLocalSpace ? TEXT("local") : TEXT("world"));
		for (const UNiagaraRendererProperties* Renderer : Data->GetRenderers())
		{
			TArray<UMaterialInterface*> Materials;
			Renderer->GetUsedMaterials(nullptr, Materials);
			FString Names;
			for (const UMaterialInterface* Material : Materials)
			{
				Names += Material != nullptr ? Material->GetPathName() + TEXT(" ") : TEXT("");
			}
			Text += FString::Printf(TEXT("    renderer %s %s\n"), *Renderer->GetClass()->GetName(), *Names);
		}
		// The module inputs typed in by hand; inputs linked to a user parameter don't show here
		for (const UNiagaraScript* Script : { Data->SpawnScriptProps.Script.Get(), Data->UpdateScriptProps.Script.Get() })
		{
			if (Script == nullptr)
			{
				continue;
			}
			TArray<FNiagaraVariable> Inputs;
			Script->RapidIterationParameters.GetParameters(Inputs);
			for (const FNiagaraVariable& Input : Inputs)
			{
				const FString Name = Input.GetName().ToString().Replace(*(TEXT("Constants.") + Handle.GetName().ToString() + TEXT(".")), TEXT(""));
				Text += FString::Printf(TEXT("    %s %s = %s\n"), Script == Data->SpawnScriptProps.Script ? TEXT("spawn") : TEXT("update"), *Name, *ValueOf(Script->RapidIterationParameters, Input));
			}
		}
	}
	return Text;
}

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
