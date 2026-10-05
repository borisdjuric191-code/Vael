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

	/** Shows how much damage every hit dealt */
	UPROPERTY(config, EditAnywhere, Category="Combat Text")
	bool bShowDamageNumbers = true;

	/** Shows words like "Zerschmettert!" when a reaction happens */
	UPROPERTY(config, EditAnywhere, Category="Combat Text")
	bool bShowReactionTexts = true;

	/** Seconds a combat text rises and fades */
	UPROPERTY(config, EditAnywhere, Category="Combat Text", meta = (ClampMin = 0.1))
	float CombatTextDuration = 1.0f;

	/** Pixels a combat text rises during its time, at HUD scale 1 */
	UPROPERTY(config, EditAnywhere, Category="Combat Text", meta = (ClampMin = 0))
	float CombatTextRise = 45.0f;

	/** Hits at least this many times as strong as normal count as a weakness: big and yellow */
	UPROPERTY(config, EditAnywhere, Category="Combat Text", meta = (ClampMin = 1))
	float WeaknessThreshold = 1.4f;

	/** Hits at most this many times as strong as normal count as a resistance: grey */
	UPROPERTY(config, EditAnywhere, Category="Combat Text", meta = (ClampMin = 0, ClampMax = 1))
	float ResistanceThreshold = 0.8f;

	/** Text height of damage numbers in pixels at HUD scale 1 */
	UPROPERTY(config, EditAnywhere, Category="Combat Text", meta = (ClampMin = 4))
	float DamageNumberSize = 16.0f;

	/** Text height of numbers of hits against a weakness */
	UPROPERTY(config, EditAnywhere, Category="Combat Text", meta = (ClampMin = 4))
	float WeaknessNumberSize = 22.0f;

	/** Text height of reaction words */
	UPROPERTY(config, EditAnywhere, Category="Combat Text", meta = (ClampMin = 4))
	float ReactionTextSize = 17.0f;

	/** Reaction words start this much higher than damage numbers, in cm */
	UPROPERTY(config, EditAnywhere, Category="Combat Text", meta = (ClampMin = 0))
	float ReactionExtraHeight = 40.0f;

	UPROPERTY(config, EditAnywhere, Category="Combat Text")
	FLinearColor DamageColor = FLinearColor(FColor(243, 230, 208));

	UPROPERTY(config, EditAnywhere, Category="Combat Text")
	FLinearColor WeaknessColor = FLinearColor(FColor(255, 210, 122));

	UPROPERTY(config, EditAnywhere, Category="Combat Text")
	FLinearColor ResistanceColor = FLinearColor(FColor(154, 143, 134));

	UPROPERTY(config, EditAnywhere, Category="Combat Text")
	FLinearColor PlayerDamageColor = FLinearColor(FColor(255, 107, 94));

	UPROPERTY(config, EditAnywhere, Category="Combat Text")
	FLinearColor ReactionColor = FLinearColor(FColor(255, 207, 107));
};
