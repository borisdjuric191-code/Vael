// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelPoppyCapsule.generated.h"

class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;
class UVaelFireComponent;
class UVaelPlantArmData;
class AVaelGlimmDome;

/** What a seed capsule of the Funkenmohn does where it bursts */
enum class EVaelPoppyBurst : uint8
{
	/** Funkenmohn: a cluster of sparks, the ground keeps burning a while */
	Sparks,
	/** Schwarzmohn: a cloud of sleep pollen; enemies in it grow drowsy and then fall asleep until they are hurt */
	Sleep,
	/** Mohnfeuerwerk: it bursts into many small capsules that scatter and burst in turn */
	Firework,
	/** Zündschnur: a poppy sprouts from the ground where it lies and bursts (no flight) */
	Fuse,
	/** Funkenwirbel: a whirl of burning petals that draws enemies in and burns them */
	Whirl,
	/** Gewittermohn: it soaks everything around, then the sparks jump as lightning from one wet enemy to the next */
	Storm,
	/** Glutblase: a glass bubble full of sparks shuts the enemies inside in, then bursts */
	Bubble,
	/** Brandsalbe: a field of embers that burns enemies and slowly heals allies */
	Salve
};

/** How a capsule flies and bursts; the thrower works out the numbers */
struct FVaelPoppyThrow
{
	EVaelPoppyBurst Burst = EVaelPoppyBurst::Sparks;

	/** Size of the capsule in cm, radius and damage of the burst */
	float Size = 9.0f;
	float Radius = 130.0f;
	float Damage = 12.0f;

	/** Seconds it glows on the ground before it bursts */
	float Fuse = 0.5f;

	/** The burning patch left behind: seconds (0 = none) and damage per second */
	float PatchSeconds = 0.0f;
	float PatchDamagePerSecond = 0.0f;

	/** Capsules it splits into halfway (rooted), and small capsules a firework scatters */
	int32 Split = 0;
	int32 Scatter = 0;

	/** Seconds the cloud, the whirl, the bubble or the field stays */
	float Linger = 0.0f;

	/** Seconds in the cloud until an enemy falls asleep, and how long it sleeps */
	float Drowsy = 1.5f;
	float Sleep = 4.0f;

	/** Seconds before it leaves (salvos of a tower, the links of a Zündschnur) */
	float Delay = 0.0f;

	/** The plant it grew together with: the Glimmklee of a Glutblase */
	const UVaelPlantArmData* Partner = nullptr;
};

/**
 *  A seed capsule of the Funkenmohn (Boris 2026-10-26): thrown by the bud of the mortar hand, by a Mohnmörser tower or by a pair,
 *  it flies in a high arc to where it was aimed, tumbling with a tail of flames, lies glowing a moment and bursts.
 *  The burst is a cluster of sparks with a small burning patch (a fire source for the mage), or as Schwarzmohn a cloud of
 *  sleep pollen; the pairs of the Funkenmohn give it their own bursts (EVaelPoppyBurst).
 */
UCLASS()
class AVaelPoppyCapsule : public AActor
{
	GENERATED_BODY()

public:

	AVaelPoppyCapsule();

	/** Throws a capsule of the plant from the muzzle to the ground at the target point */
	static AVaelPoppyCapsule* Throw(APawn* Instigator, const UVaelPlantArmData* InPlant, const FVector& From, const FVector& To, const FVaelPoppyThrow& InThrow, bool bInShadow);

	/** Seconds a throw of this length is in the air */
	static float GetFlightTime(float Distance);

	virtual void Tick(float DeltaSeconds) override;

private:

	enum class EPhase : uint8 { Waiting, Flying, Landed, Lingering };

	/** Builds the capsule, its light and its tail of flames */
	void BuildLook();

	/** Breaks into the capsules of a rooted throw, each to its own spot around the goal */
	void SplitUp();

	/** Bursts as its kind says; returns true if it stays to linger */
	bool Burst();

	/** The cluster of sparks with damage and burning around the location */
	void SparkBurst(const FVector& Location, float InRadius, float InDamage, bool bBig);

	/** While it lingers: the sleep cloud, the whirl, the bubble, the field */
	void Linger(float DeltaSeconds);

	/** Lightning from one wet enemy to the next */
	void ChainLightning();

	/** Ends it: puts its light and flames out and goes */
	void Finish();

	FVector GroundAt(const FVector& Location) const;

	UPROPERTY(Transient)
	TObjectPtr<UVaelPlantArmData> Plant;

	UPROPERTY(Transient)
	TObjectPtr<UVaelPlantArmData> PartnerPlant;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Capsule;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY(Transient)
	TObjectPtr<UVaelFireComponent> Trail;

	/** The flames of the whirl and its petals */
	UPROPERTY(Transient)
	TObjectPtr<UVaelFireComponent> Column;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> WhirlPetals;

	TWeakObjectPtr<AVaelGlimmDome> Bubble;

	FVaelPoppyThrow Spec;
	bool bShadow = false;
	EPhase Phase = EPhase::Waiting;

	FVector Start = FVector::ZeroVector;
	FVector Goal = FVector::ZeroVector;
	float FlightTime = 1.0f;
	float Apex = 100.0f;
	float PhaseTime = 0.0f;
	bool bSplit = false;

	/** Linger: seconds left, countdown to the next pulse, and how long each enemy has breathed the pollen */
	float LingerLeft = 0.0f;
	float PulseCountdown = 0.0f;
	TMap<TWeakObjectPtr<AActor>, float> Breathed;
	bool bLightningDone = false;
};
