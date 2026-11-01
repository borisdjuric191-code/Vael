// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "VaelSurfacePool.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProceduralMeshComponent;

/**
 *  A living pool on the ground, like the mud of a Schlammfeld or black water: a disc that follows the ground, wells out from the middle
 *  and sinks away at its end. The liquid itself (flow, bubbles, glowing veins) is an instance of M_Vael_Pfuetze (Scripts/CreateSpellSurfaceKit.py).
 */
UCLASS()
class UVaelSurfacePoolComponent : public USceneComponent
{
	GENERATED_BODY()

public:

	UVaelSurfacePoolComponent();

	/** Lays the pool around this component; a lifetime of 0 lasts until the owner goes. False without the material. */
	bool Spread(UMaterialInterface* Material, float InRadius, float InLifetime);

	//~Begin UActorComponent
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~End UActorComponent

private:

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> Mesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Liquid;

	float Lifetime = 0.0f;
	float Age = 0.0f;
};
