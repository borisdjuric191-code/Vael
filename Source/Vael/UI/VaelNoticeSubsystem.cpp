// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/VaelNoticeSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Vael.h"

void UVaelNoticeSubsystem::Post(const UObject* WorldContext, const FText& Title, const FText& Detail, const FLinearColor& Color, float Duration)
{
	UE_LOG(LogVael, Log, TEXT("Notice: %s %s"), *Title.ToString(), *Detail.ToString());

	const UWorld* World = GEngine != nullptr ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UVaelNoticeSubsystem* Subsystem = World != nullptr ? World->GetSubsystem<UVaelNoticeSubsystem>() : nullptr;
	if (Subsystem == nullptr)
	{
		return;
	}

	// The same message again only renews the one on screen
	Subsystem->Notices.RemoveAll([&Title](const FVaelNotice& Notice) { return Notice.Title.EqualTo(Title); });

	FVaelNotice& Notice = Subsystem->Notices.AddDefaulted_GetRef();
	Notice.Title = Title;
	Notice.Detail = Detail;
	Notice.Color = Color;
	Notice.StartTime = FPlatformTime::Seconds();
	Notice.Duration = Duration;

	if (Subsystem->Notices.Num() > MaxNotices)
	{
		Subsystem->Notices.RemoveAt(0, Subsystem->Notices.Num() - MaxNotices);
	}
}

const TArray<FVaelNotice>& UVaelNoticeSubsystem::GetActiveNotices()
{
	const double Now = FPlatformTime::Seconds();
	Notices.RemoveAll([Now](const FVaelNotice& Notice) { return Now - Notice.StartTime > Notice.Duration; });

	return Notices;
}
