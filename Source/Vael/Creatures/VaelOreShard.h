// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelOreShard.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

/** What an ore shard shows where it lands (the ore pairs of the Erztöchter) */
enum class EVaelShardImpact : uint8
{
	/** Crumbs only */
	Plain,
	/** Schwefelkies fumes that Funkenerz sparks set alight */
	SulphurSparks,
	/** Rust dust with glittering salt */
	RustSalt,
	/** Crackling sparks of lightning (Gruenspan) */
	Lightning,
	/** Beads of quicksilver rolling out (Blutzinnober) */
	Quicksilver,
	/** Rib splinters (Rippenspat) */
	Ribs
};

/**
 *  A real crystal of an ore thrown in an arc (the Splitterschlag of an Erztochter): it spins in flight and crumbles into
 *  small pieces of its ore where it lands, with the look of its pair. Look only; the hit itself is a ground strike
 *  that lands at the same moment.
 */
UCLASS()
class AVaelOreShard : public AActor
{
	GENERATED_BODY()

public:

	AVaelOreShard();

	/** Throws a crystal from Start to End, arriving after FlightTime, the arc this high; Size = its length in cm */
	static AVaelOreShard* Throw(UWorld* World, const FVector& Start, const FVector& End, float FlightTime, float Apex,
		UStaticMesh* Mesh, float Size, EVaelShardImpact Impact, const FLinearColor& Color);

	virtual void Tick(float DeltaSeconds) override;

	/** The look of an impact on its own (also used for crumbling crystals) */
	static void ShowImpact(UWorld* World, const FVector& Location, UStaticMesh* Mesh, float Size, EVaelShardImpact Impact, const FLinearColor& Color);

private:

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Crystal;

	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	float FlightTime = 0.5f;
	float Apex = 100.0f;
	float Age = 0.0f;
	float Size = 20.0f;
	FRotator Spin = FRotator::ZeroRotator;
	EVaelShardImpact Impact = EVaelShardImpact::Plain;
	FLinearColor Color = FLinearColor::White;
};
