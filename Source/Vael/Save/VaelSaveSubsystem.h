// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "VaelSaveSubsystem.generated.h"

class AVaelCharacter;
class AVaelRegion;
class UVaelSaveGame;

/**
 *  Keeps the game between sessions. One save for the group: it is loaded when the game starts and written by itself,
 *  shortly after something worth keeping happens, every now and then, and when the level ends.
 *  Off for automatic tests and with -VaelNoSave: then nothing is loaded and nothing written.
 */
UCLASS()
class UVaelSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	//~Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~End USubsystem

	/** The save of the world of the object, null outside the game */
	static UVaelSaveSubsystem* Get(const UObject* WorldContext);

	/** True if a save was found and loaded when the game started */
	bool WasLoaded() const { return bLoaded; }

	/** Something worth keeping happened: the game is written within a few seconds */
	static void RequestSave(const UObject* WorldContext);

	/** Writes the game now */
	void SaveNow();

	/** Remembers that something in a level changed for good, like a chest being opened */
	static void Mark(const AActor* Actor);

	/** True if this thing of a level changed for good in an earlier session */
	static bool IsMarked(const AActor* Actor);

	/** Remembers that a creature that exists once was defeated */
	void MarkUniqueDefeated(FName Kind);

	/** True if a creature that exists once was defeated already */
	bool IsUniqueDefeated(FName Kind) const;

	/** Gives a player what their slot owned. Returns false if the slot has nothing saved. */
	bool RestorePlayer(AVaelCharacter* Player, int32 PlayerSlot) const;

	/** Notes what a player owns, like when they leave */
	void CapturePlayer(const AVaelCharacter* Player, int32 PlayerSlot);

	/** Gives a region the corruption it had */
	void RestoreRegion(AVaelRegion* Region) const;

	/** A level has started: saving goes on after a new game */
	void OnLevelStarted() { bSuspended = false; }

	/** Throws the save away and starts the current level from the beginning */
	void StartNewGame();

	/** For the automatic test: writes the game into memory, wipes everything, reads it back and says whether the group and the player are as before. Touches no file. */
	FString TestRoundTrip(AVaelCharacter* Player, int32 PlayerSlot);

private:

	/** Hands the group's part of the save to the quests, the grimoire, the compendium and the explanations */
	void ApplyToGroup() const;

	/** Collects the state of the group, the players in the level and the regions */
	void Collect();

	/** Writes when asked to, and every now and then */
	bool Tick(float DeltaSeconds);

	/** Name of a thing of a level in the save, "Level.Actor" */
	static FName GetActorKey(const AActor* Actor);

	UPROPERTY()
	TObjectPtr<UVaelSaveGame> Current;

	FTSTicker::FDelegateHandle TickHandle;

	/** False for automatic tests and with -VaelNoSave */
	bool bEnabled = true;

	bool bLoaded = false;

	/** True between a new game and the start of its level, so the old state isn't written again */
	bool bSuspended = false;

	bool bDirty = false;

	/** Seconds since the last write */
	float TimeSinceSave = 0.0f;
};
