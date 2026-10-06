// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelPickupOrb.generated.h"

class UStaticMeshComponent;

/** What an orb gives back */
UENUM(BlueprintType)
enum class EVaelOrbKind : uint8
{
	Health,
	Mana
};

/**
 *  A small orb dropped by fallen creatures that gives health or mana back to the first player who walks over it.
 *  Health orbs stay on the ground while every player close to them is at full health.
 *  Placeholder look: a small floating sphere, red for health and blue for mana.
 */
UCLASS()
class AVaelPickupOrb : public AActor
{
	GENERATED_BODY()

	/** Placeholder look */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Mesh;

protected:

	/** What the orb gives back */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Orb")
	EVaelOrbKind Kind = EVaelOrbKind::Health;

public:

	/** Constructor */
	AVaelPickupOrb();

	/** Drops an orb on the ground below a location */
	static AVaelPickupOrb* DropOrb(UWorld* World, const FVector& Location, EVaelOrbKind InKind);

	/** Initialization */
	virtual void BeginPlay() override;

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

private:

	/** Height of the orb above the ground at rest */
	float BaseHeight = 0.0f;
};
