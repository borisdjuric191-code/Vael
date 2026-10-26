// Copyright Epic Games, Inc. All Rights Reserved.

#include "Floromant/VaelRootRunner.h"
#include "Components/PointLightComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "Floromant/VaelFloromantSettings.h"
#include "GameFramework/Pawn.h"
#include "Magic/VaelDebrisBurst.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "VaelAssets.h"
#include "World/VaelGround.h"

namespace
{
	/** Length of a step of the path in cm, sides of the tube, its radius at the foot */
	constexpr float RunnerStep = 35.0f;
	constexpr int32 RunnerSides = 6;
	constexpr float RunnerRadius = 4.5f;

	/** Seconds the root stays after it arrived, and seconds it takes to sink back */
	constexpr float RunnerStay = 1.2f;
	constexpr float RunnerSink = 0.6f;
}

AVaelRootRunner::AVaelRootRunner()
{
	PrimaryActorTick.bCanEverTick = true;
	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Root"));
	RootComponent = Mesh;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->bReceivesDecals = false;
	Mesh->SetUsingAbsoluteLocation(true);
	Mesh->SetUsingAbsoluteRotation(true);
	Mesh->SetUsingAbsoluteScale(true);

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Mesh);
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(0.0f);
	Light->SetAttenuationRadius(260.0f);
	Light->SetCastShadows(false);
}

AVaelRootRunner* AVaelRootRunner::Run(APawn* Gardener, const FVector& From, const FVector& To, float Speed, const FLinearColor& Color, TFunction<void()> OnArrive)
{
	UWorld* World = Gardener != nullptr ? Gardener->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}
	FActorSpawnParameters Parameters;
	Parameters.Owner = Gardener;
	Parameters.Instigator = Gardener;
	AVaelRootRunner* Runner = World->SpawnActor<AVaelRootRunner>(AVaelRootRunner::StaticClass(), FTransform::Identity, Parameters);
	if (Runner == nullptr)
	{
		if (OnArrive)
		{
			OnArrive();
		}
		return nullptr;
	}
	Runner->Mesh->SetWorldTransform(FTransform::Identity);
	Runner->Speed = FMath::Max(Speed, 100.0f);
	Runner->Color = Color;
	Runner->Arrive = MoveTemp(OnArrive);
	Runner->Light->SetLightColor(Color);
	Runner->PlanPath(From, To);

	if (UMaterialInterface* Material = VaelAssets::LoadOptional(UVaelFloromantSettings::Get()->RootMaterial))
	{
		UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Material, Runner);
		Instance->SetScalarParameterValue(TEXT("WindStrength"), 0.0f);
		Instance->SetVectorParameterValue(TEXT("GrowColor"), Color);
		Runner->Mesh->SetMaterial(0, Instance);
	}
	return Runner;
}

void AVaelRootRunner::PlanPath(const FVector& From, const FVector& To)
{
	// Over the ground in short steps, half sunk, wandering a little to the sides like a real root
	const FVector Flat = (To - From) * FVector(1.0f, 1.0f, 0.0f);
	const float Distance = Flat.Size();
	const int32 Steps = FMath::Max(2, FMath::CeilToInt32(Distance / RunnerStep));
	const FVector Side = FVector::CrossProduct(Flat.GetSafeNormal(), FVector::UpVector);
	const float Phase = FMath::FRandRange(0.0f, 2.0f * UE_PI);
	float Length = 0.0f;
	for (int32 Step = 0; Step <= Steps; ++Step)
	{
		const float T = Step / static_cast<float>(Steps);
		FVector Point = FMath::Lerp(From, To, T) + Side * FMath::Sin(T * 3.0f * UE_PI + Phase) * FMath::Min(Distance * 0.06f, 60.0f) * FMath::Sin(T * UE_PI);
		FHitResult Hit;
		if (VaelGround::TraceGround(GetWorld(), Point + FVector(0.0f, 0.0f, 150.0f), Point - FVector(0.0f, 0.0f, 300.0f), Hit, GetInstigator()))
		{
			Point = Hit.ImpactPoint;
		}
		Point.Z += FMath::FRandRange(-1.5f, 1.5f);
		if (!Path.IsEmpty())
		{
			Length += FVector::Dist(Path.Last(), Point);
		}
		Path.Add(Point);
		PathLengths.Add(Length);
	}
}

void AVaelRootRunner::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Total = PathLengths.IsEmpty() ? 0.0f : PathLengths.Last();
	if (SinceArrival < 0.0f)
	{
		Travelled = FMath::Min(Total, Travelled + Speed * DeltaSeconds);

		// Earth flies up at the front as it breaks through the ground
		int32 Segment = 0;
		while (Segment + 1 < PathLengths.Num() && PathLengths[Segment + 1] < Travelled)
		{
			++Segment;
		}
		const float Span = Segment + 1 < PathLengths.Num() ? FMath::Max(PathLengths[Segment + 1] - PathLengths[Segment], 0.01f) : 1.0f;
		const FVector Front = Segment + 1 < Path.Num() ? FMath::Lerp(Path[Segment], Path[Segment + 1], (Travelled - PathLengths[Segment]) / Span) : Path.Last();
		Light->SetWorldLocation(Front + FVector(0.0f, 0.0f, 25.0f));
		Light->SetIntensity(40.0f);
		SinceDust += DeltaSeconds;
		if (SinceDust > 0.07f)
		{
			SinceDust = 0.0f;
			FVaelDebris Earth;
			Earth.Count = 4;
			Earth.Spread = 12.0f;
			Earth.Speed = 70.0f;
			Earth.Lift = 150.0f;
			Earth.Lifetime = 0.6f;
			Earth.MinSize = 2.0f;
			Earth.MaxSize = 5.0f;
			Earth.Color = FLinearColor(0.15f, 0.11f, 0.08f);
			AVaelDebrisBurst::Spawn(this, Front, Earth);
		}

		BuildMesh(Travelled, 1.0f);
		if (Travelled >= Total)
		{
			SinceArrival = 0.0f;
			if (Arrive)
			{
				Arrive();
				Arrive = nullptr;
			}
		}
		return;
	}

	// It stays a moment, glowing out, then sinks back into the ground
	SinceArrival += DeltaSeconds;
	Light->SetIntensity(FMath::Max(0.0f, 40.0f * (1.0f - SinceArrival / RunnerStay)));
	if (SinceArrival > RunnerStay)
	{
		const float Sink = (SinceArrival - RunnerStay) / RunnerSink;
		if (Sink >= 1.0f)
		{
			Destroy();
			return;
		}
		BuildMesh(Total, 1.0f - Sink);
	}
}

void AVaelRootRunner::BuildMesh(float Front, float Thickness)
{
	TArray<FVector> Points;
	for (int32 Index = 0; Index < Path.Num() && PathLengths[Index] <= Front; ++Index)
	{
		Points.Add(Path[Index]);
	}
	if (Points.Num() < Path.Num() && Points.Num() > 0)
	{
		const int32 Next = Points.Num();
		const float Span = FMath::Max(PathLengths[Next] - PathLengths[Next - 1], 0.01f);
		Points.Add(FMath::Lerp(Path[Next - 1], Path[Next], (Front - PathLengths[Next - 1]) / Span));
	}
	if (Points.Num() < 2)
	{
		Mesh->ClearAllMeshSections();
		return;
	}

	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<int32> Triangles;
	for (int32 Ring = 0; Ring < Points.Num(); ++Ring)
	{
		const FVector Along = (Points[FMath::Min(Ring + 1, Points.Num() - 1)] - Points[FMath::Max(Ring - 1, 0)]).GetSafeNormal();
		const FVector Across = FVector::CrossProduct(Along, FVector::UpVector).GetSafeNormal(KINDA_SMALL_NUMBER, FVector::RightVector);
		const FVector Up = FVector::CrossProduct(Across, Along);

		// Thick at her feet, thin towards the front, swollen where it breaks through
		const float Share = Ring / static_cast<float>(Points.Num() - 1);
		const float Radius = RunnerRadius * Thickness * FMath::Lerp(1.0f, 0.45f, Share) * (Ring == Points.Num() - 1 ? 0.3f : 1.0f);
		for (int32 SideIndex = 0; SideIndex <= RunnerSides; ++SideIndex)
		{
			const float Angle = 2.0f * UE_PI * SideIndex / RunnerSides;
			const FVector Normal = Across * FMath::Cos(Angle) + Up * FMath::Sin(Angle);
			Vertices.Add(Points[Ring] + Normal * Radius - FVector(0.0f, 0.0f, Radius * 0.4f));
			Normals.Add(Normal);
			UVs.Add(FVector2D(SideIndex / static_cast<float>(RunnerSides), Ring * 0.25f));
		}
	}
	const int32 RingSize = RunnerSides + 1;
	for (int32 Ring = 0; Ring < Points.Num() - 1; ++Ring)
	{
		for (int32 SideIndex = 0; SideIndex < RunnerSides; ++SideIndex)
		{
			const int32 A = Ring * RingSize + SideIndex;
			Triangles.Append({ A, A + RingSize, A + 1, A + 1, A + RingSize, A + RingSize + 1 });
		}
	}
	Mesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
}
