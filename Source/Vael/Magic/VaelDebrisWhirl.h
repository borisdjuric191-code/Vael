// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "VaelDebrisWhirl.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;

/** One kind of piece a whirl carries: grit, pebbles, twigs, each a mesh of a pack at its real size */
USTRUCT(BlueprintType)
struct FVaelWhirlPiece
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Whirl")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** Smallest and largest piece, its longest side in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Whirl", meta = (ClampMin = 0.1))
	float MinSize = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Whirl", meta = (ClampMin = 0.1))
	float MaxSize = 5.0f;

	/** Share of all pieces that are of this kind (the shares are weighed against each other) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Whirl", meta = (ClampMin = 0))
	float Share = 1.0f;
};

/**
 *  A whirl of real debris for sand storms and dust devils: many small pieces of the Megascans packs spiral up around
 *  the middle and drop back in, with puffs of dust at the bottom. A funnel widens upwards (Staubteufel),
 *  a storm is a wide low band around its middle (Sandsturm).
 */
UCLASS()
class UVaelDebrisWhirlComponent : public USceneComponent
{
	GENERATED_BODY()

public:

	UVaelDebrisWhirlComponent();

	/** Fills the whirl; Radius in cm at its widest, Color of its dust */
	void Build(float InRadius, bool bInFunnel, const FLinearColor& InDustColor);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:

	struct FPiece
	{
		int32 Kind = 0;
		int32 Instance = 0;
		float Angle = 0.0f;
		float AngularSpeed = 0.0f;
		float Height = 0.0f;
		float RiseSpeed = 0.0f;
		float Offset = 0.0f;
		float Scale = 1.0f;
		FRotator Tumble;
		FRotator TumbleSpeed;
	};

	/** Where a piece is at its height and angle */
	FVector PlaceOf(const FPiece& Piece) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Kinds;

	TArray<FPiece> Pieces;

	float Radius = 100.0f;
	float Height = 300.0f;
	bool bFunnel = false;
	FLinearColor DustColor = FLinearColor::Gray;
	float NextDustCountdown = 0.0f;
	float NextBodyCountdown = 0.0f;
	float Age = 0.0f;

	/** Bands of sand that race round and up the whirl, their trails drawing its spiral; each is a piece without a mesh */
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UNiagaraComponent>> Bands;

	TArray<FPiece> BandPaths;
};
