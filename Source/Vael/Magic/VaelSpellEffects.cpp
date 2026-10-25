// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelSpellEffects.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Magic/VaelMagicSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "VaelAssets.h"

const FName VaelEffects::ColorParameter(TEXT("Color"));
const FName VaelEffects::RadiusParameter(TEXT("Radius"));
const FName VaelEffects::BeamEndParameter(TEXT("BeamEnd"));
const FName VaelEffects::IntensityParameter(TEXT("Intensity"));

namespace
{
	/** How long a one-shot effect may emit, and when it is removed at the latest, in seconds */
	const float OneShotEmitSeconds = 0.6f;
	const float OneShotRemoveSeconds = 6.0f;

	/** The first entry that exists: the override, the one of the element, the default */
	template<typename T>
	T* LoadFirst(const TSoftObjectPtr<T>* Override, const TSoftObjectPtr<T>* ForElement, const TSoftObjectPtr<T>& Default)
	{
		if (Override != nullptr && !Override->IsNull())
		{
			if (T* Loaded = VaelAssets::LoadOptional(*Override))
			{
				return Loaded;
			}
		}

		if (ForElement != nullptr && !ForElement->IsNull())
		{
			if (T* Loaded = VaelAssets::LoadOptional(*ForElement))
			{
				return Loaded;
			}
		}

		return VaelAssets::LoadOptional(Default);
	}

	void SetCommonParameters(UNiagaraComponent* Component, const FLinearColor& Color, float Radius)
	{
		Component->SetVariableLinearColor(VaelEffects::ColorParameter, Color);
		Component->SetVariableFloat(VaelEffects::RadiusParameter, Radius);
		VaelEffects::ApplyPackTuning(Component, Color, Radius);
	}
}

void VaelEffects::ApplyPackTuning(UNiagaraComponent* Component, const FLinearColor& Color, float Radius)
{
	const UNiagaraSystem* System = Component != nullptr ? Component->GetAsset() : nullptr;
	if (System == nullptr)
	{
		return;
	}

	const FSoftObjectPath Path(System);
	for (const FVaelPackTuning& Tuning : UVaelMagicSettings::Get()->PackTunings)
	{
		if (Tuning.System.ToSoftObjectPath() != Path)
		{
			continue;
		}

		for (const FName& Name : Tuning.ColorParameters)
		{
			Component->SetVariableLinearColor(Name, Color * Tuning.ColorScale);
		}
		for (const TPair<FName, float>& Value : Tuning.Floats)
		{
			Component->SetVariableFloat(Value.Key, Value.Value);
		}
		for (const TPair<FName, FLinearColor>& Value : Tuning.Colors)
		{
			Component->SetVariableLinearColor(Value.Key, Value.Value);
		}
		for (const FName& Emitter : Tuning.EmittersOff)
		{
			Component->SetEmitterEnable(Emitter, false);
		}
		if (Tuning.ReferenceRadius > 0.0f && Radius > 0.0f)
		{
			const float Scale = FMath::Clamp(Radius / Tuning.ReferenceRadius, Tuning.ScaleRange.X, FMath::Max(Tuning.ScaleRange.X, Tuning.ScaleRange.Y));
			if (Tuning.ScaleParameter.IsNone())
			{
				Component->SetWorldScale3D(FVector(Scale));
			}
			else
			{
				Component->SetVariableFloat(Tuning.ScaleParameter, Scale);
			}
		}
		return;
	}
}

FVaelLoadedEffects VaelEffects::Load(EVaelElement Element, const FVaelSpellEffects* Override)
{
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	const FVaelSpellEffects* ForElement = MagicSettings->ElementEffects.Find(Element);
	const FVaelSpellEffects& Default = MagicSettings->DefaultEffects;

	FVaelLoadedEffects Loaded;
	Loaded.Cast = LoadFirst(Override ? &Override->CastEffect : nullptr, ForElement ? &ForElement->CastEffect : nullptr, Default.CastEffect);
	Loaded.Trail = LoadFirst(Override ? &Override->TrailEffect : nullptr, ForElement ? &ForElement->TrailEffect : nullptr, Default.TrailEffect);
	Loaded.Impact = LoadFirst(Override ? &Override->ImpactEffect : nullptr, ForElement ? &ForElement->ImpactEffect : nullptr, Default.ImpactEffect);
	Loaded.Ground = LoadFirst(Override ? &Override->GroundEffect : nullptr, ForElement ? &ForElement->GroundEffect : nullptr, Default.GroundEffect);
	Loaded.CastSound = LoadFirst(Override ? &Override->CastSound : nullptr, ForElement ? &ForElement->CastSound : nullptr, Default.CastSound);
	Loaded.ImpactSound = LoadFirst(Override ? &Override->ImpactSound : nullptr, ForElement ? &ForElement->ImpactSound : nullptr, Default.ImpactSound);
	Loaded.Color = MagicSettings->GetElementColor(Element);

	// The spacing belongs to the ground effect that was found
	for (const FVaelSpellEffects* Source : { Override, ForElement, &Default })
	{
		if (Source != nullptr && Loaded.Ground != nullptr && Source->GroundEffect.ToSoftObjectPath() == FSoftObjectPath(Loaded.Ground))
		{
			Loaded.GroundSpacing = Source->GroundSpacing;
			Loaded.GroundScale = Source->GroundScale;
			break;
		}
	}

	return Loaded;
}

UNiagaraComponent* VaelEffects::SpawnAt(const UObject* WorldContext, UNiagaraSystem* System, const FVector& Location, const FRotator& Rotation, const FLinearColor& Color, float Radius)
{
	if (System == nullptr || WorldContext == nullptr)
	{
		return nullptr;
	}

	UNiagaraComponent* Component = UNiagaraFunctionLibrary::SpawnSystemAtLocation(WorldContext, System, Location, Rotation, FVector::OneVector, true, false);
	if (Component != nullptr)
	{
		SetCommonParameters(Component, Color, Radius);
		Component->Activate();

		// A one-shot effect must end even if it was built looping in the editor: it stops emitting, its particles
		// live out and the component destroys itself; whatever is still there long after that is removed
		if (UWorld* World = Component->GetWorld())
		{
			const TWeakObjectPtr<UNiagaraComponent> WeakComponent(Component);
			FTimerHandle StopHandle;
			World->GetTimerManager().SetTimer(StopHandle, FTimerDelegate::CreateLambda([WeakComponent]()
			{
				if (WeakComponent.IsValid())
				{
					WeakComponent->Deactivate();
				}
			}), OneShotEmitSeconds, false);

			FTimerHandle RemoveHandle;
			World->GetTimerManager().SetTimer(RemoveHandle, FTimerDelegate::CreateLambda([WeakComponent]()
			{
				if (WeakComponent.IsValid())
				{
					WeakComponent->DestroyComponent();
				}
			}), OneShotRemoveSeconds, false);
		}
	}

	return Component;
}

UNiagaraComponent* VaelEffects::Attach(UNiagaraSystem* System, USceneComponent* Parent, FName SocketName, const FLinearColor& Color, float Radius, bool bAutoDestroy)
{
	if (System == nullptr || Parent == nullptr)
	{
		return nullptr;
	}

	UNiagaraComponent* Component = UNiagaraFunctionLibrary::SpawnSystemAttached(System, Parent, SocketName, FVector::ZeroVector, FRotator::ZeroRotator,
		EAttachLocation::SnapToTarget, bAutoDestroy, false);
	if (Component != nullptr)
	{
		// Placeholder meshes are often scaled; the effect keeps its own size and gets the radius instead
		Component->SetUsingAbsoluteScale(true);
		Component->SetWorldScale3D(FVector::OneVector);

		SetCommonParameters(Component, Color, Radius);
		Component->Activate();
	}

	return Component;
}

void VaelEffects::PlaySound(const UObject* WorldContext, USoundBase* Sound, const FVector& Location)
{
	if (Sound != nullptr && WorldContext != nullptr)
	{
		UGameplayStatics::PlaySoundAtLocation(WorldContext, Sound, Location);
	}
}

void VaelEffects::PlayImpact(const UObject* WorldContext, const FVaelLoadedEffects& Effects, const FVector& Location, float Radius)
{
	SpawnAt(WorldContext, Effects.Impact, Location, FRotator::ZeroRotator, Effects.Color, Radius);
	PlaySound(WorldContext, Effects.ImpactSound, Location);
}

const FName VaelEffects::LookColorParameter(TEXT("Color"));
const FName VaelEffects::LookGlowParameter(TEXT("Glow"));
const FName VaelEffects::LookRimParameter(TEXT("Rim"));

void VaelEffects::LoadLookMaterials(UMaterialInterface*& OutCore, UMaterialInterface*& OutGlow)
{
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	UMaterialInterface* Plain = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	OutCore = VaelAssets::LoadOptional(MagicSettings->ElementOrbCoreMaterial);
	OutGlow = VaelAssets::LoadOptional(MagicSettings->ElementOrbGlowMaterial);
	OutCore = OutCore != nullptr ? OutCore : Plain;
	OutGlow = OutGlow != nullptr ? OutGlow : Plain;
}

UStaticMeshComponent* VaelEffects::AddLookShape(AActor* Owner, UMaterialInterface* Material, const FLinearColor& Color, float Glow, float Rim)
{
	static TWeakObjectPtr<UStaticMesh> SphereMesh;
	if (!SphereMesh.IsValid())
	{
		SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	}

	UStaticMeshComponent* Shape = NewObject<UStaticMeshComponent>(Owner);
	Shape->SetupAttachment(Owner->GetRootComponent());
	Shape->SetStaticMesh(SphereMesh.Get());
	Shape->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Shape->SetCastShadow(false);
	Shape->bReceivesDecals = false;
	Shape->SetVisibility(false);
	Shape->RegisterComponent();

	Shape->SetMaterial(0, Material);

	if (UMaterialInstanceDynamic* Dynamic = Shape->CreateAndSetMaterialInstanceDynamic(0))
	{
		Dynamic->SetVectorParameterValue(LookColorParameter, Color);
		Dynamic->SetScalarParameterValue(LookGlowParameter, Glow);
		Dynamic->SetScalarParameterValue(LookRimParameter, Rim);
	}

	return Shape;
}

void VaelEffects::SetLookGlow(UStaticMeshComponent* Shape, float Glow)
{
	if (UMaterialInstanceDynamic* Dynamic = Cast<UMaterialInstanceDynamic>(Shape->GetMaterial(0)))
	{
		Dynamic->SetScalarParameterValue(LookGlowParameter, Glow);
	}
}
