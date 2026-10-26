// Copyright Epic Games, Inc. All Rights Reserved.

#include "Floromant/VaelFootRootsComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "Floromant/VaelFloromantSettings.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "VaelAssets.h"
#include "World/VaelGround.h"

namespace
{
	/** Points along each root, and sides of its tube */
	constexpr int32 FootRootPoints = 8;
	constexpr int32 FootRootSides = 6;

	/** How deep the tips reach under the ground, in cm */
	constexpr float FootRootDepth = 15.0f;
}

void UVaelFootRootsComponent::SetGrowth(USkeletalMeshComponent* Body, float Growth)
{
	if (Growth <= 0.0f || Body == nullptr)
	{
		if (Mesh != nullptr && ShownGrowth != 0.0f)
		{
			Mesh->ClearAllMeshSections();
		}
		Roots.Reset();
		ShownGrowth = 0.0f;
		return;
	}

	if (Mesh == nullptr)
	{
		Mesh = NewObject<UProceduralMeshComponent>(GetOwner());
		Mesh->SetupAttachment(this);
		Mesh->SetUsingAbsoluteLocation(true);
		Mesh->SetUsingAbsoluteRotation(true);
		Mesh->SetUsingAbsoluteScale(true);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->bReceivesDecals = false;
		Mesh->RegisterComponent();
		Mesh->SetWorldTransform(FTransform::Identity);

		// The roots of the root wall, without the sway of the wind
		if (UMaterialInterface* Material = VaelAssets::LoadOptional(UVaelFloromantSettings::Get()->RootMaterial))
		{
			UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Material, this);
			Instance->SetScalarParameterValue(TEXT("WindStrength"), 0.0f);
			Mesh->SetMaterial(0, Instance);
		}
	}

	if (Roots.IsEmpty())
	{
		PlanRoots(Body);
	}

	if (!FMath::IsNearlyEqual(Growth, ShownGrowth, 0.01f) || (Growth >= 1.0f && ShownGrowth < 1.0f))
	{
		BuildMesh(Growth);
		ShownGrowth = Growth;
	}
}

void UVaelFootRootsComponent::PlanRoots(USkeletalMeshComponent* Body)
{
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	const FVector Middle = Body->GetComponentLocation();

	for (const TCHAR* FootName : { TEXT("foot_l"), TEXT("foot_r") })
	{
		const FName Foot(FootName);
		if (!Body->DoesSocketExist(Foot))
		{
			continue;
		}

		// From the sole of the foot down to the ground under it
		const FVector FootLocation = Body->GetSocketLocation(Foot);
		FHitResult Hit;
		const float GroundZ = VaelGround::TraceGround(GetWorld(), FootLocation + FVector(0.0f, 0.0f, 40.0f), FootLocation - FVector(0.0f, 0.0f, 120.0f), Hit, GetOwner())
			? Hit.ImpactPoint.Z : FootLocation.Z - 8.0f;
		const FVector Sole(FootLocation.X, FootLocation.Y, FMath::Max(GroundZ + 3.0f, FootLocation.Z - 6.0f));

		// Spread out to all sides, but mostly away from the body
		FVector Outward = (FootLocation - Middle) * FVector(1.0f, 1.0f, 0.0f);
		Outward = Outward.IsNearlyZero(1.0f) ? FVector::ForwardVector : Outward.GetSafeNormal();
		const float OutwardYaw = Outward.Rotation().Yaw;

		for (int32 Index = 0; Index < Settings->RootsPerFoot; ++Index)
		{
			const float Share = Settings->RootsPerFoot > 1 ? Index / static_cast<float>(Settings->RootsPerFoot - 1) : 0.5f;
			const float Yaw = OutwardYaw + FMath::Lerp(-100.0f, 100.0f, Share) + FMath::FRandRange(-25.0f, 25.0f);
			const FVector Direction = FRotator(0.0f, Yaw, 0.0f).Vector();
			const FVector Side(-Direction.Y, Direction.X, 0.0f);
			const float Length = Settings->RootLength * FMath::FRandRange(0.5f, 1.2f);
			const float Wiggle = FMath::FRandRange(3.0f, 8.0f);
			const float WiggleTurns = FMath::FRandRange(1.5f, 3.2f);
			const float WigglePhase = FMath::FRandRange(0.0f, 2.0f * UE_PI);

			// It creeps over the ground first and then dives in, crooked and knotty all the way, never like a claw
			FRootPath Root;
			Root.Radius = FMath::FRandRange(1.2f, 2.4f);
			for (int32 Point = 0; Point < FootRootPoints; ++Point)
			{
				const float T = Point / static_cast<float>(FootRootPoints - 1);
				const float Z = FMath::Lerp(Sole.Z, GroundZ - FootRootDepth, FMath::Pow(T, 2.2f)) + (Point > 0 ? FMath::FRandRange(-1.0f, 1.5f) : 0.0f);
				const FVector Bend = Side * FMath::Sin(T * WiggleTurns * UE_PI + WigglePhase) * Wiggle * T
					+ Side * FMath::FRandRange(-2.0f, 2.0f) * T;
				Root.Points.Add(FVector(Sole.X, Sole.Y, Z) + Direction * Length * FMath::Pow(T, 0.85f) + Bend);
			}
			Roots.Add(Root);
		}
	}
}

void UVaelFootRootsComponent::BuildMesh(float Growth)
{
	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<int32> Triangles;

	for (const FRootPath& Root : Roots)
	{
		// The root reaches as far as the growth: whole stretches and a part of the next, thinning to a tip at its end
		const float Reach = FMath::Clamp(Growth, 0.0f, 1.0f) * (Root.Points.Num() - 1);
		const int32 Whole = FMath::FloorToInt32(Reach);
		TArray<FVector> Path;
		for (int32 Point = 0; Point <= FMath::Min(Whole, Root.Points.Num() - 1); ++Point)
		{
			Path.Add(Root.Points[Point]);
		}
		if (Whole < Root.Points.Num() - 1 && Reach > Whole)
		{
			Path.Add(FMath::Lerp(Root.Points[Whole], Root.Points[Whole + 1], Reach - Whole));
		}
		if (Path.Num() < 2)
		{
			continue;
		}

		const int32 First = Vertices.Num();
		for (int32 Ring = 0; Ring < Path.Num(); ++Ring)
		{
			const FVector Along = (Path[FMath::Min(Ring + 1, Path.Num() - 1)] - Path[FMath::Max(Ring - 1, 0)]).GetSafeNormal();
			const FVector Across = FVector::CrossProduct(Along, FVector::UpVector).GetSafeNormal(KINDA_SMALL_NUMBER, FVector::RightVector);
			const FVector Up = FVector::CrossProduct(Across, Along);
			const float Share = Ring / static_cast<float>(Path.Num() - 1);
			const float Radius = Root.Radius * FMath::Lerp(1.0f, 0.15f, Share);

			for (int32 SideIndex = 0; SideIndex <= FootRootSides; ++SideIndex)
			{
				const float Angle = 2.0f * UE_PI * SideIndex / FootRootSides;
				const FVector Normal = Across * FMath::Cos(Angle) + Up * FMath::Sin(Angle);
				Vertices.Add(Path[Ring] + Normal * Radius);
				Normals.Add(Normal);
				UVs.Add(FVector2D(SideIndex / static_cast<float>(FootRootSides), Share * 2.0f));
			}
		}

		const int32 RingSize = FootRootSides + 1;
		for (int32 Ring = 0; Ring < Path.Num() - 1; ++Ring)
		{
			for (int32 SideIndex = 0; SideIndex < FootRootSides; ++SideIndex)
			{
				const int32 A = First + Ring * RingSize + SideIndex;
				const int32 B = A + 1;
				const int32 C = A + RingSize;
				const int32 D = C + 1;
				Triangles.Append({ A, C, B, B, C, D });
			}
		}
	}

	Mesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
}
