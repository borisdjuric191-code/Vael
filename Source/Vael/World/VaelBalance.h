// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "VaelBalance.generated.h"

class AActor;
class AVaelCharacter;
class UVaelSaveGame;

/** The three zones of the balance (Lore-Bibel, Die drei Wege, Die Waage) */
UENUM(BlueprintType)
enum class EVaelBalanceZone : uint8
{
	Light		UMETA(DisplayName = "Licht"),
	Twilight	UMETA(DisplayName = "Zwielicht"),
	Shadow		UMETA(DisplayName = "Schatten")
};

/**
 *  Die Waage: corruption as one rule for players and regions (Boris, 7 November 2026). Every change of a player's or a
 *  region's corruption goes through here, so it is clamped the same way everywhere and always shows: a number over the
 *  player ("+3 Verderbnis · Nid-Zauber"), a pulse on the balance in the HUD, a log line.
 *  Zones: light below 30, twilight 30 to 70 (the twilight path keeps 40 to 60), shadow above 70.
 *  It also holds what the beds of a refugium do to its region while that region's level is not loaded: the change waits
 *  here (and in the save) until the region is there again.
 */
UCLASS()
class UVaelBalanceSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:

	static constexpr float LightBelow = 30.0f;
	static constexpr float ShadowAbove = 70.0f;
	static constexpr float TwilightLow = 40.0f;
	static constexpr float TwilightHigh = 60.0f;

	static UVaelBalanceSubsystem* Get(const UObject* WorldContext);

	static EVaelBalanceZone GetZone(float Corruption);
	static FText GetZoneName(EVaelBalanceZone Zone);
	static FLinearColor GetZoneColor(EVaelBalanceZone Zone);

	/** Corruption of a player, 0 to 100 */
	static float GetPlayer(const AActor* Player);

	/** Changes a player's corruption by Delta and shows it, with the reason ("Nid-Zauber", "Reinigungstrank"). Returns the new value. */
	static float ChangePlayer(AActor* Player, float Delta, const FText& Reason);

	/** Rest purifies (Die Waage): with a Klartrunk or a Lichttropfen in the bag one is used up and the player's corruption
	 *  sinks by 1 per hour of rest. Returns how much it sank (0 without one, with a hint). */
	static float PurifyByRest(AVaelCharacter* Player, float Hours);

	/** Albrun purifies a Nidkristall: it becomes a Geläuterter Kristall and takes 5 of the player's corruption. Returns false without one. */
	static bool PurifyCrystal(AVaelCharacter* Player);

	/** The whole group rests: the picture fades, the clock moves on by Hours (or to the morning), everyone heals and purifies */
	static void RestGroup(UWorld* World, float Hours, bool bToMorning);

	/** Changes the corruption of the region at a spot, if one is loaded there */
	static void ChangeRegionAt(const UObject* WorldContext, const FVector& Location, float Delta);

	/** Changes a region by its name ("Aschenmark"): at once if it is loaded, otherwise the change waits until it is */
	void ChangeRegion(const FString& RegionName, float Delta);

	/** What waits for a region, without taking it */
	float PeekPending(const FString& RegionName) const { const float* Pending = PendingRegions.Find(RegionName); return Pending != nullptr ? *Pending : 0.0f; }

	/** The balance of the region a refugium steers as it stands now: the loaded region outside, or what the save knows
	 *  plus what waits for it while the group is down in the refugium; -1 if nothing is known */
	float GetRefugiumRegionValue() const;

	/** Takes the waiting change of a region (called by the region itself) */
	float TakePending(const FString& RegionName);

	/** Last change of a player's balance (for the pulse in the HUD): seconds since, and how much */
	bool GetLastChange(const AActor* Player, float& OutSecondsAgo, float& OutDelta) const;

	/** The region a refugium belongs to and how its beds move it now, per in-game hour (negative: light beds cleanse) */
	float GetRefugiumDrift(int32* OutLightBeds = nullptr, int32* OutShadowBeds = nullptr) const;

	void WriteSave(UVaelSaveGame& Save) const;
	void ReadSave(const UVaelSaveGame& Save);

	//~Begin FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return !IsTemplate(); }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UVaelBalanceSubsystem, STATGROUP_Tickables); }
	//~End FTickableGameObject

	/** The refugium of the Aschenmark and the region its beds steer */
	static const TCHAR* RefugiumLevel;
	static const TCHAR* RefugiumRegion;

private:

	struct FChange
	{
		double Time = 0.0;
		float Delta = 0.0f;
	};

	TMap<TWeakObjectPtr<const AActor>, FChange> LastChanges;

	UPROPERTY()
	TMap<FString, float> PendingRegions;

	/** Real seconds gathered towards the next step of the beds */
	float BedClock = 0.0f;

	/** The mark of the balance on each player: an overlay on their meshes (M_Vael_Zeichnung) */
	UPROPERTY(Transient)
	TMap<TWeakObjectPtr<AActor>, TObjectPtr<class UMaterialInstanceDynamic>> Marks;

	/** Puts the mark on every player and sets it to their balance */
	void UpdateMarks();
};
