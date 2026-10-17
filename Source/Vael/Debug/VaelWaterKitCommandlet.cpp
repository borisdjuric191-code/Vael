// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debug/VaelWaterKitCommandlet.h"
#include "Engine/Blueprint.h"
#include "Magic/VaelWaterOrb.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "NiagaraComponent.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraRibbonRendererProperties.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraSystem.h"
#include "NiagaraTypes.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
	const TCHAR* NiagaraFolder = TEXT("/Game/VFX/Water/Niagara");
	const TCHAR* MaterialFolder = TEXT("/Game/VFX/Water/Materials");

	/** Part of the emitter name and the particle material it renders with */
	struct FWaterEmitterRule
	{
		const TCHAR* Part;
		const TCHAR* Material;
	};

	const FWaterEmitterRule EmitterRules[] =
	{
		{ TEXT("Bubbles"), TEXT("MI_Water_Particle_Bubble") },
		{ TEXT("Droplets"), TEXT("MI_Water_Particle_Drop") },
		{ TEXT("Splash"), TEXT("MI_Water_Particle_Drop") },
		{ TEXT("Spray"), TEXT("MI_Water_Particle_Mist") },
		{ TEXT("Mist"), TEXT("MI_Water_Particle_Mist") },
	};

	const TCHAR* SystemNames[] = { TEXT("NS_Water_Orb"), TEXT("NS_Water_OrbImpact") };

	const FName WaterColorName(TEXT("User.WaterColor"));

#if WITH_EDITOR
	UMaterialInterface* LoadParticleMaterial(const TCHAR* Name)
	{
		return LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), MaterialFolder, Name, Name));
	}

	const FWaterEmitterRule* FindRule(const FString& EmitterName)
	{
		for (const FWaterEmitterRule& Rule : EmitterRules)
		{
			if (EmitterName.Contains(Rule.Part))
			{
				return &Rule;
			}
		}

		return nullptr;
	}

	/** Gives every sprite and ribbon renderer of the emitter the material. Returns true if something changed. */
	bool CheckRenderers(FVersionedNiagaraEmitterData* EmitterData, const FString& Label, UMaterialInterface* Expected, bool bDryRun, TArray<FString>& Report)
	{
		if (EmitterData == nullptr)
		{
			Report.Add(FString::Printf(TEXT("  %s: no emitter data"), *Label));
			return false;
		}

		bool bChanged = false;
		for (UNiagaraRendererProperties* Renderer : EmitterData->GetRenderers())
		{
			TObjectPtr<UMaterialInterface>* Slot = nullptr;
			FName PropertyName = TEXT("Material");

			if (UNiagaraSpriteRendererProperties* Sprite = Cast<UNiagaraSpriteRendererProperties>(Renderer))
			{
				Slot = &Sprite->Material;
				if (Sprite->MaterialUserParamBinding.Parameter.IsValid())
				{
					Report.Add(FString::Printf(TEXT("  %s: sprite material bound to user parameter %s, it wins over the material"), *Label, *Sprite->MaterialUserParamBinding.Parameter.GetName().ToString()));
				}
			}
			else if (UNiagaraRibbonRendererProperties* Ribbon = Cast<UNiagaraRibbonRendererProperties>(Renderer))
			{
				Slot = &Ribbon->Material;
			}
			else
			{
				Report.Add(FString::Printf(TEXT("  %s: renderer %s left alone"), *Label, *GetNameSafe(Renderer->GetClass())));
				continue;
			}

			const FString Current = GetNameSafe(Slot->Get());
			if (Slot->Get() == Expected)
			{
				Report.Add(FString::Printf(TEXT("  %s: %s %s OK"), *Label, *Renderer->GetClass()->GetName(), *Current));
				continue;
			}

			Report.Add(FString::Printf(TEXT("  %s: %s has %s, %s %s"), *Label, *Renderer->GetClass()->GetName(), *Current, bDryRun ? TEXT("should be") : TEXT("set to"), *GetNameSafe(Expected)));
			if (!bDryRun)
			{
				Renderer->Modify();
				*Slot = Expected;

				FPropertyChangedEvent ChangedEvent(FindFProperty<FProperty>(Renderer->GetClass(), PropertyName));
				Renderer->PostEditChangeProperty(ChangedEvent);
				bChanged = true;
			}
		}

		return bChanged;
	}

	bool SaveAsset(UObject* Asset, TArray<FString>& Report)
	{
		UPackage* Package = Asset->GetOutermost();
		Package->MarkPackageDirty();

		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		const bool bSaved = UPackage::SavePackage(Package, nullptr, *Filename, SaveArgs);

		Report.Add(FString::Printf(TEXT("  %s %s"), bSaved ? TEXT("saved") : TEXT("ERROR: could not save"), *Package->GetName()));
		return bSaved;
	}
#endif
}

UVaelWaterKitCommandlet::UVaelWaterKitCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UVaelWaterKitCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	const bool bDryRun = Params.Contains(TEXT("-DryRun"));
	TArray<FString> Report;
	int32 Errors = 0;

	Report.Add(FString::Printf(TEXT("Water kit check%s"), bDryRun ? TEXT(" (dry run, nothing saved)") : TEXT("")));

	// The emitter assets
	Report.Add(TEXT("Emitters:"));
	for (const FWaterEmitterRule& Rule : EmitterRules)
	{
		const FString Name = FString::Printf(TEXT("NE_Water_%s"), Rule.Part);
		UNiagaraEmitter* Emitter = LoadObject<UNiagaraEmitter>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), NiagaraFolder, *Name, *Name));
		UMaterialInterface* Expected = LoadParticleMaterial(Rule.Material);
		if (Emitter == nullptr || Expected == nullptr)
		{
			Report.Add(FString::Printf(TEXT("  ERROR: %s or %s missing"), *Name, Rule.Material));
			++Errors;
			continue;
		}

		if (CheckRenderers(Emitter->GetLatestEmitterData(), Name, Expected, bDryRun, Report) && !SaveAsset(Emitter, Report))
		{
			++Errors;
		}
	}

	// The systems keep copies of their emitters, which are checked as well
	Report.Add(TEXT("Systems:"));
	for (const TCHAR* SystemName : SystemNames)
	{
		UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), NiagaraFolder, SystemName, SystemName));
		if (System == nullptr)
		{
			Report.Add(FString::Printf(TEXT("  ERROR: %s missing"), SystemName));
			++Errors;
			continue;
		}

		Report.Add(FString::Printf(TEXT(" %s"), SystemName));

		TArray<FNiagaraVariable> UserParameters;
		System->GetExposedParameters().GetUserParameters(UserParameters);

		bool bHasWaterColor = false;
		for (const FNiagaraVariable& Parameter : UserParameters)
		{
			FString Line = FString::Printf(TEXT("  user parameter %s (%s)"), *Parameter.GetName().ToString(), *Parameter.GetType().GetName());
			if (Parameter.GetType() == FNiagaraTypeDefinition::GetColorDef())
			{
				Line += FString::Printf(TEXT(" default %s"), *System->GetExposedParameters().GetParameterValue<FLinearColor>(Parameter).ToString());
			}

			Report.Add(Line);
			bHasWaterColor |= Parameter.GetName() == WaterColorName || Parameter.GetName() == TEXT("WaterColor");
		}

		if (!bHasWaterColor)
		{
			Report.Add(TEXT("  ERROR: no user parameter WaterColor"));
			++Errors;
		}

		bool bChanged = false;
		for (FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
		{
			const FString HandleName = Handle.GetName().ToString();
			const FWaterEmitterRule* Rule = FindRule(HandleName);
			if (Rule == nullptr)
			{
				Report.Add(FString::Printf(TEXT("  %s: no rule, left alone"), *HandleName));
				continue;
			}

			bChanged |= CheckRenderers(Handle.GetEmitterData(), HandleName, LoadParticleMaterial(Rule->Material), bDryRun, Report);
		}

		if (bChanged)
		{
			System->RequestCompile(false);
			System->WaitForCompilationComplete(false, false);

			if (!SaveAsset(System, Report))
			{
				++Errors;
			}
		}
	}

	// The ball of water gets its lasting bubbles and drops
	Report.Add(TEXT("BP_WaterOrb:"));
	UBlueprint* OrbBlueprint = LoadObject<UBlueprint>(nullptr, TEXT("/Game/VFX/Water/Blueprints/BP_WaterOrb.BP_WaterOrb"));
	UNiagaraSystem* OrbSystem = LoadObject<UNiagaraSystem>(nullptr, *FString::Printf(TEXT("%s/NS_Water_Orb.NS_Water_Orb"), NiagaraFolder));
	const AVaelWaterOrb* OrbDefaults = OrbBlueprint != nullptr && OrbBlueprint->GeneratedClass != nullptr ? Cast<AVaelWaterOrb>(OrbBlueprint->GeneratedClass->GetDefaultObject()) : nullptr;
	UNiagaraComponent* OrbParticles = OrbDefaults != nullptr ? OrbDefaults->GetParticles() : nullptr;

	if (OrbParticles == nullptr || OrbSystem == nullptr)
	{
		Report.Add(TEXT("  ERROR: BP_WaterOrb or NS_Water_Orb missing"));
		++Errors;
	}
	else if (OrbParticles->GetAsset() == OrbSystem)
	{
		Report.Add(TEXT("  Particles NS_Water_Orb OK"));
	}
	else
	{
		Report.Add(FString::Printf(TEXT("  Particles has %s, %s NS_Water_Orb"), *GetNameSafe(OrbParticles->GetAsset()), bDryRun ? TEXT("should be") : TEXT("set to")));
		if (!bDryRun)
		{
			OrbParticles->Modify();
			OrbParticles->SetAsset(OrbSystem);

			if (!SaveAsset(OrbBlueprint, Report))
			{
				++Errors;
			}
		}
	}

	Report.Add(FString::Printf(TEXT("Errors: %d"), Errors));

	const FString ReportPath = FPaths::ProjectSavedDir() / TEXT("Logs/VaelWaterKit.txt");
	FFileHelper::SaveStringArrayToFile(Report, *ReportPath);

	for (const FString& Line : Report)
	{
		UE_LOG(LogTemp, Display, TEXT("VaelWaterKit: %s"), *Line);
	}

	return Errors > 0 ? 1 : 0;
#else
	return 0;
#endif
}
