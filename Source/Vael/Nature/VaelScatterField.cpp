// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nature/VaelScatterField.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"

AVaelScatterField::AVaelScatterField()
{
	PrimaryActorTick.bCanEverTick = false;

	Instances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Instances"));
	Instances->SetMobility(EComponentMobility::Static);
	RootComponent = Instances;
}

void AVaelScatterField::SetupField(UStaticMesh* Mesh, float CullDistance, bool bCastShadow, bool bCollide)
{
	Instances->ClearInstances();
	Instances->SetStaticMesh(Mesh);
	// Fade out only over the last fifth before the cull distance (a start of 0 would thin the field out from right in front of the camera)
	const int32 End = FMath::Max(0, FMath::RoundToInt(CullDistance));
	Instances->SetCullDistances(FMath::RoundToInt(End * 0.8f), End);
	Instances->SetCastShadow(bCastShadow);
	Instances->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	Instances->SetCollisionProfileName(bCollide ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
	Instances->SetCanEverAffectNavigation(bCollide);
}

int32 AVaelScatterField::AddScatterInstances(const TArray<FTransform>& WorldTransforms)
{
	Modify();
	Instances->AddInstances(WorldTransforms, false, true);
	return Instances->GetInstanceCount();
}
