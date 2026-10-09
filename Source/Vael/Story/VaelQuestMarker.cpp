// Copyright Epic Games, Inc. All Rights Reserved.

#include "Story/VaelQuestMarker.h"
#include "Components/BillboardComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Player/VaelCharacter.h"
#include "Story/VaelQuestSubsystem.h"
#include "TimerManager.h"

namespace
{
	/** Seconds between two looks for players */
	constexpr float QuestMarkerInterval = 0.5f;
}

AVaelQuestMarker::AVaelQuestMarker()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

#if WITH_EDITORONLY_DATA
	Icon = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Icon"));
	if (Icon != nullptr)
	{
		Icon->SetupAttachment(RootComponent);
	}
#endif

	SetActorHiddenInGame(true);
}

void AVaelQuestMarker::BeginPlay()
{
	Super::BeginPlay();

	if (!MarkerId.IsNone())
	{
		GetWorldTimerManager().SetTimer(CheckTimer, this, &AVaelQuestMarker::CheckPlayers, QuestMarkerInterval, true, FMath::FRandRange(0.0f, QuestMarkerInterval));
	}
}

void AVaelQuestMarker::CheckPlayers()
{
	UVaelQuestSubsystem* Quests = UVaelQuestSubsystem::Get(this);
	if (Quests == nullptr)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const AVaelCharacter* Player = It->Get() != nullptr ? It->Get()->GetPawn<AVaelCharacter>() : nullptr;
		if (Player != nullptr && FVector::Dist2D(Player->GetActorLocation(), GetActorLocation()) <= Radius)
		{
			Quests->NotifyReach(MarkerId);
			return;
		}
	}
}
