// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "Floromant/VaelFloromantSettings.h"
#include "VaelTimeOfDay.generated.h"

class UVaelSaveGame;

/** Parts of a day */
UENUM(BlueprintType)
enum class EVaelDayPhase : uint8
{
	Dawn,
	Day,
	Dusk,
	Night
};

DECLARE_MULTICAST_DELEGATE_OneParam(FVaelOnDayPhaseChanged, EVaelDayPhase /*NewPhase*/);

/**
 *  The clock of the world: one day of 40 minutes with a dawn, a long day, a dusk and a short night (UVaelTimeSettings).
 *  Shared by the whole group and kept in the save game. The sky (AVaelSky), creatures, plants, the air and the Prophet
 *  ask it what time it is.
 */
UCLASS()
class UVaelTimeOfDay : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** The clock of the game instance of the object, null if there is none */
	static UVaelTimeOfDay* Get(const UObject* WorldContext);

	EVaelDayPhase GetPhase() const { return Phase; }

	/** Hour of the clock, 0 to 24: dawn 5 to 7, day 7 to 19, dusk 19 to 21, night 21 to 5 */
	float GetHour() const;

	/** 0 in daylight, 1 in the deep night, in between at dawn and dusk */
	float GetNightFactor() const;

	/** 1 in the middle of dawn and dusk, 0 in full day and full night */
	float GetTwilightFactor() const;

	/** How far the current phase has gone, 0 to 1 */
	float GetPhaseProgress() const;

	/** How easily the Prophet fills a store right now: light by day, shadow by night, twilight at dawn and dusk; 1 is normal */
	float GetPathGain(EVaelPath Path) const;

	/** Night factor of the clock of the world of the object; 0 where there is none */
	static float NightFactorOf(const UObject* WorldContext);

	/** Sets the clock to an hour */
	void SetHour(float Hour);

	/** Lets the night pass at once: the clock jumps to the coming dawn */
	void SkipToDawn();

	/** Speeds up the clock, for testing; 1 is normal */
	void SetSpeed(float InSpeed) { Speed = FMath::Max(InSpeed, 0.0f); }

	FVaelOnDayPhaseChanged OnPhaseChanged;

	//~Begin FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return !IsTemplate(); }
	virtual UWorld* GetTickableGameObjectWorld() const override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UVaelTimeOfDay, STATGROUP_Tickables); }
	//~End FTickableGameObject

	void WriteSave(UVaelSaveGame& Save) const;
	void ReadSave(const UVaelSaveGame& Save);

private:

	/** Seconds each phase lasts now: the first day of the campaign is longer */
	float GetPhaseSeconds(EVaelDayPhase InPhase) const;

	/** Seconds since dawn began, through the whole day */
	float GetSecondsInDay() const;

	/** Puts the clock at a point of the day given in seconds since dawn began */
	void SetSecondsInDay(float Seconds);

	/** Things that happen when a phase begins: the players hear of the first night, mist rises in the morning */
	void OnPhaseBegins(EVaelDayPhase NewPhase);

	/** The air cools around the players at night */
	void CoolAir(float DeltaTime);

	EVaelDayPhase Phase = EVaelDayPhase::Day;

	/** Seconds since the current phase began */
	float PhaseTime = 0.0f;

	float Speed = 1.0f;

	/** True once a whole day has passed in the campaign; the first day can be longer until then */
	bool bCycleIntroduced = false;

	float AirTime = 0.0f;
};
