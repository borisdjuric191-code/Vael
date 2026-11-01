// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelBeamFunnel.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

namespace
{
	/** Cuts along the cone and around it */
	constexpr int32 FunnelRings = 16;
	constexpr int32 FunnelSides = 18;

	/** Radius of the cone at the hand, in cm */
	constexpr float FunnelPalmRadius = 5.0f;

	/** The skin passes this much outside the ball */
	constexpr float FunnelOverBall = 1.14f;

	/** Past the ball the cone runs on this many ball radii into the beam */
	constexpr float FunnelMouthReach = 1.8f;

	/** Streaks of fire and sparks that race along the skin: how many, their length as a share of the way, how many points */
	constexpr int32 FunnelStreaks = 16;
	constexpr int32 FunnelSparks = 18;
	constexpr int32 FunnelStreakPoints = 15;
	constexpr float FunnelStreakLength = 0.42f;
	constexpr float FunnelSparkLength = 0.05f;

	/** Rounds per second and twist of the streaks around the axis on their way, in radians */
	constexpr float FunnelFlowSpeed = 1.6f;
	constexpr float FunnelTwist = 1.3f;
}

UVaelBeamFunnelComponent::UVaelBeamFunnelComponent()
{
	SetUsingAbsoluteLocation(true);
	SetUsingAbsoluteRotation(true);
	SetUsingAbsoluteScale(true);
}

void UVaelBeamFunnelComponent::Init(UMaterialInterface* FireMaterial, UMaterialInterface* StreakMaterial)
{
	Mesh = NewObject<UProceduralMeshComponent>(GetOwner());
	Mesh->SetupAttachment(this);
	Mesh->SetUsingAbsoluteLocation(true);
	Mesh->SetUsingAbsoluteRotation(true);
	Mesh->SetUsingAbsoluteScale(true);
	Mesh->SetWorldTransform(FTransform::Identity);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->bReceivesDecals = false;
	Mesh->RegisterComponent();

	if (FireMaterial != nullptr)
	{
		UMaterialInstanceDynamic* Skin = UMaterialInstanceDynamic::Create(FireMaterial, this);
		Skin->SetScalarParameterValue(TEXT("Hitze"), 0.8f);
		Skin->SetScalarParameterValue(TEXT("Deckkraft"), 0.28f);
		Skin->SetScalarParameterValue(TEXT("Kacheln"), 1.0f);
		Mesh->SetMaterial(0, Skin);
	}

	if (StreakMaterial != nullptr)
	{
		UMaterialInstanceDynamic* Streak = UMaterialInstanceDynamic::Create(StreakMaterial, this);
		Streak->SetVectorParameterValue(TEXT("Farbe"), FLinearColor(1.0f, 0.34f, 0.04f));
		Streak->SetVectorParameterValue(TEXT("KernFarbe"), FLinearColor(1.0f, 0.82f, 0.5f));
		Streak->SetScalarParameterValue(TEXT("Leuchten"), 4.0f);
		Mesh->SetMaterial(1, Streak);
	}
}

float UVaelBeamFunnelComponent::RadiusAt(float Share) const
{
	// A cone opening from the hand to the waist over the ball, then sliding into the width of the beam
	if (Share < WaistShare)
	{
		return FMath::Lerp(FunnelPalmRadius, WaistRadius, FMath::Sin(Share / WaistShare * UE_HALF_PI));
	}
	return FMath::Lerp(WaistRadius, MouthRadius, FMath::SmoothStep(WaistShare, 1.0f, Share));
}

FVector UVaelBeamFunnelComponent::SkinPoint(float Share, float Angle, float Lift) const
{
	return Origin + Axis * Reach * Share + (Across * FMath::Cos(Angle) + Over * FMath::Sin(Angle)) * (RadiusAt(Share) + Lift);
}

void UVaelBeamFunnelComponent::AddRibbon(const TArray<FVector>& Points, float Width, float Brightness, float Core)
{
	const int32 First = Vertices.Num();
	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		const FVector Tangent = Points[FMath::Min(Index + 1, Points.Num() - 1)] - Points[FMath::Max(Index - 1, 0)];
		const FVector Side = FVector::CrossProduct(Tangent, Camera - Points[Index]).GetSafeNormal();
		const float Along = Index / static_cast<float>(Points.Num() - 1);
		// Constant width, or the line through the middle zigzags over the quads; the ends fade out by brightness instead
		const float Half = Width * 0.5f;

		Vertices.Add(Points[Index] - Side * Half);
		Vertices.Add(Points[Index] + Side * Half);
		UVs.Add(FVector2D(Along, 0.0f));
		UVs.Add(FVector2D(Along, 1.0f));
		const FLinearColor Color(Brightness * FMath::Sin(Along * UE_PI), Core, 0.0f, 1.0f);
		Colors.Add(Color);
		Colors.Add(Color);
	}

	for (int32 Index = 0; Index < Points.Num() - 1; ++Index)
	{
		const int32 A = First + Index * 2;
		Triangles.Append({ A, A + 1, A + 2, A + 1, A + 3, A + 2 });
	}
}

void UVaelBeamFunnelComponent::Update(const FVector& Palm, const FVector& Direction, const FVector& BallCenter, float BallRadius, float BeamRadius, float DeltaSeconds)
{
	if (Mesh == nullptr)
	{
		return;
	}

	Age += DeltaSeconds;

	Origin = Palm;
	Axis = Direction;
	Across = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();
	Over = FVector::CrossProduct(Across, Direction);
	const float ToBall = FVector::DotProduct(BallCenter - Palm, Direction);
	Reach = ToBall + BallRadius * FunnelMouthReach;
	WaistShare = ToBall / Reach;
	WaistRadius = BallRadius * FunnelOverBall;
	MouthRadius = BeamRadius;

	const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	Camera = PlayerController != nullptr && PlayerController->PlayerCameraManager != nullptr ? PlayerController->PlayerCameraManager->GetCameraLocation() : Palm + FVector(-600.0f, 0.0f, 900.0f);

	// The skin of streaming fire
	TArray<FVector> SkinVertices;
	TArray<FVector> SkinNormals;
	TArray<FVector2D> SkinUVs;
	TArray<FLinearColor> SkinColors;
	TArray<int32> SkinTriangles;
	for (int32 Ring = 0; Ring <= FunnelRings; ++Ring)
	{
		const float Share = Ring / static_cast<float>(FunnelRings);
		for (int32 Side = 0; Side <= FunnelSides; ++Side)
		{
			const float Around = Side / static_cast<float>(FunnelSides);
			const float Angle = Around * UE_TWO_PI + Share * FunnelTwist;
			const FVector Out = Across * FMath::Cos(Angle) + Over * FMath::Sin(Angle);
			SkinVertices.Add(SkinPoint(Share, Angle, 0.0f));
			SkinNormals.Add(Out);
			SkinUVs.Add(FVector2D(Around, Share));
			SkinColors.Add(FLinearColor::White);
		}
	}
	for (int32 Ring = 0; Ring < FunnelRings; ++Ring)
	{
		for (int32 Side = 0; Side < FunnelSides; ++Side)
		{
			const int32 A = Ring * (FunnelSides + 1) + Side;
			const int32 B = A + 1;
			const int32 C = A + FunnelSides + 1;
			const int32 D = C + 1;
			SkinTriangles.Append({ A, B, C, B, D, C });
		}
	}

	// Streaks and sparks race along the skin and over the ball, each in its own lane
	Vertices.Reset();
	UVs.Reset();
	Colors.Reset();
	Triangles.Reset();
	TArray<FVector> Points;
	const int32 NumLanes = FunnelStreaks + FunnelSparks;
	for (int32 Lane = 0; Lane < NumLanes; ++Lane)
	{
		const bool bSpark = Lane >= FunnelStreaks;
		const float Seed = Lane * 2.39996f;
		const float Speed = FunnelFlowSpeed * FMath::Lerp(0.8f, 1.3f, FMath::Frac(Seed * 0.37f)) * (bSpark ? 1.4f : 1.0f);
		const float Head = FMath::Frac(Age * Speed + Seed);
		const float Length = bSpark ? FunnelSparkLength : FunnelStreakLength;
		const float Lift = bSpark ? FMath::Lerp(2.0f, 14.0f, FMath::Frac(Seed * 0.71f)) : 2.0f;

		Points.Reset();
		for (int32 Point = 0; Point < FunnelStreakPoints; ++Point)
		{
			const float Share = FMath::Clamp(Head - Length * (1.0f - Point / static_cast<float>(FunnelStreakPoints - 1)), 0.0f, 1.0f);
			Points.Add(SkinPoint(Share, Seed + Share * FunnelTwist, Lift));
		}

		// They light up leaving the hand and go out in the beam
		const float Fade = FMath::Min(Head * 6.0f, 1.0f) * FMath::Min((1.0f - Head) * 4.0f, 1.0f);
		AddRibbon(Points, bSpark ? 5.0f : 22.0f, Fade * (bSpark ? 1.2f : 0.55f), bSpark ? 1.0f : 0.12f);
	}

	if (!bBuilt)
	{
		Mesh->CreateMeshSection_LinearColor(0, SkinVertices, SkinTriangles, SkinNormals, SkinUVs, SkinColors, TArray<FProcMeshTangent>(), false);
		Mesh->CreateMeshSection_LinearColor(1, Vertices, Triangles, TArray<FVector>(), UVs, Colors, TArray<FProcMeshTangent>(), false);
		bBuilt = true;
		return;
	}

	Mesh->UpdateMeshSection_LinearColor(0, SkinVertices, SkinNormals, SkinUVs, SkinColors, TArray<FProcMeshTangent>());
	Mesh->UpdateMeshSection_LinearColor(1, Vertices, TArray<FVector>(), UVs, Colors, TArray<FProcMeshTangent>());
}
