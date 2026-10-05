// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "VaelCheatManager.generated.h"

/**
 *  Console commands for testing. Not available in shipping builds.
 */
UCLASS()
class UVaelCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:

	/**
	 *  Queues elements and casts them, aiming along the given world yaw.
	 *  Elements are the letters of the prototype: F fire, W water, E earth, L air, M mark. Example: VaelCast FE 90
	 */
	UFUNCTION(Exec)
	void VaelCast(const FString& Elements, float AimYaw);

	/** Adds every formula of the game to the grimoire, sealed ones included */
	UFUNCTION(Exec)
	void VaelLearnAll();

	/** Puts a condition on everything the player can hurt: Wet, Burning or Frozen, for the given seconds. Example: VaelStatus Wet 5 */
	UFUNCTION(Exec)
	void VaelStatus(const FString& Status, float Duration);

	/**
	 *  Spawns creatures 8 m in front of the player: Glutkriecher, Aschharpyie, Aelteste, Prediger or Koenigin.
	 *  Example: VaelSpawn Koenigin 1
	 */
	UFUNCTION(Exec)
	void VaelSpawn(const FString& Kind, int32 Count);

	/** Kills every creature in the level */
	UFUNCTION(Exec)
	void VaelKillAll();

	/** Chooses the controller symbols of the HUD: Auto, Xbox or PlayStation. Example: VaelGlyphs PlayStation */
	UFUNCTION(Exec)
	void VaelGlyphs(const FString& Glyphs);
};
