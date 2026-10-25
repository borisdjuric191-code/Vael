// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Magic/VaelElementTypes.h"
#include "VaelSpellEffects.generated.h"

class AActor;
class UMaterialInterface;
class UNiagaraComponent;
class UNiagaraSystem;
class USceneComponent;
class USoundBase;
class UStaticMeshComponent;

/**
 *  How a Niagara system of a bought pack is fitted to Vael: it doesn't know our parameters Color and Radius, so this says
 *  which of its own parameters get the color, what it is fixed to, which of its emitters stay off and how it is sized.
 *  Applied to the system wherever it is spawned, whichever slot it sits in.
 */
USTRUCT(BlueprintType)
struct FVaelPackTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pack")
	TSoftObjectPtr<UNiagaraSystem> System;

	/** Color parameters of the system that get the color of the effect (the element, steam, dust …), times ColorScale */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pack")
	TArray<FName> ColorParameters;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pack", meta = (ClampMin = 0))
	float ColorScale = 1.0f;

	/** Fixed values of its own parameters */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pack")
	TMap<FName, float> Floats;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pack")
	TMap<FName, FLinearColor> Colors;

	/** Emitters that don't fit, like the fire flash of an earth explosion */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pack")
	TArray<FName> EmittersOff;

	/** The radius in cm the system has unscaled; it is scaled by Radius / this. 0: keeps its size */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pack", meta = (ClampMin = 0))
	float ReferenceRadius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pack")
	FVector2D ScaleRange = FVector2D(0.25f, 4.0f);

	/** Float parameter that gets the scale instead of the component, for systems with their own size parameter */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pack")
	FName ScaleParameter;
};

/**
 *  Effects and sounds of a spell, made in the editor. Every entry is optional.
 *  Effects get the user parameters Color (the color of the element) and Radius (the reach of the spell in cm) if they have them.
 */
USTRUCT(BlueprintType)
struct FVaelSpellEffects
{
	GENERATED_BODY()

	/** Flash at the hand at the moment the spell is released, played once; cones stretch it along their reach as their spray */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects")
	TSoftObjectPtr<UNiagaraSystem> CastEffect;

	/** Look of the spell itself, lasting: follows projectiles, whirlwinds and storms */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects")
	TSoftObjectPtr<UNiagaraSystem> TrailEffect;

	/** Where the spell hits: on every target, and where explosions, bursts and walls happen */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects")
	TSoftObjectPtr<UNiagaraSystem> ImpactEffect;

	/** Look of the patches the spell leaves on the ground, like burning ground */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects")
	TSoftObjectPtr<UNiagaraSystem> GroundEffect;

	/** For a ground effect of fixed size that doesn't know Radius: it is repeated all over the patch, one every this many cm. 0: one in the middle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects", meta = (ClampMin = 0))
	float GroundSpacing = 0.0f;

	/** Size of each of the repeated ground effects */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects", meta = (ClampMin = 0.1))
	float GroundScale = 1.0f;

	/** Sound when the spell is released */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects")
	TSoftObjectPtr<USoundBase> CastSound;

	/** Sound where the spell hits */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects")
	TSoftObjectPtr<USoundBase> ImpactSound;
};

/** The effects of a spell once loaded; empty entries don't exist yet */
USTRUCT()
struct FVaelLoadedEffects
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> Cast;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> Trail;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> Impact;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> Ground;

	/** How the ground effect is repeated over a patch, from where the effect itself comes from */
	float GroundSpacing = 0.0f;
	float GroundScale = 1.0f;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> CastSound;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> ImpactSound;

	/** Color the effects are tinted with */
	UPROPERTY(Transient)
	FLinearColor Color = FLinearColor::White;
};

/** Spawning of spell effects and sounds */
namespace VaelEffects
{
	/** User parameters the code sets on every effect */
	extern const FName ColorParameter;
	extern const FName RadiusParameter;
	extern const FName BeamEndParameter;
	extern const FName IntensityParameter;

	/** Loads the effects of a spell of the element: each entry from the override if set there, otherwise from the magic settings */
	FVaelLoadedEffects Load(EVaelElement Element, const FVaelSpellEffects* Override = nullptr);

	/** Plays an effect once at a location. Returns null if there is no effect. */
	UNiagaraComponent* SpawnAt(const UObject* WorldContext, UNiagaraSystem* System, const FVector& Location, const FRotator& Rotation, const FLinearColor& Color, float Radius);

	/** Attaches a lasting effect to a component, for example a trail to a projectile. Without auto destroy it can be switched off and on again. Returns null if there is no effect. */
	UNiagaraComponent* Attach(UNiagaraSystem* System, USceneComponent* Parent, FName SocketName, const FLinearColor& Color, float Radius, bool bAutoDestroy = true);

	/** Fits a spawned effect from a pack to Vael if the magic settings have a tuning for its system (call before Activate) */
	void ApplyPackTuning(UNiagaraComponent* Component, const FLinearColor& Color, float Radius);

	/** Plays a sound once at a location, if there is one */
	void PlaySound(const UObject* WorldContext, USoundBase* Sound, const FVector& Location);

	/** Plays the impact effect and sound of a spell at a location */
	void PlayImpact(const UObject* WorldContext, const FVaelLoadedEffects& Effects, const FVector& Location, float Radius);

	/** Material parameters of the glowing look materials */
	extern const FName LookColorParameter;
	extern const FName LookGlowParameter;
	extern const FName LookRimParameter;

	/** The materials of the looks built in code: the solid core and the additive glow, the plain engine material while they don't exist */
	void LoadLookMaterials(UMaterialInterface*& OutCore, UMaterialInterface*& OutGlow);

	/** Adds a hidden engine sphere (100 cm across) to an actor's root, in a look material with its color, glow and rim, without collision or shadow */
	UStaticMeshComponent* AddLookShape(AActor* Owner, UMaterialInterface* Material, const FLinearColor& Color, float Glow, float Rim = 0.0f);

	/** Sets the glow of a shape made by AddLookShape */
	void SetLookGlow(UStaticMeshComponent* Shape, float Glow);
}
