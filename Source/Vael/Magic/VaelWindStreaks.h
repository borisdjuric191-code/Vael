// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "VaelWindStreaks.generated.h"

/**
 *  Thin bright bands of wind (M_Vael_Windschliere). The owner moves the head of each streak; the band is drawn along the
 *  path the head took, turned towards the camera, frayed and fading towards its tail. Commit once a frame.
 *  Works in world space.
 */
UCLASS()
class UVaelWindStreaksComponent : public UProceduralMeshComponent
{
	GENERATED_BODY()

public:

	UVaelWindStreaksComponent(const FObjectInitializer& ObjectInitializer);

	/** Puts the bands' material on and stays where it is in the world */
	void Init(UMaterialInterface* Material);

	/** Starts a streak at a world place; width of the band and the longest trail in cm. Returns its index */
	int32 AddStreak(const FVector& Head, float Width, float MaxTrail);

	/** Moves its head on; Strength fades the whole band (0 hides it) */
	void MoveHead(int32 Index, const FVector& Head, float Strength);

	/** Lays the whole trail at once, head first, for streaks on a known path that would turn angular when followed frame by frame */
	void SetTrail(int32 Index, TArray<FVector>&& Points, float Strength);

	/** Starts a streak over at a new place, without a band to the old one */
	void Restart(int32 Index, const FVector& Head);

	/** Builds the bands */
	void Commit();

private:

	struct FStreak
	{
		TArray<FVector> Trail;
		float Width = 6.0f;
		float MaxTrail = 150.0f;
		float Strength = 1.0f;
	};

	TArray<FStreak> Streaks;
};
