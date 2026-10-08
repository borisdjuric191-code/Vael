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

	/** Height in pixels of the name of the queued formula above the head of its mage, on a 1080 pixel high screen. 0 hides it. */
	UPROPERTY(config, EditAnywhere, Category="HUD", meta = (ClampMin = 0, ClampMax = 60))
	float FormulaNameSize = 26.0f;

	/** How far above the centre of the mage the name of the queued formula floats, in cm: above the circling elements */
	UPROPERTY(config, EditAnywhere, Category="HUD", meta = (ClampMin = 0))
	float FormulaNameHeight = 205.0f;

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

	/** Hits dealing at least this much damage are heavy, like hits against a weakness or with a reaction */
	UPROPERTY(config, EditAnywhere, Category="Hit Feedback", meta = (ClampMin = 0))
	float HeavyHitDamage = 30.0f;

	/** Real seconds the game nearly stops on a heavy hit, 0 for no hit-stop */
	UPROPERTY(config, EditAnywhere, Category="Hit Feedback", meta = (ClampMin = 0, ClampMax = 0.5))
	float HitStopDuration = 0.06f;

	/** Speed of the game during a hit-stop */
	UPROPERTY(config, EditAnywhere, Category="Hit Feedback", meta = (ClampMin = 0.01, ClampMax = 1))
	float HitStopTimeDilation = 0.05f;

	/** Real seconds after a hit-stop before the next one may start, so volleys don't stutter */
	UPROPERTY(config, EditAnywhere, Category="Hit Feedback", meta = (ClampMin = 0))
	float HitStopCooldown = 0.15f;

	/** Camera shake of a heavy hit on a creature, 1 is a heavy blow */
	UPROPERTY(config, EditAnywhere, Category="Hit Feedback", meta = (ClampMin = 0))
	float HeavyHitShake = 0.2f;

	/** Camera shake when a player is hurt */
	UPROPERTY(config, EditAnywhere, Category="Hit Feedback", meta = (ClampMin = 0))
	float PlayerHurtShake = 0.25f;

	/** Creatures hit by at least this much damage stagger */
	UPROPERTY(config, EditAnywhere, Category="Hit Feedback", meta = (ClampMin = 0))
	float StaggerMinDamage = 6.0f;

	/** Seconds a creature staggers after a normal hit */
	UPROPERTY(config, EditAnywhere, Category="Hit Feedback", meta = (ClampMin = 0))
	float StaggerDuration = 0.15f;

	/** Seconds a creature staggers after a heavy hit */
	UPROPERTY(config, EditAnywhere, Category="Hit Feedback", meta = (ClampMin = 0))
	float HeavyStaggerDuration = 0.35f;

	/** How far a staggering body tips over, in degrees */
	UPROPERTY(config, EditAnywhere, Category="Hit Feedback", meta = (ClampMin = 0, ClampMax = 45))
	float StaggerTilt = 14.0f;
};
