// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VaelNoticeSubsystem.generated.h"

/** A short message for all players, shown at the top of the screen for a few seconds */
struct FVaelNotice
{
	/** Main line */
	FText Title;

	/** Smaller second line, may be empty */
	FText Detail;

	/** Color of the main line */
	FLinearColor Color = FLinearColor::White;

	/** Real time at which the notice appeared, in seconds */
	double StartTime = 0.0;

	/** Seconds the notice stays */
	float Duration = 4.0f;
};

/**
 *  Collects the messages the HUD shows to all players: new formulas, echoes, boss warnings and the like.
 *  Uses real time, so notices keep fading while the game is paused.
 */
UCLASS()
class UVaelNoticeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	/** Shows a message to all players and writes it to the log */
	static void Post(const UObject* WorldContext, const FText& Title, const FText& Detail = FText::GetEmpty(), const FLinearColor& Color = FLinearColor::White, float Duration = 4.2f);

	/** Notices still on screen, oldest first. Removes those whose time is over. */
	const TArray<FVaelNotice>& GetActiveNotices();

	/** Highest number of notices on screen at once; older ones make room */
	static constexpr int32 MaxNotices = 4;

private:

	/** Notices on screen */
	TArray<FVaelNotice> Notices;
};
