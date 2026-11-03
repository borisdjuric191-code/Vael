// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Items/VaelMaterial.h"
#include "Player/VaelInteractable.h"
#include "VaelMoonEave.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 *  Mondtraufe: the rare light plant of the Wurzelforst (Herbarium, Boris 2026-11-02). A root arch of its own grows out of the
 *  ground and brings its own little pond; from its top a giant bud (Tripo model SM_Mondtraufe) hangs on its stalk, and from
 *  the bud's tip hang thin glowing threads with Mondtropfen, moonstone pendants. Light runs down from the tip like water,
 *  a curtain into the pond, and lays a bright ring on it. All of it sways gently and glows stronger at night.
 *  The plant stands for purity: as the region's corruption rises it loses petals, one by one they fall and drift on the pond,
 *  the bud wilts, its light dims and the Mondtropfen go out one after another. Sowing, harvesting Mondtropfen and the light
 *  wall come later. Placed by Scripts/BuildMondtraufe.py, which also measures the hang point on the arch.
 *  Two kinds, by settings: the small ones under root arches only show how the land is (not harvestable); the great one hangs
 *  from a giant mangrove over open water, far bigger and pinker, and now and then a Mondtropfen ripens on it in a pure night,
 *  which a player can try to pick (it may melt away instead).
 */
UCLASS()
class AVaelMoonEave : public AActor, public IVaelInteractable
{
	GENERATED_BODY()

public:

	AVaelMoonEave();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	/** The arch the bud hangs from, scaled to ArchHeight */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Look")
	TSoftObjectPtr<UStaticMesh> ArchMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Look", meta = (ClampMin = 50))
	float ArchHeight = 420.0f;

	/** Where the arch (or the tree) stands relative to the actor, and how it is turned; the actor itself stays where players pick */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Look")
	FVector ArchOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Look")
	float ArchYaw = 0.0f;

	/** Height of the water surface relative to the actor in cm: its own pond lies here, or the lake under the great one */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Look")
	float WaterLevel = 3.0f;

	/** Where on the arch mesh (its own space, cm, before scaling) the stalk is fastened, measured by the script */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Look")
	FVector HangPoint = FVector(0.0f, 0.0f, 300.0f);

	/** The bud on its stalk, standing on its foot (the bud's tip) with the stalk's end at its top */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Look")
	TSoftObjectPtr<UStaticMesh> BudMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Look")
	TSoftObjectPtr<UMaterialInterface> BudMaterial;

	/** Length of bud and stalk in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Look", meta = (ClampMin = 10))
	float BudLength = 120.0f;

	/** A longer stalk of bark above the bud, up into the branches it hangs from (the great one), its length and radius in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Look", meta = (ClampMin = 0))
	float StalkExtension = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Look", meta = (ClampMin = 0.5))
	float StalkRadius = 5.0f;

	/** Mondtropfen, pivot at its top where the thread holds it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Look")
	TSoftObjectPtr<UStaticMesh> DropMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Look")
	TSoftObjectPtr<UMaterialInterface> DropMaterial;

	/** Size of a Mondtropfen relative to its model (6 cm long) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Look", meta = (ClampMin = 0.1))
	float DropScale = 1.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Look", meta = (ClampMin = 0, ClampMax = 24))
	int32 NumThreads = 10;

	/** Shortest and longest thread in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Look")
	FVector2D ThreadLength = FVector2D(25.0f, 85.0f);

	/** Radius around the tip the threads start from, and their thickness, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Look", meta = (ClampMin = 0))
	float ThreadSpread = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Look", meta = (ClampMin = 0.05))
	float ThreadThickness = 0.35f;

	/** Glowing threads, the curtain of light and the ring it lays on the water */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Look")
	TSoftObjectPtr<UMaterialInterface> ThreadMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Look")
	TSoftObjectPtr<UMaterialInterface> CurtainMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Look")
	TSoftObjectPtr<UMaterialInterface> RingMaterial;

	/** Width of the curtain at rest in cm; as a light wall (later) it spreads wider */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Look", meta = (ClampMin = 10))
	float CurtainWidth = 80.0f;

	/** Size of the ring of light on the water relative to the engine plane (1 m) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Look", meta = (ClampMin = 0.1))
	float RingSize = 1.6f;

	/** The pond the arch brings with it; off where it stands in water already */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Look")
	bool bOwnPond = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Look")
	TSoftObjectPtr<UMaterialInterface> PondMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Look", meta = (ClampMin = 50))
	float PondRadius = 220.0f;

	/** Color of its light: pearly moonlight with a breath of rose */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Look")
	FLinearColor LightColor = FLinearColor(1.0f, 0.82f, 0.9f);

	/** Candela of the light under the bud at night and by day */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Look")
	FVector2D LightIntensity = FVector2D(90.0f, 30.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Look", meta = (ClampMin = 50))
	float LightRadius = 480.0f;

	/** Fallen petals lying on the pond at full corruption */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Corruption")
	TSoftObjectPtr<UStaticMesh> PetalMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Corruption")
	TSoftObjectPtr<UMaterialInterface> PetalMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Corruption", meta = (ClampMin = 0, ClampMax = 40))
	int32 MaxFallenPetals = 12;

	/** Size of a fallen petal relative to its model (14 cm long) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Corruption", meta = (ClampMin = 0.1))
	float PetalScale = 1.6f;

	/** Corruption (0-100) at which the first petal falls, and at which it has lost all it can lose */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Corruption")
	FVector2D WiltCorruption = FVector2D(15.0f, 85.0f);

	/** Seconds a petal takes to fall */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Corruption", meta = (ClampMin = 0.1))
	float PetalFallSeconds = 3.0f;

	/** How sick the plant looks, 0 healthy to 1 as wilted as it gets; follows the region unless held for a test */
	UFUNCTION(BlueprintPure, Category="Mondtraufe")
	float GetWilt() const { return Wilt; }

	/** Only the great one: Mondtropfen ripen on it and can be picked */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Harvest")
	bool bHarvestable = false;

	/** Chance that one Mondtropfen ripens in a night, while the land is pure enough (wilt below RipeMaxWilt) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Harvest", meta = (ClampMin = 0, ClampMax = 1, EditCondition = "bHarvestable"))
	float RipenChance = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Harvest", meta = (ClampMin = 0, ClampMax = 10, EditCondition = "bHarvestable"))
	int32 MaxRipe = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Harvest", meta = (ClampMin = 0, ClampMax = 1, EditCondition = "bHarvestable"))
	float RipeMaxWilt = 0.2f;

	/** Chance that picking a ripe one works; otherwise it melts into the water and is gone */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Harvest", meta = (ClampMin = 0, ClampMax = 1, EditCondition = "bHarvestable"))
	float PickChance = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Harvest", meta = (EditCondition = "bHarvestable"))
	FVaelLootEntry PickLoot = FVaelLootEntry(TEXT("/Game/Vael/Items/Materials/DA_Material_Mondtropfen.DA_Material_Mondtropfen"), 1.0f);

	/** Lets Mondtropfen ripen at once, for tests (cheat VaelMondtraufeReif) */
	void RipenAll();

	int32 GetRipeDrops() const { return RipeDrops; }

	virtual bool CanInteract(const AVaelCharacter* Player) const override;
	virtual void Interact(AVaelCharacter* Player) override;
	virtual FText GetInteractPrompt() const override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Arch;

	/** At the hang point; the bud swings around it */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> BudPivot;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Bud;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Stalk;

	/** At the bud's tip, where threads and curtain begin */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> Tip;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Curtain;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Pond;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Ring;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UPointLightComponent> Glow;

	/** One pivot per thread at the tip, holding the thread and its Mondtropfen */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> ThreadPivots;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Threads;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Drops;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Petals;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> GlowMaterials;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BudLook;

	/** Where each fallen petal drifts: its spot on the pond, turn, drift speed, and the time it began to fall (<0: not yet) */
	struct FPetal
	{
		FVector2D Spot = FVector2D::ZeroVector;
		float Yaw = 0.0f;
		float Drift = 0.0f;
		float FallStart = -1.0f;
		FVector FallFrom = FVector::ZeroVector;
	};
	TArray<FPetal> PetalStates;

	/** Lengths and sway phases of the threads */
	TArray<float> ThreadLengths;
	TArray<float> ThreadPhases;

	float Wilt = 0.0f;

	/** Ripe Mondtropfen, the first ones of the threads, and whether the last look saw night */
	int32 RipeDrops = 0;
	bool bWasNight = false;

	/** Destroys the parts made by Build, so it can build them again */
	void ClearBuilt();
	float Age = 0.0f;
	float NextSurroundingsAge = 0.0f;

	/** Builds the parts from the settings; the same every time for the same place */
	void Build();

	/** Reads the corruption of the region and lets petals fall for it */
	void UpdateSurroundings();

	/** Glow of the light, threads, curtain and drops by night and wilt */
	void UpdateGlow();
};
