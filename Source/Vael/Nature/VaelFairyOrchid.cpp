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
	/** The fairy mesh looks along +Y, an actor along +X */
	constexpr float MeshYaw = -90.0f;

	/** The middle of her body above her feet in the mesh, in cm: she turns and leans around it */
	constexpr float BodyHeight = 34.0f;

	/** How hard she is pulled to her target and how far from it she starts to slow down, in cm */
	constexpr float Steering = 6.0f;
	constexpr float BrakeDistance = 90.0f;

	/** Closer than this she has arrived, in cm */
	constexpr float ArriveDistance = 14.0f;

	/** She arcs up by this much on the way to another blossom, in cm */
	constexpr float ArcHeight = 45.0f;

	/** Lean into the flight and into a turn at top speed, in degrees */
	constexpr float FlightLean = 48.0f;
	constexpr float TurnLean = 30.0f;

	/** How fast she turns, 1/s */
	constexpr float TurnSpeed = 6.0f;

	/** Hovering she drifts on a small figure of eight, in cm and radians per second */
	constexpr float DriftReach = 5.0f;
	constexpr float DriftSpeed = 1.4f;

	/** Wing angles in radians, 0 = as modelled, positive = forward: the middle of the beat and how far it swings */
	constexpr float BeatMiddle = -0.3f;
	constexpr float BeatSwing = 0.55f;

	/** The lower wings follow the upper ones by this much of a beat */
	constexpr float LowerWingLag = 0.9f;

	/** Legs trailing behind at top speed and hanging a little behind while hovering, and their slow swing, in radians */
	constexpr float LegTrailFlight = 0.95f;
	constexpr float LegTrailHover = 0.12f;
	constexpr float LegSwing = 0.13f;
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
		const float Drift = Clock * DriftSpeed + Fairy.Seed;
		Goal += FVector(FMath::Sin(Drift), FMath::Sin(Drift * 0.5f), FMath::Sin(Drift * 1.7f)) * DriftReach;
	}

	// Heads for the goal at full speed and slows down on the last stretch
	const FVector ToGoal = Goal - Fairy.Location;
	const float Speed = FlySpeed * FMath::Min(ToGoal.Size() / BrakeDistance, 1.0f);
	const FVector Wanted = ToGoal.GetSafeNormal() * Speed;
	const FVector Before = Fairy.Velocity;
	Fairy.Velocity = FMath::VInterpTo(Fairy.Velocity, Wanted, DeltaSeconds, Steering);
	Fairy.Location += Fairy.Velocity * DeltaSeconds;

	// Looks where she flies, at her blossom while hovering, at the player if one comes close
	const float SpeedShare = FMath::Min(Fairy.Velocity.Size() / FlySpeed, 1.0f);
	FVector Look = Fairy.Facing;
	if (SpeedShare > 0.25f)
	{
		Look = Fairy.Velocity;
	}
	else if (WatchedPlace != nullptr)
	{
		Look = *WatchedPlace - Fairy.Location;
	}
	const float OldYaw = Fairy.Yaw;
	Fairy.Yaw = FMath::RInterpTo(FRotator(0.0f, Fairy.Yaw, 0.0f), FRotator(0.0f, Look.Rotation().Yaw, 0.0f), DeltaSeconds, TurnSpeed).Yaw;

	// Leans forward into the flight and sideways into the turn
	const FVector Forward = FRotator(0.0f, Fairy.Yaw, 0.0f).Vector();
	const float Along = FVector::DotProduct(Fairy.Velocity, Forward) / FlySpeed;
	const float Turning = DeltaSeconds > 0.0f ? FMath::FindDeltaAngleDegrees(OldYaw, Fairy.Yaw) / DeltaSeconds / 180.0f : 0.0f;
	Fairy.Pitch = FMath::FInterpTo(Fairy.Pitch, -FMath::Clamp(Along, -0.5f, 1.0f) * FlightLean, DeltaSeconds, TurnSpeed);
	Fairy.Roll = FMath::FInterpTo(Fairy.Roll, FMath::Clamp(Turning, -1.0f, 1.0f) * TurnLean, DeltaSeconds, TurnSpeed);

	// Beats harder the more she speeds up
	const float Effort = FMath::Min(SpeedShare + (Fairy.Velocity - Before).Size() / FMath::Max(DeltaSeconds, 0.001f) / (FlySpeed * Steering), 1.0f);
	Fairy.BeatPhase = FMath::Fmod(Fairy.BeatPhase + DeltaSeconds * UE_TWO_PI * FMath::Lerp(BeatRate.X, BeatRate.Y, Effort), UE_TWO_PI);
}

void AVaelFairyOrchid::ShowFairy(int32 Index, bool bLast)
{
	const FFairy& Fairy = Flock[Index];

	// Turns around the middle of her body, her feet swing out behind her
	const FRotator Body(Fairy.Pitch, Fairy.Yaw, Fairy.Roll);
	const FQuat Turn = Body.Quaternion() * FRotator(0.0f, MeshYaw, 0.0f).Quaternion();
	const FVector Middle(0.0f, 0.0f, BodyHeight * FairyScale);
	const FVector Place = Fairy.Location + Middle - Turn.RotateVector(Middle);
	Fairies->UpdateInstanceTransform(Index, FTransform(Turn, Place, FVector(FairyScale)), false, false);

	const float SpeedShare = FMath::Min(Fairy.Velocity.Size() / FlySpeed, 1.0f);
	const float Swing = FMath::Sin(Clock * 2.2f + Fairy.Seed);
	Fairies->SetCustomDataValue(Index, 0, BeatMiddle + BeatSwing * FMath::Sin(Fairy.BeatPhase), false);
	Fairies->SetCustomDataValue(Index, 1, BeatMiddle + BeatSwing * FMath::Sin(Fairy.BeatPhase - LowerWingLag), false);
	Fairies->SetCustomDataValue(Index, 2, FMath::Lerp(LegTrailHover, LegTrailFlight, SpeedShare), false);
	Fairies->SetCustomDataValue(Index, 3, LegSwing * Swing, bLast);
}
