// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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
 */
UCLASS()
class AVaelMoonEave : public AActor
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

	/** Fallen petals lying on the pond at full corruption */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Corruption")
	TSoftObjectPtr<UStaticMesh> PetalMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Corruption")
	TSoftObjectPtr<UMaterialInterface> PetalMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Corruption", meta = (ClampMin = 0, ClampMax = 30))
	int32 MaxFallenPetals = 12;

	/** Corruption (0-100) at which the first petal falls, and at which it has lost all it can lose */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Corruption")
	FVector2D WiltCorruption = FVector2D(15.0f, 85.0f);

	/** Seconds a petal takes to fall */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mondtraufe|Corruption", meta = (ClampMin = 0.1))
	float PetalFallSeconds = 3.0f;

	/** How sick the plant looks, 0 healthy to 1 as wilted as it gets; follows the region unless held for a test */
	UFUNCTION(BlueprintPure, Category="Mondtraufe")
	float GetWilt() const { return Wilt; }

protected:

	virtual void BeginPlay() override;

private:

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Arch;

	/** At the hang point; the bud swings around it */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> BudPivot;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Bud;

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
	float Age = 0.0f;
	float NextSurroundingsAge = 0.0f;

	/** Builds the parts from the settings; the same every time for the same place */
	void Build();

	/** Reads the corruption of the region and lets petals fall for it */
	void UpdateSurroundings();

	/** Glow of the light, threads, curtain and drops by night and wilt */
	void UpdateGlow();
};
