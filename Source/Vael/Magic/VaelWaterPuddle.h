// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelWaterPuddle.generated.h"

class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UTexture2D;
class UVaelElementSource;

/**
 *  Water left on the ground where balls of water burst. The water arrives with the falling drops, a little later and spread around;
 *  it runs down off ledges and rocks and spreads around obstacles instead of through them. Water landing on or beside a puddle
 *  makes it bigger, blobs that grow into each other flow together. A puddle slowly dries out.
 *  Only once enough water has gathered does it count as water of the environment: mages standing in it draw water,
 *  and it wets the enemies of whoever made it while they stand in it.
 */
UCLASS()
class AVaelWaterPuddle : public AActor
{
	GENERATED_BODY()

private:

	/** One flat surface of water over the whole puddle; its outline is painted into the mask */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Surface;

	/** Outline of the puddle seen from above: the blobs flow into each other like drops of mercury, 0.5 is the edge */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> Mask;

	/** Middle of every blob on the ground; the blobs are where the water lies, the surface joins them into one */
	TArray<FVector> BlobCenters;

	/** Water in every blob, in User.WaterAmount units: 1 is one Wassergeschoss */
	TArray<float> BlobWater;

	/** Radius every blob shows right now; it follows its water smoothly */
	TArray<float> BlobShownRadius;

	/** Whose water this is */
	UPROPERTY(Transient)
	TObjectPtr<UVaelElementSource> Source;

	/** Material of the source, tuned to lie flat on the ground */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;

public:

	/** Constructor */
	AVaelWaterPuddle();

	/** Dries the puddle, lets the blobs grow and shrink and wets those standing in it */
	virtual void Tick(float DeltaSeconds) override;

	/**
	 *  Lets the water of a burst ball rain down around a location: a part straight below it, the rest spread around as drops,
	 *  each landing a moment later. The instigator decides whose enemies the puddles wet.
	 */
	static void PourSplash(UWorld* World, const FVector& Location, float Amount, UVaelElementSource* InSource, APawn* InInstigator);

	/**
	 *  Lets water fall to the ground below a location right now: joins the puddles of the same water and side close by, or starts a new one.
	 *  Returns the puddle, or null if there is no ground below.
	 */
	static AVaelWaterPuddle* AddSplash(UWorld* World, const FVector& Location, float Amount, UVaelElementSource* InSource, APawn* InInstigator);

	/** Draws all water lying within reach of a location out of every puddle, like a wave of water sucking it up; returns the water drawn */
	static float DrainAround(UWorld* World, const FVector& Location, float Reach);

	/** True if a puddle that holds enough water to count lies under the location */
	static bool IsDeepWaterAt(const UWorld* World, const FVector& Location);

	/** Water in the whole puddle, in User.WaterAmount units */
	float GetWater() const;

	/** True if enough water has gathered for the puddle to count as water of the environment */
	bool IsDeepEnough() const;

	/** True if the location is on one of the blobs */
	bool Covers(const FVector& Location, float ExtraDistance = 0.0f) const;

private:

	/** True if the puddle may take water of this source from this instigator */
	bool Accepts(const UVaelElementSource* InSource, const APawn* InInstigator) const;

	/** Takes over all blobs of another puddle, which is then gone */
	void Absorb(AVaelWaterPuddle* Other);

	/** Pours water onto the puddle: onto the blob it lands on or beside, or as a new blob */
	void AddWater(const FVector& GroundLocation, float Amount, int32 SpillDepth = 0);

	/** Adds a blob holding the given water */
	int32 AddBlob(const FVector& GroundLocation, float Water, float ShownRadius);

	/** Removes a blob */
	void RemoveBlob(int32 Index);

	/** Keeps a blob out of obstacles: water that has no room spills over to a free spot beside it */
	void FitBlob(int32 Index, int32 SpillDepth);

	/** Lets blobs that have grown into each other flow together where there is room for the joined blob */
	void MergeBlobs();

	/** Radius in cm of a blob holding the given water */
	static float GetBlobRadius(float Water);

	/** Water a blob of the given radius holds */
	static float GetBlobWater(float Radius);

	/** Places the surface over all blobs and paints their outline into the mask */
	void RefreshSurface();

	/** Makes the material of the surface from the source, and the mask it is cut out with */
	void CreateSurfaceMaterial();

	/** Seconds since the outline was last painted */
	float RepaintTime = 0.0f;

	/** Number of blobs */
	int32 GetNumBlobs() const { return BlobCenters.Num(); }

	/** True if the surface has to be painted again */
	bool bSurfaceDirty = true;

	/** Seconds since the last wetting step */
	float WetStepTime = 0.0f;
};
