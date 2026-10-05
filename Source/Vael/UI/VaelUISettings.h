// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "VaelUISettings.generated.h"

/** Button symbols the HUD shows for an input device */
UENUM(BlueprintType)
enum class EVaelInputGlyphs : uint8
{
	Keyboard,
	Xbox,
	PlayStation
};

/** Which controller symbols to show */
UENUM(BlueprintType)
enum class EVaelGamepadGlyphPreference : uint8
{
	/** Detected from the controller; PlayStation controllers that report themselves as Xbox controllers show Xbox symbols */
	Auto,
	Xbox,
	PlayStation
};

/**
 *  Settings of the HUD and menus.
 *  Edited under Project Settings > Game > Vael UI, stored in DefaultGame.ini.
 */
UCLASS(config=Game, defaultconfig, meta = (DisplayName = "Vael UI"))
class UVaelUISettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	/** Returns the settings object */
	static const UVaelUISettings* Get() { return GetDefault<UVaelUISettings>(); }

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** Controller symbols shown in the HUD and the grimoire */
	UPROPERTY(config, EditAnywhere, Category="Input")
	EVaelGamepadGlyphPreference GamepadGlyphs = EVaelGamepadGlyphPreference::Auto;

	/** Size of the HUD relative to a 1080 pixel high screen */
	UPROPERTY(config, EditAnywhere, Category="HUD", meta = (ClampMin = 0.5, ClampMax = 2))
	float HudScale = 1.0f;
};
