// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelWeatherWind.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Magic/VaelCloudCards.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelWindStreaks.h"
#include "Materials/MaterialInterface.h"
#include "Player/VaelCharacter.h"
#include "VaelAssets.h"
#include "World/VaelGround.h"
#include "World/VaelRegion.h"
#include "World/VaelWorldSettings.h"

namespace
{
	/** Streaks race faster than the air they ride on, the dust is slower */
	constexpr float WindStreakSpeedShare = 1.6f;
	constexpr float WindDustSpeedShare = 0.7f;

	/** Longest trail of a streak in cm */
	constexpr float WindStreakTrail = 420.0f;
}

AVaelWeatherWind::AVaelWeatherWind()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AVaelWeatherWind* AVaelWeatherWind::Spawn(AVaelRegion* InRegion)
{
	UWorld* World = InRegion != nullptr ? InRegion->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}

	FActorSpawnParameters Parameters;
	Parameters.Owner = InRegion;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVaelWeatherWind* Wind = World->SpawnActor<AVaelWeatherWind>(AVaelWeatherWind::StaticClass(), FTransform::Identity, Parameters);
	if (Wind != nullptr)
	{
		Wind->Region = InRegion;
		Wind->Build();
	}

	return Wind;
}

void AVaelWeatherWind::Build()
{
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	const UVaelWorldSettings* Settings = UVaelWorldSettings::Get();

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
	Ash = MakeCards(MagicSettings->AshCardMaterial);
	Sand = MakeCards(MagicSettings->SandCardMaterial);

	if (UMaterialInterface* StreakMaterial = VaelAssets::LoadOptional(MagicSettings->StreakMaterial))
	{
		Streaks = NewObject<UVaelWindStreaksComponent>(this);
		Streaks->SetupAttachment(RootComponent);
		Streaks->RegisterComponent();
		Streaks->Init(StreakMaterial);
	}

	// Everything starts somewhere on its way, so the wind doesn't begin in one wave
	FVector Middle;
	const bool bHasMiddle = FindMiddle(Middle);
	const FVector Direction = Region.IsValid() ? Region->GetWindDirection() : FVector::ForwardVector;

	const auto Fill = [&](TArray<FGust>& Gusts, int32 Count, UVaelCloudCardsComponent* Cards, float MinLife, float MaxLife)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FGust& Gust = Gusts.AddDefaulted_GetRef();
			if (bHasMiddle)
			{
				Restart(Gust, Middle, Direction, MinLife, MaxLife);
			}
			Gust.Age = FMath::FRandRange(0.0f, Gust.Life);
			if (Cards != nullptr)
			{
				Cards->AddCard(Gust.Place, Gust.Size, 0.0f);
			}
			else if (&Gusts == &StreakGusts && Streaks != nullptr)
			{
				Streaks->AddStreak(Gust.Place, MagicSettings->AirStreakWidth * FMath::FRandRange(0.6f, 1.1f), WindStreakTrail * FMath::FRandRange(0.6f, 1.0f));
			}
		}
	};
	Fill(StreakGusts, Settings->WindStreakCount, nullptr, 0.9f, 1.6f);
	Fill(AshGusts, Settings->WindAshCount, Ash, 2.0f, 3.5f);
	Fill(SandGusts, Settings->WindSandCount, Sand, 1.6f, 2.8f);
}

bool AVaelWeatherWind::FindMiddle(FVector& OutMiddle) const
{
	FVector Sum = FVector::ZeroVector;
	int32 Count = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		const APawn* Pawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
		if (Pawn != nullptr && (!Region.IsValid() || Region->Contains(Pawn->GetActorLocation())))
		{
			Sum += Pawn->GetActorLocation();
			++Count;
		}
	}

	if (Count == 0)
	{
		return false;
	}

	OutMiddle = Sum / Count;
	return true;
}

void AVaelWeatherWind::Restart(FGust& Gust, const FVector& Middle, const FVector& Direction, float MinLife, float MaxLife)
{
	// Anywhere across the wind, mostly upwind of the players, so it blows through the picture
	const float Reach = UVaelWorldSettings::Get()->WindLookReach;
	const FVector Side(-Direction.Y, Direction.X, 0.0f);
	const FVector Start = Middle + Side * FMath::FRandRange(-Reach, Reach) + Direction * FMath::FRandRange(-Reach, Reach * 0.4f);

	FHitResult GroundHit;
	const float Ground = VaelGround::TraceGround(GetWorld(), Start + FVector(0.0f, 0.0f, 600.0f), Start - FVector(0.0f, 0.0f, 1200.0f), GroundHit) ? GroundHit.Location.Z : Middle.Z - 90.0f;

	Gust.Place = FVector(Start.X, Start.Y, Ground);
	Gust.Age = 0.0f;
	Gust.Life = FMath::FRandRange(MinLife, MaxLife);
	Gust.Size = FMath::FRandRange(120.0f, 220.0f);
	Gust.Height = FMath::FRandRange(15.0f, 150.0f);
	Gust.Wobble = FMath::FRandRange(0.0f, 10.0f);
}

void AVaelWeatherWind::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const AVaelRegion* WindRegion = Region.Get();
	FVector Middle;
	if (WindRegion == nullptr || !FindMiddle(Middle))
	{
		return;
	}

	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	// The wind where each gust is: the weather of the region and local weathers like a thermal or a wind front; new gusts start upwind of the players
	UWorld* World = GetWorld();
	const FVector MiddleWind = AVaelRegion::GetWindAt(World, Middle);
	const FVector Direction = MiddleWind.SizeSquared() > 1.0f ? MiddleWind.GetSafeNormal2D() : WindRegion->GetWindDirection();
	const float FullWind = FMath::Max(UVaelWorldSettings::Get()->WindSpeed, 1.0f);

	for (int32 Index = 0; Index < StreakGusts.Num(); ++Index)
	{
		FGust& Gust = StreakGusts[Index];
		Gust.Age += DeltaSeconds;
		if (Gust.Age >= Gust.Life)
		{
			Restart(Gust, Middle, Direction, 0.9f, 1.6f);
			if (Streaks != nullptr)
			{
				Streaks->Restart(Index, Gust.Place + FVector(0.0f, 0.0f, Gust.Height));
			}
			continue;
		}

		const FVector Wind = AVaelRegion::GetWindAt(World, Gust.Place);
		const FVector Side = FVector(-Wind.Y, Wind.X, 0.0f).GetSafeNormal();

		// How hard it blows here against the plain wind weather: gusts bring more and stronger streaks
		const float Share = FMath::Clamp(Wind.Size() / FullWind, 0.0f, 1.5f);

		// Racing over the land, weaving a little across the wind and up and down
		Gust.Place += Wind * WindStreakSpeedShare * DeltaSeconds;
		const float Weave = FMath::Sin(Gust.Age * 3.1f + Gust.Wobble) * 30.0f;
		const float Life = Gust.Age / Gust.Life;

		// In a lull only some of the streaks show
		const float Shown = FMath::Clamp(Share * 1.4f - (Index % 4) * 0.25f, 0.0f, 1.0f);
		if (Streaks != nullptr)
		{
			Streaks->MoveHead(Index, Gust.Place + Side * Weave + FVector(0.0f, 0.0f, Gust.Height + FMath::Sin(Gust.Age * 2.0f + Gust.Wobble) * 12.0f),
				FMath::Sin(Life * UE_PI) * Shown);
		}
	}

	const auto Drift = [&](TArray<FGust>& Gusts, UVaelCloudCardsComponent* Cards, float MinLife, float MaxLife, float Solid)
	{
		if (Cards == nullptr)
		{
			return;
		}

		for (int32 Index = 0; Index < Gusts.Num(); ++Index)
		{
			FGust& Gust = Gusts[Index];
			Gust.Age += DeltaSeconds;
			if (Gust.Age >= Gust.Life)
			{
				Restart(Gust, Middle, Direction, MinLife, MaxLife);
			}

			const FVector Wind = AVaelRegion::GetWindAt(World, Gust.Place);
			const float Share = FMath::Clamp(Wind.Size() / FullWind, 0.0f, 1.5f);

			// Torn off the ground, it rolls downwind, rises a little and spreads out as it thins
			Gust.Place += Wind * WindDustSpeedShare * DeltaSeconds;
			const float Life = Gust.Age / Gust.Life;
			const float Shown = FMath::Clamp(Share * 1.2f - (Index % 3) * 0.3f, 0.0f, 1.0f);
			Cards->SetCard(Index, Gust.Place + FVector(0.0f, 0.0f, 20.0f + 60.0f * Life), Gust.Size * (0.7f + 0.8f * Life),
				Solid * FMath::Sin(FMath::Min(Life * 2.0f, 1.0f) * UE_HALF_PI) * (1.0f - Life) * Shown);
		}
		Cards->Commit();
	};
	Drift(AshGusts, Ash, 2.0f, 3.5f, MagicSettings->AirDustSolid);
	Drift(SandGusts, Sand, 1.6f, 2.8f, MagicSettings->AirDustSolid * 0.8f);

	if (Streaks != nullptr)
	{
		Streaks->Commit();
	}
}
