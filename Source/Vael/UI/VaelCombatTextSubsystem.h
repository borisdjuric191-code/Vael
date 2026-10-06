// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VaelCombatTextSubsystem.generated.h"

class AActor;

/** A number or word that rises from a spot in the world and fades, like a damage number */
struct FVaelCombatText
{
	/** Where it starts in the world */
	FVector Location = FVector::ZeroVector;

	FText Text;
	FLinearColor Color = FLinearColor::White;

	/** Text height in pixels at HUD scale 1 */
	float Size = 16.0f;

	/** World time at which it appeared; stops while the game is paused */
	double StartTime = 0.0;
};

/**
 *  Collects the floating combat texts the HUD draws: damage numbers and reactions like "Zerschmettert!".
 */
UCLASS()
class UVaelCombatTextSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	/** Shows the damage an actor took, colored by how effective the hit was */
	static void PostDamage(const AActor* Target, float Damage, float EffectMultiplier, bool bTargetIsPlayer);

	/** Shows a word over an actor, slightly above the damage numbers */
	static void PostReaction(const AActor* Target, const FText& Text);

	/** Shows a pickup or loot over an actor, like "+25" or "Glutdruese", in its own color */
	static void PostPickup(const AActor* Target, const FText& Text, const FLinearColor& Color);

	/** Texts still on screen, oldest first. Removes those whose time is over. */
	const TArray<FVaelCombatText>& GetActiveTexts();

private:

	/** Adds a text above the actor */
	static void Post(const AActor* Target, const FText& Text, const FLinearColor& Color, float Size, float ExtraHeight);

	/** Texts on screen */
	TArray<FVaelCombatText> Texts;
};
