// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "VaelCrossBarrelComponent.generated.h"

class UMaterialInstanceDynamic;
class USplineMeshComponent;
class UStaticMeshComponent;
class UVaelPlantArmData;

/**
 *  The Wurzelrohr (Boris 2026-11-19): crossed, the arms of two shooting plants grow into one. Out of each wrist two strands of
 *  the root wood shoot forward, wind round each other and braid into one twisted barrel along the aim, thorns standing off it;
 *  at its front a crown of both plants opens (their heads, blades, petals, in turn). While it charges, rings of the pair's light
 *  run down the barrel into a glowing heart in the crown and the barrel trembles; firing it kicks back, the crown flares open,
 *  then the braid unwinds back into the arms. Only for pairs that shoot (VaelSymbiosis::IsShotPair). All in world space.
 */
UCLASS()
class UVaelCrossBarrelComponent : public USceneComponent
{
	GENERATED_BODY()

public:

	/** Makes the strands, thorns, crown and lights for the two plants of the pair; the strands start at the elbows and wind down
	 *  the forearms, so the arms themselves become the barrel */
	void Begin(const UVaelPlantArmData* A, const UVaelPlantArmData* B, const FLinearColor& PairColor, bool bShadow);

	/** Lays the barrel from the two hands along the aim: Grow 0 to 1 is how far it has grown (back to 0 it unwinds), Charge
	 *  how far the shot is charged */
	void Update(const FVector& LeftElbow, const FVector& LeftHand, const FVector& RightElbow, const FVector& RightHand, const FVector& Aim, float Grow, float Charge, float DeltaSeconds);

	/** The shot leaves: the barrel kicks back and the crown flares open */
	void Fire();

	/** Where the shot leaves, in front of the crown */
	FVector GetMuzzle() const { return Muzzle; }

	/** Removes everything */
	void Clear();

protected:

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Strands of the braid (per strand one piece per segment), thorns standing off it, the crown, the heart, the light rings */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> Strands;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> Thorns;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Crown;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Heart;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Rings;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Wood;

	/** Size of each crown piece and whether it sits like a head (along its Z) or a petal */
	TArray<float> CrownSizes;

	FLinearColor Color = FLinearColor::White;
	FVector Muzzle = FVector::ZeroVector;
	float Clock = 0.0f;
	float KickLeft = 0.0f;
	float FlareLeft = 0.0f;
};
