// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/VaelWorldMap.h"

#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "UI/VaelUISettings.h"
#include "VaelAssets.h"

namespace VaelPaintedMap
{
	/** Anchors are solved in km, so the numbers stay small */
	constexpr double Km = 100000.0;

	/** Slight smoothing: anchors set by eye may disagree a little; the bend then passes near them instead of kinking */
	constexpr double Smoothing = 0.02;

	double Kernel(double R2)
	{
		return R2 > 1e-12 ? 0.5 * R2 * FMath::Loge(R2) : 0.0;   // r^2 log r
	}

	/** Gaussian elimination with partial pivoting; A is N x N row major, B is solved in place */
	bool Solve(TArray<double>& A, TArray<double>& B, int32 N)
	{
		for (int32 Col = 0; Col < N; ++Col)
		{
			int32 Pivot = Col;
			for (int32 Row = Col + 1; Row < N; ++Row)
			{
				if (FMath::Abs(A[Row * N + Col]) > FMath::Abs(A[Pivot * N + Col]))
				{
					Pivot = Row;
				}
			}
			if (FMath::Abs(A[Pivot * N + Col]) < 1e-12)
			{
				return false;
			}
			if (Pivot != Col)
			{
				for (int32 K = 0; K < N; ++K)
				{
					Swap(A[Col * N + K], A[Pivot * N + K]);
				}
				Swap(B[Col], B[Pivot]);
			}
			for (int32 Row = Col + 1; Row < N; ++Row)
			{
				const double F = A[Row * N + Col] / A[Col * N + Col];
				for (int32 K = Col; K < N; ++K)
				{
					A[Row * N + K] -= F * A[Col * N + K];
				}
				B[Row] -= F * B[Col];
			}
		}
		for (int32 Row = N - 1; Row >= 0; --Row)
		{
			double Sum = B[Row];
			for (int32 K = Row + 1; K < N; ++K)
			{
				Sum -= A[Row * N + K] * B[K];
			}
			B[Row] = Sum / A[Row * N + Row];
		}
		return true;
	}
}

void UVaelWorldMap::SolveSpline() const
{
	using namespace VaelPaintedMap;
	const int32 Count = PaintedAnchors.Num();
	SplineAnchorCount = Count;
	SplineU.Reset();
	SplineV.Reset();
	if (Count < 3)
	{
		return;
	}

	// [K + sI  P; P^T 0] [w; a] = [target; 0] with P = (1, x, y)
	const int32 N = Count + 3;
	TArray<double> Matrix;
	Matrix.SetNumZeroed(N * N);
	SplineU.SetNumZeroed(N);
	SplineV.SetNumZeroed(N);
	for (int32 I = 0; I < Count; ++I)
	{
		const FVector2D Pi = PaintedAnchors[I].World / Km;
		for (int32 J = 0; J < Count; ++J)
		{
			Matrix[I * N + J] = Kernel(FVector2D::DistSquared(Pi, PaintedAnchors[J].World / Km)) + (I == J ? Smoothing : 0.0);
		}
		const double Row[3] = { 1.0, Pi.X, Pi.Y };
		for (int32 K = 0; K < 3; ++K)
		{
			Matrix[I * N + Count + K] = Row[K];
			Matrix[(Count + K) * N + I] = Row[K];
		}
		SplineU[I] = PaintedAnchors[I].Picture.X;
		SplineV[I] = PaintedAnchors[I].Picture.Y;
	}
	TArray<double> MatrixV = Matrix;
	if (!Solve(Matrix, SplineU, N) || !Solve(MatrixV, SplineV, N))
	{
		SplineU.Reset();
		SplineV.Reset();
	}
}

FVector2D UVaelWorldMap::WorldToPainted(const FVector2D& World) const
{
	using namespace VaelPaintedMap;
	if (SplineAnchorCount != PaintedAnchors.Num())
	{
		SolveSpline();
	}
	const int32 Count = PaintedAnchors.Num();
	if (SplineU.Num() != Count + 3)
	{
		return WorldToPicture(World);
	}
	const FVector2D P = World / Km;
	double U = SplineU[Count] + SplineU[Count + 1] * P.X + SplineU[Count + 2] * P.Y;
	double V = SplineV[Count] + SplineV[Count + 1] * P.X + SplineV[Count + 2] * P.Y;
	for (int32 I = 0; I < Count; ++I)
	{
		const double K = Kernel(FVector2D::DistSquared(P, PaintedAnchors[I].World / Km));
		U += SplineU[I] * K;
		V += SplineV[I] * K;
	}
	return FVector2D(U, V);
}

const TArray<FVector2f>& UVaelWorldMap::GetFogCellsOnPainted() const
{
	if (FogCellsOnPainted.Num() != FogCells * FogCells || SplineAnchorCount != PaintedAnchors.Num())
	{
		FogCellsOnPainted.SetNumUninitialized(FogCells * FogCells);
		const double Cell = FMath::Max(WorldSize, 1.0f) / FogCells;
		for (int32 Y = 0; Y < FogCells; ++Y)
		{
			for (int32 X = 0; X < FogCells; ++X)
			{
				// Fog cells lie over the terrain picture: cell (X, Y) covers WorldMin + (X, Y) * Cell
				const FVector2D World = WorldMin + FVector2D((X + 0.5) * Cell, (Y + 0.5) * Cell);
				FogCellsOnPainted[Y * FogCells + X] = FVector2f(WorldToPainted(World));
			}
		}
	}
	return FogCellsOnPainted;
}

const UVaelWorldMap* UVaelWorldMap::ForWorld(const UWorld* World)
{
	if (World == nullptr)
	{
		return nullptr;
	}
	// Set in the project settings, otherwise by name: L_Vael shows /Game/Vael/UI/Karten/DA_Karte_Vael
	const FString LevelName = UWorld::RemovePIEPrefix(World->GetMapName());
	if (const TSoftObjectPtr<UVaelWorldMap>* Entry = UVaelUISettings::Get()->LevelMaps.Find(FName(*LevelName)))
	{
		return VaelAssets::LoadOptional(*Entry);
	}
	FString Short = LevelName;
	Short.RemoveFromStart(TEXT("L_"));
	const TSoftObjectPtr<UVaelWorldMap> ByName(FSoftObjectPath(FString::Printf(TEXT("/Game/Vael/UI/Karten/DA_Karte_%s.DA_Karte_%s"), *Short, *Short)));
	return VaelAssets::LoadOptional(ByName);
}
