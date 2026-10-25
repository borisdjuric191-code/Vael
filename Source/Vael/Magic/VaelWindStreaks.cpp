// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelWindStreaks.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"

namespace
{
	/** A new point of the trail once the head has moved this far, and at most this many points in a trail */
	constexpr float StreakPointSpacing = 10.0f;
	constexpr int32 StreakMaxPoints = 24;
}

UVaelWindStreaksComponent::UVaelWindStreaksComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCastShadow(false);
}

void UVaelWindStreaksComponent::Init(UMaterialInterface* Material)
{
	// The bands are built in world space and must not move with the owner
	SetUsingAbsoluteLocation(true);
	SetUsingAbsoluteRotation(true);
	SetUsingAbsoluteScale(true);
	SetMaterial(0, Material);
}

int32 UVaelWindStreaksComponent::AddStreak(const FVector& Head, float Width, float MaxTrail)
{
	FStreak& Streak = Streaks.AddDefaulted_GetRef();
	Streak.Trail.Add(Head);
	Streak.Width = Width;
	Streak.MaxTrail = MaxTrail;
	return Streaks.Num() - 1;
}

void UVaelWindStreaksComponent::MoveHead(int32 Index, const FVector& Head, float Strength)
{
	if (!Streaks.IsValidIndex(Index))
	{
		return;
	}
	FStreak& Streak = Streaks[Index];
	Streak.Strength = Strength;

	// The head always sits on the newest point; a new point is laid once it has moved far enough from the one before
	if (Streak.Trail.Num() < 2 || FVector::Dist(Streak.Trail[1], Head) >= StreakPointSpacing)
	{
		Streak.Trail.Insert(Head, 0);
	}
	else
	{
		Streak.Trail[0] = Head;
	}

	// The trail is cut at its longest length, its last point pulled in so it ends exactly there
	float Length = 0.0f;
	for (int32 Point = 1; Point < Streak.Trail.Num(); ++Point)
	{
		const float Step = FVector::Dist(Streak.Trail[Point - 1], Streak.Trail[Point]);
		if (Length + Step > Streak.MaxTrail || Point >= StreakMaxPoints)
		{
			const float Keep = Step > UE_KINDA_SMALL_NUMBER ? FMath::Clamp((Streak.MaxTrail - Length) / Step, 0.0f, 1.0f) : 0.0f;
			Streak.Trail[Point] = FMath::Lerp(Streak.Trail[Point - 1], Streak.Trail[Point], Keep);
			Streak.Trail.SetNum(Point + 1);
			break;
		}
		Length += Step;
	}
}

void UVaelWindStreaksComponent::SetTrail(int32 Index, TArray<FVector>&& Points, float Strength)
{
	if (Streaks.IsValidIndex(Index))
	{
		Streaks[Index].Trail = MoveTemp(Points);
		Streaks[Index].Strength = Strength;
	}
}

void UVaelWindStreaksComponent::Restart(int32 Index, const FVector& Head)
{
	if (Streaks.IsValidIndex(Index))
	{
		Streaks[Index].Trail.Reset();
		Streaks[Index].Trail.Add(Head);
	}
}

void UVaelWindStreaksComponent::Commit()
{
	FVector Eye = FVector::ZeroVector;
	if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		Eye = Camera->GetCameraLocation();
	}

	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;
	for (const FStreak& Streak : Streaks)
	{
		if (Streak.Trail.Num() < 2 || Streak.Strength <= 0.0f)
		{
			continue;
		}
		float Total = 0.0f;
		for (int32 Point = 1; Point < Streak.Trail.Num(); ++Point)
		{
			Total += FVector::Dist(Streak.Trail[Point - 1], Streak.Trail[Point]);
		}
		if (Total < 1.0f)
		{
			continue;
		}

		// Two points across the band at every point of the trail, the band turned to face the camera
		const int32 First = Vertices.Num();
		float Along = 0.0f;
		for (int32 Point = 0; Point < Streak.Trail.Num(); ++Point)
		{
			const FVector& Here = Streak.Trail[Point];
			const FVector Next = Streak.Trail[FMath::Min(Point + 1, Streak.Trail.Num() - 1)];
			const FVector Prev = Streak.Trail[FMath::Max(Point - 1, 0)];
			const FVector Tangent = (Prev - Next).GetSafeNormal();
			const FVector ToEye = (Eye - Here).GetSafeNormal();
			const FVector Side = FVector::CrossProduct(Tangent, ToEye).GetSafeNormal() * Streak.Width * 0.5f;
			if (Point > 0)
			{
				Along += FVector::Dist(Streak.Trail[Point - 1], Here);
			}
			const float U = Along / Total;
			// Thin at the head, fullest a little behind it
			const float Swell = FMath::Clamp(U * 5.0f, 0.3f, 1.0f);
			Vertices.Add(Here + Side * Swell);
			Vertices.Add(Here - Side * Swell);
			Normals.Add(ToEye);
			Normals.Add(ToEye);
			UVs.Add(FVector2D(U, 0.0f));
			UVs.Add(FVector2D(U, 1.0f));
			Colors.Add(FLinearColor(1.0f, 1.0f, 1.0f, Streak.Strength));
			Colors.Add(FLinearColor(1.0f, 1.0f, 1.0f, Streak.Strength));
		}
		for (int32 Point = 0; Point + 1 < Streak.Trail.Num(); ++Point)
		{
			const int32 A = First + Point * 2;
			Triangles.Append({ A, A + 2, A + 1, A + 1, A + 2, A + 3 });
		}
	}

	if (Triangles.IsEmpty())
	{
		ClearMeshSection(0);
		return;
	}
	// The trails are kept in world space; the mesh wants them relative to wherever the component ended up
	const FTransform& ToWorld = GetComponentTransform();
	for (FVector& Vertex : Vertices)
	{
		Vertex = ToWorld.InverseTransformPosition(Vertex);
	}
	for (FVector& Normal : Normals)
	{
		Normal = ToWorld.InverseTransformVectorNoScale(Normal);
	}
	CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, TArray<FProcMeshTangent>(), false);
}
