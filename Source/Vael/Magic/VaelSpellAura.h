// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/VaelCombatStatics.h"
#include "VaelSpellAura.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class UStaticMeshComponent;

/** What a storm around the caster does and how long it lasts */
struct FVaelAuraSettings
{
	/** Element, condition and knockback of each hit; its damage is replaced by the collected damage */
	FVaelSpellHit Hit;

	/** Damage per second to everyone inside */
	float DamagePerSecond = 0.0f;

	/** Damage collects on each enemy and is dealt once it reaches this much */
	float DamageStep = 6.0f;

	/** Radius around the caster in cm, plus the radius of the enemy */
	float Radius = 420.0f;

	/** Seconds the storm lasts */
	float Duration = 4.2f;

	/** Enemies inside can't see for this long, renewed as long as they stay inside */
	float BlindDuration = 0.4f;

	/** Look of the spell; gets the user parameters Color and Radius. Null for the placeholder. */
	UNiagaraSystem* Visual = nullptr;

	/** Placeholder color */
	FLinearColor Color = FLinearColor::White;
};

/**
 *  A storm that whirls around the caster for some seconds, like the Sandsturm.
 *  It moves with the caster, grinds everyone close by, pushes them back a little and blinds them.
 *  The caster can walk and cast freely meanwhile. Casting it again renews the storm.
 */
UCLASS()
class AVaelSpellAura : public AActor
{
	GENERATED_BODY()

	/** Placeholder look: a flat disc of sand on the ground */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Mesh;

public:

	/** Constructor */
	AVaelSpellAura();

	/** Starts a storm around the caster, or renews the one they already have */
	static AVaelSpellAura* StartAura(APawn* Caster, const FVaelAuraSettings& InSettings);

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

protected:

	/** Starts the duration and sizes the placeholder */
	virtual void BeginPlay() override;

private:

	/** Takes over the settings and starts the duration anew */
	void Restart(const FVaelAuraSettings& InSettings);

	/** Hits and blinds everyone close to the caster */
	void HitEnemies(float DeltaSeconds);

	/** What the storm does */
	FVaelAuraSettings Settings;

	/** Damage collected on each enemy that hasn't been dealt yet */
	FVaelDamageCollector CollectedDamage;

	/** The effect of the storm, null while the placeholder shows */
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> VisualComponent;

	/** World time at which the storm ends */
	float EndTime = 0.0f;
};
