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
 *  The maps (RT_AM_Spuren_0..3, 2048 px over 48 m, RGBA16F): R = depth of the print, G = when it was made (seconds / 600,
 *  wrapped). One map follows each player (goal 9.11.: one map round the middle of the group lost every print of a player
 *  who walked more than 24 m from the others); a print is pressed into every map that covers its place, the material
 *  reads all four and keeps the deepest. Their places, size and the clock go to the material through MPC_AM_Spuren
 *  (SpurenUrsprung0..3, SpurenUhr). A map moves along with its player and carries its prints. A template for the ground
 *  of later regions.
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

	/** Moves map Slot so Centre is its middle, carrying the prints along */
	void Recentre(int32 Slot, const FVector2D& Centre);

	/** A map no player uses: parked far away, so the material finds nothing in it */
	void Park(int32 Slot);

	static constexpr int32 NumMaps = 4;

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
	TObjectPtr<UTextureRenderTarget2D> Maps[NumMaps];

	/** A second map the prints are moved through when the map recentres */
	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> Spare;

	UPROPERTY()
	TObjectPtr<UTexture2D> Shapes;

	UPROPERTY()
	TObjectPtr<UMaterialParameterCollection> Parameters;

	FVector2D Origins[NumMaps];
	bool bPlaced[NumMaps] = { false, false, false, false };
	bool bParked[NumMaps] = { false, false, false, false };
	bool bTriedResources = false;
	float CreatureScan = 0.0f;
	TArray<TWeakObjectPtr<AActor>> NearCreatures;

	/** The protocol (goal 9.11., footprints stopping after a few minutes): counts, and a print read back from the map
	 *  after the map has moved, every LogEvery seconds with -VaelSpurLog or r.Vael.SpurLog 1 */
	int32 StampCount = 0;
	int32 SkippedOutside = 0;
	int32 SkippedNoGround = 0;
	int32 RecentreCount = 0;
	float LogClock = 0.0f;
	FVector ProbeWorld = FVector::ZeroVector;
	FVector LastStampWorld = FVector::ZeroVector;
	FLinearColor ProbeValue = FLinearColor::Transparent;
	bool bProbe = false;
	void WriteProtocol(const FVector& PlayerAt);
	FLinearColor ReadAt(const FVector& World) const;

	static constexpr float MapSize = 4800.0f;
	static constexpr float RecentreDistance = 800.0f;
};
