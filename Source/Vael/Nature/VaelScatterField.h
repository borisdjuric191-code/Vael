// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelScatterField.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;

/**
 * Many copies of one mesh in one actor: rocks, logs, roots and plants a dressing script scatters over a region.
 * The script (e.g. Scripts/DressWurzelforst.py) calls SetupField once and then AddScatterInstances with world transforms;
 * the instances are saved with the level. Single instances can be moved or removed by hand in the editor.
 */
UCLASS()
class VAEL_API AVaelScatterField : public AActor
{
	GENERATED_BODY()

public:
	AVaelScatterField();

	/** Sets the mesh and how it is drawn; removes all instances. CullDistance 0 = never culled */
	UFUNCTION(BlueprintCallable, Category="Vael|Scatter")
	void SetupField(UStaticMesh* Mesh, float CullDistance, bool bCastShadow, bool bCollide);

	/** Adds instances at these world transforms; returns how many the field holds now */
	UFUNCTION(BlueprintCallable, Category="Vael|Scatter")
	int32 AddScatterInstances(const TArray<FTransform>& WorldTransforms);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vael|Scatter")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Instances;
};
