// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelProgressSubsystem.h"
#include "Save/VaelSaveGame.h"

bool UVaelProgressSubsystem::MarkWeatherIntroduced(EVaelWeather Weather)
{
	bool bAlreadyIntroduced = false;
	IntroducedWeathers.Add(Weather, &bAlreadyIntroduced);

	return !bAlreadyIntroduced;
}

void UVaelProgressSubsystem::WriteSave(UVaelSaveGame& Save) const
{
	Save.HeardIntros = HeardIntros;
	Save.IntroducedWeathers = IntroducedWeathers;
	Save.MapFog = MapFog;
}

void UVaelProgressSubsystem::ReadSave(const UVaelSaveGame& Save)
{
	HeardIntros = Save.HeardIntros;
	IntroducedWeathers = Save.IntroducedWeathers;
	MapFog = Save.MapFog;
	++FogVersion;
}

bool UVaelProgressSubsystem::RevealMap(const UVaelWorldMap* Map, const FVector2D& World, float Radius)
{
	if (Map == nullptr || !Map->bFogOfWar || Radius <= 0.0f)
	{
		return false;
	}
	constexpr int32 N = UVaelWorldMap::FogCells;
	TArray<uint8>& Cells = MapFog.FindOrAdd(Map->GetFName()).Cells;
	if (Cells.Num() != N * N)
	{
		Cells.SetNumZeroed(N * N);
	}
	const FVector2D Centre = Map->WorldToPicture(World) * N;
	const float R = Radius / FMath::Max(Map->WorldSize / N, 1.0f);
	bool bChanged = false;
	for (int32 Y = FMath::Max(0, FMath::FloorToInt(Centre.Y - R)); Y <= FMath::Min(N - 1, FMath::CeilToInt(Centre.Y + R)); ++Y)
	{
		for (int32 X = FMath::Max(0, FMath::FloorToInt(Centre.X - R)); X <= FMath::Min(N - 1, FMath::CeilToInt(Centre.X + R)); ++X)
		{
			const float Along = FVector2D::Distance(FVector2D(X + 0.5f, Y + 0.5f), Centre) / R;
			if (Along >= 1.0f)
			{
				continue;
			}
			// Fully seen in the inner two thirds, fading towards the rim
			const uint8 Value = static_cast<uint8>(FMath::RoundToInt(255.0f * FMath::Clamp((1.0f - Along) / 0.35f, 0.0f, 1.0f)));
			uint8& Old = Cells[Y * N + X];
			if (Value > Old)
			{
				Old = Value;
				bChanged = true;
			}
		}
	}
	if (bChanged)
	{
		++FogVersion;
	}
	return bChanged;
}

void UVaelProgressSubsystem::RevealWholeMap(const UVaelWorldMap* Map)
{
	if (Map != nullptr)
	{
		TArray<uint8>& Cells = MapFog.FindOrAdd(Map->GetFName()).Cells;
		Cells.Init(255, UVaelWorldMap::FogCells * UVaelWorldMap::FogCells);
		++FogVersion;
	}
}

float UVaelProgressSubsystem::GetRevealed(const UVaelWorldMap* Map, const FVector2D& World) const
{
	if (Map == nullptr || !Map->bFogOfWar)
	{
		return 1.0f;
	}
	const TArray<uint8>* Cells = GetMapFog(Map);
	if (Cells == nullptr)
	{
		return 0.0f;
	}
	constexpr int32 N = UVaelWorldMap::FogCells;
	const FVector2D Cell = Map->WorldToPicture(World) * N;
	const int32 X = FMath::Clamp(FMath::FloorToInt(Cell.X), 0, N - 1);
	const int32 Y = FMath::Clamp(FMath::FloorToInt(Cell.Y), 0, N - 1);
	return (*Cells)[Y * N + X] / 255.0f;
}

const TArray<uint8>* UVaelProgressSubsystem::GetMapFog(const UVaelWorldMap* Map) const
{
	const FVaelMapFog* Fog = Map != nullptr ? MapFog.Find(Map->GetFName()) : nullptr;
	return Fog != nullptr && Fog->Cells.Num() == UVaelWorldMap::FogCells * UVaelWorldMap::FogCells ? &Fog->Cells : nullptr;
}

TArray<FVaelMapLabel> UVaelProgressSubsystem::GetFoundCamps(const UVaelWorldMap* Map) const
{
	TArray<FVaelMapLabel> Found;
	if (Map != nullptr)
	{
		for (const FVaelMapLabel& Camp : Map->Camps)
		{
			if (GetRevealed(Map, Camp.Location) > 0.5f)
			{
				Found.Add(Camp);
			}
		}
	}
	return Found;
}
