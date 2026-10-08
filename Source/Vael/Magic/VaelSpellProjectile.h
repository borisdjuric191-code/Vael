// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/VaelCombatStatics.h"
#include "Magic/VaelSpellEffects.h"
#include "VaelSpellProjectile.generated.h"

class UMaterialInterface;
class UNiagaraSystem;
class UPointLightComponent;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

/**
 *  A spell flying in a straight line. Hits the first enemy it touches and stops at walls.
 *  Placeholder look: a sphere in the color of its element.
 */
UCLASS()
class AVaelSpellProjectile : public AActor
{
	GENERATED_BODY()

private:

	/** Collision of the projectile */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> Collision;

	/** Placeholder look */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Moves the projectile */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UProjectileMovementComponent> Movement;

public:

	/** Constructor */
	AVaelSpellProjectile();

	/** Sets what the projectile does. Has to be called between deferred spawning and FinishSpawning. */
	void InitSpell(const FVaelSpellHit& InHit, float Speed, float Radius, float Lifetime, int32 Pierce, const FLinearColor& Color);

	/** Leaves a patch of the element on the ground where the projectile ends. Has to be called before FinishSpawning. */
	void SetImpactArea(EVaelElement Element, float Radius, float Lifetime, float DamagePerSecond, EVaelGroundEffect Effect = EVaelGroundEffect::None);

	/** Lets the projectile fly through walls, rocks and rock walls. Has to be called before FinishSpawning. */
	void SetPassesWalls();

	/** Gives the projectile its trail, impact effect and sound. Has to be called before FinishSpawning. */
	void SetEffects(const FVaelLoadedEffects& InEffects, UNiagaraSystem* InImpactAreaVisual = nullptr);

	/** Chooses the look of the projectile while it has no trail effect. Has to be called before FinishSpawning. */
	void SetLook(EVaelProjectileLook InLook) { Look = InLook; }

	/** Makes the projectile burst where its flight ends, hitting everyone around. Has to be called before FinishSpawning. */
	void SetExplosion(const FVaelSpellHit& InExplosionHit, float Radius);

	/** Spawns a projectile of the default class flying in a horizontal direction, for attacks of creatures. A fire radius above 0 leaves a fire where it ends. */
	static AVaelSpellProjectile* Launch(APawn* Attacker, const FVector& Location, const FVector& Direction, const FVaelSpellHit& InHit, float Speed, float Radius, float Lifetime, const FLinearColor& Color, float FireRadius = 0.0f, float FireLifetime = 0.0f, float FireDamagePerSecond = 0.0f);

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

	/** Ends the flight when the lifetime is over */
	virtual void LifeSpanExpired() override;

protected:

	/** Starts the trail */
	virtual void BeginPlay() override;

	/** Called when the projectile touches something it can pass through, like a character */
	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** Called when the projectile runs into a wall */
	UFUNCTION()
	void OnStopped(const FHitResult& ImpactResult);

	/** What the projectile does to its target */
	FVaelSpellHit Hit;

	/** Enemies the projectile has already hit */
	TSet<TWeakObjectPtr<AActor>> HitActors;

	/** Number of enemies the projectile can still fly through */
	int32 RemainingPierce = 0;

private:

	/** Destroys the projectile, bursts and leaves the impact area, if it has them */
	void EndFlight(bool bHitSomething);

	/** Hits everyone the caster may hurt within the explosion radius */
	void Explode();

	/** What the burst does to each target, only used while the explosion radius is above 0 */
	FVaelSpellHit ExplosionHit;

	/** Radius of the burst, 0 for none */
	float ExplosionRadius = 0.0f;

	/** Element of the patch left on the ground */
	EVaelElement ImpactElement = EVaelElement::Fire;

	/** Radius of the patch left on the ground, 0 for none */
	float ImpactRadius = 0.0f;

	/** Seconds the patch lasts */
	float ImpactLifetime = 0.0f;

	/** Damage per second of the patch to the enemies of the instigator */
	float ImpactDamagePerSecond = 0.0f;

	/** What the patch does to enemies standing in it */
	EVaelGroundEffect ImpactEffect = EVaelGroundEffect::None;

	/** Trail, impact effect and sound */
	UPROPERTY(Transient)
	FVaelLoadedEffects Effects;

	/** Own look of the patch left on the ground, null for the one from the magic settings */
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> ImpactAreaVisual;

	/** Builds the spark: a slim glowing streak with a tail, a light and embers waiting to be shed */
	void BuildSpark();

	/** Lets the spark flicker and shed its embers */
	void AnimateSpark(float DeltaSeconds);

	/** Adds a sphere to the spark. Embers keep their place in the world instead of flying along. */
	UStaticMeshComponent* AddSparkShape(UMaterialInterface* Material, const FLinearColor& ShapeColor, float Glow, bool bStaysBehind = false);

	/** Look while no trail effect exists */
	EVaelProjectileLook Look = EVaelProjectileLook::Sphere;

	/** Color of the element */
	FLinearColor SpellColor = FLinearColor::White;

	/** Shapes of the spark that fly along: core, halo and the pieces of its tail */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> SparkShapes;

	/** Embers of the spark; each one falls on its own once it has been shed */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Embers;

	/** Light of the spark */
	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> SparkLight;

	/** Speed of each ember and seconds since it was shed; an age below 0 means it waits */
	TArray<FVector> EmberVelocities;
	TArray<float> EmberAges;

	/** Seconds until the next ember is shed, and which one it will be */
	float NextEmberCountdown = 0.0f;
	int32 NextEmber = 0;
};
