// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelRegion.h"
#include "Combat/VaelCombatStatics.h"
#include "Components/BoxComponent.h"
#include "Creatures/VaelCreature.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Magic/VaelGroundArea.h"
#include "Player/VaelCharacter.h"
#include "UI/VaelNoticeSubsystem.h"
#include "Vael.h"
#include "World/VaelLightningStrike.h"
#include "World/VaelWorldSettings.h"

#define LOCTEXT_NAMESPACE "VaelWorld"

namespace
{
	/** Seconds between two refreshes of the wetness in the rain */
	constexpr float RainRefreshInterval = 0.5f;
}

AVaelRegion::AVaelRegion()
{
	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	Bounds->InitBoxExtent(FVector(5000.0f, 5000.0f, 2000.0f));
	Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bounds->ShapeColor = FColor(162, 77, 255);
	RootComponent = Bounds;

	RegionName = LOCTEXT("DefaultRegionName", "Aschenmark");

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AVaelRegion::BeginPlay()
{
	Super::BeginPlay();

	const UVaelWorldSettings* Settings = UVaelWorldSettings::Get();

	Corruption = FMath::Clamp(StartCorruption >= 0.0f ? StartCorruption : Settings->DefaultCorruption, 0.0f, 100.0f);
	Weather = StartWeather;
	WeatherTimeLeft = FMath::FRandRange(Settings->WeatherDuration.X, Settings->WeatherDuration.Y);
	LightningCooldown = FMath::FRandRange(Settings->LightningInterval.X, Settings->LightningInterval.Y);
}

void AVaelRegion::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bChangingWeather)
	{
		WeatherTimeLeft -= DeltaSeconds;
		if (WeatherTimeLeft <= 0.0f)
		{
			SetWeather(PickNextWeather());
		}
	}

	if (Weather == EVaelWeather::Rain)
	{
		TickRain(DeltaSeconds);
	}
	else if (Weather == EVaelWeather::Storm)
	{
		TickStorm(DeltaSeconds);
	}
}

AVaelRegion* AVaelRegion::GetRegionAt(UWorld* World, const FVector& Location)
{
	if (World == nullptr)
	{
		return nullptr;
	}

	AVaelRegion* WholeLevel = nullptr;
	for (TActorIterator<AVaelRegion> It(World); It; ++It)
	{
		if (It->bCoversWholeLevel)
		{
			WholeLevel = WholeLevel != nullptr ? WholeLevel : *It;
		}
		else if (It->Contains(Location))
		{
			return *It;
		}
	}

	// Levels without regions still have weather and corruption
	if (WholeLevel == nullptr && World->IsGameWorld())
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParameters.bDeferConstruction = true;

		WholeLevel = World->SpawnActor<AVaelRegion>(AVaelRegion::StaticClass(), FTransform::Identity, SpawnParameters);
		if (WholeLevel != nullptr)
		{
			WholeLevel->bCoversWholeLevel = true;
			WholeLevel->FinishSpawning(FTransform::Identity);

			UE_LOG(LogVael, Log, TEXT("No region in the level, '%s' covers all of it"), *WholeLevel->RegionName.ToString());
		}
	}

	return WholeLevel;
}

bool AVaelRegion::Contains(const FVector& Location) const
{
	if (bCoversWholeLevel)
	{
		return true;
	}

	const FVector Local = Bounds->GetComponentTransform().InverseTransformPosition(Location);
	const FVector Extent = Bounds->GetUnscaledBoxExtent();

	return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
}

void AVaelRegion::AddCorruption(float Amount)
{
	SetCorruption(Corruption + Amount);
}

void AVaelRegion::SetCorruption(float NewCorruption)
{
	Corruption = FMath::Clamp(NewCorruption, 0.0f, 100.0f);
}

void AVaelRegion::SetWeather(EVaelWeather NewWeather)
{
	const UVaelWorldSettings* Settings = UVaelWorldSettings::Get();

	WeatherTimeLeft = FMath::FRandRange(Settings->WeatherDuration.X, Settings->WeatherDuration.Y);

	if (NewWeather == Weather)
	{
		return;
	}

	Weather = NewWeather;
	RainRefreshTime = 0.0f;

	UE_LOG(LogVael, Log, TEXT("Weather in '%s': %s"), *RegionName.ToString(), *GetWeatherName(Weather).ToString());

	// Fires already burning die sooner once the rain starts
	if (Weather == EVaelWeather::Rain)
	{
		for (TActorIterator<AVaelGroundArea> It(GetWorld()); It; ++It)
		{
			const float LifeSpan = It->GetLifeSpan();
			if (It->GetElement() == EVaelElement::Fire && LifeSpan > 0.0f && Contains(It->GetActorLocation()))
			{
				It->SetLifeSpan(FMath::Max(LifeSpan * 0.625f, 0.1f));
			}
		}
	}

	if (!HasPlayerInside())
	{
		return;
	}

	switch (Weather)
	{
	case EVaelWeather::Rain:
		UVaelNoticeSubsystem::Post(this, LOCTEXT("RainStarts", "Ascheregen setzt ein"),
			LOCTEXT("RainStartsDetail", "Alles wird nass: Blitze treffen doppelt, Feuer erlischt schneller."), FLinearColor(FColor(158, 199, 232)));
		break;
	case EVaelWeather::Storm:
		UVaelNoticeSubsystem::Post(this, LOCTEXT("StormStarts", "Ein Sturm zieht auf"),
			LOCTEXT("StormStartsDetail", "Luft liegt \u00FCberall in der Umgebung. Achtung, Blitzeinschl\u00E4ge!"), FLinearColor(FColor(214, 236, 255)));
		break;
	default:
		UVaelNoticeSubsystem::Post(this, LOCTEXT("SkyClears", "Der Himmel klart auf"), FText::GetEmpty(), FLinearColor(FColor(214, 236, 255)));
		break;
	}
}

FText AVaelRegion::GetWeatherName(EVaelWeather InWeather)
{
	switch (InWeather)
	{
	case EVaelWeather::Rain: return LOCTEXT("WeatherRain", "Ascheregen");
	case EVaelWeather::Storm: return LOCTEXT("WeatherStorm", "Sturm");
	default: return LOCTEXT("WeatherClear", "Klarer Himmel");
	}
}

EVaelWeather AVaelRegion::PickNextWeather() const
{
	// Between the weights of a pure and a fully corrupted region; never the same weather twice in a row
	const UVaelWorldSettings* Settings = UVaelWorldSettings::Get();
	const FVector Weights = FMath::Lerp(Settings->WeatherWeightsPure, Settings->WeatherWeightsCorrupted, Corruption / 100.0f);
	const EVaelWeather Options[] = { EVaelWeather::Clear, EVaelWeather::Rain, EVaelWeather::Storm };

	float TotalWeight = 0.0f;
	for (int32 OptionIndex = 0; OptionIndex < 3; ++OptionIndex)
	{
		TotalWeight += Options[OptionIndex] != Weather ? FMath::Max(Weights[OptionIndex], 0.0f) : 0.0f;
	}

	float Pick = FMath::FRandRange(0.0f, TotalWeight);
	for (int32 OptionIndex = 0; OptionIndex < 3; ++OptionIndex)
	{
		if (Options[OptionIndex] == Weather)
		{
			continue;
		}

		Pick -= FMath::Max(Weights[OptionIndex], 0.0f);
		if (Pick <= 0.0f)
		{
			return Options[OptionIndex];
		}
	}

	return Weather == EVaelWeather::Clear ? EVaelWeather::Rain : EVaelWeather::Clear;
}

void AVaelRegion::TickRain(float DeltaSeconds)
{
	RainRefreshTime -= DeltaSeconds;
	if (RainRefreshTime > 0.0f)
	{
		return;
	}

	RainRefreshTime = RainRefreshInterval;

	const float WetDuration = UVaelWorldSettings::Get()->RainWetDuration;
	for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
	{
		if (!It->IsDead() && Contains(It->GetActorLocation()))
		{
			UVaelCombatStatics::ApplyStatus(nullptr, *It, EVaelStatus::Wet, WetDuration);
		}
	}
}

void AVaelRegion::TickStorm(float DeltaSeconds)
{
	LightningCooldown -= DeltaSeconds;
	if (LightningCooldown > 0.0f)
	{
		return;
	}

	// Strikes come faster in a corrupted region
	const UVaelWorldSettings* Settings = UVaelWorldSettings::Get();
	LightningCooldown = FMath::FRandRange(Settings->LightningInterval.X, Settings->LightningInterval.Y) * (1.0f - Settings->LightningCorruptionSpeedup * Corruption / 100.0f);

	// Near a random player in the region
	TArray<AVaelCharacter*> Players;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
		if (Player != nullptr && !Player->IsDowned() && Contains(Player->GetActorLocation()))
		{
			Players.Add(Player);
		}
	}

	if (Players.IsEmpty())
	{
		return;
	}

	const AVaelCharacter* Target = Players[FMath::RandRange(0, Players.Num() - 1)];
	const float Angle = FMath::FRandRange(0.0f, UE_TWO_PI);
	const float Distance = FMath::FRandRange(Settings->LightningDistance.X, Settings->LightningDistance.Y);

	FVector Ground;
	if (AVaelCreature::FindGround(GetWorld(), Target->GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Distance, Ground))
	{
		AVaelLightningStrike::SpawnStrike(this, Ground);
	}
}

bool AVaelRegion::HasPlayerInside() const
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		const APawn* Pawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
		if (Pawn != nullptr && Contains(Pawn->GetActorLocation()))
		{
			return true;
		}
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
