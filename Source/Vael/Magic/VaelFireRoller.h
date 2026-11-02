// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelFireRoller.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;
class UVaelFlakesComponent;
class UVaelWindStreaksComponent;

/**
 *  The look of the Feuerwalze (Boris 2026-11-02): a drum of flame lying across the way rolls over the ground, its tongues
 *  licking outwards all round it and trailing back, short ones filling it round the axis. The wind drives it: streaks of wind push from
 *  behind, embers fly ahead, and it grows as it rolls. Behind it the ground keeps burning in a strip that hurts the enemies
 *  of the caster who walk into it. It follows the projectile of the spell, which does the hits, and collapses in a burst of
 *  sparks when that is gone.
 */
UCLASS()
class AVaelFireRoller : public AActor
{
	GENERATED_BODY()

	/** Light of the fire on the ground around it */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPointLightComponent> Light;

public:

	AVaelFireRoller();

	/** Rolls a drum this wide (cm) along with the projectile; the trail it leaves burns the enemies of the instigator */
	static AVaelFireRoller* Spawn(AActor* InCarrier, float InWidth, APawn* InInstigator);

	virtual void Tick(float DeltaSeconds) override;

private:

	/** One tongue of the drum: where round the drum and across it, how long, at which moment of its life */
	struct FTongue
	{
		float Around = 0.0f;
		float Across = 0.0f;
		float Depth = 0.55f;
		float Length = 1.0f;
		float Age = 0.0f;
		float Life = 0.5f;
		float Yaw = 0.0f;
		int32 Mesh = 0;
	};

	void Build();

	/** Places the tongues and the core round the axis */
	void UpdateDrum(float DeltaSeconds);

	/** Streaks of wind behind it, embers flung ahead */
	void UpdateWind(float DeltaSeconds);

	/** Lays the next rows of the burning trail */
	void LayTrail();

	/** The projectile is gone: sparks burst, the flames sink */
	void Collapse();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> TongueMeshes;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TongueMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UVaelWindStreaksComponent> Streaks;

	UPROPERTY(Transient)
	TObjectPtr<UVaelFlakesComponent> Embers;

	TWeakObjectPtr<AActor> Carrier;
	TArray<FTongue> Tongues;

	/** Where the axis of the drum lies, the way it rolls and across it */
	FVector Axis = FVector::ZeroVector;
	FVector Forward = FVector::ForwardVector;
	FVector LastCarrier = FVector::ZeroVector;
	float GroundZ = 0.0f;

	float Width = 350.0f;
	float Age = 0.0f;
	float Roll = 0.0f;
	float Rolled = 0.0f;
	float NextTrailRow = 0.0f;
	float EmberCredit = 0.0f;

	/** Seconds since the projectile was gone, below 0 while it rolls */
	float Collapsed = -1.0f;
};
