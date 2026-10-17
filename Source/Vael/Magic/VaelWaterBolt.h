// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Magic/VaelWaterOrb.h"
#include "VaelWaterBolt.generated.h"

class UMaterialInstanceDynamic;
class UNiagaraComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 *  A shot of water like a jet from a high-pressure pump: a slim jet with a round nose that tapers off behind it, like a falling star of water,
 *  and a white spiral of spray that winds around it and drills forward. Points along its flight, flickers, foams on its rim. The shapes are built in code;
 *  the source decides material, light and particle color like on the water orb. Optional Niagara: a head effect and a spray trail.
 */
UCLASS()
class AVaelWaterBolt : public AVaelWaterOrb
{
	GENERATED_BODY()

private:

	/** Spray left behind along the flight, in world space */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraComponent> Trail;

	/** White spiral of spray around the jet */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Spiral;

	/** The jet mesh, shared by all bolts of the same shape */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> BoltMesh;

	/** Material of the source with more foam on the rim */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BoltMaterial;

	/** Material of the source foamed up to white spray, for the spiral */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SpiralMaterial;

public:

	/** Constructor */
	AVaelWaterBolt();

	/** Gives the bolt its shape and its effects */
	virtual void BeginPlay() override;

protected:

	/** Applies the source, then foams up the rim and the spiral */
	virtual void ApplySource() override;

	/** Points along the flight, lets the jet flicker and turns the spiral */
	virtual void UpdateMotion(float DeltaSeconds) override;

private:

	/** Builds the jet pointing along X: a round nose of radius 50 × Thickness, a body the given number of ball radii (50) long that tapers to a point */
	static UStaticMesh* BuildBoltMesh(float TailLength, float Thickness);

	/** Builds the spiral of spray around a jet of the same shape, widening behind the nose */
	static UStaticMesh* BuildSpiralMesh(float TailLength, float Thickness, float Turns);

	/** Builds a tube mesh from rings of points with their outward normals; every ring has the same number of points */
	static UStaticMesh* BuildRingMesh(const TArray<TArray<FVector3f>>& RingPositions, const TArray<TArray<FVector3f>>& RingNormals);

	/** Where the bolt was last frame, to find its direction */
	FVector LastLocation = FVector::ZeroVector;

	/** True once the direction is known */
	bool bHasLastLocation = false;
};
