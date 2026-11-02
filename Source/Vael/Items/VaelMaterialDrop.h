// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelMaterialDrop.generated.h"

class AVaelCharacter;
class UPointLightComponent;
class UStaticMeshComponent;
class UVaelMaterial;

/**
 *  A material lying on the ground as loot, like a Nidkristall or a potion, for one player: only its owner picks it up,
 *  by walking over it, into their material bag. In co-op every player gets their own copy beside the others.
 *  It shows the model of the material and glints so it can be found in the isometric view: it turns slowly,
 *  a small light in the color of the material breathes, and the loot material gets the parameter "AmBoden" = 1.
 */
UCLASS()
class AVaelMaterialDrop : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> Spinner;

	/** Model of the material, like the crystal or the bottle */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Model;

	/** Second model inside it, like the liquid of a potion */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> InnerModel;

	/** Breathing glow on the ground */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPointLightComponent> Glow;

public:

	AVaelMaterialDrop();

	/** Drops a number of a material on the ground near a location for one player */
	static AVaelMaterialDrop* DropMaterial(UWorld* World, const FVector& Location, const UVaelMaterial* InMaterial, int32 InCount, AVaelCharacter* InOwner);

	virtual void Tick(float DeltaSeconds) override;

	const UVaelMaterial* GetMaterial() const { return Material; }
	int32 GetCount() const { return Count; }
	AVaelCharacter* GetOwningPlayer() const { return OwningPlayer.Get(); }

protected:

	virtual void BeginPlay() override;

private:

	UPROPERTY()
	TObjectPtr<const UVaelMaterial> Material;

	int32 Count = 1;

	TWeakObjectPtr<AVaelCharacter> OwningPlayer;

	/** Seconds since it fell, drives the turn and the glow */
	float Age = 0.0f;

	/** Random start of the turn and the breathing, so drops side by side don't move in step */
	float Phase = 0.0f;

	/** Full brightness of the glow */
	float GlowIntensity = 0.0f;
};
