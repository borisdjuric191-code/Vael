// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelWaterMesh.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "ProceduralMeshComponent.h"

void FVaelWaveSection::Reset(int32 NumVertices)
{
	Vertices.SetNumZeroed(NumVertices);
	Normals.SetNumZeroed(NumVertices);
	UVs.SetNumZeroed(NumVertices);
	Colors.SetNumZeroed(NumVertices);
}

void FVaelWaveSection::Apply(UProceduralMeshComponent* Mesh, int32 SectionIndex, bool bCreate) const
{
	const TArray<FProcMeshTangent> NoTangents;
	if (bCreate)
	{
		Mesh->CreateMeshSection_LinearColor(SectionIndex, Vertices, Triangles, Normals, UVs, Colors, NoTangents, false);
		return;
	}

	Mesh->UpdateMeshSection_LinearColor(SectionIndex, Vertices, Normals, UVs, Colors, NoTangents);
}

void FVaelWaterDrops::Init(int32 Num)
{
	Pool.SetNum(Num);
	for (FVaelWaveDrop& Drop : Pool)
	{
		Drop.Lifetime = 0.0f;
	}
}

FVaelWaveDrop* FVaelWaterDrops::FindFree()
{
	for (FVaelWaveDrop& Drop : Pool)
	{
		if (Drop.Lifetime <= 0.0f)
		{
			return &Drop;
		}
	}

	return nullptr;
}

bool FVaelWaterDrops::Throw(const FVector& Location, const FVector& Velocity, float Lifetime, float Size, bool bClump)
{
	FVaelWaveDrop* Drop = FindFree();
	if (Drop == nullptr)
	{
		return false;
	}

	Drop->Location = Location;
	Drop->Velocity = Velocity;
	Drop->Age = 0.0f;
	Drop->Lifetime = FMath::Max(Lifetime, 0.01f);
	Drop->Size = Size;
	Drop->bClump = bClump;
	return true;
}

void FVaelWaterDrops::Update(float DeltaSeconds, float GroundZ, float Gravity, float Drag)
{
	for (FVaelWaveDrop& Drop : Pool)
	{
		if (Drop.Lifetime <= 0.0f)
		{
			continue;
		}

		Drop.Velocity.Z -= Gravity * DeltaSeconds;
		Drop.Velocity *= FMath::Exp(-Drag * DeltaSeconds);
		Drop.Location += Drop.Velocity * DeltaSeconds;
		Drop.Age += DeltaSeconds;
		if (Drop.Age >= Drop.Lifetime || Drop.Location.Z < GroundZ - 5.0f)
		{
			Drop.Lifetime = 0.0f;
		}
	}
}

void FVaelWaterDrops::Build(FVaelWaveSection& Out, const FTransform& Transform, const FVector& CameraLocation, float Fade, float Foam) const
{
	// A quad facing the camera for every slot of the pool, stretched along the flight of the bigger lumps
	Out.Reset(Pool.Num() * 4);

	static const FVector2D Corners[4] = { FVector2D(0.0f, 0.0f), FVector2D(1.0f, 0.0f), FVector2D(1.0f, 1.0f), FVector2D(0.0f, 1.0f) };
	for (int32 DropIndex = 0; DropIndex < Pool.Num(); ++DropIndex)
	{
		const FVaelWaveDrop& Drop = Pool[DropIndex];
		const int32 First = DropIndex * 4;
		const FVector Center = Transform.InverseTransformPosition(Drop.Location);

		if (Drop.Lifetime <= 0.0f)
		{
			for (int32 Corner = 0; Corner < 4; ++Corner)
			{
				Out.Vertices[First + Corner] = Center;
				Out.UVs[First + Corner] = Corners[Corner];
				Out.Normals[First + Corner] = FVector::UpVector;
			}
			continue;
		}

		const FVector ToCamera = Transform.InverseTransformVectorNoScale(CameraLocation - Drop.Location).GetSafeNormal();
		const FVector Flight = Transform.InverseTransformVectorNoScale(Drop.Velocity);
		const FVector Along = Flight.GetSafeNormal().IsNearlyZero() ? FVector::UpVector : Flight.GetSafeNormal();
		FVector Side = FVector::CrossProduct(Along, ToCamera).GetSafeNormal();
		if (Side.IsNearlyZero())
		{
			Side = FVector::RightVector;
		}

		const float Stretch = 1.0f + Flight.Size() / (Drop.bClump ? 260.0f : 600.0f);
		const FVector HalfSide = Side * Drop.Size * 0.5f;
		const FVector HalfAlong = Along * Drop.Size * 0.5f * Stretch;
		const float Life = Drop.Age / Drop.Lifetime;
		const FLinearColor Color(1.0f, Drop.bClump ? FMath::Min(Foam + 0.25f, 1.0f) : Foam, 1.0f, Fade * FMath::Pow(1.0f - Life, 0.6f));

		const FVector Positions[4] = { Center - HalfSide - HalfAlong, Center + HalfSide - HalfAlong, Center + HalfSide + HalfAlong, Center - HalfSide + HalfAlong };
		for (int32 Corner = 0; Corner < 4; ++Corner)
		{
			Out.Vertices[First + Corner] = Positions[Corner];
			Out.UVs[First + Corner] = Corners[Corner];
			Out.Normals[First + Corner] = ToCamera;
			Out.Colors[First + Corner] = Color;
		}
	}
}

void FVaelWaterDrops::BuildTriangles(FVaelWaveSection& Out) const
{
	Out.Triangles.Reset();
	for (int32 DropIndex = 0; DropIndex < Pool.Num(); ++DropIndex)
	{
		const int32 First = DropIndex * 4;
		Out.Triangles.Append({ First, First + 2, First + 1, First, First + 3, First + 2 });
	}
}

namespace VaelWaterMesh
{
	float Noise(float X, float Y)
	{
		return FMath::Clamp(FMath::PerlinNoise2D(FVector2D(X, Y)) * 1.4f, -1.0f, 1.0f);
	}

	void ComputeGridNormals(const TArray<FVector>& Vertices, int32 Columns, int32 Rows, TArray<FVector>& OutNormals)
	{
		for (int32 Column = 0; Column < Columns; ++Column)
		{
			for (int32 Row = 0; Row < Rows; ++Row)
			{
				const FVector& Next = Vertices[FMath::Min(Column + 1, Columns - 1) * Rows + Row];
				const FVector& Previous = Vertices[FMath::Max(Column - 1, 0) * Rows + Row];
				const FVector& Up = Vertices[Column * Rows + FMath::Min(Row + 1, Rows - 1)];
				const FVector& Down = Vertices[Column * Rows + FMath::Max(Row - 1, 0)];

				const FVector Normal = FVector::CrossProduct(Up - Down, Next - Previous).GetSafeNormal();
				OutNormals[Column * Rows + Row] = Normal.IsNearlyZero() ? FVector::UpVector : Normal;
			}
		}
	}

	void BuildGridTriangles(int32 Columns, int32 Rows, TArray<int32>& OutTriangles)
	{
		OutTriangles.Reset();
		for (int32 Column = 0; Column < Columns - 1; ++Column)
		{
			for (int32 Row = 0; Row < Rows - 1; ++Row)
			{
				const int32 A = Column * Rows + Row;
				const int32 B = (Column + 1) * Rows + Row;
				OutTriangles.Append({ A, A + 1, B, B, A + 1, B + 1 });
			}
		}
	}

	FVector GetCameraLocation(const UWorld* World, const FVector& Fallback)
	{
		const APlayerController* PlayerController = World != nullptr ? World->GetFirstPlayerController() : nullptr;
		return PlayerController != nullptr && PlayerController->PlayerCameraManager != nullptr ? PlayerController->PlayerCameraManager->GetCameraLocation() : Fallback;
	}
}

void VaelWaterMesh::BuildViewSortedTubeTriangles(int32 Rings, int32 Around, const FVector& AxisA, const FVector& AxisB, const FVector& ToCamera, TArray<int32>& OutTriangles)
{
	const int32 Rows = Around + 1;

	// Strips facing away from the camera first
	TArray<int32, TInlineAllocator<64>> Strips;
	TArray<float, TInlineAllocator<64>> Facing;
	for (int32 Step = 0; Step < Around; ++Step)
	{
		const float Angle = UE_TWO_PI * (Step + 0.5f) / Around;
		Strips.Add(Step);
		Facing.Add(FVector::DotProduct(AxisA * FMath::Cos(Angle) + AxisB * FMath::Sin(Angle), ToCamera));
	}
	Strips.Sort([&Facing](int32 A, int32 B) { return Facing[A] < Facing[B]; });

	OutTriangles.Reset();
	for (const int32 Step : Strips)
	{
		for (int32 Ring = 0; Ring < Rings - 1; ++Ring)
		{
			const int32 A = Ring * Rows + Step;
			const int32 B = (Ring + 1) * Rows + Step;
			OutTriangles.Append({ A, A + 1, B, B, A + 1, B + 1 });
		}
	}
}
