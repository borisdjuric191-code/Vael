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

FVector2D UVaelWorldMap::SnapToPaintedLand(const FVector2D& Place) const
{
	const int32 N = PaintedLandSize;
	if (N <= 0 || PaintedLand.Num() != N * N)
	{
		return Place;
	}
	if (NearestLandCell.Num() != N * N)
	{
		// Breadth first from the inland cells (land with land all round), then from any land if a cell is reached by none
		NearestLandCell.Init(INDEX_NONE, N * N);
		TArray<int32> Queue;
		Queue.Reserve(N * N);
		const auto IsLand = [this, N](int32 X, int32 Y) { return X >= 0 && Y >= 0 && X < N && Y < N && PaintedLand[Y * N + X] != 0; };
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			Queue.Reset();
			for (int32 Y = 0; Y < N; ++Y)
			{
				for (int32 X = 0; X < N; ++X)
				{
					const int32 Index = Y * N + X;
					if (NearestLandCell[Index] != INDEX_NONE || !IsLand(X, Y))
					{
						continue;
					}
					const bool bInland = IsLand(X - 1, Y) && IsLand(X + 1, Y) && IsLand(X, Y - 1) && IsLand(X, Y + 1)
						&& IsLand(X - 1, Y - 1) && IsLand(X + 1, Y - 1) && IsLand(X - 1, Y + 1) && IsLand(X + 1, Y + 1);
					if (Pass == 1 || bInland)
					{
						NearestLandCell[Index] = Index;
						Queue.Add(Index);
					}
				}
			}
			for (int32 Head = 0; Head < Queue.Num(); ++Head)
			{
				const int32 Index = Queue[Head];
				const int32 X = Index % N;
				const int32 Y = Index / N;
				const int32 Next[4][2] = { { X + 1, Y }, { X - 1, Y }, { X, Y + 1 }, { X, Y - 1 } };
				for (const auto& Cell : Next)
				{
					if (Cell[0] < 0 || Cell[1] < 0 || Cell[0] >= N || Cell[1] >= N)
					{
						continue;
					}
					const int32 Other = Cell[1] * N + Cell[0];
					// Land cells keep themselves; only sea cells (and in the first pass coastal land) take the nearest inland cell
					if (NearestLandCell[Other] == INDEX_NONE && (Pass == 0 || PaintedLand[Other] == 0))
					{
						NearestLandCell[Other] = NearestLandCell[Index];
						Queue.Add(Other);
					}
				}
			}
		}
		// Land always stays where it is
		for (int32 Index = 0; Index < N * N; ++Index)
		{
			if (PaintedLand[Index] != 0)
			{
				NearestLandCell[Index] = Index;
			}
		}
	}

	const int32 X = FMath::Clamp(FMath::FloorToInt(Place.X * N), 0, N - 1);
	const int32 Y = FMath::Clamp(FMath::FloorToInt(Place.Y * N), 0, N - 1);
	const int32 Index = Y * N + X;
	if (PaintedLand[Index] != 0 || NearestLandCell[Index] == INDEX_NONE)
	{
		return Place;
	}
	const int32 Target = NearestLandCell[Index];
	return FVector2D((Target % N + 0.5) / N, (Target / N + 0.5) / N);
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
