// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Magic/VaelWaterOrb.h"
#include "VaelWaterBolt.generated.h"

class UMaterialInstanceDynamic;
class UNiagaraComponent;
class UStaticMesh;

/**
 *  A shot of water like a jet from a water gun: a round head with a tail that tapers off behind it, like a falling star of water.
 *  Points along its flight, its tail flickers, white spray foams on its rim. The teardrop shape is built in code;
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

	/** The teardrop mesh, shared by all bolts with the same tail */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> BoltMesh;

	/** Material of the source with more foam on the rim */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BoltMaterial;

public:

	/** Constructor */
	AVaelWaterBolt();

	/** Gives the bolt its shape and its effects */
	virtual void BeginPlay() override;

protected:

	/** Applies the source, then foams up the rim */
	virtual void ApplySource() override;

	/** Points along the flight and lets the tail flicker */
	virtual void UpdateMotion(float DeltaSeconds) override;

private:

	/** Builds the teardrop: a half ball of radius 50 at the front, a tail the given number of radii long behind it, pointing along X */
	static UStaticMesh* BuildBoltMesh(float TailLength);

	/** Where the bolt was last frame, to find its direction */
	FVector LastLocation = FVector::ZeroVector;

	/** True once the direction is known */
	bool bHasLastLocation = false;
};
