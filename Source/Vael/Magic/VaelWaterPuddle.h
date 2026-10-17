// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelWaterPuddle.generated.h"

class UMaterialInstanceDynamic;
class UStaticMesh;
class UStaticMeshComponent;
class UVaelElementSource;

/**
 *  Water left on the ground where balls of water burst. Every splash adds a flat blob of the water's own material;
 *  splashes close to each other flow together into one puddle. A puddle slowly dries out.
 *  Only once enough water has gathered does it count as water of the environment: mages standing in it draw water,
 *  and it wets the enemies of whoever made it while they stand in it.
 */
UCLASS()
class AVaelWaterPuddle : public AActor
{
	GENERATED_BODY()

private:

	/** Mesh of a blob: the engine sphere, pressed flat */
	UPROPERTY()
	TObjectPtr<UStaticMesh> BlobMesh;

	/** One flat blob per splash that didn't land on an existing one */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Blobs;

	/** Water in every blob, in User.WaterAmount units: 1 is one Wassergeschoss */
	TArray<float> BlobWater;

	/** Whose water this is */
	UPROPERTY(Transient)
	TObjectPtr<UVaelElementSource> Source;

	/** Material of the source, tuned to lie flat on the ground */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;

public:

	/** Constructor */
	AVaelWaterPuddle();

	/** Dries the puddle and wets those standing in it */
	virtual void Tick(float DeltaSeconds) override;

	/**
	 *  Lets water fall to the ground below a location: joins a puddle of the same water and side close by, or starts a new one.
	 *  The instigator decides whose enemies the puddle wets. Returns the puddle, or null if there is no ground below.
	 */
	static AVaelWaterPuddle* AddSplash(UWorld* World, const FVector& Location, float Amount, UVaelElementSource* InSource, APawn* InInstigator);

	/** True if a puddle that holds enough water to count lies under the location */
	static bool IsDeepWaterAt(const UWorld* World, const FVector& Location);

	/** Water in the whole puddle, in User.WaterAmount units */
	float GetWater() const;

	/** True if enough water has gathered for the puddle to count as water of the environment */
	bool IsDeepEnough() const;

	/** True if the location is on one of the blobs */
	bool Covers(const FVector& Location, float ExtraDistance = 0.0f) const;

private:

	/** Pours water onto the puddle: onto the blob it lands on, or as a new blob next to the others */
	void AddWater(const FVector& GroundLocation, float Amount);

	/** Radius in cm of a blob holding the given water */
	static float GetBlobRadius(float Water);

	/** Sizes a blob after its water */
	void RefreshBlob(int32 Index);

	/** Seconds since the last wetting step */
	float WetStepTime = 0.0f;
};
