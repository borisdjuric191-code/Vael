// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Creatures/VaelCreature.h"
#include "Creatures/VaelCreatureData.h"
#include "VaelSchuerfameise.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

/** One empty setting in the shell of the ant, measured on the Blender model (Tools/Ameisen) */
USTRUCT(BlueprintType)
struct FVaelAmeisenFassung
{
	GENERATED_BODY()

	/** Bone it moves with: "head" or "body" (Erztochter also "wing_L", "wing_R") */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ameise")
	FName Bone;

	/** Part of the body, used by the Erztochter to choose the ore (Kamm, Kopf, Sattel, Hinterleib, Fluegel); none for workers */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ameise")
	FName Zone;

	/** Floor of the setting, Blender model space in metres (faces -Y, feet on Z = 0) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ameise")
	FVector Location = FVector::ZeroVector;

	/** Axis out of the setting, Blender model space */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ameise")
	FVector Normal = FVector::UpVector;

	/** Normal of the body surface there, Blender model space (zero = same as Normal); bent pieces curl their tip towards it */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ameise")
	FVector Surface = FVector::ZeroVector;

	/** Radius up to the rim, metres */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ameise")
	float Radius = 0.02f;

	/** Depth from rim to floor, metres */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ameise")
	float Depth = 0.0f;
};

/** One crystal cut out of an ore of the Eisenschlund (Tools/Ameisen/ameise_kristalle.py, mode 'export') */
USTRUCT(BlueprintType)
struct FVaelAmeisenKristall
{
	GENERATED_BODY()

	/** Ore it is cut from, like "Salzspat" */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ameise")
	FName Ore;

	/** The piece: foot at 0, axis along +Z */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ameise")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** Width at the foot and length along the axis, metres as cut (the mesh is 100 times that in cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ameise")
	float Width = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ameise")
	float Length = 0.2f;
};

/**
 *  Schürfameise of the Eisenschlund. Every worker carries the ore of its colony in the settings of its shell; old ones
 *  (Ahnenpanzer, elite) carry bigger crystals in all settings with side spikes growing out of the rims.
 *  Free ants are neutral and only fight back; ants enslaved by the Kobolde (fed with Tropfseuche) attack.
 *  Values are first proposals; the attacks are not designed yet (only a bite).
 */
UCLASS(BlueprintType)
class UVaelSchuerfameiseData : public UVaelCreatureData
{
	GENERATED_BODY()

public:

	UVaelSchuerfameiseData();

	/** All settings of the shell */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ameise")
	TArray<FVaelAmeisenFassung> Fassungen;

	/** Crystal pieces of all ores */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ameise")
	TArray<FVaelAmeisenKristall> Kristalle;

	/** Share of the settings a young worker has filled; the biggest settings fill first */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ameise", meta = (ClampMin = 0, ClampMax = 1))
	float WorkerFill = 0.6f;

	/** Size of an Ahnenpanzer compared to a worker */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ameise", meta = (ClampMin = 1))
	float AncestorScale = 1.7f;

	/** Growth of the crystals of an Ahnenpanzer: 0 = fills the setting, 0.5 = much bigger with side spikes */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ameise", meta = (ClampMin = 0))
	float AncestorGrowth = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ameise", meta = (ClampMin = 0))
	float BiteDamage = 9.0f;

	/** Reach of the bite beyond the capsule, cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ameise", meta = (ClampMin = 0))
	float BiteReach = 90.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ameise", meta = (ClampMin = 0.1))
	float BiteInterval = 1.4f;

	/** How far a free ant wanders around its place, cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ameise", meta = (ClampMin = 0))
	float WanderRadius = 600.0f;

	/** Seconds a free ant stays angry after being hit; ants of its colony nearby join in */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ameise", meta = (ClampMin = 0))
	float AngerSeconds = 20.0f;

	/** Distance in which hit ants call the others of their colony, cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ameise", meta = (ClampMin = 0))
	float AlarmRange = 1500.0f;
};

UCLASS()
class AVaelSchuerfameise : public AVaelCreature
{
	GENERATED_BODY()

public:

	AVaelSchuerfameise();

	virtual void BeginPlay() override;

	/** Ore of its colony, the rank and whether the Kobolde enslaved it; rebuilds the crystals */
	void SetupAnt(FName InOre, bool bInAncestor, bool bInEnslaved);

	/** Gets angry (fights back) for a while */
	void Anger();

	FName GetOre() const { return Ore; }
	bool IsAncestor() const { return bAncestor; }
	bool IsEnslaved() const { return bEnslaved; }
	bool IsAngry() const { return AngerTime > 0.0f; }
	int32 GetCrystalCount() const { return CrystalCount; }

protected:

	virtual TSubclassOf<UVaelCreatureData> GetDefaultDataClass() const override;
	virtual void TickBehavior(float DeltaSeconds) override;
	virtual void OnDamageTaken(float Damage, const FGameplayTagContainer& DamageTags) override;
	virtual float GetStatusTextHeight() const override { return 90.0f * GetActorScale3D().Z; }

	/** Ore of the colony, like "Salzspat" */
	UPROPERTY(EditAnywhere, Category="Ameise")
	FName Ore = TEXT("Rosteisen");

	/** Old ant with the thick crystal shell (elite) */
	UPROPERTY(EditAnywhere, Category="Ameise")
	bool bAncestor = false;

	/** Fed with Tropfseuche by the Kobolde: attacks everyone */
	UPROPERTY(EditAnywhere, Category="Ameise")
	bool bEnslaved = false;

	/** Seed of the crystal layout, so every ant looks different */
	UPROPERTY(EditAnywhere, Category="Ameise")
	int32 CrystalSeed = 0;

	/** Puts the crystals of its ore into the settings */
	virtual void BuildCrystals();

	/** Grows one crystal piece out of a setting (plus side spikes when old); Age 0 = fills the setting, LengthScale stretches it,
	 *  Jitter scales the random size and tilt (0 = orderly rows, 1 = natural); below 1 the piece is also fitted by its full width */
	void PlaceCrystal(const FVaelAmeisenFassung& Fassung, const FVaelAmeisenKristall& Piece, float Age, float LengthScale, FRandomStream& Random, float Jitter = 1.0f,
		bool bBent = false, float Sink = 0.12f);

	/** Removes all crystals */
	void ClearCrystals();

	/** Adds one crystal piece to the ant mesh, attached to the bone; transform in mesh component space.
	 *  (Instanced components would be cheaper for the Erztochter's few hundred pieces, but the ore materials are not
	 *  flagged for instancing and render grey; that needs Boris' OK to change the material assets.) */
	void AddCrystal(UStaticMesh* PieceMesh, FName Bone, const FTransform& InMesh);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Crystals;

	int32 CrystalCount = 0;

private:

	float AngerTime = 0.0f;
	float BiteCooldown = 0.0f;
	float WanderPause = 0.0f;
	FVector WanderGoal = FVector::ZeroVector;
	bool bSetUp = false;
};
