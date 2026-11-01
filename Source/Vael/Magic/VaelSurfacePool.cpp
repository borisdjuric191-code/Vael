// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelSurfacePool.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "World/VaelGround.h"

namespace
{
	/** Rings and pieces of the disc; the outer edge of the mesh lies past the pool so its torn edge fits */
	constexpr int32 PoolRings = 9;
	constexpr int32 PoolSegments = 28;
	constexpr float PoolMeshReach = 1.3f;

	/** The surface lies this far above the ground, in cm */
	constexpr float PoolLift = 2.5f;

	/** Seconds it takes to well out and to sink away */
	constexpr float PoolGrowSeconds = 0.7f;
	constexpr float PoolSinkSeconds = 0.9f;

	/** Where the torn edge reaches at full size, in units of the radius of the UVs */
	constexpr float PoolFullGrowth = 1.0f;

	const FName PoolGrowthParameter(TEXT("Wachstum"));
}

UVaelSurfacePoolComponent::UVaelSurfacePoolComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

bool UVaelSurfacePoolComponent::Spread(UMaterialInterface* Material, float InRadius, float InLifetime)
{
	if (Material == nullptr || Mesh != nullptr)
	{
		return false;
	}

	Lifetime = InLifetime;

	Liquid = UMaterialInstanceDynamic::Create(Material, this);
	Liquid->SetScalarParameterValue(PoolGrowthParameter, 0.0f);

	Mesh = NewObject<UProceduralMeshComponent>(GetOwner());
	Mesh->SetupAttachment(this);
	Mesh->SetUsingAbsoluteScale(true);
	Mesh->SetUsingAbsoluteRotation(true);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->bReceivesDecals = false;
	Mesh->RegisterComponent();

	// A disc of rings that lies on the ground, however it runs
	const FVector Center = GetComponentLocation();
	const float Reach = InRadius * PoolMeshReach;
	const auto PlaceOf = [&](const FVector2D& Spot)
	{
		FHitResult GroundHit;
		const FVector Above = Center + FVector(Spot.X, Spot.Y, 120.0f);
		const float Ground = VaelGround::TraceGround(GetWorld(), Above, Above - FVector(0.0f, 0.0f, 360.0f), GroundHit, GetOwner()) ? GroundHit.Location.Z - Center.Z : 0.0f;
		return FVector(Spot.X, Spot.Y, Ground + PoolLift);
	};

	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<int32> Triangles;

	Vertices.Add(PlaceOf(FVector2D::ZeroVector));
	Normals.Add(FVector::UpVector);
	UVs.Add(FVector2D::ZeroVector);

	for (int32 Ring = 1; Ring <= PoolRings; ++Ring)
	{
		const float Share = Ring / static_cast<float>(PoolRings);
		for (int32 Segment = 0; Segment < PoolSegments; ++Segment)
		{
			const float Angle = Segment * UE_TWO_PI / PoolSegments;
			const FVector2D Along(FMath::Cos(Angle), FMath::Sin(Angle));
			Vertices.Add(PlaceOf(Along * Reach * Share));
			Normals.Add(FVector::UpVector);
			UVs.Add(Along * PoolMeshReach * Share);
		}
	}

	for (int32 Segment = 0; Segment < PoolSegments; ++Segment)
	{
		Triangles.Append({ 0, 1 + (Segment + 1) % PoolSegments, 1 + Segment });
	}
	for (int32 Ring = 0; Ring < PoolRings - 1; ++Ring)
	{
		for (int32 Segment = 0; Segment < PoolSegments; ++Segment)
		{
			const int32 Next = (Segment + 1) % PoolSegments;
			const int32 A = 1 + Ring * PoolSegments + Segment;
			const int32 B = 1 + Ring * PoolSegments + Next;
			const int32 C = A + PoolSegments;
			const int32 D = B + PoolSegments;
			Triangles.Append({ A, B, C, B, D, C });
		}
	}

	Mesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
	Mesh->SetMaterial(0, Liquid);

	SetComponentTickEnabled(true);
	return true;
}

void UVaelSurfacePoolComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	Age += DeltaTime;

	const float Grown = FMath::Clamp(Age / PoolGrowSeconds, 0.0f, 1.0f);
	const float Left = Lifetime > 0.0f ? FMath::Clamp((Lifetime - Age) / PoolSinkSeconds, 0.0f, 1.0f) : 1.0f;
	Liquid->SetScalarParameterValue(PoolGrowthParameter, PoolFullGrowth * FMath::InterpEaseOut(0.0f, 1.0f, Grown, 2.0f) * Left);
}
