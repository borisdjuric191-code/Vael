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

	/**
	 *  Lays a scroll in front of the player, 3 m unless a distance in cm is given, that teaches the formula of the elements, letters as in VaelCast.
	 *  Example: VaelScroll EEE
	 */
	UFUNCTION(Exec)
	void VaelScroll(const FString& Elements, float Distance = 300.0f);

	/** Kills every creature in the level */
	UFUNCTION(Exec)
	void VaelKillAll();

	/** Chooses the controller symbols of the HUD: Auto, Xbox or PlayStation. Example: VaelGlyphs PlayStation */
	UFUNCTION(Exec)
	void VaelGlyphs(const FString& Glyphs);

	/** Changes the weather of the region the player stands in: Klar, Regen, Sturm or Duerre. Example: VaelWeather Sturm */
	UFUNCTION(Exec)
	void VaelWeather(const FString& Weather);

	/** Sets the corruption of the region the player stands in, 0 to 100. Creatures spawned afterwards may be marked. Example: VaelCorruption 80 */
	UFUNCTION(Exec)
	void VaelCorruption(float Corruption);

	/** Sets the personal corruption of the player, 0 to 100. Example: VaelPlayerCorruption 30 */
	UFUNCTION(Exec)
	void VaelPlayerCorruption(float Corruption);

	/** Puts on or takes off a gear effect for testing until items exist: WeatherWard or WeatherAttunement. Example: VaelGear WeatherWard */
	UFUNCTION(Exec)
	void VaelGear(const FString& Gear);
};
