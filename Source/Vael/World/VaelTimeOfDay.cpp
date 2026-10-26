// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelTimeOfDay.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Save/VaelSaveGame.h"
#include "UI/VaelNoticeSubsystem.h"
#include "Vael.h"
#include "World/VaelAtmosphere.h"
#include "World/VaelLocalWeather.h"
#include "World/VaelRegion.h"
#include "World/VaelSky.h"
#include "World/VaelTimeSettings.h"
#include "World/VaelWorldSettings.h"

#define LOCTEXT_NAMESPACE "VaelWorld"

namespace
{
	/** Hour each phase begins at and how many hours it spans */
	constexpr float PhaseStartHour[] = { 5.0f, 7.0f, 19.0f, 21.0f };
	constexpr float PhaseHours[] = { 2.0f, 12.0f, 2.0f, 8.0f };

	EVaelDayPhase NextPhase(EVaelDayPhase Phase)
	{
		return static_cast<EVaelDayPhase>((static_cast<uint8>(Phase) + 1) % 4);
	}
}

void UVaelTimeOfDay::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	SetHour(UVaelTimeSettings::Get()->StartHour);
}

UVaelTimeOfDay* UVaelTimeOfDay::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	return GameInstance != nullptr ? GameInstance->GetSubsystem<UVaelTimeOfDay>() : nullptr;
}

float UVaelTimeOfDay::NightFactorOf(const UObject* WorldContext)
{
	const UVaelTimeOfDay* Clock = Get(WorldContext);
	return Clock != nullptr ? Clock->GetNightFactor() : 0.0f;
}

UWorld* UVaelTimeOfDay::GetTickableGameObjectWorld() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance != nullptr ? GameInstance->GetWorld() : nullptr;
}

float UVaelTimeOfDay::GetPhaseSeconds(EVaelDayPhase InPhase) const
{
	const UVaelTimeSettings* Settings = UVaelTimeSettings::Get();
	switch (InPhase)
	{
	case EVaelDayPhase::Dawn: return Settings->DawnSeconds;
	case EVaelDayPhase::Dusk: return Settings->DuskSeconds;
	case EVaelDayPhase::Night: return Settings->NightSeconds;
	default:
		// The calm start of the campaign: a long first day before the night is introduced
		return Settings->DaySeconds + (Settings->bLongFirstDay && !bCycleIntroduced ? Settings->FirstDayExtraSeconds : 0.0f);
	}
}

float UVaelTimeOfDay::GetPhaseProgress() const
{
	return FMath::Clamp(PhaseTime / FMath::Max(GetPhaseSeconds(Phase), 1.0f), 0.0f, 1.0f);
}

float UVaelTimeOfDay::GetHour() const
{
	const int32 Index = static_cast<int32>(Phase);
	return FMath::Fmod(PhaseStartHour[Index] + PhaseHours[Index] * GetPhaseProgress(), 24.0f);
}

float UVaelTimeOfDay::GetNightFactor() const
{
	const float Progress = GetPhaseProgress();
	switch (Phase)
	{
	case EVaelDayPhase::Dawn: return 1.0f - FMath::SmoothStep(0.0f, 1.0f, Progress);
	case EVaelDayPhase::Dusk: return FMath::SmoothStep(0.0f, 1.0f, Progress);
	case EVaelDayPhase::Night: return 1.0f;
	default: return 0.0f;
	}
}

float UVaelTimeOfDay::GetTwilightFactor() const
{
	return Phase == EVaelDayPhase::Dawn || Phase == EVaelDayPhase::Dusk ? FMath::Sin(GetPhaseProgress() * UE_PI) : 0.0f;
}

float UVaelTimeOfDay::GetPathGain(EVaelPath Path) const
{
	// Light comes easily by day, shadow by night (Boris 2026-10-26); twilight is the third way, strongest at dawn and dusk
	const UVaelTimeSettings* Settings = UVaelTimeSettings::Get();
	const float Night = GetNightFactor();
	switch (Path)
	{
	case EVaelPath::Light: return FMath::Lerp(1.0f + Settings->PathTimeBonus, 1.0f - Settings->PathTimeBonus, Night);
	case EVaelPath::Shadow: return FMath::Lerp(1.0f - Settings->PathTimeBonus, 1.0f + Settings->PathTimeBonus, Night);
	case EVaelPath::Twilight: return 1.0f + Settings->TwilightBonus * GetTwilightFactor();
	default: return 1.0f;
	}
}

float UVaelTimeOfDay::GetSecondsInDay() const
{
	float Seconds = 0.0f;
	for (int32 Index = 0; Index < static_cast<int32>(Phase); ++Index)
	{
		Seconds += GetPhaseSeconds(static_cast<EVaelDayPhase>(Index));
	}
	return Seconds + PhaseTime;
}

void UVaelTimeOfDay::SetSecondsInDay(float Seconds)
{
	Phase = EVaelDayPhase::Dawn;
	PhaseTime = FMath::Max(Seconds, 0.0f);
	for (int32 Guard = 0; Guard < 8 && PhaseTime >= GetPhaseSeconds(Phase); ++Guard)
	{
		PhaseTime -= GetPhaseSeconds(Phase);
		Phase = NextPhase(Phase);
	}
}

void UVaelTimeOfDay::SetHour(float Hour)
{
	// From the hour to the phase it falls in and how far into it
	const float Wrapped = FMath::Fmod(FMath::Fmod(Hour, 24.0f) + 24.0f, 24.0f);
	const float SinceDawn = FMath::Fmod(Wrapped - PhaseStartHour[0] + 24.0f, 24.0f);
	float Start = 0.0f;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		if (SinceDawn < Start + PhaseHours[Index] || Index == 3)
		{
			const EVaelDayPhase OldPhase = Phase;
			Phase = static_cast<EVaelDayPhase>(Index);
			PhaseTime = (SinceDawn - Start) / PhaseHours[Index] * GetPhaseSeconds(Phase);
			if (OldPhase != Phase)
			{
				OnPhaseChanged.Broadcast(Phase);
			}
			return;
		}
		Start += PhaseHours[Index];
	}
}

void UVaelTimeOfDay::SkipToDawn()
{
	if (Phase != EVaelDayPhase::Dawn)
	{
		Phase = EVaelDayPhase::Dawn;
		PhaseTime = 0.0f;
		OnPhaseChanged.Broadcast(Phase);
		OnPhaseBegins(Phase);
	}
}

void UVaelTimeOfDay::Tick(float DeltaTime)
{
	UWorld* World = GetTickableGameObjectWorld();
	if (World == nullptr || !World->IsGameWorld() || !World->HasBegunPlay())
	{
		return;
	}

	// Every level gets its sky that follows the clock
	AVaelSky::EnsureSky(World);

	PhaseTime += DeltaTime * Speed;
	if (PhaseTime >= GetPhaseSeconds(Phase))
	{
		PhaseTime -= GetPhaseSeconds(Phase);
		Phase = NextPhase(Phase);
		UE_LOG(LogVael, Log, TEXT("Time of day: %s begins (%.1f h)"), *UEnum::GetValueAsString(Phase), GetHour());
		OnPhaseChanged.Broadcast(Phase);
		OnPhaseBegins(Phase);
	}

	CoolAir(DeltaTime);
}

void UVaelTimeOfDay::OnPhaseBegins(EVaelDayPhase NewPhase)
{
	UWorld* World = GetTickableGameObjectWorld();
	if (World == nullptr)
	{
		return;
	}

	// The first night is explained, afterwards a short line is enough
	const FLinearColor DuskColor(FColor(150, 160, 205));
	if (NewPhase == EVaelDayPhase::Dusk)
	{
		const bool bFirst = !bCycleIntroduced;
		UVaelNoticeSubsystem::Post(World, LOCTEXT("DuskTitle", "Die Dämmerung bricht herein"),
			bFirst ? LOCTEXT("DuskFirst", "Bald wird es Nacht. Glutkriecher und Puppen werden wach, Harpyien und Roller schlafen. Leuchtendes leuchtet heller, und der Schatten des Propheten wächst.")
				: LOCTEXT("DuskShort", "Bald wird es Nacht."), DuskColor, bFirst ? 9.0f : 4.0f);
	}
	else if (NewPhase == EVaelDayPhase::Night)
	{
		bCycleIntroduced = true;
	}
	else if (NewPhase == EVaelDayPhase::Dawn)
	{
		UVaelNoticeSubsystem::Post(World, LOCTEXT("DawnTitle", "Der Morgen graut"), LOCTEXT("DawnDetail", "Tau liegt auf den Kelchen."), FLinearColor(FColor(236, 206, 170)), 4.0f);

		// Morning mist rises where the night left the air cold and moist, never in a drought
		const APlayerController* PlayerController = World->GetFirstPlayerController();
		const APawn* Pawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
		const AVaelRegion* Region = Pawn != nullptr ? AVaelRegion::GetRegionAt(World, Pawn->GetActorLocation()) : nullptr;
		const bool bDry = Region != nullptr && Region->GetWeather() == EVaelWeather::Drought;
		if (Pawn != nullptr && !bDry && FMath::FRand() < UVaelTimeSettings::Get()->MorningMistChance)
		{
			const UVaelWorldSettings* WorldSettings = UVaelWorldSettings::Get();
			AVaelLocalWeather::Spawn(World, EVaelLocalWeather::ColdFog, Pawn->GetActorLocation(), WorldSettings->LocalWeatherRadius, GetPhaseSeconds(EVaelDayPhase::Dawn) + 60.0f, FVector::ForwardVector);
		}
	}
}

void UVaelTimeOfDay::CoolAir(float DeltaTime)
{
	AirTime += DeltaTime;
	if (AirTime < 0.5f)
	{
		return;
	}

	const float Step = AirTime;
	AirTime = 0.0f;
	const float Night = GetNightFactor();
	UWorld* World = GetTickableGameObjectWorld();
	if (Night <= 0.0f || World == nullptr)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APawn* Pawn = It->Get() != nullptr ? It->Get()->GetPawn() : nullptr;
		if (Pawn != nullptr)
		{
			UVaelAtmosphere::Add(World, Pawn->GetActorLocation(), EVaelAir::Cold, UVaelTimeSettings::Get()->NightColdPerSecond * Night * Step);
		}
	}
}

void UVaelTimeOfDay::WriteSave(UVaelSaveGame& Save) const
{
	Save.TimeOfDaySeconds = GetSecondsInDay();
	Save.bDayCycleIntroduced = bCycleIntroduced;
}

void UVaelTimeOfDay::ReadSave(const UVaelSaveGame& Save)
{
	bCycleIntroduced = Save.bDayCycleIntroduced;
	if (Save.TimeOfDaySeconds >= 0.0f)
	{
		SetSecondsInDay(Save.TimeOfDaySeconds);
	}
	else
	{
		SetHour(UVaelTimeSettings::Get()->StartHour);
	}
}

#undef LOCTEXT_NAMESPACE
