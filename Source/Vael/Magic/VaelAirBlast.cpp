// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelAirBlast.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Magic/VaelCloudCards.h"
#include "Magic/VaelDebrisBurst.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelWindStreaks.h"
#include "Materials/MaterialInterface.h"
#include "VaelAssets.h"
#include "World/VaelGround.h"

namespace
{
	/** Seconds the wave of a blast takes to its end; the wave starts this share of the reach out of the hand */
	constexpr float AirWaveTime = 0.45f;
	constexpr float AirWaveStartShare = 0.12f;

	/** Seconds a streak of a blast flies until it has faded out */
	constexpr float AirStreakTime = 0.85f;

	/** Seconds torn-up dust drifts and a card left by a trail lasts */
	constexpr float AirDustTime = 0.8f;
	constexpr float AirTrailCardTime = 0.6f;

	/** A trail leaves shimmer and dust every this many cm */
	constexpr float AirTrailSpacing = 70.0f;
}

AVaelAirBlast::AVaelAirBlast()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AVaelAirBlast* AVaelAirBlast::SpawnBlast(AActor* Caster, const FVector& InOrigin, const FVector& Direction, float InRange, float InHalfAngle)
{
	UWorld* World = Caster != nullptr ? Caster->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}
	FActorSpawnParameters Parameters;
	Parameters.Owner = Caster;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVaelAirBlast* Blast = World->SpawnActor<AVaelAirBlast>(AVaelAirBlast::StaticClass(), FTransform(InOrigin), Parameters);
	if (Blast == nullptr)
	{
		return nullptr;
	}
	Blast->Origin = InOrigin;
	Blast->Aim = Direction.GetSafeNormal2D();
	Blast->Range = InRange;
	Blast->HalfAngle = InHalfAngle;
	Blast->Duration = AirWaveTime + AirDustTime + 0.2f;
	FHitResult GroundHit;
	Blast->GroundZ = VaelGround::TraceGround(World, InOrigin, InOrigin - FVector(0.0f, 0.0f, 400.0f), GroundHit, Cast<APawn>(Caster)) ? GroundHit.Location.Z : InOrigin.Z - 90.0f;
	const bool bRing = InHalfAngle >= 179.0f;
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	Blast->BuildParts(bRing ? MagicSettings->AirRingShimmer : MagicSettings->AirConeShimmer, bRing ? MagicSettings->AirRingStreaks : MagicSettings->AirConeStreaks, MagicSettings->AirDustCards);
	Blast->SetLifeSpan(Blast->Duration);
	return Blast;
}

AVaelAirBlast* AVaelAirBlast::SpawnTrail(APawn* InRider, float InDuration)
{
	UWorld* World = InRider != nullptr ? InRider->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}
	FActorSpawnParameters Parameters;
	Parameters.Owner = InRider;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVaelAirBlast* Trail = World->SpawnActor<AVaelAirBlast>(AVaelAirBlast::StaticClass(), FTransform(InRider->GetActorLocation()), Parameters);
	if (Trail == nullptr)
	{
		return nullptr;
	}
	Trail->bTrail = true;
	Trail->Rider = InRider;
	Trail->Duration = InDuration;
	Trail->LastTrailPlace = InRider->GetActorLocation();
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	Trail->BuildParts(MagicSettings->AirTrailShimmer, MagicSettings->AirTrailStreaks, MagicSettings->AirTrailShimmer);
	Trail->SetLifeSpan(InDuration + AirTrailCardTime + 0.3f);
	return Trail;
}

void AVaelAirBlast::BuildParts(int32 NumShimmer, int32 NumStreaks, int32 NumDust)
{
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	const auto MakeCards = [this](const TSoftObjectPtr<UMaterialInterface>& Material) -> UVaelCloudCardsComponent*
	{
		UMaterialInterface* Loaded = VaelAssets::LoadOptional(Material);
		if (Loaded == nullptr)
		{
			return nullptr;
		}
		UVaelCloudCardsComponent* Cards = NewObject<UVaelCloudCardsComponent>(this);
		Cards->SetupAttachment(RootComponent);
		Cards->SetUsingAbsoluteLocation(true);
		Cards->SetUsingAbsoluteRotation(true);
		Cards->RegisterComponent();
		Cards->SetWorldTransform(FTransform::Identity);
		Cards->Init(Loaded);
		return Cards;
	};
	Shimmer = MakeCards(MagicSettings->AirCardMaterial);
	Sand = MakeCards(MagicSettings->SandCardMaterial);
	Ash = MakeCards(MagicSettings->AshCardMaterial);

	if (UMaterialInterface* StreakMaterial = VaelAssets::LoadOptional(MagicSettings->StreakMaterial))
	{
		Streaks = NewObject<UVaelWindStreaksComponent>(this);
		Streaks->SetupAttachment(RootComponent);
		Streaks->RegisterComponent();
		Streaks->Init(StreakMaterial);
	}

	const float Spread = bTrail ? 180.0f : HalfAngle;
	for (int32 Index = 0; Index < NumShimmer; ++Index)
	{
		ShimmerAngles.Add(FMath::Lerp(-Spread, Spread, (Index + 0.5f) / NumShimmer) + FMath::FRandRange(-5.0f, 5.0f));
		ShimmerHeights.Add(FMath::FRandRange(40.0f, 130.0f));
		if (Shimmer != nullptr)
		{
			Shimmer->AddCard(Origin, Range * 0.3f, 0.0f);
		}
		TrailPlaces.Add(FVector::ZeroVector);
		TrailTimes.Add(-10.0f);
	}
	for (int32 Index = 0; Index < NumStreaks; ++Index)
	{
		StreakAngles.Add(bTrail ? Index * 360.0f / NumStreaks : FMath::FRandRange(-Spread, Spread) * 0.9f);
		StreakCurls.Add(FMath::FRandRange(-40.0f, 40.0f));
		StreakHeights.Add(FMath::FRandRange(25.0f, 140.0f));
		StreakSpeeds.Add(FMath::FRandRange(0.9f, 1.25f));
		StreakDelays.Add(FMath::FRandRange(0.0f, 0.14f));
		if (Streaks != nullptr)
		{
			Streaks->AddStreak(Origin, MagicSettings->AirStreakWidth, bTrail ? 260.0f : Range * 0.35f);
		}
	}
	// Torn-up dust: on the ground inside the cone, half sand, half ash; a trail places its own as it goes
	for (int32 Index = 0; Index < NumDust; ++Index)
	{
		const float Angle = FMath::DegreesToRadians(FMath::FRandRange(-Spread, Spread) * 0.9f);
		const FVector Out = Aim.RotateAngleAxis(FMath::RadiansToDegrees(Angle), FVector::UpVector);
		const float Distance = Range * FMath::FRandRange(0.25f, 0.95f);
		const FVector Place = FVector(Origin.X, Origin.Y, GroundZ) + Out * Distance;
		DustPlaces.Add(Place);
		DustStarts.Add(bTrail ? -10.0f : WaveArrival(Distance));
		UVaelCloudCardsComponent* Cards = Index % 2 == 0 ? Sand : Ash;
		if (Cards != nullptr)
		{
			Cards->AddCard(Place, 120.0f, 0.0f);
		}
	}
}

float AVaelAirBlast::WaveDistance(float Time) const
{
	const float Progress = FMath::Clamp(Time / AirWaveTime, 0.0f, 1.0f);
	return Range * FMath::Lerp(AirWaveStartShare, 1.0f, 1.0f - FMath::Square(1.0f - Progress));
}

float AVaelAirBlast::WaveArrival(float Distance) const
{
	const float Eased = FMath::Clamp((Distance / Range - AirWaveStartShare) / (1.0f - AirWaveStartShare), 0.0f, 1.0f);
	return (1.0f - FMath::Sqrt(1.0f - Eased)) * AirWaveTime;
}

void AVaelAirBlast::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (bTrail)
	{
		TickTrail(DeltaSeconds);
	}
	else
	{
		TickBlast();
	}
	for (UVaelCloudCardsComponent* Cards : { Shimmer.Get(), Sand.Get(), Ash.Get() })
	{
		if (Cards != nullptr)
		{
			Cards->Commit();
		}
	}
	if (Streaks != nullptr)
	{
		Streaks->Commit();
	}
}

void AVaelAirBlast::TickBlast()
{
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	const FVector Base(Origin.X, Origin.Y, GroundZ);

	// The wave: a front of shimmering air rolling out through the cone, strongest in the middle of its run
	const float Front = WaveDistance(Age);
	const float Run = FMath::Clamp(Age / (AirWaveTime * 1.35f), 0.0f, 1.0f);
	// (a ring is short in reach: its cards are made larger and fainter so they run together into one band)
	const bool bRing = HalfAngle >= 179.0f;
	const float CardSize = Range * FMath::Lerp(0.35f, 0.7f, Run) * (bRing ? 1.7f : 1.0f);
	const float CardSolid = FMath::Sin(Run * UE_PI) * (bRing ? 0.4f : 0.9f);
	for (int32 Index = 0; Index < ShimmerAngles.Num() && Shimmer != nullptr; ++Index)
	{
		const FVector Out = Aim.RotateAngleAxis(ShimmerAngles[Index], FVector::UpVector);
		Shimmer->SetCard(Index, Base + Out * Front + FVector(0.0f, 0.0f, ShimmerHeights[Index]), CardSize, CardSolid);
	}

	// The streaks race out, curling a little, and fade at the end of the reach
	for (int32 Index = 0; Index < StreakAngles.Num() && Streaks != nullptr; ++Index)
	{
		const float Flight = FMath::Max(Age - StreakDelays[Index], 0.0f);
		const float Progress = FMath::Clamp(Flight * StreakSpeeds[Index] / AirStreakTime, 0.0f, 1.0f);
		const float Distance = Range * FMath::Lerp(AirWaveStartShare, 1.05f, 1.0f - FMath::Square(1.0f - Progress));
		const FVector Out = Aim.RotateAngleAxis(StreakAngles[Index] + StreakCurls[Index] * Flight, FVector::UpVector);
		const float Strength = FMath::Clamp(Flight / 0.06f, 0.0f, 1.0f) * FMath::Clamp((1.0f - Progress) / 0.4f, 0.0f, 1.0f);
		Streaks->MoveHead(Index, Base + Out * Distance + FVector(0.0f, 0.0f, StreakHeights[Index] + Flight * 60.0f), Age < StreakDelays[Index] ? 0.0f : Strength);
	}

	// Dust and ash torn up as the wave passes, blown on, rising and thinning; a few pebbles thrown along
	for (int32 Index = 0; Index < DustPlaces.Num(); ++Index)
	{
		UVaelCloudCardsComponent* Cards = Index % 2 == 0 ? Sand.Get() : Ash.Get();
		if (Cards == nullptr)
		{
			continue;
		}
		const float Since = Age - DustStarts[Index];
		const int32 Card = Index / 2;
		if (Since < 0.0f)
		{
			continue;
		}
		if (Since < 0.05f && NumPebbleBursts < MagicSettings->AirPebbleBursts && Index % 3 == 0)
		{
			++NumPebbleBursts;
			FVaelDebris Pebbles;
			Pebbles.Count = 4;
			Pebbles.Spread = 25.0f;
			Pebbles.Speed = 220.0f;
			Pebbles.Lift = 180.0f;
			Pebbles.Lifetime = 0.6f;
			Pebbles.MinSize = 2.0f;
			Pebbles.MaxSize = 5.0f;
			Pebbles.Color = FLinearColor(0.32f, 0.27f, 0.22f);
			AVaelDebrisBurst::Spawn(this, DustPlaces[Index] + FVector(0.0f, 0.0f, 5.0f), Pebbles);
		}
		const float Drift = FMath::Clamp(Since / AirDustTime, 0.0f, 1.0f);
		const float Eased = 1.0f - FMath::Square(1.0f - Drift);
		const FVector Out = (DustPlaces[Index] - Base).GetSafeNormal2D();
		Cards->SetCard(Card, DustPlaces[Index] + Out * 170.0f * Eased + FVector(0.0f, 0.0f, 25.0f + 70.0f * Eased), FMath::Lerp(110.0f, 230.0f, Eased),
			MagicSettings->AirDustSolid * FMath::Sin(FMath::Min(Drift * 1.6f, 1.0f) * UE_HALF_PI) * (1.0f - Drift));
	}
}

void AVaelAirBlast::TickTrail(float DeltaSeconds)
{
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	const APawn* Who = Rider.Get();
	const bool bRiding = Who != nullptr && Age < Duration;
	const FVector Here = Who != nullptr ? Who->GetActorLocation() : LastTrailPlace;
	const float Strength = bRiding ? FMath::Clamp(Age / 0.08f, 0.0f, 1.0f) : FMath::Clamp(1.0f - (Age - Duration) / 0.25f, 0.0f, 1.0f);

	// Streaks wrap the rider, turning slowly; as the rider rushes on, their trails stream out behind
	for (int32 Index = 0; Index < StreakAngles.Num() && Streaks != nullptr; ++Index)
	{
		const float Angle = FMath::DegreesToRadians(StreakAngles[Index] + Age * 90.0f);
		const FVector Around(FMath::Cos(Angle) * 55.0f, FMath::Sin(Angle) * 55.0f, StreakHeights[Index] - 80.0f);
		Streaks->MoveHead(Index, Here + Around, Strength);
	}

	// Every stretch of the path gets a breath of shimmer at body height and a puff of dust and ash on the ground
	if (bRiding && FVector::Dist2D(Here, LastTrailPlace) >= AirTrailSpacing && !TrailPlaces.IsEmpty())
	{
		LastTrailPlace = Here;
		FHitResult GroundHit;
		const float Ground = VaelGround::TraceGround(GetWorld(), Here, Here - FVector(0.0f, 0.0f, 400.0f), GroundHit, Rider.Get()) ? GroundHit.Location.Z : Here.Z - 90.0f;
		TrailPlaces[NextTrailCard] = FVector(Here.X, Here.Y, Ground);
		TrailTimes[NextTrailCard] = Age;
		NextTrailCard = (NextTrailCard + 1) % TrailPlaces.Num();
	}
	for (int32 Index = 0; Index < TrailPlaces.Num(); ++Index)
	{
		const float Since = Age - TrailTimes[Index];
		const float Left = FMath::Clamp(1.0f - Since / AirTrailCardTime, 0.0f, 1.0f);
		const FVector Place = TrailPlaces[Index];
		if (Shimmer != nullptr)
		{
			Shimmer->SetCard(Index, Place + FVector(0.0f, 0.0f, 90.0f + Since * 40.0f), 160.0f + Since * 120.0f, 0.8f * Left);
		}
		UVaelCloudCardsComponent* Cards = Index % 2 == 0 ? Sand.Get() : Ash.Get();
		if (Cards != nullptr && Index / 2 < Cards->Num())
		{
			Cards->SetCard(Index / 2, Place + FVector(0.0f, 0.0f, 20.0f + Since * 70.0f), 110.0f + Since * 160.0f, MagicSettings->AirDustSolid * Left);
		}
	}
}
