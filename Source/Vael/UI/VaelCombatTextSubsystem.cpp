// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/VaelCombatTextSubsystem.h"
#include "Creatures/VaelCreature.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UI/VaelUISettings.h"

namespace
{
	/** Highest number of texts on screen; the oldest make room */
	constexpr int32 MaxCombatTexts = 64;
}

void UVaelCombatTextSubsystem::PostDamage(const AActor* Target, float Damage, float EffectMultiplier, bool bTargetIsPlayer)
{
	const UVaelUISettings* Settings = UVaelUISettings::Get();
	const int32 Rounded = FMath::RoundToInt(Damage);
	if (!Settings->bShowDamageNumbers || Rounded <= 0)
	{
		return;
	}

	// Players see their losses in red; against creatures the color tells how well the hit worked
	if (bTargetIsPlayer)
	{
		Post(Target, FText::Format(NSLOCTEXT("VaelHUD", "PlayerDamage", "-{0}"), Rounded), Settings->PlayerDamageColor, Settings->DamageNumberSize, 0.0f);
	}
	else if (EffectMultiplier >= Settings->WeaknessThreshold)
	{
		Post(Target, FText::AsNumber(Rounded), Settings->WeaknessColor, Settings->WeaknessNumberSize, 0.0f);
	}
	else if (EffectMultiplier <= Settings->ResistanceThreshold)
	{
		Post(Target, FText::AsNumber(Rounded), Settings->ResistanceColor, Settings->DamageNumberSize, 0.0f);
	}
	else
	{
		Post(Target, FText::AsNumber(Rounded), Settings->DamageColor, Settings->DamageNumberSize, 0.0f);
	}
}

void UVaelCombatTextSubsystem::PostReaction(const AActor* Target, const FText& Text)
{
	const UVaelUISettings* Settings = UVaelUISettings::Get();
	if (Settings->bShowReactionTexts)
	{
		Post(Target, Text, Settings->ReactionColor, Settings->ReactionTextSize, Settings->ReactionExtraHeight);
	}
}

void UVaelCombatTextSubsystem::Post(const AActor* Target, const FText& Text, const FLinearColor& Color, float Size, float ExtraHeight)
{
	UWorld* World = Target != nullptr ? Target->GetWorld() : nullptr;
	UVaelCombatTextSubsystem* Subsystem = World != nullptr ? World->GetSubsystem<UVaelCombatTextSubsystem>() : nullptr;
	if (Subsystem == nullptr)
	{
		return;
	}

	// Above the head; flying creatures carry their body higher
	const AVaelCreature* Creature = Cast<AVaelCreature>(Target);
	const float Height = Creature != nullptr ? Creature->GetHealthBarHeight() : Target->GetSimpleCollisionHalfHeight() + 40.0f;

	// A little sideways scatter keeps numbers of quick hits apart
	const FVector Scatter(FMath::FRandRange(-25.0f, 25.0f), FMath::FRandRange(-25.0f, 25.0f), 0.0f);

	FVaelCombatText& CombatText = Subsystem->Texts.AddDefaulted_GetRef();
	CombatText.Location = Target->GetActorLocation() + FVector(0.0f, 0.0f, Height + ExtraHeight) + Scatter;
	CombatText.Text = Text;
	CombatText.Color = Color;
	CombatText.Size = Size;
	CombatText.StartTime = World->GetTimeSeconds();

	if (Subsystem->Texts.Num() > MaxCombatTexts)
	{
		Subsystem->Texts.RemoveAt(0, Subsystem->Texts.Num() - MaxCombatTexts);
	}
}

const TArray<FVaelCombatText>& UVaelCombatTextSubsystem::GetActiveTexts()
{
	const double Now = GetWorld()->GetTimeSeconds();
	const float Duration = UVaelUISettings::Get()->CombatTextDuration;

	Texts.RemoveAll([Now, Duration](const FVaelCombatText& CombatText) { return Now - CombatText.StartTime > Duration; });

	return Texts;
}
