// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelWeatherRain.generated.h"

class AVaelRegion;
class UMaterialInstanceDynamic;
class UProceduralMeshComponent;
class UVaelCloudCardsComponent;

/**
 *  The look of the ash rain (and the rain of a storm) around the players, made of the water of the water kit:
 *  thin streaks of water falling aslant in the wind, drops thrown up where they hit the ground, rings spreading on the
 *  ground and on puddles, now and then a crown splash of the water kit, and pale veils of rain drifting through it.
 *  Drops, splashes and rings are one mesh of the wave material (M_Water_Wave) built anew every frame.
 *  Spawned by the region while it rains and a player is inside; it follows the middle of the players.
 */
UCLASS()
class AVaelWeatherRain : public AActor
{
	GENERATED_BODY()

public:

	AVaelWeatherRain();

	/** Lets the rain of the region fall around its players */
	static AVaelWeatherRain* Spawn(AVaelRegion* InRegion);

	/** The same rain on one round patch for some seconds, out of a dark cloud above it (the Regenruf) */
	static AVaelWeatherRain* SpawnPatch(UWorld* World, const FVector& Center, float Radius, float Lifetime);

	/** A rain on a patch stops now: the last drops still fall, the cloud thins away */
	void StopPatch() { PatchLifetime = FMath::Min(PatchLifetime, PatchAge + 1.0f); }

	virtual void Tick(float DeltaSeconds) override;

private:

	struct FDrop
	{
		FVector Head = FVector::ZeroVector;
		float GroundZ = 0.0f;
		float Speed = 1000.0f;
		float Length = 60.0f;
		bool bOnPuddle = false;
		bool bWaiting = true;
	};

	struct FSplash
	{
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float Age = 0.0f;
		float Life = 0.3f;
		float Size = 4.0f;
	};

	struct FRing
	{
		FVector Center = FVector::ZeroVector;
		float Age = 0.0f;
		float Life = 0.6f;
		float Radius = 20.0f;
	};

	struct FVeil
	{
		FVector Place = FVector::ZeroVector;
		float Age = 0.0f;
		float Life = 6.0f;
		float Size = 600.0f;
	};

	void Build();

	/** Starts a drop anew at a random place above the ground around the players; bAnyHeight lets it start anywhere on its way down */
	void Restart(FDrop& Drop, const FVector& Middle, const FVector& Fall, bool bAnyHeight);

	/** A drop hit the ground: drops jump up, maybe a ring spreads */
	void Land(const FDrop& Drop);

	/** Middle of the players in the region; false if none is there */
	bool FindMiddle(FVector& OutMiddle) const;

	/** Builds the mesh of all drops, splashes and rings for this frame */
	void BuildMesh(const FVector& Fall, float Shown);

	/** How hard it rains right now (1 is ash rain), the wind blowing it, how far around the middle and from how high it falls */
	float GetStrength() const;
	FVector GetWind() const;
	float GetReach() const;
	float GetHeight() const;

	TWeakObjectPtr<AVaelRegion> Region;

	/** Rain on one patch instead of around the players: its middle on the ground, radius, seconds it rains and has rained */
	bool bPatch = false;
	FVector PatchCenter = FVector::ZeroVector;
	float PatchRadius = 500.0f;
	float PatchLifetime = 9.0f;
	float PatchAge = 0.0f;

	/** The dark cloud over a patch of rain, where its parts float and how big they are */
	UPROPERTY(Transient)
	TObjectPtr<UVaelCloudCardsComponent> Cloud;

	TArray<FVector> CloudPlaces;
	TArray<float> CloudSizes;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> Water;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> WaterMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UVaelCloudCardsComponent> Veils;

	TArray<FDrop> Drops;
	TArray<FSplash> Splashes;
	TArray<FRing> Rings;
	TArray<FVeil> VeilParts;

	/** Next splash and ring to reuse */
	int32 NextSplash = 0;
	int32 NextRing = 0;

	/** Water kit splashes still owed this frame */
	float KitSplashCredit = 0.0f;

	/** The mesh buffers, kept between frames */
	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;
};
