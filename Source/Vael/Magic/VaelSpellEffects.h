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
