// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelCaveDrips.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Magic/VaelMagicSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "VaelAssets.h"

namespace
{
	constexpr float SphereSize = 100.0f;
	/** A drop: cm across, seconds it swells at the tip */
	constexpr float DropSize = 2.6f;
	constexpr float SwellTime = 0.7f;
	constexpr float DropGravity = 1600.0f;
	/** The splash on the floor */
	constexpr int32 SplashDroplets = 4;
	constexpr float DropletLife = 0.32f;
	/** Most drops in the air at once */
	constexpr int32 MaxDrops = 96;
}

AVaelCaveDrips::AVaelCaveDrips()
{
	PrimaryActorTick.bCanEverTick = true;
	Mesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Drops"));
	RootComponent = Mesh;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->bReceivesDecals = false;
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetUsingAbsoluteLocation(true);
	Mesh->SetUsingAbsoluteRotation(true);
	Mesh->SetUsingAbsoluteScale(true);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		Mesh->SetStaticMesh(Sphere.Object);
	}
}

void AVaelCaveDrips::BeginPlay()
{
	Super::BeginPlay();
	Mesh->SetWorldTransform(FTransform::Identity);
	if (UMaterialInterface* Ice = VaelAssets::LoadOptional(UVaelMagicSettings::Get()->HailIceMaterial))
	{
		Material = UMaterialInstanceDynamic::Create(Ice, this);
		Material->SetVectorParameterValue(TEXT("Farbe"), Color);
		Material->SetScalarParameterValue(TEXT("Leuchten"), Glow);
		Mesh->SetMaterial(0, Material);
	}
	NextDrop.SetNum(Tips.Num());
	for (float& Each : NextDrop)
	{
		Each = FMath::FRandRange(0.0f, Interval.Y);
	}
	// a fixed pool of instances, hidden ones shrunk to nothing: changing the count would rebuild the whole component
	TArray<FTransform> Pool;
	Pool.Init(FTransform(FQuat::Identity, GetActorLocation(), FVector::ZeroVector), MaxDrops);
	Mesh->ClearInstances();
	Mesh->AddInstances(Pool, false, true);
	SetActorTickInterval(0.0f);
	UpdateActiveTips();
}

void AVaelCaveDrips::UpdateActiveTips()
{
	Active.Reset();
	TArray<FVector, TInlineAllocator<4>> Players;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (const APawn* Pawn = It->Get() != nullptr ? It->Get()->GetPawn() : nullptr)
		{
			Players.Add(Pawn->GetActorLocation());
		}
	}
	const float RangeSquared = ActiveRange * ActiveRange;
	for (int32 Index = 0; Index < Tips.Num(); ++Index)
	{
		for (const FVector& Player : Players)
		{
			if (FVector::DistSquared2D(Player, Tips[Index]) < RangeSquared && FMath::Abs(Player.Z - Tips[Index].Z) < 2500.0f)
			{
				Active.Add(Index);
				break;
			}
		}
	}
}

void AVaelCaveDrips::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ActiveClock -= DeltaSeconds;
	if (ActiveClock <= 0.0f)
	{
		ActiveClock = 0.5f;
		UpdateActiveTips();
	}
	// nobody near and nothing falling: look again only now and then
	if (Active.IsEmpty() && Drops.IsEmpty())
	{
		SetActorTickInterval(0.5f);
		if (bShown)
		{
			bShown = false;
			TArray<FTransform> Hidden;
			Hidden.Init(FTransform(FQuat::Identity, GetActorLocation(), FVector::ZeroVector), Mesh->GetInstanceCount());
			Mesh->BatchUpdateInstancesTransforms(0, Hidden, true, false, true);
		}
		return;
	}
	SetActorTickInterval(0.0f);

	for (const int32 Tip : Active)
	{
		NextDrop[Tip] -= DeltaSeconds;
		if (NextDrop[Tip] <= 0.0f && Drops.Num() < MaxDrops)
		{
			NextDrop[Tip] = FMath::FRandRange(Interval.X, Interval.Y);
			FDrop& Drop = Drops.AddDefaulted_GetRef();
			Drop.Tip = Tip;
			Drop.Location = Tips[Tip];
			Drop.Size = DropSize * FMath::FRandRange(0.75f, 1.25f);
		}
	}

	for (int32 Index = Drops.Num() - 1; Index >= 0; --Index)
	{
		FDrop& Drop = Drops[Index];
		Drop.Age += DeltaSeconds;
		if (Drop.Phase == 0)
		{
			if (Drop.Age >= SwellTime)
			{
				Drop.Phase = 1;
				Drop.Age = 0.0f;
			}
			continue;
		}
		Drop.Velocity.Z -= DropGravity * DeltaSeconds * (Drop.Phase == 2 ? 0.8f : 1.0f);
		Drop.Location += Drop.Velocity * DeltaSeconds;
		const float Floor = FloorHeights.IsValidIndex(Drop.Tip) ? FloorHeights[Drop.Tip] : 0.0f;
		if (Drop.Phase == 1 && Drop.Location.Z <= Floor + 1.0f)
		{
			// bursts on the floor: a few droplets jump up and out
			const FVector Splash(Drop.Location.X, Drop.Location.Y, Floor + 1.0f);
			const int32 Tip = Drop.Tip;
			const float Size = Drop.Size;
			Drops.RemoveAtSwap(Index);
			for (int32 Droplet = 0; Droplet < SplashDroplets && Drops.Num() < MaxDrops; ++Droplet)
			{
				const float Angle = FMath::FRandRange(0.0f, UE_TWO_PI);
				FDrop& Bit = Drops.AddDefaulted_GetRef();
				Bit.Tip = Tip;
				Bit.Phase = 2;
				Bit.Location = Splash;
				Bit.Size = Size * FMath::FRandRange(0.35f, 0.55f);
				Bit.Velocity = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * FMath::FRandRange(60.0f, 130.0f) + FVector(0.0f, 0.0f, FMath::FRandRange(110.0f, 190.0f));
			}
			continue;
		}
		if (Drop.Phase == 2 && (Drop.Age >= DropletLife || Drop.Location.Z < Floor - 2.0f))
		{
			Drops.RemoveAtSwap(Index);
		}
	}

	// the instances: one per drop, a swelling drop grows, a falling one is stretched by its speed
	TArray<FTransform> Transforms;
	Transforms.Reserve(Drops.Num());
	for (const FDrop& Drop : Drops)
	{
		float Scale = Drop.Size / SphereSize;
		FVector Stretch(1.0f);
		if (Drop.Phase == 0)
		{
			Scale *= FMath::Clamp(Drop.Age / SwellTime, 0.2f, 1.0f);
			Stretch.Z = 1.0f + 0.4f * Drop.Age / SwellTime;
		}
		else if (Drop.Phase == 1)
		{
			Stretch.Z = FMath::Clamp(1.0f + FMath::Abs(Drop.Velocity.Z) / 500.0f, 1.0f, 3.2f);
			Stretch.X = Stretch.Y = 0.8f;
		}
		else
		{
			Scale *= 1.0f - Drop.Age / DropletLife * 0.6f;
		}
		Transforms.Add(FTransform(FQuat::Identity, Drop.Location - FVector(0.0f, 0.0f, Drop.Phase == 0 ? Drop.Size * 0.4f : 0.0f), Stretch * Scale));
	}
	const FTransform Nothing(FQuat::Identity, GetActorLocation(), FVector::ZeroVector);
	while (Transforms.Num() < Mesh->GetInstanceCount())
	{
		Transforms.Add(Nothing);
	}
	Transforms.SetNum(Mesh->GetInstanceCount());
	if (!Transforms.IsEmpty())
	{
		Mesh->BatchUpdateInstancesTransforms(0, Transforms, true, false, true);
		bShown = true;
	}
}
