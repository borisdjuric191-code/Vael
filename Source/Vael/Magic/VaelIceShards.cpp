// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelIceShards.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Magic/VaelGroundArea.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelWaterPuddle.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Vael.h"
#include "VaelAssets.h"
#include "World/VaelFireSpot.h"

namespace
{
	/** Size of the engine cone in cm; seconds between two looks for fire */
	constexpr float IceConeSize = 100.0f;
	constexpr float IceFireCheckInterval = 0.4f;
}

AVaelIceShards::AVaelIceShards()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AVaelIceShards* AVaelIceShards::Spawn(UWorld* World, const FVector& Ground, float InRadius, APawn* InInstigator)
{
	if (World == nullptr)
	{
		return nullptr;
	}
	FActorSpawnParameters Parameters;
	Parameters.Instigator = InInstigator;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVaelIceShards* Ice = World->SpawnActor<AVaelIceShards>(AVaelIceShards::StaticClass(), FTransform(Ground), Parameters);
	if (Ice != nullptr)
	{
		Ice->Radius = InRadius;
		Ice->Build();
	}
	return Ice;
}

void AVaelIceShards::Build()
{
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	Shards = NewObject<UInstancedStaticMeshComponent>(this);
	Shards->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone")));
	Shards->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Shards->SetCastShadow(false);
	Shards->SetupAttachment(RootComponent);
	Shards->SetUsingAbsoluteScale(true);
	Shards->RegisterComponent();
	if (UMaterialInterface* IceMaterial = VaelAssets::LoadOptional(MagicSettings->HailIceMaterial))
	{
		UMaterialInstanceDynamic* Ice = UMaterialInstanceDynamic::Create(IceMaterial, this);
		Ice->SetVectorParameterValue(TEXT("Farbe"), MagicSettings->HailColor);
		Ice->SetScalarParameterValue(TEXT("Leuchten"), 0.8f);
		Shards->SetMaterial(0, Ice);
	}

	// Broken lances lying every which way, half sunk in the ground, thicker towards the middle
	for (int32 Index = 0; Index < MagicSettings->IceShardCount; ++Index)
	{
		const FVector2D Spot = FMath::RandPointInCircle(Radius);
		const float Length = FMath::FRandRange(8.0f, 22.0f);
		const float Width = FMath::FRandRange(3.0f, 6.0f);
		const FRotator Lie(FMath::FRandRange(-100.0f, -60.0f), FMath::FRandRange(0.0f, 360.0f), FMath::FRandRange(0.0f, 360.0f));
		const FTransform Shard(Lie, GetActorLocation() + FVector(Spot.X, Spot.Y, 2.0f), FVector(Width, Width, Length) / IceConeSize);
		Rest.Add(Shard);
		Shards->AddInstance(Shard, true);
	}
}

bool AVaelIceShards::IsFireClose() const
{
	const UWorld* World = GetWorld();
	const float Reach = UVaelMagicSettings::Get()->IceMeltFireReach;
	if (AVaelGroundArea::IsFireNear(World, GetActorLocation(), Reach))
	{
		return true;
	}
	for (TActorIterator<AVaelFireSpot> It(World); It; ++It)
	{
		if (FVector::Dist(It->GetActorLocation(), GetActorLocation()) <= Reach + Radius)
		{
			return true;
		}
	}
	return false;
}

void AVaelIceShards::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	Age += DeltaSeconds;

	// Fire close by sets it melting
	NextFireCheck -= DeltaSeconds;
	if (MeltAge < 0.0f && NextFireCheck <= 0.0f)
	{
		NextFireCheck = IceFireCheckInterval;
		if (IsFireClose())
		{
			MeltAge = 0.0f;
		}
	}

	// Melting in fire it shrinks in moments and runs together into a puddle; left alone it thaws slowly and seeps away
	float Size = 1.0f;
	if (MeltAge >= 0.0f)
	{
		MeltAge += DeltaSeconds;
		Size = 1.0f - FMath::Clamp(MeltAge / MagicSettings->IceMeltSeconds, 0.0f, 1.0f);
		if (Size <= 0.0f)
		{
			AVaelWaterPuddle::AddSplash(GetWorld(), GetActorLocation() + FVector(0.0f, 0.0f, 30.0f), MagicSettings->IceMeltWater, nullptr, GetInstigator());
			UE_LOG(LogVael, Log, TEXT("Hail ice melts into a puddle at %s"), *GetActorLocation().ToCompactString());
			Destroy();
			return;
		}
	}
	else
	{
		const float ThawStart = MagicSettings->IceLieSeconds * 0.7f;
		Size = Age < ThawStart ? 1.0f : 1.0f - FMath::Clamp((Age - ThawStart) / (MagicSettings->IceLieSeconds - ThawStart), 0.0f, 1.0f);
		if (Size <= 0.0f)
		{
			Destroy();
			return;
		}
	}

	if (Size < 1.0f)
	{
		TArray<FTransform> Shrunk = Rest;
		for (FTransform& Shard : Shrunk)
		{
			Shard.SetScale3D(Shard.GetScale3D() * Size);
		}
		Shards->BatchUpdateInstancesTransforms(0, Shrunk, true, true, true);
	}
}
