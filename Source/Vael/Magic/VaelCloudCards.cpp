// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelCloudCards.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"

namespace
{
	/** Width of the engine plane in cm, and the custom data each card hands its material (frame, solidity, brightness) */
	constexpr float CloudPlaneSize = 100.0f;
	constexpr int32 CloudCustomFloats = 3;
}

UVaelCloudCardsComponent::UVaelCloudCardsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVaelCloudCardsComponent::Init(UMaterialInterface* Material)
{
	Instances = NewObject<UInstancedStaticMeshComponent>(GetOwner());
	Instances->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")));
	Instances->SetMaterial(0, Material);
	Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Instances->SetCastShadow(false);
	Instances->NumCustomDataFloats = CloudCustomFloats;
	Instances->SetupAttachment(this);
	Instances->SetUsingAbsoluteScale(true);
	Instances->RegisterComponent();
	Instances->SetWorldScale3D(FVector::OneVector);
}

int32 UVaelCloudCardsComponent::AddCard(const FVector& Place, float Size, float Solid, bool bFlat)
{
	FCard& Card = Cards.AddDefaulted_GetRef();
	Card.Place = Place;
	Card.Size = Size;
	Card.Solid = Solid;
	Card.bFlat = bFlat;
	Card.Roll = FMath::FRandRange(0.0f, 360.0f);
	Card.Frame = float(FMath::RandRange(0, 63));
	Transforms.Add(FTransform::Identity);
	if (Instances != nullptr)
	{
		const int32 Instance = Instances->AddInstance(FTransform(Place), false);
		Instances->SetCustomDataValue(Instance, 0, Card.Frame);
		Instances->SetCustomDataValue(Instance, 1, Solid);
		// Some cards a little darker, so the mass has depth
		Instances->SetCustomDataValue(Instance, 2, FMath::FRandRange(0.82f, 1.05f));
	}
	return Cards.Num() - 1;
}

void UVaelCloudCardsComponent::SetCard(int32 Index, const FVector& Place, float Size, float Solid)
{
	if (!Cards.IsValidIndex(Index))
	{
		return;
	}
	FCard& Card = Cards[Index];
	Card.Place = Place;
	Card.Size = Size;
	if (!FMath::IsNearlyEqual(Card.Solid, Solid, 0.01f) && Instances != nullptr)
	{
		Instances->SetCustomDataValue(Index, 1, Solid, false);
	}
	Card.Solid = Solid;
}

void UVaelCloudCardsComponent::Commit()
{
	if (Instances == nullptr || Cards.IsEmpty())
	{
		return;
	}

	// Cards face the camera everyone shares; flat ones lie on the ground, each turned on its own
	FVector Eye = GetComponentLocation() + FVector(0.0f, -1000.0f, 1000.0f);
	if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		Eye = Camera->GetCameraLocation();
	}
	const FTransform Frame = GetComponentTransform();
	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		const FCard& Card = Cards[Index];
		const FVector World = Frame.TransformPosition(Card.Place);
		const FVector Facing = Card.bFlat ? FVector::UpVector : (Eye - World).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		const FQuat Turn = FQuat(Facing, FMath::DegreesToRadians(Card.Roll)) * FRotationMatrix::MakeFromZ(Facing).ToQuat();
		Transforms[Index] = FTransform(Turn, World, FVector(Card.Size / CloudPlaneSize));
	}
	Instances->BatchUpdateInstancesTransforms(0, Transforms, true, true, true);
}
