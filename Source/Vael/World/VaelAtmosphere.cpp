// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelAtmosphere.h"
#include "Creatures/VaelCreature.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Magic/VaelCloudCards.h"
#include "Magic/VaelGroundArea.h"
#include "Magic/VaelIceShards.h"
#include "Magic/VaelMagicSettings.h"
#include "Materials/MaterialInterface.h"
#include "Vael.h"
#include "VaelAssets.h"
#include "World/VaelFireComponent.h"
#include "World/VaelFireSpot.h"
#include "World/VaelGround.h"
#include "World/VaelLocalWeather.h"
#include "World/VaelRegion.h"
#include "World/VaelTimeOfDay.h"
#include "World/VaelWorldSettings.h"

namespace
{
	/** Seconds between two looks at the fires, steam and ice of the world, and at whether a weather breaks out */
	constexpr float AirStep = 0.5f;

	/** Cells showing signs at once, and cards per sign */
	constexpr int32 AirSignCells = 8;
	constexpr int32 AirSignCards = 3;

	/** A sign shows once the air holds this share of what its weather needs */
	constexpr float AirSignStart = 0.35f;

	/** Height of the gathering clouds over the ground, like the clouds of the Hagelsturm and the Regenruf */
	constexpr float AirCloudHeight = 650.0f;
}

UVaelAtmosphere* UVaelAtmosphere::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
	return World != nullptr ? World->GetSubsystem<UVaelAtmosphere>() : nullptr;
}

bool UVaelAtmosphere::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UVaelAtmosphere::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UVaelAtmosphere, STATGROUP_Tickables);
}

FIntPoint UVaelAtmosphere::CellOf(const FVector& Location) const
{
	const float Size = UVaelWorldSettings::Get()->AirCellSize;
	return FIntPoint(FMath::FloorToInt(Location.X / Size), FMath::FloorToInt(Location.Y / Size));
}

FVector UVaelAtmosphere::CenterOf(const FIntPoint& Cell) const
{
	const float Size = UVaelWorldSettings::Get()->AirCellSize;
	const float* Ground = CellGround.Find(Cell);
	return FVector((Cell.X + 0.5f) * Size, (Cell.Y + 0.5f) * Size, Ground != nullptr ? *Ground : 0.0f);
}

void UVaelAtmosphere::Add(const UObject* WorldContext, const FVector& Location, EVaelAir Kind, float Amount)
{
	UVaelAtmosphere* Air = Get(WorldContext);
	if (Air == nullptr || Amount <= 0.0f)
	{
		return;
	}

	// A drought heats the air more easily
	if (Kind == EVaelAir::Heat)
	{
		const AVaelRegion* Region = AVaelRegion::GetRegionAt(Air->GetWorld(), Location);
		Amount *= Region != nullptr && Region->GetWeather() == EVaelWeather::Drought ? 1.3f : 1.0f;
	}

	const FIntPoint Cell = Air->CellOf(Location);
	Air->Cells.FindOrAdd(Cell).Get(Kind) += Amount;
	if (!Air->CellGround.Contains(Cell))
	{
		Air->CellGround.Add(Cell, Location.Z);
	}
}

void UVaelAtmosphere::NotifySpellImpact(const UObject* WorldContext, const FVector& Location, float Radius, EVaelElement Element)
{
	// Bigger blasts give more, but a huge one not endlessly more
	const UVaelWorldSettings* Settings = UVaelWorldSettings::Get();
	const float Size = FMath::Clamp(Radius / 100.0f, 0.5f, 3.0f);
	if (Element == EVaelElement::Fire)
	{
		Add(WorldContext, Location, EVaelAir::Heat, Settings->FireImpactHeat * Size);
	}
	else if (Element == EVaelElement::Water)
	{
		Add(WorldContext, Location, EVaelAir::Moisture, Settings->WaterImpactMoisture * Size);
	}
}

FVaelAirCell UVaelAtmosphere::Sample(const FVector& Location) const
{
	FVaelAirCell Sum;
	const FIntPoint Middle = CellOf(Location);
	for (int32 X = -1; X <= 1; ++X)
	{
		for (int32 Y = -1; Y <= 1; ++Y)
		{
			if (const FVaelAirCell* Cell = Cells.Find(Middle + FIntPoint(X, Y)))
			{
				Sum.Heat += Cell->Heat;
				Sum.Moisture += Cell->Moisture;
				Sum.Cold += Cell->Cold;
			}
		}
	}
	return Sum;
}

void UVaelAtmosphere::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UWorld* World = GetWorld();
	if (World == nullptr || !World->HasBegunPlay())
	{
		return;
	}

	Events.RemoveAll([](const TObjectPtr<AVaelLocalWeather>& Event) { return Event == nullptr || Event->IsActorBeingDestroyed(); });

	StepTime += DeltaTime;
	if (StepTime >= AirStep)
	{
		const float Step = StepTime;
		StepTime = 0.0f;
		Gather(Step);
		Fade(Step);
		CheckEvents();
	}

	UpdateSigns();
}

void UVaelAtmosphere::Gather(float Step)
{
	const UVaelWorldSettings* Settings = UVaelWorldSettings::Get();
	UWorld* World = GetWorld();

	// Brandherde heat the air by their size; campfires only a little, they burn there for ever
	for (TActorIterator<AVaelFireSpot> It(World); It; ++It)
	{
		if (It->CanGiveFire())
		{
			const float Share = It->BurnTime > 0.0f ? 1.0f : Settings->CampfireHeatShare;
			Add(World, It->GetActorLocation(), EVaelAir::Heat, Settings->FireHeatPerMeter * It->GetFire()->GetShape().Radius / 100.0f * Share * Step);
		}
	}

	// Steam and mist rise as moisture; areas lying for ever (ponds of the level) don't
	for (TActorIterator<AVaelGroundArea> It(World); It; ++It)
	{
		const EVaelGroundEffect Effect = It->GetEffect();
		if ((Effect == EVaelGroundEffect::Blind || Effect == EVaelGroundEffect::Veil) && It->GetLifeSpan() > 0.0f)
		{
			Add(World, It->GetActorLocation(), EVaelAir::Moisture, Settings->SteamMoisture * Step);
		}
	}

	// Broken ice cools the air while it lies
	for (TActorIterator<AVaelIceShards> It(World); It; ++It)
	{
		Add(World, It->GetActorLocation(), EVaelAir::Cold, Settings->IceCold * Step);
	}

	// Hot creatures like the Glutkriecher and the Glutkönigin heat the air around them, the queen far more at night
	const float NightFactor = UVaelTimeOfDay::NightFactorOf(World);
	for (TActorIterator<AVaelCreature> It(World); It; ++It)
	{
		if (It->AirHeatPerSecond > 0.0f && !It->IsDefeated())
		{
			Add(World, It->GetActorLocation(), EVaelAir::Heat, It->AirHeatPerSecond * FMath::Lerp(1.0f, It->NightAirHeatScale, NightFactor) * Step);
		}
	}
}

void UVaelAtmosphere::Fade(float Step)
{
	const UVaelWorldSettings* Settings = UVaelWorldSettings::Get();
	for (auto It = Cells.CreateIterator(); It; ++It)
	{
		// Rain keeps the air moist, a drought dries it, wind blows everything away faster
		const AVaelRegion* Region = AVaelRegion::GetRegionAt(GetWorld(), CenterOf(It.Key()));
		const EVaelWeather Weather = Region != nullptr ? Region->GetWeather() : EVaelWeather::Clear;
		const float WindFactor = Weather == EVaelWeather::Wind || Weather == EVaelWeather::Storm ? 1.5f : 1.0f;
		const float MoistureFactor = Weather == EVaelWeather::Rain ? 0.5f : Weather == EVaelWeather::Drought ? 2.0f : 1.0f;

		FVaelAirCell& Cell = It.Value();
		Cell.Heat *= FMath::Max(1.0f - Settings->AirFadePerSecond * WindFactor * Step, 0.0f);
		Cell.Cold *= FMath::Max(1.0f - Settings->AirFadePerSecond * WindFactor * Step, 0.0f);
		Cell.Moisture *= FMath::Max(1.0f - Settings->AirFadePerSecond * WindFactor * MoistureFactor * Step, 0.0f);
		if (Cell.IsEmpty())
		{
			CellGround.Remove(It.Key());
			It.RemoveCurrent();
		}
	}
}

void UVaelAtmosphere::CheckEvents()
{
	const UVaelWorldSettings* Settings = UVaelWorldSettings::Get();
	if (Events.Num() >= Settings->MaxLocalWeathers)
	{
		return;
	}

	TArray<FIntPoint> Keys;
	Cells.GetKeys(Keys);
	for (const FIntPoint& Key : Keys)
	{
		const FVector Center = CenterOf(Key);
		const FVaelAirCell Air = Sample(Center);

		// The strongest weather the air holds, thunder before rain
		TOptional<EVaelLocalWeather> Kind;
		if (Air.Moisture >= Settings->ThunderstormMoisture && Air.Heat >= Settings->ThunderstormHeat)
		{
			Kind = EVaelLocalWeather::Thunderstorm;
		}
		else if (Air.Cold >= Settings->GraupelCold && Air.Moisture >= Settings->GraupelMoisture)
		{
			Kind = EVaelLocalWeather::Graupel;
		}
		else if (Air.Moisture >= Settings->ShowerMoisture)
		{
			Kind = EVaelLocalWeather::Shower;
		}
		else if (Air.Cold >= Settings->ColdFogCold && Air.Moisture >= Settings->ColdFogMoisture)
		{
			Kind = EVaelLocalWeather::ColdFog;
		}
		else if (Air.Heat >= Settings->WindFrontHeat && Air.Cold >= Settings->WindFrontCold)
		{
			Kind = EVaelLocalWeather::WindFront;
		}
		else if (Air.Heat >= Settings->ThermalHeat && Air.Moisture <= Settings->ThermalMaxMoisture)
		{
			Kind = EVaelLocalWeather::Thermal;
		}

		if (!Kind.IsSet())
		{
			continue;
		}

		// Never two weathers on top of each other
		const bool bTaken = Events.ContainsByPredicate([&Center](const AVaelLocalWeather* Event) { return FVector::Dist2D(Event->GetActorLocation(), Center) < Event->GetRadius(); });
		if (bTaken)
		{
			continue;
		}

		// A wind front blows from where the cold lies towards the heat
		FVector HeatPlace = FVector::ZeroVector;
		FVector ColdPlace = FVector::ZeroVector;
		float HeatSum = 0.0f;
		float ColdSum = 0.0f;
		for (int32 X = -1; X <= 1; ++X)
		{
			for (int32 Y = -1; Y <= 1; ++Y)
			{
				const FIntPoint Near = Key + FIntPoint(X, Y);
				if (FVaelAirCell* Cell = Cells.Find(Near))
				{
					HeatPlace += CenterOf(Near) * Cell->Heat;
					ColdPlace += CenterOf(Near) * Cell->Cold;
					HeatSum += Cell->Heat;
					ColdSum += Cell->Cold;

					// The weather uses up most of what had gathered
					Cell->Heat *= Settings->AirLeftAfterWeather;
					Cell->Moisture *= Settings->AirLeftAfterWeather;
					Cell->Cold *= Settings->AirLeftAfterWeather;
				}
			}
		}
		FVector Direction = HeatSum > 0.0f && ColdSum > 0.0f ? (HeatPlace / HeatSum - ColdPlace / ColdSum).GetSafeNormal2D() : FVector::ZeroVector;
		if (Direction.IsNearlyZero())
		{
			const float Angle = FMath::FRandRange(0.0f, UE_TWO_PI);
			Direction = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
		}

		if (AVaelLocalWeather* Event = AVaelLocalWeather::Spawn(GetWorld(), Kind.GetValue(), Center, Settings->LocalWeatherRadius, Settings->LocalWeatherLifetime, Direction))
		{
			Events.Add(Event);
			UE_LOG(LogVael, Log, TEXT("The air breaks out into %s at %s (heat %.0f, moisture %.0f, cold %.0f)"), *UEnum::GetValueAsString(Kind.GetValue()), *Center.ToCompactString(), Air.Heat, Air.Moisture, Air.Cold);
		}

		if (Events.Num() >= Settings->MaxLocalWeathers)
		{
			return;
		}
	}
}

void UVaelAtmosphere::UpdateSigns()
{
	UWorld* World = GetWorld();
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	const UVaelWorldSettings* Settings = UVaelWorldSettings::Get();

	if (SignActor == nullptr)
	{
		if (Cells.IsEmpty())
		{
			return;
		}

		FActorSpawnParameters Parameters;
		Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SignActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Parameters);
		if (SignActor == nullptr)
		{
			return;
		}
		USceneComponent* Root = NewObject<USceneComponent>(SignActor);
		SignActor->SetRootComponent(Root);
		Root->RegisterComponent();

		const auto MakeCards = [this, Root](const TSoftObjectPtr<UMaterialInterface>& Material) -> UVaelCloudCardsComponent*
		{
			UMaterialInterface* Loaded = VaelAssets::LoadOptional(Material);
			if (Loaded == nullptr)
			{
				return nullptr;
			}
			UVaelCloudCardsComponent* Cards = NewObject<UVaelCloudCardsComponent>(SignActor);
			Cards->SetupAttachment(Root);
			Cards->RegisterComponent();
			Cards->Init(Loaded);
			for (int32 Index = 0; Index < AirSignCells * AirSignCards; ++Index)
			{
				Cards->AddCard(FVector::ZeroVector, 100.0f, 0.0f);
			}
			return Cards;
		};
		CloudSigns = MakeCards(MagicSettings->StormCloudMaterial);
		HeatSigns = MakeCards(MagicSettings->AirCardMaterial);
		ColdSigns = MakeCards(MagicSettings->CloudCardMaterial);
	}

	// How close each cell is to its weathers: moist air gathers clouds, hot air shimmers, cold air lies as a thin mist
	struct FSign
	{
		FIntPoint Cell;
		float Moist = 0.0f;
		float Hot = 0.0f;
		float Cool = 0.0f;
	};
	TArray<FSign> Signs;
	for (const TPair<FIntPoint, FVaelAirCell>& Pair : Cells)
	{
		const FVaelAirCell Air = Sample(CenterOf(Pair.Key));
		FSign Sign;
		Sign.Cell = Pair.Key;
		Sign.Moist = Air.Moisture / FMath::Min(Settings->ShowerMoisture, Settings->ThunderstormMoisture);
		Sign.Hot = Air.Heat / FMath::Min(Settings->ThermalHeat, FMath::Min(Settings->ThunderstormHeat, Settings->WindFrontHeat));
		Sign.Cool = Air.Cold / FMath::Min(Settings->ColdFogCold, Settings->WindFrontCold);
		if (FMath::Max3(Sign.Moist, Sign.Hot, Sign.Cool) >= AirSignStart)
		{
			Signs.Add(Sign);
		}
	}
	Signs.Sort([](const FSign& A, const FSign& B) { return FMath::Max3(A.Moist, A.Hot, A.Cool) > FMath::Max3(B.Moist, B.Hot, B.Cool); });

	const float Time = World->GetTimeSeconds();
	const auto Show = [](float Progress) { return FMath::Clamp((Progress - AirSignStart) / (1.0f - AirSignStart), 0.0f, 1.0f); };
	for (int32 SignIndex = 0; SignIndex < AirSignCells; ++SignIndex)
	{
		const bool bUsed = Signs.IsValidIndex(SignIndex);
		const FVector Center = bUsed ? CenterOf(Signs[SignIndex].Cell) : FVector::ZeroVector;
		for (int32 Part = 0; Part < AirSignCards; ++Part)
		{
			const int32 Card = SignIndex * AirSignCards + Part;
			const float Angle = Part * UE_TWO_PI / AirSignCards + SignIndex * 1.3f;
			const FVector Spread(FMath::Cos(Angle) * 260.0f, FMath::Sin(Angle) * 260.0f, 0.0f);
			const float Churn = FMath::Sin(Time * 0.6f + Card) * 30.0f;
			if (CloudSigns != nullptr)
			{
				const float Moist = bUsed ? Show(Signs[SignIndex].Moist) : 0.0f;
				CloudSigns->SetCard(Card, Center + Spread * (0.6f + 0.4f * Moist) + FVector(Churn, -Churn, AirCloudHeight + Part * 40.0f), 250.0f + 350.0f * Moist, 0.7f * Moist);
			}
			if (HeatSigns != nullptr)
			{
				// Shimmer rises off the hot ground, over and over
				const float Hot = bUsed ? Show(Signs[SignIndex].Hot) : 0.0f;
				const float Rise = FMath::Frac(Time * 0.35f + Part / static_cast<float>(AirSignCards));
				HeatSigns->SetCard(Card, Center + Spread * 0.5f + FVector(0.0f, 0.0f, 80.0f + Rise * 320.0f), 220.0f + 120.0f * Rise, 0.6f * Hot * FMath::Sin(Rise * UE_PI));
			}
			if (ColdSigns != nullptr)
			{
				const float Cool = bUsed ? Show(Signs[SignIndex].Cool) : 0.0f;
				ColdSigns->SetCard(Card, Center + Spread + FVector(Churn, Churn, 40.0f), 500.0f, 0.25f * Cool);
			}
		}
	}

	for (UVaelCloudCardsComponent* Cards : { CloudSigns.Get(), HeatSigns.Get(), ColdSigns.Get() })
	{
		if (Cards != nullptr)
		{
			Cards->Commit();
		}
	}
}
