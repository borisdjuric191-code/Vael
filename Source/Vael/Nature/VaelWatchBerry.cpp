// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nature/VaelWatchBerry.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "World/VaelTimeOfDay.h"

namespace
{
	/** Beyond this many watch ranges the beams rest and only the clock is followed, once a second */
	constexpr float AwakeRanges = 2.5f;

	/** A watched player is looked at this high above his feet, in cm */
	constexpr float WatchHeight = 70.0f;

	/** Sweep speed in radians per second */
	constexpr float SweepSpeed = 0.35f;
}

AVaelWatchBerry::AVaelWatchBerry()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 1.0f;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Shrub = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shrub"));
	Shrub->SetupAttachment(Root);
	Shrub->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Shrub->SetGenerateOverlapEvents(false);

	Trunk = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Trunk"));
	Trunk->SetupAttachment(Root);
	Trunk->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Trunk->SetGenerateOverlapEvents(false);

	Beams = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Beams"));
	Beams->SetupAttachment(Root);
	Beams->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Beams->SetGenerateOverlapEvents(false);
	Beams->SetCastShadow(false);
	Beams->NumCustomDataFloats = 1;
}

void AVaelWatchBerry::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildShrub();
}

void AVaelWatchBerry::BuildShrub()
{
	Shrub->SetStaticMesh(ShrubMesh);
	Trunk->SetCapsuleSize(TrunkSize.X, TrunkSize.Y);
	Trunk->SetRelativeLocation(FVector(0.0f, 0.0f, TrunkSize.Y));

	if (ShrubMaterial != nullptr)
	{
		ShrubMaterialInstance = UMaterialInstanceDynamic::Create(ShrubMaterial, this);
		ShrubMaterialInstance->SetScalarParameterValue(TEXT("Versatz"), PhaseShift);
		Shrub->SetMaterial(0, ShrubMaterialInstance);
	}

	Beams->ClearInstances();
	Beams->SetStaticMesh(BeamMesh);
	if (BeamMaterial != nullptr)
	{
		BeamMaterialInstance = UMaterialInstanceDynamic::Create(BeamMaterial, this);
		Beams->SetMaterial(0, BeamMaterialInstance);
	}
	LastNight = -1.0f;

	RestDirections.Reset();
	Directions.Reset();
	if (BeamMesh == nullptr)
	{
		return;
	}
	for (int32 Index = 0; Index < Eyes.Num(); ++Index)
	{
		// Out of the eye, bent down towards the ground so the beams light the path below the shrub
		const FVector Rest = FMath::Lerp(Eyes[Index].Direction.GetSafeNormal(), FVector::DownVector, BeamDroop).GetSafeNormal();
		RestDirections.Add(Rest);
		Directions.Add(Rest);
		const int32 Instance = Beams->AddInstance(BeamTransform(Index, Rest));
		Beams->SetCustomDataValue(Instance, 0, FMath::Frac(Eyes[Index].Phase + PhaseShift), false);
	}
	Beams->MarkRenderStateDirty();
}

FTransform AVaelWatchBerry::BeamTransform(int32 Index, const FVector& Direction) const
{
	// The cone's tip sits in the eye and its axis (-Z of the mesh, tip to base) runs along the look
	const FQuat Turn = FQuat::FindBetweenNormals(FVector::DownVector, Direction);
	const float Long = BeamLength / BeamMeshLength;
	const float Wide = BeamLength * FMath::Tan(FMath::DegreesToRadians(BeamHalfAngle)) / BeamMeshRadius;
	const FVector Tip = Turn.RotateVector(FVector(0.0f, 0.0f, BeamTipZ * Long));
	return FTransform(Turn, Eyes[Index].Location - Tip, FVector(Wide, Wide, Long));
}

void AVaelWatchBerry::BeginPlay()
{
	Super::BeginPlay();
	// Construction results are saved with the level, the dynamic materials are not
	BuildShrub();
	Clock = PhaseShift * 20.0f;
}

APawn* AVaelWatchBerry::FindNearestPlayer(float& OutDistance) const
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

void AVaelWatchBerry::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Night = UVaelTimeOfDay::NightFactorOf(this);
	if (FMath::Abs(Night - LastNight) > 0.01f)
	{
		LastNight = Night;
		if (ShrubMaterialInstance != nullptr)
		{
			ShrubMaterialInstance->SetScalarParameterValue(TEXT("Nacht"), Night);
		}
		if (BeamMaterialInstance != nullptr)
		{
			BeamMaterialInstance->SetScalarParameterValue(TEXT("Nacht"), Night);
		}
	}

	float Distance = 0.0f;
	const APawn* Player = FindNearestPlayer(Distance);
	const bool bAwake = Player != nullptr && Distance < WatchRange * AwakeRanges;
	SetActorTickInterval(bAwake ? 0.033f : 1.0f);
	if (!bAwake || Directions.Num() != Beams->GetInstanceCount())
	{
		return;
	}

	Clock += DeltaSeconds;
	Watch = FMath::FInterpTo(Watch, Distance < WatchRange ? 1.0f : 0.0f, DeltaSeconds, 1.5f);
	const FVector Target = GetActorTransform().InverseTransformPosition(Player->GetActorLocation() + FVector(0.0f, 0.0f, WatchHeight));

	TArray<FTransform> Transforms;
	Transforms.Reserve(Directions.Num());
	for (int32 Index = 0; Index < Directions.Num(); ++Index)
	{
		const float Phase = Eyes[Index].Phase * UE_TWO_PI;
		// Calm: each beam sweeps slowly around its resting direction
		const FQuat Sweep(FVector::UpVector, FMath::DegreesToRadians(SweepAngle) * FMath::Sin(Clock * SweepSpeed + Phase));
		FVector Wanted = Sweep.RotateVector(RestDirections[Index]);

		// Watched: over to the player, but never further than MaxTurn from where the eye looks
		if (Watch > 0.01f)
		{
			const FVector ToPlayer = (Target - Eyes[Index].Location).GetSafeNormal();
			const float Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(RestDirections[Index], ToPlayer), -1.0f, 1.0f));
			const float Limit = FMath::DegreesToRadians(MaxTurn);
			const FVector Allowed = Angle > Limit
				? FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(RestDirections[Index], ToPlayer), Limit / Angle).RotateVector(RestDirections[Index])
				: ToPlayer;
			Wanted = FMath::Lerp(Wanted, Allowed, Watch).GetSafeNormal();
		}
		Directions[Index] = FMath::VInterpTo(Directions[Index], Wanted, DeltaSeconds, FollowSpeed).GetSafeNormal();
		Transforms.Add(BeamTransform(Index, Directions[Index]));
	}
	Beams->BatchUpdateInstancesTransforms(0, Transforms, false, true, false);
}
