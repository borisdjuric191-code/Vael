// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelCaveDrips.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Water dripping from the ceiling of a cave (Boris 2026-11-27: "tropfende Animationen"). One actor holds many drip points (the tips of
 * stalactites and slime icicles, set by Scripts/BuildHoehle.py); only those near a player drip. A drop swells at the tip, falls,
 * and bursts into a few droplets on the floor or the water below. All drops and droplets are instances of one sphere in the unlit
 * hail material, moved on the game thread: no Niagara asset, a handful of instances, ticked a few times a second when nobody is near.
 */
UCLASS(meta = (DisplayName = "Vael Cave Drips"))
class AVaelCaveDrips : public AActor
{
	GENERATED_BODY()

public:
	AVaelCaveDrips();

	/** Where the drops hang (world space; X, Y, Z of the tip) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drips")
	TArray<FVector> Tips;

	/** Height of the floor or water under each tip (same order as Tips; missing = 0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drips")
	TArray<float> FloorHeights;

	/** Colour of the drops: clear water, or the glow of the slime above */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drips")
	FLinearColor Color = FLinearColor(0.55f, 0.7f, 0.75f);

	/** Glow of the drops (0 dark water; the slime drips glow) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drips")
	float Glow = 2.4f;

	/** Seconds between two drops of one tip, from .. to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drips")
	FVector2D Interval = FVector2D(1.6f, 5.0f);

	/** Only tips closer than this to a player drip, cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drips")
	float ActiveRange = 2600.0f;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	struct FDrop
	{
		int32 Tip = 0;
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float Age = 0.0f;
		float Size = 1.0f;
		/** 0 swelling at the tip, 1 falling, 2 a droplet of the splash */
		uint8 Phase = 0;
	};

	void UpdateActiveTips();

	UPROPERTY(VisibleAnywhere, Category = "Drips")
	TObjectPtr<UInstancedStaticMeshComponent> Mesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;

	TArray<int32> Active;
	TArray<float> NextDrop;
	TArray<FDrop> Drops;
	float ActiveClock = 0.0f;
	bool bShown = false;
};
