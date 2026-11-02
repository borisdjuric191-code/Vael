// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nature/VaelFairyOrchid.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
	/** Closer than this she has arrived, in cm */
	constexpr float ArriveDistance = 14.0f;

	/** She arcs up by this much on the way to another blossom, in cm */
	constexpr float ArcHeight = 45.0f;
}

AVaelFairyOrchid::AVaelFairyOrchid()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 1.0f;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Plant = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Plant"));
	Plant->SetupAttachment(Root);
	Plant->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Plant->SetGenerateOverlapEvents(false);

	Trunk = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Trunk"));
	Trunk->SetupAttachment(Root);
	Trunk->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Trunk->SetGenerateOverlapEvents(false);

	Fairies = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Fairies"));
	Fairies->SetupAttachment(Root);
	Fairies->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Fairies->SetGenerateOverlapEvents(false);
	Fairies->NumCustomDataFloats = 4;
}

void AVaelFairyOrchid::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildOrchid();
}

void AVaelFairyOrchid::BuildOrchid()
{
	Plant->SetStaticMesh(PlantMesh);
	if (PlantMaterial != nullptr)
	{
		Plant->SetMaterial(0, PlantMaterial);
	}
	Trunk->SetCapsuleSize(TrunkSize.X, TrunkSize.Y);
	Trunk->SetRelativeLocation(FVector(0.0f, 0.0f, TrunkSize.Y));

	Fairies->ClearInstances();
	Fairies->SetHasPerInstancePrevTransforms(true);
	Fairies->SetStaticMesh(FairyMesh);
	if (FairyMaterial != nullptr)
	{
		Fairies->SetMaterial(0, FairyMaterial);
	}

	Flock.Reset();
	if (FairyMesh == nullptr || Blossoms.Num() == 0)
	{
		return;
	}
	FRandomStream Random(GetTypeHash(GetActorLocation().ToString()));
	for (int32 Index = 0; Index < FairyCount; ++Index)
	{
		const int32 Blossom = PickBlossom(INDEX_NONE);
		FFairy& Fairy = Flock.AddDefaulted_GetRef();
		Fairy.Blossom = Blossom;
		Fairy.Target = HoverPoint(Fairy.Blossom);
		Fairy.Location = Fairy.Target;
		Fairy.Facing = -Blossoms[Fairy.Blossom].Direction;
		Fairy.Yaw = Fairy.Facing.Rotation().Yaw;
		Fairy.Seconds = Random.FRandRange(HoverTime.X, HoverTime.Y);
		Fairy.Seed = Random.FRandRange(0.0f, UE_TWO_PI);
		Fairy.BeatPhase = Fairy.Seed;
		Fairies->AddInstance(FTransform::Identity);
		ShowFairy(Index, Index == FairyCount - 1);
	}
}

FVector AVaelFairyOrchid::HoverPoint(int32 Index) const
{
	const FVaelOrchidBlossom& Blossom = Blossoms[Index];
	return Blossom.Location + Blossom.Direction.GetSafeNormal() * HoverGap;
}

int32 AVaelFairyOrchid::PickBlossom(int32 Except) const
{
	TArray<int32> Free;
	for (int32 Index = 0; Index < Blossoms.Num(); ++Index)
	{
		const bool bTaken = Flock.ContainsByPredicate([Index](const FFairy& Other) { return Other.Blossom == Index; });
		if (!bTaken && Index != Except)
		{
			Free.Add(Index);
		}
	}
	return Free.Num() > 0 ? Free[FMath::RandHelper(Free.Num())] : FMath::RandHelper(Blossoms.Num());
}

void AVaelFairyOrchid::BeginPlay()
{
	Super::BeginPlay();
	// The flock is not saved with the level
	BuildOrchid();
}

APawn* AVaelFairyOrchid::FindNearestPlayer(float& OutDistance) const
{
	APawn* Nearest = nullptr;
	OutDistance = TNumericLimits<float>::Max();
	if (const UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APawn* Pawn = It->IsValid() ? (*It)->GetPawn() : nullptr;
			if (Pawn == nullptr)
			{
				continue;
			}
			const float Distance = FVector::Dist2D(Pawn->GetActorLocation(), GetActorLocation());
			if (Distance < OutDistance)
			{
				OutDistance = Distance;
				Nearest = Pawn;
			}
		}
	}
	return Nearest;
}

void AVaelFairyOrchid::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	float Distance = 0.0f;
	const APawn* Player = FindNearestPlayer(Distance);
	const bool bAwake = Player != nullptr && Distance < AwakeRange;
	SetActorTickInterval(bAwake ? 0.0f : 1.0f);
	if (!bAwake || Flock.Num() != Fairies->GetInstanceCount())
	{
		return;
	}

	Clock += DeltaSeconds;
	FVector Watched = FVector::ZeroVector;
	const bool bWatch = Distance < WatchRange;
	if (bWatch)
	{
		Watched = GetActorTransform().InverseTransformPosition(Player->GetActorLocation());
	}
	for (int32 Index = 0; Index < Flock.Num(); ++Index)
	{
		MoveFairy(Flock[Index], DeltaSeconds, bWatch ? &Watched : nullptr);
		ShowFairy(Index, Index == Flock.Num() - 1);
	}
}

void AVaelFairyOrchid::MoveFairy(FFairy& Fairy, float DeltaSeconds, const FVector* WatchedPlace)
{
	Fairy.Seconds -= DeltaSeconds;
	if (!Fairy.bFlying && Fairy.Seconds <= 0.0f)
	{
		Fairy.Blossom = PickBlossom(Fairy.Blossom);
		Fairy.Target = HoverPoint(Fairy.Blossom);
		Fairy.Facing = -Blossoms[Fairy.Blossom].Direction;
		Fairy.bFlying = true;
	}

	FVector Goal = Fairy.Target;
	if (Fairy.bFlying)
	{
		// In a shallow arc, so she never cuts straight through the stem
		const float Left = FVector::Dist(Fairy.Location, Fairy.Target);
		Goal.Z += FMath::Min(Left * 0.5f, ArcHeight);
		if (Left < ArriveDistance)
		{
			Fairy.bFlying = false;
			Fairy.Seconds = FMath::FRandRange(HoverTime.X, HoverTime.Y);
		}
	}
	else
	{
		Goal += VaelFairyFlight::Drift(Fairy, Clock);
	}

	// Looks at her blossom while hovering, at the player if one comes close
	const FVector Look = WatchedPlace != nullptr ? *WatchedPlace - Fairy.Location : Fairy.Facing;
	VaelFairyFlight::Steer(Fairy, Goal, Look, DeltaSeconds, GetFlight());
}

VaelFairyFlight::FSettings AVaelFairyOrchid::GetFlight() const
{
	VaelFairyFlight::FSettings Flight;
	Flight.FlySpeed = FlySpeed;
	Flight.BeatRate = BeatRate;
	Flight.Scale = FairyScale;
	return Flight;
}

void AVaelFairyOrchid::ShowFairy(int32 Index, bool bLast)
{
	VaelFairyFlight::Show(Fairies, Index, Flock[Index], Clock, GetFlight(), bLast);
}
