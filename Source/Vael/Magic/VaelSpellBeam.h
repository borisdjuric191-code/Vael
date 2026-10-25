// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/VaelCombatStatics.h"
#include "VaelSpellBeam.generated.h"

class UAbilitySystemComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class UStaticMeshComponent;

/** What a channeled beam does and how long it lasts */
struct FVaelBeamSettings
{
	/** Element, condition and knockback of each hit; its damage is replaced by the collected damage */
	FVaelSpellHit Hit;

	/** Damage per second to everyone in the beam */
	float DamagePerSecond = 0.0f;

	/** Damage collects on each enemy and is dealt once it reaches this much */
	float DamageStep = 8.0f;

	/** Seconds the beam lasts */
	float Duration = 1.0f;

	/** Reach in cm, walls stop it earlier */
	float Length = 1000.0f;

	/** Distance from the middle line at which enemies are still hit, plus half their radius, in cm */
	float HalfWidth = 75.0f;

	/** Seconds between two fires where the beam ends, 0 for none */
	float FireInterval = 0.0f;

	/** Radius of each fire in cm */
	float FireRadius = 0.0f;

	/** Seconds each fire burns */
	float FireLifetime = 0.0f;

	/** Damage per second of each fire to the enemies of the caster */
	float FireDamagePerSecond = 0.0f;

	/** Effect of the beam; gets the user parameter BeamEnd. Null for the placeholder cylinder. */
	UNiagaraSystem* Visual = nullptr;

	/** Fire of a pack streaming along the beam instead of the effect (Feuerstrahl), repeated along it with its wind along the beam */
	UNiagaraSystem* Jet = nullptr;

	/** True for a fire beam: its body is a cylinder of streaming fire (UVaelFireSettings::BeamCoreMaterial) */
	bool bFireCore = false;

	/** Placeholder color */
	FLinearColor Color = FLinearColor::White;
};

/**
 *  A beam the caster channels from their hands, like the fire beam.
 *  It follows the position and aim of the caster, burns every enemy along it and sets the ground on fire where it ends.
 *  While it lasts the caster is channeling: they walk slower and cast nothing else.
 */
UCLASS()
class AVaelSpellBeam : public AActor
{
	GENERATED_BODY()

	/** Placeholder look: a stretched cylinder */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Mesh;

public:

	/** Constructor */
	AVaelSpellBeam();

	/** Starts a beam from the caster. Returns nullptr if the caster can't channel right now. */
	static AVaelSpellBeam* StartBeam(APawn* Caster, const FVaelBeamSettings& InSettings);

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

protected:

	/** Marks the caster as channeling */
	virtual void BeginPlay() override;

	/** Lets the caster cast and walk normally again */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Hits everyone along the beam and lays the fires */
	void UpdateBeam(float DeltaSeconds);

	/** Stretches the placeholder from the hands of the caster to the end of the beam */
	void UpdateLook(const FVector& Start, const FVector& Direction, float Length);

	/** What the beam does */
	FVaelBeamSettings Settings;

	/** Ability system of the caster, which carries the channeling tag */
	TWeakObjectPtr<UAbilitySystemComponent> CasterAbilitySystem;

	/** Damage collected on each enemy that hasn't been dealt yet */
	FVaelDamageCollector CollectedDamage;

	/** The beam effect, null while the placeholder cylinder shows */
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> VisualComponent;

	/** The flames of the jet, from the hands outwards */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> JetFlames;

	/** Radius of the fire core cylinder, 0 while the placeholder shows */
	float CoreRadius = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<class UMaterialInstanceDynamic> CoreMaterialInstance;

	/** Fire beam: the wide glow around it and the white-hot core in it, the fire at its source and where it hits, its lights */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> OuterLayer;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> InnerLayer;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class UMaterialInstanceDynamic>> LayerMaterials;

	UPROPERTY(Transient)
	TObjectPtr<class UVaelFireComponent> SourceFlames;

	UPROPERTY(Transient)
	TObjectPtr<class UVaelFireComponent> ImpactFlames;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class UPointLightComponent>> BeamLights;

	float ImpactCountdown = 0.0f;

	/** World time at which the beam ends */
	float EndTime = 0.0f;

	/** Seconds until the next fire */
	float NextFireCountdown = 0.0f;
};
