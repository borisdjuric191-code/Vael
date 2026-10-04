// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/VaelCombatStatics.h"
#include "VaelSpellProjectile.generated.h"

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

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

protected:

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
};
