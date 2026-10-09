// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/VaelCombatStatics.h"
#include "Magic/VaelSpellEffects.h"
#include "VaelSpellProjectile.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UNiagaraSystem;
class UPointLightComponent;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 *  A spell flying in a straight line. Hits the first enemy it touches and stops at walls.
 *  Placeholder look: a sphere in the color of its element, or the look its formula picks (EVaelProjectileLook).
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

	/** Chooses the look of the projectile; it plays together with a trail effect. Has to be called before FinishSpawning. */
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

	/** Builds the look of the projectile from glowing spheres: the spark or the ball of water */
	void BuildLook();

	/** Adds a sphere to the look. Loose pieces are shed later and then move on their own. */
	UStaticMeshComponent* AddLookShape(UMaterialInterface* Material, const FLinearColor& ShapeColor, float Glow, float Rim = 0.0f, bool bLoose = false, UStaticMesh* OwnMesh = nullptr);

	/** Adds the light of the look */
	void AddLookLight(const FLinearColor& LightColor, float Intensity);

	/** Moves the look: the flying shapes, the loose pieces, and what plays out after the flight */
	void AnimateLook(float DeltaSeconds);
	void AnimateSpark(float Time, float DeltaSeconds);
	void AnimateWaterOrb(float Time);
	void AnimateWaterBurst();
	void AnimateRock(float Time, float DeltaSeconds);
	void AnimateIceLance(float Time, float DeltaSeconds);
	void AnimateLavaBall(float Time, float DeltaSeconds);
	void AnimateLavaBurst();
	void AnimateMarkShard(float Time, float DeltaSeconds);

	/** Adds a cone to the look, pointed like a crystal */
	UStaticMeshComponent* AddLookCone(UMaterialInterface* Material, const FLinearColor& ShapeColor, float Glow, float Rim = 0.0f, bool bLoose = false);

	/** Lays a cone of the look from its base towards its point, in the space of the projectile */
	static void PlaceCone(UStaticMeshComponent* Cone, const FVector& Base, const FVector& Direction, float Length, float Width);

	/** Throws a few pieces off where the projectile flies through an enemy */
	void ShedOnPierce(const FVector& Location);
	/** Lets go of the next loose piece at a place with a speed */
	void ShedPiece(const FVector& Location, const FVector& Velocity);

	/** Ends the flight but keeps the projectile for a moment: embers glow out, water bursts */
	void BeginAfterglow(bool bHitSomething);

	/** Own look of the projectile, shown together with the trail effect */
	EVaelProjectileLook Look = EVaelProjectileLook::Sphere;

	/** Color of the element */
	FLinearColor SpellColor = FLinearColor::White;

	/** Shapes of the look that fly along, in the order the look builds them */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> LookShapes;

	/** Embers or drops; each one moves on its own once it has been shed */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> LoosePieces;

	/** Light of the look */
	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> LookLight;

	/** Engine cone for the crystals of the look */
	UPROPERTY()
	TObjectPtr<UStaticMesh> ConeMesh;

	/** Material of the ring of spray, to let it fade */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BurstMaterial;

	/** Speed of each loose piece and seconds since it was shed; an age below 0 means it waits */
	TArray<FVector> LooseVelocities;
	TArray<float> LooseAges;

	/** How the loose pieces of this look behave: size as scale of the engine sphere, seconds they last, fall in cm/s², slowing by the air, flicker */
	float LooseSize = 0.05f;
	float LooseLifetime = 0.5f;
	float LooseGravity = 500.0f;
	float LooseDrag = 2.0f;
	bool bLooseFlicker = false;

	/** True if the loose pieces tumble, like pebbles */
	bool bLooseTumble = false;

	/** Size of each loose piece where they differ, like pebbles among sand; empty if all share LooseSize */
	TArray<float> LoosePieceSizes;

	/** True if the stone is made of the rock mesh, false while spheres stand in for it */
	bool bRealRock = false;

	/** Seconds until the next piece is shed in flight, and which one it will be */
	float NextShedCountdown = 0.0f;
	int32 NextLoosePiece = 0;

	/** Brightness of the light before flicker and flash, in candela */
	float LookLightIntensity = 0.0f;

	/** True once the flight is over and only the afterglow plays, and seconds since then */
	bool bFlightEnded = false;
	float AfterglowTime = 0.0f;

	/** True if the ball of water burst on something instead of running out in the air */
	bool bBurst = false;
};
