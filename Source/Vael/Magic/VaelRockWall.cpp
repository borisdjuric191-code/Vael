// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelRockWall.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Magic/VaelMagicSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	/** Edge length of the engine cube mesh used as placeholder */
	constexpr float PlaceholderCubeSize = 100.0f;
}

AVaelRockWall::AVaelRockWall()
{
	// Static like the rest of the level: stops characters and projectiles and counts as rock for the earth element
	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	Collision->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Collision->SetCollisionObjectType(ECC_WorldStatic);
	Collision->SetMobility(EComponentMobility::Movable);
	Collision->CanCharacterStepUpOn = ECB_No;
	RootComponent = Collision;

	// Placeholder look from engine assets
	Block = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Block"));
	Block->SetupAttachment(RootComponent);
	Block->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Block->bReceivesDecals = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Block->SetStaticMesh(CubeMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CubeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (CubeMaterial.Succeeded())
	{
		Block->SetMaterial(0, CubeMaterial.Object);
	}

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AVaelRockWall::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The look rises out of the ground, the collision is there right away
	RiseTime += DeltaSeconds;
	const float Rise = RiseDuration > 0.0f ? FMath::Clamp(RiseTime / RiseDuration, 0.0f, 1.0f) : 1.0f;

	const FVector Scale = Block->GetRelativeScale3D();
	Block->SetRelativeScale3D(FVector(Scale.X, Scale.Y, FMath::Max(0.01f, FullHeight * Rise / PlaceholderCubeSize)));
	Block->SetRelativeLocation(FVector(0.0f, 0.0f, -FullHeight * 0.5f * (1.0f - Rise)));

	if (Rise >= 1.0f)
	{
		SetActorTickEnabled(false);
	}
}

AVaelRockWall* AVaelRockWall::RaiseBlock(UWorld* World, const FVector& GroundLocation, float Yaw, float Size, float Height, float Lifetime, APawn* InInstigator)
{
	if (World == nullptr)
	{
		return nullptr;
	}

	const FTransform SpawnTransform(FRotator(0.0f, Yaw, 0.0f), GroundLocation + FVector(0.0f, 0.0f, Height * 0.5f));

	AVaelRockWall* Wall = World->SpawnActorDeferred<AVaelRockWall>(AVaelRockWall::StaticClass(), SpawnTransform, InInstigator, InInstigator, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Wall != nullptr)
	{
		Wall->SetBlockSize(Size, Height);
		Wall->InitialLifeSpan = Lifetime;
		Wall->FinishSpawning(SpawnTransform);
	}

	return Wall;
}

void AVaelRockWall::SetBlockSize(float Size, float Height)
{
	FullHeight = Height;

	Collision->SetBoxExtent(FVector(Size * 0.5f, Size * 0.5f, Height * 0.5f));
	Block->SetRelativeScale3D(FVector(Size / PlaceholderCubeSize, Size / PlaceholderCubeSize, 0.01f));

	if (UMaterialInstanceDynamic* Material = Block->CreateAndSetMaterialInstanceDynamic(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), UVaelMagicSettings::Get()->GetElementColor(EVaelElement::Earth));
	}
}
