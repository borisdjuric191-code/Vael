// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "VaelFlakes.generated.h"

class UInstancedStaticMeshComponent;

/** Kinds of flakes; each falls, flutters and shows in its own way */
enum class EVaelFlake : uint8
{
	/** White, slow, sways wide */
	Snow,
	/** Black and large, heavy flutter */
	Soot,
	/** Small, glowing orange, falls fast and cools to ash on the way */
	Ember,
	/** Grey, a cooled ember */
	Ash
};

/**
 *  Flakes that really tumble: each is a thin disc that tilts, turns and sails sideways while it sinks, drifts a little with
 *  the wind and comes to lie flat on the ground, where it stays until the flakes fade. Embers cool to ash as they fall; one
 *  that is still hot when it lands is reported, so it can light a small fire. Shared by the Ascheregen, the Lawine and,
 *  later, snow weather. All flakes of a kind are instances of one mesh in M_Vael_Hageleis (unlit, made for instances).
 */
UCLASS()
class UVaelFlakesComponent : public USceneComponent
{
	GENERATED_BODY()

public:

	UVaelFlakesComponent();

	/** Lets a flake go at a world location with a starting velocity; it lands on the ground height GroundZ */
	void Add(EVaelFlake Kind, const FVector& Location, const FVector& Velocity, float Size, float GroundZ);

	/** Scales every flake, in the air and on the ground (1 shown, 0 gone); for fading out at the end */
	void SetFade(float InFade) { Fade = InFade; }

	/** Flakes still falling */
	int32 NumFalling() const;

	/** Called with the ground location where an ember lands while still hot */
	TFunction<void(const FVector&)> OnHotLanding;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:

	struct FFlake
	{
		EVaelFlake Kind = EVaelFlake::Snow;
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		FVector SpinAxis = FVector::UpVector;
		float Spin = 0.0f;
		float Phase = 0.0f;
		float Size = 4.0f;
		float GroundZ = 0.0f;
		/** Embers: 1 hot, 0 cooled; seconds it takes to cool */
		float Heat = 0.0f;
		float CoolTime = 1.0f;
		/** Seconds on the ground, below 0 while falling */
		float Landed = -1.0f;
		float LandYaw = 0.0f;
	};

	/** Creates the instanced mesh of each kind */
	void Build();

	/** Moves one falling flake, lands it and cools it */
	void Fall(FFlake& Flake, float DeltaTime, const FVector& Wind);

	/** Where a flake shows this frame */
	FTransform Place(const FFlake& Flake) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Meshes;

	TArray<FFlake> Flakes;

	/** Transforms of every kind this frame */
	TArray<TArray<FTransform>> Frames;

	float Fade = 1.0f;
	float Age = 0.0f;
};
