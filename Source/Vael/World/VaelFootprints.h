// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VaelFootprints.generated.h"

class UTextureRenderTarget2D;
class UTexture2D;
class UMaterialParameterCollection;

/** Shapes in the footprint atlas T_AM_Fussabdruecke (Tools/Welt/fussabdruecke.py) */
enum class EVaelPrint : uint8
{
	Boot = 0,
	Claw = 1,
	Roll = 2,
	Paw = 3
};

/**
 *  Footprints in the ground (goal 8.11. evening): every step of a player, and of the creatures walking near them, presses
 *  a print into a map of tracks around the group. M_AM_Boden reads that map: the ground sinks in where a print is (Nanite
 *  displacement), darkens and fades out after a while; deep in ash and sand, shallow on earth, hardly on rock and roads.
 *
 *  The map (RT_AM_Spuren, 2048 px over 48 m, RGBA16F): R = depth of the print, G = when it was made (seconds / 600, wrapped).
 *  Its place, size and the clock go to the material through MPC_AM_Spuren. When the group walks away from the middle of
 *  the map, the map moves along and carries its prints with it. A template for the ground of later regions.
 */
UCLASS()
class UVaelFootprintSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UVaelFootprintSubsystem, STATGROUP_Tickables); }

	/** Presses a print into the map at a world location, turned to Yaw (degrees, the way the walker goes), Size in cm */
	void Stamp(const FVector& Location, float Yaw, EVaelPrint Print, float Size, float Depth = 1.0f);

	/** Seconds a print takes to fade */
	static constexpr float FadeSeconds = 100.0f;

private:

	bool EnsureResources();

	/** Moves the map so Centre is its middle, carrying the prints along */
	void Recentre(const FVector2D& Centre);

	/** Steps of the walkers: where each one last pressed a print and with which foot */
	struct FWalker
	{
		FVector2D Last = FVector2D::ZeroVector;
		bool bLeft = false;
		bool bStarted = false;
	};
	TMap<TWeakObjectPtr<AActor>, FWalker> Walkers;

	void FollowWalker(AActor* Walker, EVaelPrint Print, float Stride, float Size, float Side, float Depth);

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> Map;

	/** A second map the prints are moved through when the map recentres */
	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> Spare;

	UPROPERTY()
	TObjectPtr<UTexture2D> Shapes;

	UPROPERTY()
	TObjectPtr<UMaterialParameterCollection> Parameters;

	FVector2D Origin = FVector2D::ZeroVector;
	bool bPlaced = false;
	bool bTriedResources = false;
	float CreatureScan = 0.0f;
	TArray<TWeakObjectPtr<AActor>> NearCreatures;

	static constexpr float MapSize = 4800.0f;
	static constexpr float RecentreDistance = 800.0f;
};
