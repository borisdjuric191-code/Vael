// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelNidVeins.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProceduralMeshComponent;
class UStaticMesh;

/**
 *  The veins of the Nid, the blood of the Sleeper, spreading from a Nid spring like the vessels of a living body: a few thick
 *  trunks at the spring that fork again and again into ever thinner veins, winding, never straight, uneven in width
 *  (Boris 2026-10-22). Most of the way they lie open, a thick dark red-violet liquid creeping slowly away from the spring,
 *  with foam here and there, bubbles that swell and burst and let out poisonous mist; between the open stretches they run
 *  under the ground and only swell it. Walking through an open vein hurts.
 *
 *  The network is grown by Scripts/PlaceNidVeins.py on the height map of the region (the landscape streams in cells, so it
 *  cannot be traced reliably when the level starts) and kept here as points in world space; the actor builds the meshes
 *  from them. Look: M_Nid_Ader, M_Nid_Wulst, M_Nid_Blase, M_Nid_Nebel
 *  (Scripts/CreateNidKit.py).
 */
/** A point on a vein: where it lies on the ground, its width in cm, how far along its way from the spring in cm, whether it
 *  lies open or runs under the ground, and which vein it belongs to */
USTRUCT(BlueprintType)
struct FVaelNidVeinPoint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nid")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nid")
	float Width = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nid")
	float Along = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nid")
	bool bOpen = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nid")
	int32 Vein = 0;

	/** Ground height left and right of the point in cm, relative to Location, at the edges of the ridge (1.3 widths), so the
	 *  vein lies in the ground across a slope instead of sinking into it with one side */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nid")
	float GroundLeft = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nid")
	float GroundRight = 0.0f;
};

UCLASS()
class AVaelNidVeins : public AActor
{
	GENERATED_BODY()

public:

	/** Constructor */
	AVaelNidVeins();

	/** The points of all veins, vein after vein, each from the spring outwards (written by Scripts/PlaceNidVeins.py) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nid|Network")
	TArray<FVaelNidVeinPoint> Points;

	/** Speed the liquid creeps away from the spring, in cm per second; negative for towards it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Nid|Liquid")
	float FlowSpeed = 12.0f;

	/** Bubbles that rise per second for every 10 m of open vein, and how many of them let out a puff of mist */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Nid|Liquid", meta = (ClampMin = 0))
	float BubblesPer10Metres = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Nid|Liquid", meta = (ClampMin = 0, ClampMax = 1))
	float MistShare = 0.6f;

	/** Damage per second while standing in an open vein */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Nid|Danger", meta = (ClampMin = 0))
	float DamagePerSecond = 8.0f;

	/** Look of the liquid, of a vein under the ground, of the bubbles and the mist */
	UPROPERTY(EditAnywhere, Category="Nid|Look")
	TSoftObjectPtr<UMaterialInterface> LiquidMaterial;

	UPROPERTY(EditAnywhere, Category="Nid|Look")
	TSoftObjectPtr<UMaterialInterface> RidgeMaterial;

	UPROPERTY(EditAnywhere, Category="Nid|Look")
	TSoftObjectPtr<UMaterialInterface> BubbleMaterial;

	UPROPERTY(EditAnywhere, Category="Nid|Look")
	TSoftObjectPtr<UMaterialInterface> MistMaterial;

	UPROPERTY(EditAnywhere, Category="Nid|Look")
	TSoftObjectPtr<UStaticMesh> BubbleMesh;

	/** Length of all open veins in m, after growing */
	float GetOpenMetres() const { return OpenMetres; }

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

protected:

	virtual void BeginPlay() override;

private:

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UProceduralMeshComponent> Veins;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UInstancedStaticMeshComponent> Bubbles;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UInstancedStaticMeshComponent> Mist;

	/** Turns the points of every vein into the meshes of the liquid and of the swollen ground */
	void BuildMesh();

	/** Open points to rise bubbles from (index into the open list, weighted by width) and their summed widths */
	TArray<FVaelNidVeinPoint> OpenPoints;
	float OpenWidthSum = 0.0f;
	float MaxOpenWidth = 1.0f;
	float OpenMetres = 0.0f;

	/** A bubble swelling and bursting, or a puff of mist rising */
	struct FPuff
	{
		FVector Location = FVector::ZeroVector;
		float Size = 10.0f;
		float Age = 0.0f;
		float Life = 1.0f;
	};
	TArray<FPuff> LiveBubbles;
	TArray<FPuff> LiveMist;
	float BubbleDebt = 0.0f;
	float DamageClock = 0.0f;

	void UpdatePuffs(float DeltaSeconds);
	void HurtWaders(float DeltaSeconds);
};
