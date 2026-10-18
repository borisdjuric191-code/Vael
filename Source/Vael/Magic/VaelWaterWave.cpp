// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelWaterWave.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Magic/VaelMagicSettings.h"
#include "Materials/MaterialInterface.h"
#include "VaelAssets.h"

namespace
{
	/** Columns along the width of the Welle and the Flutwelle, points along the profile from back to front, rows of the spray veil */
	constexpr int32 WaveConeColumns = 44;
	constexpr int32 WaveCarriedColumns = 48;
	constexpr int32 WaveProfileSamples = 30;
	constexpr int32 WaveSprayRows = 7;

	/** Drops in the pool of the Welle and the Flutwelle, and how many the crest throws per second at full violence */
	constexpr int32 WaveConeDrops = 160;
	constexpr int32 WaveCarriedDrops = 340;
	constexpr float WaveConeDropRate = 170.0f;
	constexpr float WaveCarriedDropRate = 420.0f;

	/** Drops thrown at once when the Flutwelle crashes */
	constexpr int32 WaveCrashDrops = 160;

	/** Size of the drops in cm: most are tiny, a few are bigger lumps */
	constexpr float WaveDropMinSize = 1.2f;
	constexpr float WaveConeDropMaxSize = 4.0f;
	constexpr float WaveCarriedDropMaxSize = 6.5f;
	constexpr float WaveClumpShare = 0.12f;

	/** Fall of the drops in cm/s² and their slowing by the air */
	constexpr float WaveDropGravity = 980.0f;
	constexpr float WaveDropDrag = 0.6f;

	/** The Welle starts at this share of its reach and runs out as foam for this share of its roll time after arriving */
	constexpr float WaveStartShare = 0.12f;
	constexpr float WaveTailShare = 0.55f;

	/** Seconds the Flutwelle needs to crash and run out once its spell ends */
	constexpr float WaveCrashTime = 1.2f;

	/** Length of the profile compared to the height: the Welle is a long rolling wave, the Flutwelle a steep wall */
	constexpr float WaveConeLength = 1.3f;
	constexpr float WaveCarriedLength = 1.05f;

	/** How fast the foam pattern streams over the water, m/s */
	constexpr float WaveFoamFlow = 0.7f;

	/** Control points of the profile of one column, from the foot of the back over the crest and the lip down to the toe of the face */
	constexpr int32 WaveControlPoints = 10;

	struct FWaveProfilePoint
	{
		FVector2D Position = FVector2D::ZeroVector;

		/** Vertex colour: height in the wave (dark to pale), foam, solidity */
		float Light = 0.0f;
		float Foam = 0.0f;
		float Alpha = 1.0f;
	};

	/**
	 *  The profile of a breaking wave of height H and length L: a long back, the crest, a lip that reaches forward and curls
	 *  down as Break grows (0 a steep swell, 1 a curling barrel), the face under it and its toe. Collapse flattens it into foam.
	 */
	void BuildProfile(float H, float L, float Break, float Collapse, float LipJitterX, float LipJitterZ, float BackLimit, FWaveProfilePoint (&Points)[WaveControlPoints])
	{
		const float B = Break;
		const float HeightLeft = H * (1.0f - 0.8f * Collapse);
		const float BackScale = FMath::Min(1.0f, BackLimit / (1.5f * L));

		auto Set = [&Points](int32 Index, float X, float Z, float Light, float Foam, float Alpha)
		{
			Points[Index].Position = FVector2D(X, Z);
			Points[Index].Light = Light;
			Points[Index].Foam = Foam;
			Points[Index].Alpha = Alpha;
		};

		Set(0, -1.5f * L * BackScale, 0.0f, 0.0f, 0.02f, 0.9f);
		Set(1, -0.85f * L * BackScale, 0.15f * HeightLeft, 0.1f, 0.04f, 1.0f);
		Set(2, -0.35f * L, 0.62f * HeightLeft, 0.4f, 0.1f, 1.0f);
		Set(3, 0.0f, HeightLeft, 0.85f, 0.35f + 0.35f * B, 1.0f);
		Set(4, (0.12f + 0.3f * B) * L + LipJitterX * 0.5f, HeightLeft * (0.95f - 0.08f * B) + LipJitterZ * 0.5f, 0.92f, 0.45f + 0.3f * B, 0.95f);
		Set(5, (0.18f + 0.48f * B) * L + LipJitterX, HeightLeft * (0.82f - 0.52f * B) + LipJitterZ, 1.0f, 0.6f + 0.3f * B, 0.55f + 0.25f * (1.0f - B));
		Set(6, (0.16f + 0.26f * B) * L, HeightLeft * (0.74f - 0.32f * B), 0.72f, 0.35f + 0.25f * B, 0.85f);
		Set(7, 0.14f * L, HeightLeft * 0.62f, 0.5f, 0.12f + 0.15f * B, 1.0f);
		Set(8, 0.22f * L, HeightLeft * 0.3f, 0.28f, 0.1f, 1.0f);
		Set(9, (0.45f + 1.6f * Collapse) * L, 0.02f * H, 0.22f, 0.75f, 0.55f);

		// Falling apart, the whole wave turns into churning white water that spills forward and fades
		for (FWaveProfilePoint& Point : Points)
		{
			Point.Foam = FMath::Lerp(Point.Foam, 0.95f, Collapse);
			Point.Light = FMath::Lerp(Point.Light, 0.9f, Collapse * 0.6f);
			Point.Alpha *= 1.0f - FMath::SmoothStep(0.6f, 1.0f, Collapse);
		}
	}

	/** Point on the Catmull-Rom curve through the control points, at Parameter 0 .. number of points - 1 */
	FWaveProfilePoint SampleProfile(const FWaveProfilePoint (&Points)[WaveControlPoints], float Parameter)
	{
		const int32 Segment = FMath::Clamp(FMath::FloorToInt(Parameter), 0, WaveControlPoints - 2);
		const float T = FMath::Clamp(Parameter - Segment, 0.0f, 1.0f);

		const FVector2D P0 = Points[FMath::Max(Segment - 1, 0)].Position;
		const FVector2D P1 = Points[Segment].Position;
		const FVector2D P2 = Points[Segment + 1].Position;
		const FVector2D P3 = Points[FMath::Min(Segment + 2, WaveControlPoints - 1)].Position;

		const float T2 = T * T;
		const float T3 = T2 * T;

		FWaveProfilePoint Result;
		Result.Position = 0.5f * ((2.0f * P1) + (-P0 + P2) * T + (2.0f * P0 - 5.0f * P1 + 4.0f * P2 - P3) * T2 + (-P0 + 3.0f * P1 - 3.0f * P2 + P3) * T3);
		Result.Light = FMath::Lerp(Points[Segment].Light, Points[Segment + 1].Light, T);
		Result.Foam = FMath::Lerp(Points[Segment].Foam, Points[Segment + 1].Foam, T);
		Result.Alpha = FMath::Lerp(Points[Segment].Alpha, Points[Segment + 1].Alpha, T);
		return Result;
	}

	/** Perlin noise from -1 to 1 */
	float WaveNoise(float X, float Y)
	{
		return FMath::Clamp(FMath::PerlinNoise2D(FVector2D(X, Y)) * 1.4f, -1.0f, 1.0f);
	}

	/** Normals of a grid of Columns x Rows vertices, row by row within each column */
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

	/** Triangles of a grid of Columns x Rows vertices */
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
}

void FVaelWaveSection::Reset(int32 NumVertices)
{
	Vertices.SetNumZeroed(NumVertices);
	Normals.SetNumZeroed(NumVertices);
	UVs.SetNumZeroed(NumVertices);
	Colors.SetNumZeroed(NumVertices);
}

AVaelWaterWave::AVaelWaterWave()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->bUseAsyncCooking = true;
	RootComponent = Mesh;
}

AVaelWaterWave* AVaelWaterWave::SpawnCone(UWorld* World, const FVector& Origin, const FRotator& Facing, float InRange, float HalfAngle, float InRollTime, float InHeight, float InViolence)
{
	UMaterialInterface* Material = World != nullptr ? VaelAssets::LoadOptional(UVaelMagicSettings::Get()->WaveMaterial) : nullptr;
	if (Material == nullptr)
	{
		return nullptr;
	}

	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVaelWaterWave* Wave = World->SpawnActor<AVaelWaterWave>(Origin, FRotator(0.0f, Facing.Yaw, 0.0f), Parameters);
	if (Wave != nullptr)
	{
		Wave->Range = InRange;
		Wave->Spread = FMath::DegreesToRadians(HalfAngle);
		Wave->RollTime = InRollTime;
		Wave->MaxHeight = InHeight;
		Wave->Violence = InViolence;
		Wave->Drops.SetNum(WaveConeDrops);
		for (int32 Section = 0; Section < 3; ++Section)
		{
			Wave->Mesh->SetMaterial(Section, Material);
		}
		Wave->Tick(0.0f);
	}

	return Wave;
}

AVaelWaterWave* AVaelWaterWave::SpawnCarried(AActor* InCarrier, float InHalfWidth, float InHeight, float Foot, float InViolence)
{
	UWorld* World = InCarrier != nullptr ? InCarrier->GetWorld() : nullptr;
	UMaterialInterface* Material = World != nullptr ? VaelAssets::LoadOptional(UVaelMagicSettings::Get()->WaveMaterial) : nullptr;
	if (Material == nullptr)
	{
		return nullptr;
	}

	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Location = InCarrier->GetActorLocation() + FVector(0.0f, 0.0f, Foot);
	AVaelWaterWave* Wave = World->SpawnActor<AVaelWaterWave>(Location, FRotator(0.0f, InCarrier->GetActorRotation().Yaw, 0.0f), Parameters);
	if (Wave != nullptr)
	{
		Wave->bCarried = true;
		Wave->Carrier = InCarrier;
		Wave->CarrierFoot = Foot;
		Wave->LastCarrierLocation = Location;
		Wave->HalfWidth = InHalfWidth;
		Wave->MaxHeight = InHeight;
		Wave->Violence = InViolence;
		Wave->Drops.SetNum(WaveCarriedDrops);
		for (int32 Section = 0; Section < 3; ++Section)
		{
			Wave->Mesh->SetMaterial(Section, Material);
		}
		Wave->Tick(0.0f);
	}

	return Wave;
}

void AVaelWaterWave::Crash()
{
	if (!bCarried || CrashTime >= 0.0f)
	{
		return;
	}

	CrashTime = Elapsed;
	Carrier.Reset();

	// The whole crest is thrown forward and up at once
	const FTransform& Transform = GetActorTransform();
	int32 Thrown = 0;
	for (FVaelWaveDrop& Drop : Drops)
	{
		if (Drop.Lifetime > 0.0f || Thrown >= WaveCrashDrops || LipTips.IsEmpty())
		{
			continue;
		}

		const int32 Column = FMath::RandRange(0, LipTips.Num() - 1);
		const FVector Forward = Transform.TransformVectorNoScale(LipForwards[Column]);
		Drop.Location = Transform.TransformPosition(LipTips[Column]) + FMath::VRand() * 15.0f;
		Drop.Velocity = Forward * FMath::FRandRange(250.0f, 650.0f) + FMath::VRand() * 120.0f + FVector(0.0f, 0.0f, FMath::FRandRange(200.0f, 600.0f));
		Drop.Age = 0.0f;
		Drop.Lifetime = FMath::FRandRange(0.6f, 1.2f);
		Drop.bClump = FMath::FRand() < WaveClumpShare * 2.0f;
		Drop.Size = FMath::Lerp(WaveDropMinSize, WaveCarriedDropMaxSize, FMath::Pow(FMath::FRand(), 2.0f)) * (Drop.bClump ? 1.8f : 1.0f);
		++Thrown;
	}
}

void AVaelWaterWave::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Elapsed += DeltaSeconds;
	UpdateState(DeltaSeconds);

	if (IsActorBeingDestroyed())
	{
		return;
	}

	BuildMesh();
	UpdateDrops(DeltaSeconds);
}

void AVaelWaterWave::UpdateState(float DeltaSeconds)
{
	if (Seed == 0.0f)
	{
		Seed = FMath::FRandRange(1.0f, 100.0f);
	}

	if (!bCarried)
	{
		// Rises while it rolls out, breaks over the second half, then falls apart into foam that still creeps on a little
		const float Progress = Elapsed / RollTime;
		if (Progress >= 1.0f + WaveTailShare)
		{
			Destroy();
			return;
		}

		const float Rolled = FMath::Min(Progress, 1.0f);
		const float PreviousDistance = Distance;
		Distance = Range * (FMath::Lerp(WaveStartShare, 1.0f, 1.0f - FMath::Square(1.0f - Rolled)) + 0.12f * FMath::Max(Progress - 1.0f, 0.0f));
		Speed = DeltaSeconds > 0.0f ? (Distance - PreviousDistance) / DeltaSeconds : Range / RollTime;

		Height = MaxHeight * (0.3f + 0.7f * FMath::SmoothStep(0.0f, 0.5f, Progress));
		Break = FMath::SmoothStep(0.3f, 0.85f, Progress);
		Collapse = FMath::SmoothStep(0.85f, 1.0f + WaveTailShare, Progress);
		Fade = FMath::Clamp(Elapsed / 0.06f, 0.0f, 1.0f) * (1.0f - FMath::SmoothStep(1.0f + WaveTailShare * 0.6f, 1.0f + WaveTailShare, Progress));
		return;
	}

	if (CrashTime < 0.0f)
	{
		// Rides with its spell, always on the edge of breaking, the crest heaving
		AActor* CarrierActor = Carrier.Get();
		if (CarrierActor == nullptr)
		{
			Crash();
		}
		else
		{
			const FVector Location = CarrierActor->GetActorLocation() + FVector(0.0f, 0.0f, CarrierFoot);
			if (DeltaSeconds > 0.0f)
			{
				Velocity = (Location - LastCarrierLocation) / DeltaSeconds;
			}
			LastCarrierLocation = Location;
			SetActorLocationAndRotation(Location, FRotator(0.0f, CarrierActor->GetActorRotation().Yaw, 0.0f));

			Speed = Velocity.Size2D();
			Height = MaxHeight * FMath::SmoothStep(0.0f, 0.25f, Elapsed) * (1.0f + 0.06f * FMath::Sin(Elapsed * 4.5f));
			Break = FMath::Clamp(0.55f + 0.25f * WaveNoise(Elapsed * 1.3f, Seed), 0.0f, 1.0f);
			Collapse = 0.0f;
			Fade = FMath::Clamp(Elapsed / 0.12f, 0.0f, 1.0f);
			return;
		}
	}

	// Crashing: rears up once more, the lip comes down, everything spills forward as white water and fades
	const float Crashed = Elapsed - CrashTime;
	if (Crashed >= WaveCrashTime)
	{
		Destroy();
		return;
	}

	Velocity *= FMath::Exp(-2.5f * DeltaSeconds);
	SetActorLocation(GetActorLocation() + Velocity * DeltaSeconds);
	Speed = Velocity.Size2D();

	Height = MaxHeight * (1.0f + 0.25f * FMath::Sin(FMath::Min(Crashed / 0.25f, 1.0f) * UE_HALF_PI)) * (1.0f + 0.06f * FMath::Sin(Elapsed * 4.5f));
	Break = FMath::Lerp(Break, 1.0f, FMath::Min(DeltaSeconds * 6.0f, 1.0f));
	Collapse = FMath::SmoothStep(0.15f, 0.9f, Crashed);
	Fade = 1.0f - FMath::SmoothStep(0.75f, WaveCrashTime, Crashed);
}

int32 AVaelWaterWave::GetNumColumns() const
{
	return bCarried ? WaveCarriedColumns : WaveConeColumns;
}

void AVaelWaterWave::GetColumn(float Across, FVector& OutBase, FVector& OutForward) const
{
	if (!bCarried)
	{
		// Along the arc of the cone, a little behind at its ends
		const float Angle = Across * Spread;
		OutForward = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
		OutBase = OutForward * Distance * (1.0f - 0.05f * FMath::Abs(Across));
		return;
	}

	// A front bulging forward in the middle, its ends dragging behind
	const float Sweep = HalfWidth * 0.18f;
	OutBase = FVector(-Sweep * Across * Across, Across * HalfWidth, 0.0f);
	OutForward = FVector(HalfWidth, 2.0f * Sweep * Across, 0.0f).GetSafeNormal();
}

void AVaelWaterWave::BuildMesh()
{
	const int32 Columns = GetNumColumns() + 1;
	const int32 Rows = WaveProfileSamples;
	const float Length = bCarried ? WaveCarriedLength : WaveConeLength;

	// Width of the whole front in metres, for the UVs
	const float Width = (bCarried ? 2.0f * HalfWidth : 2.0f * Spread * Distance) / 100.0f;
	const float Flow = Elapsed * WaveFoamFlow;

	Sheet.Reset(Columns * Rows);
	Spray.Reset(Columns * WaveSprayRows);
	LipTips.SetNum(Columns);
	LipForwards.SetNum(Columns);
	LipBreaks.SetNum(Columns);
	LipHeights.SetNum(Columns);

	for (int32 Column = 0; Column < Columns; ++Column)
	{
		const float Share = Column / (Columns - 1.0f);
		const float Across = Share * 2.0f - 1.0f;

		FVector Base;
		FVector Forward;
		GetColumn(Across, Base, Forward);

		// Every column is a little different and keeps changing: higher here, breaking earlier there, a torn lip
		const float EndFade = 1.0f - FMath::SmoothStep(0.72f, 1.0f, FMath::Abs(Across));
		const float ColumnHeight = Height * (1.0f - 0.3f * Across * Across) * FMath::Lerp(0.45f, 1.0f, EndFade)
			* (1.0f + 0.24f * Violence * WaveNoise(Share * 3.1f + Seed, Elapsed * 1.1f));
		const float ColumnBreak = FMath::Clamp(Break + 0.3f * Violence * WaveNoise(Share * 2.2f + Seed * 1.7f, Elapsed * 0.8f), 0.0f, 1.0f);
		const float ColumnLength = ColumnHeight * Length;
		const float LipJitterX = ColumnLength * 0.1f * Violence * WaveNoise(Share * 9.0f + Seed, Elapsed * 3.0f);
		const float LipJitterZ = ColumnHeight * 0.07f * Violence * WaveNoise(Share * 13.0f + Seed * 0.3f, Elapsed * 3.5f);

		// The back of the Welle must not reach behind the caster; the Flutwelle is a steep wall with a short back
		const float BackLimit = bCarried ? ColumnLength * 1.1f : FMath::Max(Distance * 0.9f, 1.0f);

		FWaveProfilePoint Points[WaveControlPoints];
		BuildProfile(ColumnHeight, ColumnLength, ColumnBreak, Collapse, LipJitterX, LipJitterZ, BackLimit, Points);

		LipTips[Column] = Base + Forward * Points[5].Position.X + FVector(0.0f, 0.0f, Points[5].Position.Y);
		LipForwards[Column] = Forward;
		LipBreaks[Column] = ColumnBreak;
		LipHeights[Column] = ColumnHeight;

		float Along = 0.0f;
		FVector Previous = FVector::ZeroVector;
		for (int32 Row = 0; Row < Rows; ++Row)
		{
			const FWaveProfilePoint Point = SampleProfile(Points, Row / (Rows - 1.0f) * (WaveControlPoints - 1));
			const FVector Location = Base + Forward * Point.Position.X + FVector(0.0f, 0.0f, Point.Position.Y);
			Along += Row > 0 ? FVector::Dist(Location, Previous) / 100.0f : 0.0f;
			Previous = Location;

			const int32 Index = Column * Rows + Row;
			Sheet.Vertices[Index] = Location;
			Sheet.UVs[Index] = FVector2D(Share * Width, Along - Flow);
			Sheet.Colors[Index] = FLinearColor(Point.Light, Point.Foam, 0.0f, Point.Alpha * EndFade * Fade);
		}

		// A veil of spray torn up from the breaking lip and blown back over the crest
		const float SprayHeight = ColumnHeight * (0.12f + 0.33f * Violence) * (0.55f + 0.45f * WaveNoise(Share * 5.0f + Seed, Elapsed * 2.2f))
			* FMath::SmoothStep(0.3f, 0.75f, ColumnBreak) * (1.0f - 0.5f * Collapse)
			+ ColumnHeight * 2.0f * Collapse * (1.0f - Collapse) * Violence;
		const float SprayLean = -ColumnLength * 0.25f;

		for (int32 Row = 0; Row < WaveSprayRows; ++Row)
		{
			const float Up = Row / (WaveSprayRows - 1.0f);
			const int32 Index = Column * WaveSprayRows + Row;
			Spray.Vertices[Index] = LipTips[Column] + FVector(0.0f, 0.0f, SprayHeight * Up) + Forward * SprayLean * Up;
			Spray.UVs[Index] = FVector2D(Share * Width * 1.3f, Up * SprayHeight / 100.0f - Elapsed * 1.6f);
			Spray.Colors[Index] = FLinearColor(1.0f, 1.0f, 0.0f, 0.4f * FMath::Pow(1.0f - Up, 1.5f) * EndFade * Fade * FMath::Min(SprayHeight / 20.0f, 1.0f));
		}
	}

	ComputeGridNormals(Sheet.Vertices, Columns, Rows, Sheet.Normals);
	ComputeGridNormals(Spray.Vertices, Columns, WaveSprayRows, Spray.Normals);

	// Drops: a quad facing the camera for every slot of the pool, stretched along the flight of the bigger lumps
	DropMesh.Reset(Drops.Num() * 4);
	const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	const FVector CameraLocation = PlayerController != nullptr && PlayerController->PlayerCameraManager != nullptr ? PlayerController->PlayerCameraManager->GetCameraLocation() : GetActorLocation() + FVector(-600.0f, 0.0f, 900.0f);
	const FTransform& Transform = GetActorTransform();

	for (int32 DropIndex = 0; DropIndex < Drops.Num(); ++DropIndex)
	{
		const FVaelWaveDrop& Drop = Drops[DropIndex];
		const int32 First = DropIndex * 4;
		const FVector Center = Transform.InverseTransformPosition(Drop.Location);

		static const FVector2D Corners[4] = { FVector2D(0.0f, 0.0f), FVector2D(1.0f, 0.0f), FVector2D(1.0f, 1.0f), FVector2D(0.0f, 1.0f) };
		if (Drop.Lifetime <= 0.0f)
		{
			for (int32 Corner = 0; Corner < 4; ++Corner)
			{
				DropMesh.Vertices[First + Corner] = Center;
				DropMesh.UVs[First + Corner] = Corners[Corner];
				DropMesh.Normals[First + Corner] = FVector::UpVector;
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
		const FLinearColor Color(1.0f, Drop.bClump ? 0.75f : 0.5f, 1.0f, Fade * FMath::Pow(1.0f - Life, 0.6f));

		const FVector Positions[4] = { Center - HalfSide - HalfAlong, Center + HalfSide - HalfAlong, Center + HalfSide + HalfAlong, Center - HalfSide + HalfAlong };
		for (int32 Corner = 0; Corner < 4; ++Corner)
		{
			DropMesh.Vertices[First + Corner] = Positions[Corner];
			DropMesh.UVs[First + Corner] = Corners[Corner];
			DropMesh.Normals[First + Corner] = ToCamera;
			DropMesh.Colors[First + Corner] = Color;
		}
	}

	const TArray<FProcMeshTangent> NoTangents;
	if (!bMeshCreated)
	{
		BuildGridTriangles(Columns, Rows, Sheet.Triangles);
		BuildGridTriangles(Columns, WaveSprayRows, Spray.Triangles);

		DropMesh.Triangles.Reset();
		for (int32 DropIndex = 0; DropIndex < Drops.Num(); ++DropIndex)
		{
			const int32 First = DropIndex * 4;
			DropMesh.Triangles.Append({ First, First + 2, First + 1, First, First + 3, First + 2 });
		}

		Mesh->CreateMeshSection_LinearColor(0, Sheet.Vertices, Sheet.Triangles, Sheet.Normals, Sheet.UVs, Sheet.Colors, NoTangents, false);
		Mesh->CreateMeshSection_LinearColor(1, Spray.Vertices, Spray.Triangles, Spray.Normals, Spray.UVs, Spray.Colors, NoTangents, false);
		Mesh->CreateMeshSection_LinearColor(2, DropMesh.Vertices, DropMesh.Triangles, DropMesh.Normals, DropMesh.UVs, DropMesh.Colors, NoTangents, false);
		bMeshCreated = true;
		return;
	}

	Mesh->UpdateMeshSection_LinearColor(0, Sheet.Vertices, Sheet.Normals, Sheet.UVs, Sheet.Colors, NoTangents);
	Mesh->UpdateMeshSection_LinearColor(1, Spray.Vertices, Spray.Normals, Spray.UVs, Spray.Colors, NoTangents);
	Mesh->UpdateMeshSection_LinearColor(2, DropMesh.Vertices, DropMesh.Normals, DropMesh.UVs, DropMesh.Colors, NoTangents);
}

void AVaelWaterWave::UpdateDrops(float DeltaSeconds)
{
	if (DeltaSeconds <= 0.0f || LipTips.IsEmpty())
	{
		return;
	}

	const FTransform& Transform = GetActorTransform();
	const float GroundZ = GetActorLocation().Z;

	// The breaking crest throws drops, more the wilder the wave
	const float Rate = (bCarried ? WaveCarriedDropRate : WaveConeDropRate) * Violence * FMath::SmoothStep(0.25f, 0.6f, Break) * (1.0f - 0.7f * Collapse) * Fade;
	DropDebt += Rate * DeltaSeconds;

	for (FVaelWaveDrop& Drop : Drops)
	{
		if (Drop.Lifetime > 0.0f)
		{
			// Flying: slowed by the air, falling, gone when it lands or its time is up
			Drop.Velocity.Z -= WaveDropGravity * DeltaSeconds;
			Drop.Velocity *= FMath::Exp(-WaveDropDrag * DeltaSeconds);
			Drop.Location += Drop.Velocity * DeltaSeconds;
			Drop.Age += DeltaSeconds;
			if (Drop.Age >= Drop.Lifetime || Drop.Location.Z < GroundZ - 5.0f)
			{
				Drop.Lifetime = 0.0f;
			}
			continue;
		}

		if (DropDebt < 1.0f)
		{
			continue;
		}
		DropDebt -= 1.0f;

		// From a random place on the lip, where the crest is breaking
		const int32 Column = FMath::RandRange(0, LipTips.Num() - 1);
		if (LipBreaks[Column] < 0.25f)
		{
			continue;
		}

		const FVector Forward = Transform.TransformVectorNoScale(LipForwards[Column]);
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Forward);
		const float Lift = LipHeights[Column] / FMath::Max(MaxHeight, 1.0f);

		Drop.Location = Transform.TransformPosition(LipTips[Column]) + FMath::VRand() * 8.0f;
		Drop.Velocity = Forward * Speed * FMath::FRandRange(0.3f, 0.85f) + Forward * FMath::FRandRange(0.0f, 150.0f)
			+ Side * FMath::FRandRange(-90.0f, 90.0f) + FVector(0.0f, 0.0f, FMath::FRandRange(120.0f, 420.0f) * (0.6f + 0.6f * Violence) * Lift);
		Drop.Age = 0.0f;
		Drop.Lifetime = FMath::FRandRange(0.45f, 1.0f);
		Drop.bClump = FMath::FRand() < WaveClumpShare;
		Drop.Size = FMath::Lerp(WaveDropMinSize, bCarried ? WaveCarriedDropMaxSize : WaveConeDropMaxSize, FMath::Pow(FMath::FRand(), 2.5f)) * (Drop.bClump ? 1.8f : 1.0f);
	}

	DropDebt = FMath::Min(DropDebt, 20.0f);
}
