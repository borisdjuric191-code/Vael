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
		{ TEXT("DropHit"), TEXT("MI_Water_Particle_Drop") },
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

	/** Writes one value of a parameter store as text */
	FString ParameterValueText(const FNiagaraParameterStore& Store, const FNiagaraVariableWithOffset& Parameter)
	{
		const FNiagaraTypeDefinition& Type = Parameter.GetType();
		const int32 Size = Type.GetSize();
		const uint8* Data = Store.GetParameterData(Parameter.Offset, Size);
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
			return *reinterpret_cast<const int32*>(Data) != 0 ? TEXT("true") : TEXT("false");
		}
		if (Size % sizeof(float) == 0 && Size <= 4 * sizeof(float))
		{
			// Vectors, colors and positions are floats in a row
			TArray<FString> Parts;
			for (int32 Index = 0; Index < Size / int32(sizeof(float)); ++Index)
			{
				Parts.Add(FString::SanitizeFloat(reinterpret_cast<const float*>(Data)[Index]));
			}
			return FString::Printf(TEXT("(%s)"), *FString::Join(Parts, TEXT(", ")));
		}

		return FString::Printf(TEXT("<%d bytes>"), Size);
	}

	/** Lists how an emitter is set up: space, sim target, renderers and the constant inputs of its modules */
	void DumpEmitter(const UNiagaraSystem* System, const FVersionedNiagaraEmitterData* EmitterData, const FString& Label, TArray<FString>& Report)
	{
		if (EmitterData == nullptr)
		{
			return;
		}

		Report.Add(FString::Printf(TEXT("  %s: local space %s, %s, persistent IDs %s"), *Label, EmitterData->bLocalSpace ? TEXT("yes") : TEXT("no"),
			EmitterData->SimTarget == ENiagaraSimTarget::GPUComputeSim ? TEXT("GPU") : TEXT("CPU"), EmitterData->bRequiresPersistentIDs ? TEXT("yes") : TEXT("no")));

		for (const FNiagaraEventScriptProperties& Handler : EmitterData->GetEventHandlers())
		{
			// The emitter the events come from, by its id in the system
			FString SourceName = Handler.SourceEmitterID.ToString();
			for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
			{
				if (Handle.GetId() == Handler.SourceEmitterID)
				{
					SourceName = Handle.GetName().ToString();
				}
			}

			Report.Add(FString::Printf(TEXT("    event handler: %s from %s, mode %d, spawn %u (random %s, min %u), max events per frame %u"), *Handler.SourceEventName.ToString(), *SourceName,
				int32(Handler.ExecutionMode), Handler.SpawnNumber, Handler.bRandomSpawnNumber ? TEXT("yes") : TEXT("no"), Handler.MinSpawnNumber, Handler.MaxEventsPerFrame));

			if (Handler.Script != nullptr)
			{
				for (const FNiagaraVariableWithOffset& Parameter : Handler.Script->RapidIterationParameters.ReadParameterVariables())
				{
					Report.Add(FString::Printf(TEXT("      %s = %s"), *Parameter.GetName().ToString(), *ParameterValueText(Handler.Script->RapidIterationParameters, Parameter)));
				}
			}
		}

		for (const UNiagaraRendererProperties* Renderer : EmitterData->GetRenderers())
		{
			if (const UNiagaraSpriteRendererProperties* Sprite = Cast<UNiagaraSpriteRendererProperties>(Renderer))
			{
				Report.Add(FString::Printf(TEXT("    sprite: enabled %s, alignment %d, facing %d, sort %d, sort hint %d"), Sprite->GetIsEnabled() ? TEXT("yes") : TEXT("no"),
					int32(Sprite->Alignment), int32(Sprite->FacingMode), int32(Sprite->SortMode), Sprite->SortOrderHint));
			}
		}

		const TPair<const TCHAR*, const UNiagaraScript*> Scripts[] =
		{
			{ TEXT("emitter spawn"), EmitterData->EmitterSpawnScriptProps.Script },
			{ TEXT("emitter update"), EmitterData->EmitterUpdateScriptProps.Script },
			{ TEXT("particle spawn"), EmitterData->SpawnScriptProps.Script },
			{ TEXT("particle update"), EmitterData->UpdateScriptProps.Script },
		};

		for (const TPair<const TCHAR*, const UNiagaraScript*>& Script : Scripts)
		{
			if (Script.Value == nullptr)
			{
				continue;
			}

			Report.Add(FString::Printf(TEXT("    %s:"), Script.Key));
			const FNiagaraParameterStore& Store = Script.Value->RapidIterationParameters;
			for (const FNiagaraVariableWithOffset& Parameter : Store.ReadParameterVariables())
			{
				Report.Add(FString::Printf(TEXT("      %s = %s"), *Parameter.GetName().ToString(), *ParameterValueText(Store, Parameter)));
			}
		}
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
	const bool bDump = Params.Contains(TEXT("-Dump"));
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

			// The particles only get the color of the source if one of the compiled scripts reads it
			bool bReadsWaterColor = false;
			if (const FVersionedNiagaraEmitterData* EmitterData = Handle.GetEmitterData())
			{
				TArray<UNiagaraScript*> Scripts;
				EmitterData->GetScripts(Scripts, false, true);
				for (const UNiagaraScript* Script : Scripts)
				{
					for (const FNiagaraVariable& Parameter : Script->GetVMExecutableData().Parameters.Parameters)
					{
						bReadsWaterColor |= Parameter.GetName() == WaterColorName;
					}
				}
			}

			Report.Add(FString::Printf(TEXT("  %s: %s"), *HandleName, bReadsWaterColor ? TEXT("reads User.WaterColor") : TEXT("ERROR: does not read User.WaterColor, stays white")));
			Errors += bReadsWaterColor ? 0 : 1;

			if (bDump)
			{
				Report.Add(FString::Printf(TEXT("  %s: enabled %s"), *HandleName, Handle.GetIsEnabled() ? TEXT("yes") : TEXT("no")));
				DumpEmitter(System, Handle.GetEmitterData(), HandleName, Report);
			}
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
