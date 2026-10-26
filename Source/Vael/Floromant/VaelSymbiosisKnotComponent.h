// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "VaelSymbiosisKnotComponent.generated.h"

class UMaterialInstanceDynamic;
class UPointLightComponent;
class USplineMeshComponent;
class UStaticMeshComponent;
class UVaelPlantArmData;

/**
 *  How a symbiosis looks (Boris 2026-10-26): no new model, both plants keep theirs. Vines of each plant grow from where the
 *  plant stands (the top of a tower, a hand) to a knot in the middle, where pieces of both plants (heads, petals, stalks)
 *  are tangled into each other and glow in the colour of the pair. Used by fused towers and by the crossed hands.
 *  Everything is placed in world space.
 */
UCLASS()
class UVaelSymbiosisKnotComponent : public USceneComponent
{
	GENERATED_BODY()

public:

	/** Makes the vines and pieces of the two plants */
	void Setup(const UVaelPlantArmData* A, const UVaelPlantArmData* B, const FLinearColor& Color, bool bShadow);

	/** Grows the vines from the two starts to the knot as far as Grow says (0 to 1) */
	void Update(const FVector& StartA, const FVector& StartB, const FVector& Knot, float Grow);

	/** Flares the glow of the knot, when the pair acts */
	void Flash() { FlashLeft = 1.0f; }

	/** Removes everything */
	void Clear();

protected:

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Vines of each side: first those of A, then those of B */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> Vines;

	/** Pieces tangled in the knot, and their random direction, spin and size */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Pieces;

	TArray<FVector4> PieceShapes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> Glow;

	FLinearColor GlowColor = FLinearColor::White;
	float FlashLeft = 0.0f;
	int32 VinesPerSide = 0;
};
