// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelLocalWeather.h"
#include "AbilitySystemComponent.h"
#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelCombatStatics.h"
#include "Creatures/VaelCreature.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Magic/VaelElementSource.h"
#include "Magic/VaelGameplayTags.h"
#include "Magic/VaelGroundArea.h"
#include "Magic/VaelHailStorm.h"
#include "Magic/VaelIceShards.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelWaterPuddle.h"
#include "Player/VaelCharacter.h"
#include "UI/VaelNoticeSubsystem.h"
#include "Vael.h"
#include "VaelAssets.h"
#include "World/VaelFireSpot.h"
#include "World/VaelGround.h"
#include "World/VaelLightningStrike.h"
#include "World/VaelRegion.h"
#include "World/VaelWeatherRain.h"
#include "World/VaelWorldSettings.h"

#define LOCTEXT_NAMESPACE "VaelWorld"

namespace
{
	/** Seconds a weather needs to rise and to die down */
	constexpr float LocalRiseTime = 4.0f;

	/** Seconds between two rounds of wetting and rain puddles */
	constexpr float LocalRainStep = 0.5f;

	/** Fog banks of a cold fog, and how wide each is */
	constexpr int32 LocalFogBanks = 5;
	constexpr float LocalFogRadius = 900.0f;

	/** How far around a player graupel falls, and how wide one lance hits */
	constexpr float LocalGraupelReach = 650.0f;
	constexpr float LocalGraupelHitRadius = 80.0f;
}

AVaelLocalWeather::AVaelLocalWeather()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AVaelLocalWeather* AVaelLocalWeather::Spawn(UWorld* World, EVaelLocalWeather InKind, const FVector& Center, float InRadius, float InLifetime, const FVector& InDirection)
{
	if (World == nullptr)
	{
		return nullptr;
	}

	// It lies on the ground under the air that made it
	FVector Ground = Center;
	FHitResult GroundHit;
	if (VaelGround::TraceGround(World, Center + FVector(0.0f, 0.0f, 1500.0f), Center - FVector(0.0f, 0.0f, 3000.0f), GroundHit))
	{
		Ground = GroundHit.Location;
	}

	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVaelLocalWeather* Weather = World->SpawnActor<AVaelLocalWeather>(AVaelLocalWeather::StaticClass(), FTransform(Ground), Parameters);
	if (Weather == nullptr)
	{
		return nullptr;
	}

	Weather->Kind = InKind;
	Weather->Radius = InRadius;
	Weather->Lifetime = InLifetime;
	Weather->Direction = InDirection.GetSafeNormal2D();
	Weather->LightningTime = FMath::FRandRange(2.0f, 4.0f);

	if (InKind == EVaelLocalWeather::Shower || InKind == EVaelLocalWeather::Thunderstorm)
	{
		Weather->RainLook = AVaelWeatherRain::SpawnPatch(World, Ground, InRadius, InLifetime, InKind == EVaelLocalWeather::Thunderstorm);
	}
	else if (InKind == EVaelLocalWeather::ColdFog)
	{
		Weather->LayFog();
	}

	// Players close by are told what is brewing
	bool bPlayerNear = false;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APawn* Pawn = It->Get() != nullptr ? It->Get()->GetPawn() : nullptr;
		bPlayerNear |= Pawn != nullptr && FVector::Dist2D(Pawn->GetActorLocation(), Ground) < InRadius * 1.5f;
	}
	if (bPlayerNear)
	{
		FText Detail;
		switch (InKind)
		{
		case EVaelLocalWeather::Shower: Detail = LOCTEXT("ShowerDetail", "Der aufsteigende Dampf regnet ab: nass, Feuer erlischt, Wasser überall."); break;
		case EVaelLocalWeather::Thunderstorm: Detail = LOCTEXT("ThunderDetail", "Hitze und Dampf türmen sich auf. Regen, Böen und Blitze!"); break;
		case EVaelLocalWeather::Thermal: Detail = LOCTEXT("ThermalDetail", "Die Hitze steigt, Wind zieht von allen Seiten herein: Luft überall."); break;
		case EVaelLocalWeather::WindFront: Detail = LOCTEXT("FrontDetail", "Kalte Luft stürzt zur Hitze und facht die Feuer an."); break;
		case EVaelLocalWeather::ColdFog: Detail = LOCTEXT("FogDetail", "Kälte und Nässe legen sich als Nebel auf den Boden."); break;
		case EVaelLocalWeather::Graupel: Detail = LOCTEXT("GraupelDetail", "Die feuchte Luft gefriert: Graupel prasselt herab."); break;
		}
		UVaelNoticeSubsystem::Post(World, GetDisplayName(InKind), Detail, FLinearColor(FColor(196, 214, 230)), 4.5f);
	}

	return Weather;
}

FText AVaelLocalWeather::GetDisplayName(EVaelLocalWeather InKind)
{
	switch (InKind)
	{
	case EVaelLocalWeather::Shower: return LOCTEXT("Shower", "Ein Schauer zieht auf");
	case EVaelLocalWeather::Thunderstorm: return LOCTEXT("Thunderstorm", "Eine Gewitterzelle bricht los");
	case EVaelLocalWeather::Thermal: return LOCTEXT("Thermal", "Aufwind");
	case EVaelLocalWeather::WindFront: return LOCTEXT("WindFront", "Eine Windfront bricht herein");
	case EVaelLocalWeather::ColdFog: return LOCTEXT("ColdFog", "Kältenebel");
	default: return LOCTEXT("Graupel", "Graupelschauer");
	}
}

bool AVaelLocalWeather::Covers(const FVector& Location) const
{
	return FVector::DistSquared2D(Location, GetActorLocation()) <= FMath::Square(Radius);
}

float AVaelLocalWeather::GetStrength() const
{
	return FMath::Min(FMath::Clamp(Age / LocalRiseTime, 0.0f, 1.0f), FMath::Clamp((Lifetime - Age) / LocalRiseTime, 0.0f, 1.0f));
}

FVector AVaelLocalWeather::GetOwnWind(const FVector& Location) const
{
	const UVaelWorldSettings* Settings = UVaelWorldSettings::Get();
	const FVector ToMiddle = (GetActorLocation() - Location) * FVector(1.0f, 1.0f, 0.0f);
	const float Distance = ToMiddle.Size();
	if (Distance > Radius)
	{
		return FVector::ZeroVector;
	}

	// Calm in the very middle and fading out at the edge
	const float Edge = FMath::Clamp((Radius - Distance) / (Radius * 0.25f), 0.0f, 1.0f);
	const float Gust = 0.7f + 0.5f * FMath::PerlinNoise1D(GetWorld()->GetTimeSeconds() * 0.4f + GetActorLocation().X * 0.001f);
	const float Strength = GetStrength() * Edge;

	switch (Kind)
	{
	case EVaelLocalWeather::Thermal:
	{
		// The rising air pulls in from all sides and turns a little on its way in
		const FVector In = ToMiddle.GetSafeNormal();
		const FVector Around(-In.Y, In.X, 0.0f);
		const float Middle = FMath::Clamp(Distance / (Radius * 0.2f), 0.0f, 1.0f);
		return (In + Around * 0.3f).GetSafeNormal() * Settings->ThermalWind * Strength * Middle * Gust;
	}
	case EVaelLocalWeather::WindFront:
		return Direction * Settings->WindFrontWind * Strength * Gust;
	case EVaelLocalWeather::Thunderstorm:
		return Direction * Settings->ThunderstormWind * Strength * Gust;
	default:
		return FVector::ZeroVector;
	}
}

FVector AVaelLocalWeather::GetWindAt(const UWorld* World, const FVector& Location)
{
	FVector Wind = FVector::ZeroVector;
	for (TActorIterator<AVaelLocalWeather> It(World); World != nullptr && It; ++It)
	{
		Wind += It->GetOwnWind(Location);
	}
	return Wind;
}

bool AVaelLocalWeather::IsRainingAt(const UWorld* World, const FVector& Location)
{
	for (TActorIterator<AVaelLocalWeather> It(World); World != nullptr && It; ++It)
	{
		if ((It->Kind == EVaelLocalWeather::Shower || It->Kind == EVaelLocalWeather::Thunderstorm) && It->GetStrength() > 0.2f && It->Covers(Location))
		{
			return true;
		}
	}
	return false;
}

void AVaelLocalWeather::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;
	if (Age >= Lifetime)
	{
		Destroy();
		return;
	}

	switch (Kind)
	{
	case EVaelLocalWeather::Shower:
		TickRain(DeltaSeconds);
		break;
	case EVaelLocalWeather::Thunderstorm:
		TickRain(DeltaSeconds);
		TickLightning(DeltaSeconds);
		break;
	case EVaelLocalWeather::WindFront:
		TickFans(DeltaSeconds);
		break;
	case EVaelLocalWeather::Graupel:
		TickGraupel(DeltaSeconds);
		break;
	default:
		break;
	}
}

void AVaelLocalWeather::TickRain(float DeltaSeconds)
{
	RainStepTime -= DeltaSeconds;
	if (RainStepTime > 0.0f || GetStrength() < 0.2f)
	{
		return;
	}
	RainStepTime = LocalRainStep;

	UWorld* World = GetWorld();
	const UVaelWorldSettings* Settings = UVaelWorldSettings::Get();
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();

	// Everybody inside gets wet, as in the ash rain; the Sturmmantel keeps its wearer dry
	for (TActorIterator<AVaelCharacterBase> It(World); It; ++It)
	{
		const bool bWarded = It->GetAbilitySystemComponent()->HasMatchingGameplayTag(VaelTags::Gear_WeatherWard);
		if (!bWarded && !It->IsDefeated() && (It->IsA<AVaelCharacter>() || It->IsA<AVaelCreature>()) && Covers(It->GetActorLocation()))
		{
			UVaelCombatStatics::ApplyStatus(nullptr, *It, EVaelStatus::Wet, Settings->RainWetDuration);
		}
	}

	// Rain water gathers into puddles around the players inside
	UVaelElementSource* RainSource = VaelAssets::LoadOptional(MagicSettings->RainWaterSource);
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It && RainSource != nullptr; ++It)
	{
		const APawn* Pawn = It->Get() != nullptr ? It->Get()->GetPawn() : nullptr;
		if (Pawn != nullptr && Covers(Pawn->GetActorLocation()) && FMath::FRand() < LocalRainStep / FMath::Max(MagicSettings->RainPuddleInterval, 0.1f))
		{
			AVaelWaterPuddle::GatherRain(World, Pawn->GetActorLocation(), MagicSettings->RainPuddleReach * 0.5f, MagicSettings->RainPuddleWater, RainSource);
		}
	}

	// Fires die: burning ground goes out patch by patch, Brandherde now and then
	const float Angle = FMath::FRandRange(0.0f, UE_TWO_PI);
	const float Out = Radius * FMath::Sqrt(FMath::FRand());
	AVaelGroundArea::ExtinguishFires(World, GetActorLocation() + FVector(FMath::Cos(Angle) * Out, FMath::Sin(Angle) * Out, 0.0f), 500.0f);
	for (TActorIterator<AVaelFireSpot> It(World); It; ++It)
	{
		if (It->BurnTime > 0.0f && It->CanGiveFire() && Covers(It->GetActorLocation()) && FMath::FRand() < 0.08f)
		{
			It->PutOut(1.5f);
		}
	}
}

void AVaelLocalWeather::TickLightning(float DeltaSeconds)
{
	LightningTime -= DeltaSeconds;
	if (LightningTime > 0.0f || GetStrength() < 0.5f)
	{
		return;
	}
	const UVaelWorldSettings* Settings = UVaelWorldSettings::Get();
	LightningTime = FMath::FRandRange(Settings->LightningInterval.X, Settings->LightningInterval.Y);

	// Near a random player inside, as in a storm
	TArray<const APawn*> Players;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APawn* Pawn = It->Get() != nullptr ? It->Get()->GetPawn() : nullptr;
		if (Pawn != nullptr && Covers(Pawn->GetActorLocation()))
		{
			Players.Add(Pawn);
		}
	}
	if (Players.IsEmpty())
	{
		return;
	}

	const APawn* Target = Players[FMath::RandRange(0, Players.Num() - 1)];
	const float Angle = FMath::FRandRange(0.0f, UE_TWO_PI);
	const float Distance = FMath::FRandRange(Settings->LightningDistance.X, Settings->LightningDistance.Y);
	FVector Ground;
	if (AVaelCreature::FindGround(GetWorld(), Target->GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Distance, Ground))
	{
		AVaelLightningStrike::SpawnStrike(AVaelRegion::GetRegionAt(GetWorld(), Ground), Ground);
	}
}

void AVaelLocalWeather::TickFans(float DeltaSeconds)
{
	FanTime -= DeltaSeconds;
	if (FanTime > 0.0f || GetStrength() < 0.3f)
	{
		return;
	}
	FanTime = 1.0f;

	// The cold air rushing in fans the fires: they grow and creep downwind
	for (TActorIterator<AVaelFireSpot> It(GetWorld()); It; ++It)
	{
		if (It->BurnTime > 0.0f && It->CanGiveFire() && Covers(It->GetActorLocation()))
		{
			It->Feed(It->GetActorLocation() + Direction * 80.0f, 35.0f * GetStrength());
		}
	}
}

void AVaelLocalWeather::TickGraupel(float DeltaSeconds)
{
	const UVaelWorldSettings* Settings = UVaelWorldSettings::Get();
	UWorld* World = GetWorld();

	// Lances that land now hit whoever stands there and leave a little ice, which cools the air again and melts into water
	for (int32 Index = GraupelPoints.Num() - 1; Index >= 0; --Index)
	{
		if (Age < GraupelTimes[Index])
		{
			continue;
		}
		const FVector Point = GraupelPoints[Index];
		FVaelSpellHit Hit;
		Hit.Damage = Settings->GraupelDamage;
		Hit.Element = EVaelElement::Water;
		for (TActorIterator<AVaelCharacterBase> It(World); It; ++It)
		{
			if (!It->IsDefeated() && FVector::Dist2D(It->GetActorLocation(), Point) <= LocalGraupelHitRadius + It->GetSimpleCollisionRadius())
			{
				UVaelCombatStatics::ApplyNatureHit(*It, Hit, FVector::ZeroVector);
			}
		}
		if (FMath::FRand() < 0.35f)
		{
			AVaelIceShards::Spawn(World, Point, 50.0f, nullptr);
		}
		GraupelPoints.RemoveAtSwap(Index);
		GraupelTimes.RemoveAtSwap(Index);
	}

	// A volley a second around every player inside, out of the dark cloud of the Hagelsturm
	GraupelTime -= DeltaSeconds;
	if (GraupelTime > 0.0f || GetStrength() < 0.3f)
	{
		return;
	}
	GraupelTime = 1.0f;

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APawn* Pawn = It->Get() != nullptr ? It->Get()->GetPawn() : nullptr;
		if (Pawn == nullptr || !Covers(Pawn->GetActorLocation()))
		{
			continue;
		}

		TArray<FVector> Points;
		TArray<float> Delays;
		const int32 Count = FMath::Max(1, FMath::RoundToInt(Settings->GraupelPerSecond * GetStrength()));
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Angle = FMath::FRandRange(0.0f, UE_TWO_PI);
			const float Out = LocalGraupelReach * FMath::Sqrt(FMath::FRand());
			FVector Ground;
			if (AVaelCreature::FindGround(World, Pawn->GetActorLocation() + FVector(FMath::Cos(Angle) * Out, FMath::Sin(Angle) * Out, 0.0f), Ground))
			{
				const float Delay = FMath::FRandRange(0.0f, 0.9f);
				Points.Add(Ground);
				Delays.Add(Delay);
				GraupelPoints.Add(Ground);
				GraupelTimes.Add(Age + Delay);
			}
		}
		if (!Points.IsEmpty())
		{
			AVaelHailStorm::Spawn(this, Pawn->GetActorLocation() - FVector(0.0f, 0.0f, Pawn->GetSimpleCollisionHalfHeight()), LocalGraupelReach, Points, Delays);
		}
	}
}

void AVaelLocalWeather::LayFog()
{
	// Banks of mist lie on the ground; laid by nobody, they hide everyone from those further away
	const FVector Center = GetActorLocation();
	for (int32 Bank = 0; Bank < LocalFogBanks; ++Bank)
	{
		const float Angle = Bank * UE_TWO_PI / (LocalFogBanks - 1) + FMath::FRandRange(-0.3f, 0.3f);
		const float Out = Bank == 0 ? 0.0f : Radius * FMath::FRandRange(0.25f, 0.5f);
		FVector Place = Center + FVector(FMath::Cos(Angle) * Out, FMath::Sin(Angle) * Out, 0.0f);
		FHitResult GroundHit;
		if (VaelGround::TraceGround(GetWorld(), Place + FVector(0.0f, 0.0f, 600.0f), Place - FVector(0.0f, 0.0f, 1500.0f), GroundHit))
		{
			Place = GroundHit.Location;
		}
		AVaelGroundArea::SpawnArea(GetWorld(), Place + FVector(0.0f, 0.0f, 2.0f), EVaelElement::Water, LocalFogRadius, Lifetime, 0.0f, nullptr, false, EVaelGroundEffect::Veil);
	}
}

void AVaelLocalWeather::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (RainLook.IsValid())
	{
		RainLook->StopPatch();
	}
	Super::EndPlay(EndPlayReason);
}

#undef LOCTEXT_NAMESPACE
