// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelRegion.h"
#include "Save/VaelSaveSubsystem.h"
#include "AbilitySystemComponent.h"
#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelCombatStatics.h"
#include "Components/BoxComponent.h"
#include "Creatures/VaelCreature.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Magic/VaelGroundArea.h"
#include "Magic/VaelGameplayTags.h"
#include "Magic/VaelElementSource.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelWaterPuddle.h"
#include "Player/VaelCharacter.h"
#include "UI/VaelNoticeSubsystem.h"
#include "Vael.h"
#include "VaelAssets.h"
#include "World/VaelLightningStrike.h"
#include "World/VaelProgressSubsystem.h"
#include "World/VaelWeatherRain.h"
#include "World/VaelWeatherWind.h"
#include "World/VaelWorldSettings.h"
#include "UI/VaelUISettings.h"
#include "UI/VaelWorldMap.h"

#define LOCTEXT_NAMESPACE "VaelWorld"

namespace
{
	/** Seconds between two refreshes of wetness in the rain or dryness in a drought */
	constexpr float WetnessRefreshInterval = 0.5f;
}

AVaelRegion::AVaelRegion()
{
	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	Bounds->InitBoxExtent(FVector(5000.0f, 5000.0f, 2000.0f));
	Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bounds->ShapeColor = FColor(162, 77, 255);
	RootComponent = Bounds;

	RegionName = LOCTEXT("DefaultRegionName", "Aschenmark");
	AvailableWeathers = { EVaelWeather::Clear, EVaelWeather::Rain, EVaelWeather::Storm, EVaelWeather::Drought, EVaelWeather::Wind };

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

#if WITH_EDITORONLY_DATA
	// In an open world the region and its corruption stay loaded even when nobody is near
	bIsSpatiallyLoaded = false;
#endif
}

void AVaelRegion::BeginPlay()
{
	Super::BeginPlay();

	const UVaelWorldSettings* Settings = UVaelWorldSettings::Get();

	Corruption = FMath::Clamp(StartCorruption >= 0.0f ? StartCorruption : Settings->DefaultCorruption, 0.0f, 100.0f);
	Weather = StartWeather;
	WeatherTimeLeft = bCalmStart ? FMath::Max(Settings->CalmStartDuration, PickWeatherDuration()) : PickWeatherDuration();
	LightningCooldown = FMath::FRandRange(Settings->LightningInterval.X, Settings->LightningInterval.Y);

	// What an earlier session left of the region
	if (const UVaelSaveSubsystem* Save = UVaelSaveSubsystem::Get(this))
	{
		Save->RestoreRegion(this);
	}
}

void AVaelRegion::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bChangingWeather)
	{
		WeatherTimeLeft -= DeltaSeconds;
		if (WeatherTimeLeft <= 0.0f)
		{
			SetWeather(GetComingWeather());
		}
	}

	switch (Weather)
	{
	case EVaelWeather::Rain:
		TickRain(DeltaSeconds);
		break;
	case EVaelWeather::Storm:
		TickStorm(DeltaSeconds);
		break;
	case EVaelWeather::Drought:
		TickDrought(DeltaSeconds);
		break;
	default:
		break;
	}

	TickWind(DeltaSeconds);
}

FVector AVaelRegion::GetWindAt(UWorld* World, const FVector& Location)
{
	const AVaelRegion* Region = GetRegionAt(World, Location);
	return Region != nullptr ? Region->GetWind() : FVector::ZeroVector;
}

void AVaelRegion::TickWind(float DeltaSeconds)
{
	// The wind rises over a few seconds when the weather brings it and dies down when it goes
	const UVaelWorldSettings* Settings = UVaelWorldSettings::Get();
	const float TargetStrength = Weather == EVaelWeather::Wind ? Settings->WindSpeed : Weather == EVaelWeather::Storm ? Settings->StormWindSpeed : 0.0f;
	const float RiseSpeed = FMath::Max(Settings->WindSpeed, Settings->StormWindSpeed) / Settings->WindRiseTime;
	WindStrength = FMath::FInterpConstantTo(WindStrength, TargetStrength, DeltaSeconds, RiseSpeed);

	// Gusts swell and drop without a rhythm
	const float GustTime = GetWorld()->GetTimeSeconds() * Settings->WindGustsPerMinute / 60.0f;
	const float Gust = 0.5f + 0.5f * FMath::Clamp(FMath::PerlinNoise1D(GustTime) * 1.6f, -1.0f, 1.0f);
	WindGust = FMath::Lerp(Settings->WindGustRange.X, Settings->WindGustRange.Y, Gust);

	// Ash, sand and streaks drift over the land around the players while it blows
	const bool bBlowing = WindStrength > 1.0f && HasPlayerInside();
	if (bBlowing && WindLook == nullptr)
	{
		WindLook = AVaelWeatherWind::Spawn(this);
	}
	else if (!bBlowing && WindLook != nullptr)
	{
		WindLook->Destroy();
		WindLook = nullptr;
	}

	// The rain sets in and stops over a few seconds; a storm pours harder
	const float TargetRain = Weather == EVaelWeather::Rain ? 1.0f : Weather == EVaelWeather::Storm ? 1.4f : 0.0f;
	RainStrength = FMath::FInterpConstantTo(RainStrength, TargetRain, DeltaSeconds, 0.35f);
	const bool bRaining = RainStrength > 0.01f && HasPlayerInside();
	if (bRaining && RainLook == nullptr)
	{
		RainLook = AVaelWeatherRain::Spawn(this);
	}
	else if (!bRaining && RainLook != nullptr)
	{
		RainLook->Destroy();
		RainLook = nullptr;
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

			// Named after the map of the level, if it has one
			const FName LevelName(*UWorld::RemovePIEPrefix(World->GetMapName()));
			if (const TSoftObjectPtr<UVaelWorldMap>* MapEntry = UVaelUISettings::Get()->LevelMaps.Find(LevelName))
			{
				const UVaelWorldMap* Map = VaelAssets::LoadOptional(*MapEntry);
				if (Map != nullptr && !Map->Title.IsEmpty())
				{
					WholeLevel->RegionName = Map->Title;
				}
			}
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

float AVaelRegion::GetArea() const
{
	if (bCoversWholeLevel)
	{
		return 0.0f;
	}

	const FVector Extent = Bounds->GetScaledBoxExtent();
	return 4.0f * Extent.X * Extent.Y / 10000.0f;
}

void AVaelRegion::AddCorruption(float Amount)
{
	SetCorruption(Corruption + Amount);
}

void AVaelRegion::SetCorruption(float NewCorruption)
{
	Corruption = FMath::Clamp(NewCorruption, ScarredCorruption, 100.0f);
}

void AVaelRegion::AddScar(float Amount)
{
	ScarredCorruption = FMath::Clamp(ScarredCorruption + Amount, 0.0f, 100.0f);
	AddCorruption(Amount);
}

EVaelWeather AVaelRegion::GetComingWeather() const
{
	if (!ComingWeather.IsSet())
	{
		ComingWeather = PickNextWeather();
	}

	return ComingWeather.GetValue();
}

void AVaelRegion::SetWeather(EVaelWeather NewWeather)
{
	WeatherTimeLeft = PickWeatherDuration();
	ComingWeather.Reset();

	if (NewWeather == Weather)
	{
		return;
	}

	// A new wind comes from a new side; a storm rising out of the wind keeps its direction
	const bool bWindBefore = Weather == EVaelWeather::Wind || Weather == EVaelWeather::Storm;
	if (!bWindBefore || WindStrength <= 1.0f)
	{
		const float Angle = FMath::FRandRange(0.0f, UE_TWO_PI);
		WindDirection = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
	}

	Weather = NewWeather;
	WetnessRefreshTime = 0.0f;

	UE_LOG(LogVael, Log, TEXT("Weather in '%s': %s"), *RegionName.ToString(), *GetWeatherName(Weather).ToString());

	// Fires already burning die sooner once the rain starts and last longer in a drought
	if (Weather == EVaelWeather::Rain || Weather == EVaelWeather::Drought)
	{
		const float LifeSpanScale = Weather == EVaelWeather::Rain ? 0.625f : UVaelWorldSettings::Get()->DroughtFireLifetimeMultiplier;

		for (TActorIterator<AVaelGroundArea> It(GetWorld()); It; ++It)
		{
			const float LifeSpan = It->GetLifeSpan();
			if (It->GetElement() == EVaelElement::Fire && LifeSpan > 0.0f && Contains(It->GetActorLocation()))
			{
				It->SetLifeSpan(FMath::Max(LifeSpan * LifeSpanScale, 0.1f));
			}
		}
	}

	if (HasPlayerInside())
	{
		AnnounceWeather();
	}
}

void AVaelRegion::AnnounceWeather()
{
	// The first time a weather comes, it is explained at length; afterwards a short line is enough
	const UGameInstance* GameInstance = GetGameInstance();
	UVaelProgressSubsystem* Progress = GameInstance != nullptr ? GameInstance->GetSubsystem<UVaelProgressSubsystem>() : nullptr;
	const bool bFirstTime = Progress != nullptr && Progress->MarkWeatherIntroduced(Weather);
	const float Duration = bFirstTime ? UVaelWorldSettings::Get()->WeatherExplanationDuration : 4.2f;

	FText Title;
	FText Detail;
	FLinearColor Color(FColor(214, 236, 255));

	switch (Weather)
	{
	case EVaelWeather::Rain:
		Title = LOCTEXT("RainStarts", "Ascheregen setzt ein");
		Detail = bFirstTime
			? LOCTEXT("RainExplained", "Alle werden nass, auch ihr: Blitze treffen Nasse doppelt, und wer nass einen Blitz wirkt, bekommt selbst Schaden. Wasserzauber kosten weniger und treffen h\u00E4rter, Feuer erlischt schneller.")
			: LOCTEXT("RainShort", "Wasser st\u00E4rker, Blitze gef\u00E4hrlich, Feuer erlischt schneller.");
		Color = FLinearColor(FColor(158, 199, 232));
		break;

	case EVaelWeather::Storm:
		Title = LOCTEXT("StormStarts", "Ein Sturm zieht auf");
		Detail = bFirstTime
			? LOCTEXT("StormExplained", "Luft liegt \u00FCberall in der Umgebung und ist billiger und st\u00E4rker. Helle Kreise am Boden k\u00FCndigen Blitzeinschl\u00E4ge an: geht aus ihnen heraus.")
			: LOCTEXT("StormShort", "Luft \u00FCberall. Achtung, Blitzeinschl\u00E4ge!");
		break;

	case EVaelWeather::Drought:
		Title = LOCTEXT("DroughtStarts", "D\u00FCrre legt sich \u00FCber das Land");
		Detail = bFirstTime
			? LOCTEXT("DroughtExplained", "N\u00E4sse verdunstet sofort. Feuerzauber kosten weniger und treffen h\u00E4rter, Feuer brennt l\u00E4nger. Wasserzauber sind teurer und schw\u00E4cher.")
			: LOCTEXT("DroughtShort", "Feuer st\u00E4rker, Wasser schw\u00E4cher.");
		Color = FLinearColor(FColor(240, 176, 96));
		break;

	case EVaelWeather::Wind:
		Title = LOCTEXT("WindStarts", "Wind kommt auf");
		Detail = bFirstTime
			? LOCTEXT("WindExplained", "Luft liegt überall in der Umgebung und ist billiger und stärker. Geschosse werden abgetrieben, Feuer und Wirbel ziehen mit dem Wind: zielt gegen ihn etwas vor.")
			: LOCTEXT("WindShort", "Luft überall, Geschosse treiben ab.");
		Color = FLinearColor(FColor(196, 214, 220));
		break;

	default:
		Title = LOCTEXT("SkyClears", "Der Himmel klart auf");
		break;
	}

	UVaelNoticeSubsystem::Post(this, Title, Detail, Color, Duration);
}

FText AVaelRegion::GetWeatherName(EVaelWeather InWeather)
{
	switch (InWeather)
	{
	case EVaelWeather::Rain: return LOCTEXT("WeatherRain", "Ascheregen");
	case EVaelWeather::Storm: return LOCTEXT("WeatherStorm", "Sturm");
	case EVaelWeather::Drought: return LOCTEXT("WeatherDrought", "D\u00FCrre");
	case EVaelWeather::Wind: return LOCTEXT("WeatherWind", "Wind");
	default: return LOCTEXT("WeatherClear", "Klarer Himmel");
	}
}

float AVaelRegion::PickWeatherDuration() const
{
	const UVaelWorldSettings* Settings = UVaelWorldSettings::Get();
	return FMath::FRandRange(Settings->WeatherDuration.X, Settings->WeatherDuration.Y) * WeatherDurationScale;
}

EVaelWeather AVaelRegion::PickNextWeather() const
{
	// Among the weathers of the region, between the weights of a pure and a fully corrupted region; never the same twice in a row
	const UVaelWorldSettings* Settings = UVaelWorldSettings::Get();
	const float CorruptionShare = Corruption / 100.0f;

	TArray<TPair<EVaelWeather, float>> Options;
	float TotalWeight = 0.0f;

	for (const EVaelWeather Option : AvailableWeathers)
	{
		const float* PureWeight = Settings->WeatherWeightsPure.Find(Option);
		const float* CorruptedWeight = Settings->WeatherWeightsCorrupted.Find(Option);
		const float Weight = FMath::Max(FMath::Lerp(PureWeight != nullptr ? *PureWeight : 0.0f, CorruptedWeight != nullptr ? *CorruptedWeight : 0.0f, CorruptionShare), 0.0f);

		if (Option != Weather && Weight > 0.0f && !Options.ContainsByPredicate([Option](const TPair<EVaelWeather, float>& Entry) { return Entry.Key == Option; }))
		{
			Options.Emplace(Option, Weight);
			TotalWeight += Weight;
		}
	}

	float Pick = FMath::FRandRange(0.0f, TotalWeight);
	for (const TPair<EVaelWeather, float>& Option : Options)
	{
		Pick -= Option.Value;
		if (Pick <= 0.0f)
		{
			return Option.Key;
		}
	}

	return Options.IsEmpty() ? Weather : Options.Last().Key;
}

void AVaelRegion::TickRain(float DeltaSeconds)
{
	TickRainPuddles(DeltaSeconds);

	WetnessRefreshTime -= DeltaSeconds;
	if (WetnessRefreshTime > 0.0f)
	{
		return;
	}

	WetnessRefreshTime = WetnessRefreshInterval;

	// Players are as wet as the creatures
	const float WetDuration = UVaelWorldSettings::Get()->RainWetDuration;
	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		// Gear like the Sturmmantel keeps its wearer dry
		const bool bWarded = It->GetAbilitySystemComponent()->HasMatchingGameplayTag(VaelTags::Gear_WeatherWard);
		if (!bWarded && !It->IsDefeated() && (It->IsA<AVaelCharacter>() || It->IsA<AVaelCreature>()) && Contains(It->GetActorLocation()))
		{
			UVaelCombatStatics::ApplyStatus(nullptr, *It, EVaelStatus::Wet, WetDuration);
		}
	}
}

void AVaelRegion::TickRainPuddles(float DeltaSeconds)
{
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	RainPuddleTime -= DeltaSeconds;
	if (RainPuddleTime > 0.0f)
	{
		return;
	}

	RainPuddleTime = MagicSettings->RainPuddleInterval;

	UVaelElementSource* RainSource = VaelAssets::LoadOptional(MagicSettings->RainWaterSource);
	if (RainSource == nullptr)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		const AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
		if (Player != nullptr && Contains(Player->GetActorLocation()))
		{
			AVaelWaterPuddle::GatherRain(GetWorld(), Player->GetActorLocation(), MagicSettings->RainPuddleReach, MagicSettings->RainPuddleWater, RainSource);
		}
	}
}

void AVaelRegion::TickDrought(float DeltaSeconds)
{
	WetnessRefreshTime -= DeltaSeconds;
	if (WetnessRefreshTime > 0.0f)
	{
		return;
	}

	WetnessRefreshTime = WetnessRefreshInterval;

	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		if (Contains(It->GetActorLocation()))
		{
			UVaelCombatStatics::RemoveStatus(*It, EVaelStatus::Wet);
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

void AVaelRegion::RestoreCorruption(float InCorruption, float InScarredCorruption)
{
	ScarredCorruption = FMath::Clamp(InScarredCorruption, 0.0f, 100.0f);
	SetCorruption(InCorruption);
}
