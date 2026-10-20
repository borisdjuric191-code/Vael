// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelWaterGeyser.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Magic/VaelElementSource.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelWaterPuddle.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "VaelAssets.h"

namespace
{
	/** Points around the column and its rings, rings up the column, rows of the crown, the ring on the ground and the steam */
	constexpr int32 GeyserAround = 24;
	constexpr int32 GeyserColumnRings = 26;
	constexpr int32 GeyserCrownRows = 9;
	constexpr int32 GeyserRingRows = 6;
	constexpr int32 GeyserSteamRows = 6;

	/** Drops in the pool, and how many the top, the foot and the falling column throw per second at full violence */
	constexpr int32 GeyserDrops = 380;
	constexpr float GeyserTopDropRate = 320.0f;
	constexpr float GeyserFootDropRate = 90.0f;
	constexpr float GeyserFallDropRate = 170.0f;

	/** Size of the drops in cm: most are tiny, a few are bigger lumps */
	constexpr float GeyserDropMinSize = 1.4f;
	constexpr float GeyserDropMaxSize = 7.0f;
	constexpr float GeyserClumpShare = 0.15f;

	/** Width of the column at its foot against the reach of the strike */
	constexpr float GeyserColumnShare = 0.32f;

	/** Shares of its life: shooting up, then standing until the fall starts, fallen back at the end of the fall */
	constexpr float GeyserRiseShare = 0.12f;
	constexpr float GeyserFallStart = 0.42f;
	constexpr float GeyserFallEnd = 0.8f;

	/** How deep under the ground the column starts, in cm */
	constexpr float GeyserFootDepth = 30.0f;

	/** How fast the foam pattern streams up the column, m/s */
	constexpr float GeyserFoamFlow = 4.0f;

	/** The point of an angle on the circle, for noise that wraps around the column without a seam */
	FVector2D GeyserCircle(float Angle, float Scale)
	{
		return FVector2D(FMath::Cos(Angle) * Scale, FMath::Sin(Angle) * Scale);
	}
}

AVaelWaterGeyser::AVaelWaterGeyser()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->bUseAsyncCooking = true;
	RootComponent = Mesh;
}

AVaelWaterGeyser* AVaelWaterGeyser::Spawn(UWorld* World, const FVector& GroundLocation, float InRadius, APawn* InInstigator)
{
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	UMaterialInterface* Material = World != nullptr ? VaelAssets::LoadOptional(MagicSettings->WaveMaterial) : nullptr;
	if (Material == nullptr)
	{
		return nullptr;
	}

	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Parameters.Instigator = InInstigator;
	AVaelWaterGeyser* Geyser = World->SpawnActor<AVaelWaterGeyser>(GroundLocation, FRotator::ZeroRotator, Parameters);
	if (Geyser != nullptr)
	{
		Geyser->Radius = InRadius;
		Geyser->ColumnRadius = InRadius * GeyserColumnShare;
		Geyser->MaxHeight = MagicSettings->GeyserHeight * FMath::FRandRange(0.9f, 1.1f);
		Geyser->Violence = MagicSettings->GeyserViolence;
		Geyser->Lifetime = MagicSettings->GeyserTime;
		Geyser->Seed = FMath::FRandRange(1.0f, 100.0f);
		Geyser->Drops.Init(GeyserDrops);
		for (int32 Section = 0; Section < 5; ++Section)
		{
			Geyser->Mesh->SetMaterial(Section, Material);
		}
		Geyser->Tick(0.0f);
	}

	return Geyser;
}

void AVaelWaterGeyser::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Elapsed += DeltaSeconds;
	if (Elapsed >= Lifetime)
	{
		Destroy();
		return;
	}

	UpdateState();

	// Its water falls back to the ground and stays there as a puddle
	const float Progress = Elapsed / Lifetime;
	if (!bPoured && Progress >= GeyserFallStart)
	{
		bPoured = true;

		const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
		if (MagicSettings->GeyserPuddleWater > 0.0f)
		{
			if (UVaelElementSource* Source = VaelAssets::LoadOptional(MagicSettings->MageWaterSource))
			{
				AVaelWaterPuddle::PourSplash(GetWorld(), GetActorLocation() + FVector(0.0f, 0.0f, 150.0f), MagicSettings->GeyserPuddleWater, Source, GetInstigator());
			}
		}
	}

	if (DeltaSeconds > 0.0f)
	{
		Drops.Update(DeltaSeconds, GetActorLocation().Z);
		ThrowDrops(DeltaSeconds);
	}

	BuildMesh();
}

void AVaelWaterGeyser::UpdateState()
{
	const float Progress = Elapsed / Lifetime;

	// Shoots up fast, stands heaving, then sinks back into itself and thins out
	const float Rise = 1.0f - FMath::Pow(1.0f - FMath::Clamp(Progress / GeyserRiseShare, 0.0f, 1.0f), 3.0f);
	const float Fall = FMath::SmoothStep(GeyserFallStart, GeyserFallEnd, Progress);

	Height = MaxHeight * Rise * (1.0f - Fall) * (1.0f + 0.05f * FMath::Sin(Elapsed * 13.0f) + 0.07f * Violence * VaelWaterMesh::Noise(Elapsed * 2.5f, Seed));
	Width = (0.75f + 0.25f * Rise) * (1.0f - 0.45f * Fall);
	Crown = FMath::SmoothStep(0.05f, 0.32f, Progress);
	CrownFade = FMath::SmoothStep(0.04f, 0.14f, Progress) * (1.0f - Fall);
	Collapse = Fall;

	// The ring of white water runs out over the ground, a little beyond the reach of the strike
	const float RingProgress = FMath::Clamp(Progress / 0.7f, 0.0f, 1.0f);
	RingRadius = FMath::Lerp(ColumnRadius * 0.9f, Radius * 1.15f, 1.0f - FMath::Square(1.0f - RingProgress));
	RingFade = FMath::SmoothStep(0.0f, 0.08f, Progress) * (1.0f - FMath::SmoothStep(0.6f, 1.0f, Progress));

	Fade = FMath::Clamp(Elapsed / 0.04f, 0.0f, 1.0f) * (1.0f - FMath::SmoothStep(0.85f, 1.0f, Progress));

	// The top sways a little in the boiling
	const float SwayReach = 0.08f * Violence * Height;
	SwayTop = FVector(VaelWaterMesh::Noise(Elapsed * 1.7f, Seed) * SwayReach, VaelWaterMesh::Noise(Seed * 1.9f, Elapsed * 1.7f) * SwayReach, 0.0f);
}

FVector AVaelWaterGeyser::GetSway(float HeightShare) const
{
	return SwayTop * FMath::Pow(FMath::Clamp(HeightShare, 0.0f, 1.0f), 1.5f);
}

FVector AVaelWaterGeyser::GetColumnPoint(float HeightShare, float Angle) const
{
	// Narrow where it shoots out of the vent, widening as it climbs and boils, fullest where it foams over, closing into a rounded tip;
	// lumps of water rise up its sides
	const float Profile = FMath::Lerp(0.55f, 1.0f, FMath::SmoothStep(0.0f, 0.75f, HeightShare)) + 0.2f * FMath::SmoothStep(0.75f, 0.95f, HeightShare);
	const float Dome = FMath::Sqrt(FMath::Clamp((1.0f - HeightShare) / 0.1f, 0.0f, 1.0f));
	const FVector2D Noise = GeyserCircle(Angle, 1.4f);
	const float Lump = 1.0f + 0.22f * Violence * VaelWaterMesh::Noise(Noise.X + HeightShare * 3.5f - Elapsed * 6.0f + Seed, Noise.Y + Seed * 0.37f);
	const float PointRadius = ColumnRadius * Width * Profile * Dome * Lump;

	// Its foot starts a little under the ground, so the vent never shows as an open end
	return FVector(FMath::Cos(Angle) * PointRadius, FMath::Sin(Angle) * PointRadius, FMath::Lerp(-GeyserFootDepth, Height, HeightShare)) + GetSway(HeightShare);
}

void AVaelWaterGeyser::BuildMesh()
{
	constexpr int32 Rows = GeyserAround + 1;
	const float Progress = Elapsed / Lifetime;

	// The column: churning foam where it leaves the vent, darker water above it, white boiling foam towards its top, its foam streaming upwards
	Column.Reset(GeyserColumnRings * Rows);
	for (int32 RingIndex = 0; RingIndex < GeyserColumnRings; ++RingIndex)
	{
		const float Up = RingIndex / (GeyserColumnRings - 1.0f);
		for (int32 Step = 0; Step < Rows; ++Step)
		{
			const float Angle = UE_TWO_PI * Step / GeyserAround;
			const int32 Index = RingIndex * Rows + Step;
			Column.Vertices[Index] = GetColumnPoint(Up, Angle);
			Column.UVs[Index] = FVector2D(Angle * ColumnRadius / 100.0f, Up * Height / 100.0f - Elapsed * GeyserFoamFlow);

			const float Light = 0.3f + 0.65f * FMath::Pow(Up, 1.3f);
			const float Churn = 0.45f * (1.0f - FMath::SmoothStep(0.0f, 0.15f, Up));
			const float Foam = FMath::Lerp(FMath::Min(0.3f + 0.65f * FMath::Pow(Up, 1.2f) + Churn, 1.0f), 0.95f, Collapse);
			const float Alpha = 0.95f * (1.0f - 0.45f * FMath::SmoothStep(0.88f, 1.0f, Up)) * (1.0f - 0.7f * Collapse) * Fade;
			Column.Colors[Index] = FLinearColor(Light, Foam, 0.0f, Alpha);
		}
	}

	// The crown: water foaming over the top, spreading out and falling back as a torn skirt
	CrownSheet.Reset(Rows * GeyserCrownRows);
	const float CrownBase = 0.9f;
	const FVector CrownSway = GetSway(CrownBase);
	for (int32 Step = 0; Step < Rows; ++Step)
	{
		const float Angle = UE_TWO_PI * Step / GeyserAround;
		const FVector2D Noise = GeyserCircle(Angle, 2.0f);
		const float Spread = ColumnRadius * (1.5f + 2.5f * Crown) * (1.0f + 0.25f * Violence * VaelWaterMesh::Noise(Noise.X + Seed, Noise.Y + Elapsed * 2.5f));
		const float StartRadius = ColumnRadius * Width * 0.9f;

		for (int32 Row = 0; Row < GeyserCrownRows; ++Row)
		{
			const float Out = Row / (GeyserCrownRows - 1.0f);
			const float PointRadius = StartRadius + Out * Spread;
			const float Z = Height * CrownBase + Height * 0.35f * Out * (1.0f - Out) - Height * 0.6f * Out * Out * Crown;
			const int32 Index = Step * GeyserCrownRows + Row;

			CrownSheet.Vertices[Index] = FVector(FMath::Cos(Angle) * PointRadius, FMath::Sin(Angle) * PointRadius, Z) + CrownSway;
			CrownSheet.UVs[Index] = FVector2D(Angle * ColumnRadius * 2.0f / 100.0f, Out * Spread / 100.0f - Elapsed * 2.0f);
			CrownSheet.Colors[Index] = FLinearColor(0.95f, 0.7f + 0.25f * Out, 0.0f, 0.8f * (1.0f - FMath::Pow(Out, 1.6f)) * CrownFade * Fade);
		}
	}

	// White water running out over the ground in a ring, a low swell on it
	Ring.Reset(Rows * GeyserRingRows);
	for (int32 Step = 0; Step < Rows; ++Step)
	{
		const float Angle = UE_TWO_PI * Step / GeyserAround;
		const FVector2D Noise = GeyserCircle(Angle, 2.5f);
		const float Outer = RingRadius * (1.0f + 0.12f * Violence * VaelWaterMesh::Noise(Noise.X + Seed, Noise.Y + Elapsed * 1.5f));

		for (int32 Row = 0; Row < GeyserRingRows; ++Row)
		{
			const float Out = Row / (GeyserRingRows - 1.0f);
			const float PointRadius = FMath::Lerp(ColumnRadius * Width * 0.5f, Outer, Out);
			const int32 Index = Step * GeyserRingRows + Row;

			Ring.Vertices[Index] = FVector(FMath::Cos(Angle) * PointRadius, FMath::Sin(Angle) * PointRadius, 4.0f + 12.0f * FMath::Sin(Out * PI) * RingFade);
			Ring.UVs[Index] = FVector2D(Angle * RingRadius / 100.0f, Out * RingRadius / 100.0f - Elapsed * 1.2f);
			Ring.Colors[Index] = FLinearColor(0.75f, 0.95f, 0.0f, 0.9f * FMath::Pow(1.0f - Out, 0.8f) * RingFade * Fade);
		}
	}

	// Steam rising from the boiling top, a soft veil that widens as it climbs
	Steam.Reset(Rows * GeyserSteamRows);
	const float SteamFade = 0.2f * Violence * FMath::SmoothStep(0.08f, 0.25f, Progress) * (1.0f - 0.5f * Collapse) * Fade;
	const float SteamHeight = FMath::Max(Height, MaxHeight * 0.5f);
	for (int32 Step = 0; Step < Rows; ++Step)
	{
		const float Angle = UE_TWO_PI * Step / GeyserAround;
		const FVector2D Noise = GeyserCircle(Angle, 1.6f);

		for (int32 Row = 0; Row < GeyserSteamRows; ++Row)
		{
			const float Up = Row / (GeyserSteamRows - 1.0f);
			const float HeightShare = 0.5f + 0.75f * Up;
			const float PointRadius = ColumnRadius * (1.3f + 1.8f * Up) * (1.0f + 0.2f * VaelWaterMesh::Noise(Noise.X + Up * 2.0f - Elapsed * 1.5f, Noise.Y + Seed));
			const int32 Index = Step * GeyserSteamRows + Row;

			Steam.Vertices[Index] = FVector(FMath::Cos(Angle) * PointRadius, FMath::Sin(Angle) * PointRadius, HeightShare * SteamHeight) + GetSway(HeightShare);
			Steam.UVs[Index] = FVector2D(Angle * ColumnRadius * 3.0f / 100.0f, Up * SteamHeight / 100.0f - Elapsed);
			Steam.Colors[Index] = FLinearColor(1.0f, 1.0f, 0.0f, SteamFade * FMath::Sin(Up * PI));
		}
	}

	VaelWaterMesh::ComputeGridNormals(Column.Vertices, GeyserColumnRings, Rows, Column.Normals);
	VaelWaterMesh::ComputeGridNormals(CrownSheet.Vertices, Rows, GeyserCrownRows, CrownSheet.Normals);
	VaelWaterMesh::ComputeGridNormals(Ring.Vertices, Rows, GeyserRingRows, Ring.Normals);
	VaelWaterMesh::ComputeGridNormals(Steam.Vertices, Rows, GeyserSteamRows, Steam.Normals);

	const FVector CameraLocation = VaelWaterMesh::GetCameraLocation(GetWorld(), GetActorLocation() + FVector(-600.0f, 0.0f, 900.0f));
	Drops.Build(DropMesh, GetActorTransform(), CameraLocation, Fade, 0.65f);

	const bool bCreate = !bMeshCreated;
	if (bCreate)
	{
		VaelWaterMesh::BuildGridTriangles(Rows, GeyserCrownRows, CrownSheet.Triangles);
		VaelWaterMesh::BuildGridTriangles(Rows, GeyserRingRows, Ring.Triangles);
		VaelWaterMesh::BuildGridTriangles(Rows, GeyserSteamRows, Steam.Triangles);
		Drops.BuildTriangles(DropMesh);
		bMeshCreated = true;
	}

	// The column is created anew every frame, its back drawn before its front
	VaelWaterMesh::BuildViewSortedTubeTriangles(GeyserColumnRings, GeyserAround, FVector::ForwardVector, FVector::RightVector, GetActorTransform().InverseTransformVectorNoScale(CameraLocation - GetActorLocation()), Column.Triangles);
	Column.Apply(Mesh, 0, true);
	CrownSheet.Apply(Mesh, 1, bCreate);
	Ring.Apply(Mesh, 2, bCreate);
	Steam.Apply(Mesh, 3, bCreate);
	DropMesh.Apply(Mesh, 4, bCreate);
}

void AVaelWaterGeyser::ThrowDrops(float DeltaSeconds)
{
	const float Progress = Elapsed / Lifetime;

	// The top throws drops high up while it stands, the foot splashes outwards, the falling column rains down
	const float TopRate = GeyserTopDropRate * Violence * FMath::SmoothStep(0.03f, 0.12f, Progress) * (1.0f - Collapse);
	const float FootRate = GeyserFootDropRate * Violence * RingFade;
	const float FallRate = GeyserFallDropRate * Violence * Collapse * (1.0f - FMath::SmoothStep(0.85f, 1.0f, Progress));
	const float TotalRate = TopRate + FootRate + FallRate;
	if (TotalRate <= 0.0f)
	{
		return;
	}

	const FTransform& Transform = GetActorTransform();
	Drops.Debt = FMath::Min(Drops.Debt + TotalRate * DeltaSeconds, 30.0f);

	for (; Drops.Debt >= 1.0f; Drops.Debt -= 1.0f)
	{
		const float Angle = FMath::FRandRange(0.0f, UE_TWO_PI);
		const FVector Outward(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
		const float Pick = FMath::FRand() * TotalRate;

		FVector Location;
		FVector DropVelocity;
		float Life;
		float Size;
		bool bClump = false;

		if (Pick < TopRate)
		{
			Location = GetColumnPoint(FMath::FRandRange(0.85f, 1.0f), Angle);
			DropVelocity = FVector(0.0f, 0.0f, FMath::FRandRange(250.0f, 900.0f) * (0.6f + 0.4f * Violence)) + Outward * FMath::FRandRange(120.0f, 480.0f) + FMath::VRand() * 60.0f;
			Life = FMath::FRandRange(0.7f, 1.5f);
			bClump = FMath::FRand() < GeyserClumpShare;
			Size = FMath::Lerp(GeyserDropMinSize, GeyserDropMaxSize, FMath::Pow(FMath::FRand(), 2.5f));
		}
		else if (Pick < TopRate + FootRate)
		{
			Location = Outward * ColumnRadius * FMath::FRandRange(0.8f, 1.3f) + FVector(0.0f, 0.0f, 10.0f);
			DropVelocity = Outward * FMath::FRandRange(200.0f, 500.0f) + FVector(0.0f, 0.0f, FMath::FRandRange(80.0f, 280.0f));
			Life = FMath::FRandRange(0.4f, 0.8f);
			Size = FMath::Lerp(GeyserDropMinSize, GeyserDropMaxSize * 0.6f, FMath::Pow(FMath::FRand(), 2.0f));
		}
		else
		{
			Location = GetColumnPoint(FMath::FRandRange(0.2f, 1.0f), Angle);
			DropVelocity = Outward * FMath::FRandRange(60.0f, 200.0f) + FVector(0.0f, 0.0f, FMath::FRandRange(-50.0f, 100.0f));
			Life = FMath::FRandRange(0.5f, 1.0f);
			bClump = FMath::FRand() < GeyserClumpShare;
			Size = FMath::Lerp(GeyserDropMinSize, GeyserDropMaxSize, FMath::Pow(FMath::FRand(), 2.5f));
		}

		if (!Drops.Throw(Transform.TransformPosition(Location), Transform.TransformVectorNoScale(DropVelocity), Life, Size * (bClump ? 1.8f : 1.0f), bClump))
		{
			break;
		}
	}
}
