// Copyright Epic Games, Inc. All Rights Reserved.

#include "Story/VaelQuestSubsystem.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Items/VaelInventory.h"
#include "Player/VaelCharacter.h"
#include "UI/VaelNoticeSubsystem.h"
#include "Vael.h"

#define LOCTEXT_NAMESPACE "VaelQuests"

namespace
{
	/** Color of quest notices: warm parchment */
	const FLinearColor QuestNoticeColor(FColor(232, 205, 140));
}

void UVaelQuestSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UAssetManager& AssetManager = UAssetManager::Get();

	TArray<FPrimaryAssetId> QuestIds;
	AssetManager.GetPrimaryAssetIdList(UVaelQuest::PrimaryAssetType, QuestIds);

	for (const FPrimaryAssetId& QuestAssetId : QuestIds)
	{
		UVaelQuest* Quest = Cast<UVaelQuest>(AssetManager.GetPrimaryAssetPath(QuestAssetId).TryLoad());
		if (Quest == nullptr || Quest->QuestId.IsNone() || Quest->Steps.IsEmpty())
		{
			UE_LOG(LogVael, Warning, TEXT("Quest '%s' could not be loaded, has no id or no steps"), *QuestAssetId.ToString());
			continue;
		}

		Quests.Add(Quest);
	}

	Quests.Sort([](const UVaelQuest& A, const UVaelQuest& B)
	{
		return A.bMainQuest != B.bMainQuest ? A.bMainQuest : A.SortOrder < B.SortOrder;
	});

	// Quests without predecessor are there from the start, without a notice: the story begins with them
	for (const UVaelQuest* Quest : Quests)
	{
		if (Quest->Prerequisite.IsNone())
		{
			Progress.FindOrAdd(Quest->QuestId).bActive = true;
		}
	}

	UE_LOG(LogVael, Log, TEXT("Quests loaded: %d"), Quests.Num());
}

UVaelQuestSubsystem* UVaelQuestSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	return GameInstance != nullptr ? GameInstance->GetSubsystem<UVaelQuestSubsystem>() : nullptr;
}

const UVaelQuest* UVaelQuestSubsystem::FindQuest(FName QuestId) const
{
	for (const UVaelQuest* Quest : Quests)
	{
		if (Quest->QuestId == QuestId)
		{
			return Quest;
		}
	}

	return nullptr;
}

FVaelQuestProgress UVaelQuestSubsystem::GetProgress(const UVaelQuest* Quest) const
{
	const FVaelQuestProgress* Found = Quest != nullptr ? Progress.Find(Quest->QuestId) : nullptr;
	return Found != nullptr ? *Found : FVaelQuestProgress();
}

const FVaelQuestStep* UVaelQuestSubsystem::GetCurrentStep(const UVaelQuest* Quest) const
{
	const FVaelQuestProgress Current = GetProgress(Quest);
	return Current.bActive && !Current.bDone && Quest->Steps.IsValidIndex(Current.Step) ? &Quest->Steps[Current.Step] : nullptr;
}

TArray<const UVaelQuest*> UVaelQuestSubsystem::GetOpenQuests() const
{
	TArray<const UVaelQuest*> Open;
	for (const UVaelQuest* Quest : Quests)
	{
		if (GetCurrentStep(Quest) != nullptr)
		{
			Open.Add(Quest);
		}
	}

	return Open;
}

void UVaelQuestSubsystem::NotifyKill(FName Kind)
{
	CountFor(EVaelQuestObjective::Kill, Kind);
}

void UVaelQuestSubsystem::NotifyGather(FName SubjectId)
{
	CountFor(EVaelQuestObjective::Gather, SubjectId);
}

void UVaelQuestSubsystem::NotifyCast(FName FormulaName)
{
	CountFor(EVaelQuestObjective::Cast, FormulaName);
}

void UVaelQuestSubsystem::NotifyReach(FName MarkerId)
{
	CountFor(EVaelQuestObjective::Reach, MarkerId);
}

void UVaelQuestSubsystem::NotifyEvent(FName EventName)
{
	CountFor(EVaelQuestObjective::Event, EventName);
}

void UVaelQuestSubsystem::CountFor(EVaelQuestObjective Type, FName Target)
{
	// Collected first: advancing may activate further quests
	TArray<const UVaelQuest*> Matching;
	for (const UVaelQuest* Quest : GetOpenQuests())
	{
		const FVaelQuestStep* Step = GetCurrentStep(Quest);
		if (Step->Type == Type && (Step->Target.IsNone() || Step->Target == Target))
		{
			Matching.Add(Quest);
		}
	}

	for (const UVaelQuest* Quest : Matching)
	{
		Advance(Quest);
	}
}

bool UVaelQuestSubsystem::IsWaitingForTalk(FName SpeakerKey) const
{
	for (const UVaelQuest* Quest : GetOpenQuests())
	{
		const FVaelQuestStep* Step = GetCurrentStep(Quest);
		if (Step->Type == EVaelQuestObjective::Talk && Step->Target == SpeakerKey)
		{
			return true;
		}
	}

	return false;
}

bool UVaelQuestSubsystem::TakeTalk(FName SpeakerKey, TArray<FText>& OutLines)
{
	// One talk finishes one step: the first quest that waits for this person
	for (const UVaelQuest* Quest : GetOpenQuests())
	{
		const FVaelQuestStep* Step = GetCurrentStep(Quest);
		if (Step->Type == EVaelQuestObjective::Talk && Step->Target == SpeakerKey)
		{
			OutLines = Step->Lines;
			Advance(Quest);
			return true;
		}
	}

	return false;
}

void UVaelQuestSubsystem::Advance(const UVaelQuest* Quest)
{
	FVaelQuestProgress& Current = Progress.FindOrAdd(Quest->QuestId);
	const FVaelQuestStep& Step = Quest->Steps[Current.Step];

	if (++Current.Count < Step.Count)
	{
		UE_LOG(LogVael, Log, TEXT("Quest %s: %s %d/%d"), *Quest->QuestId.ToString(), *Step.Objective.ToString(), Current.Count, Step.Count);
		return;
	}

	Current.Count = 0;
	if (++Current.Step >= Quest->Steps.Num())
	{
		Finish(Quest);
		return;
	}

	const FVaelQuestStep& Next = Quest->Steps[Current.Step];
	UE_LOG(LogVael, Log, TEXT("Quest %s: next step %d, %s"), *Quest->QuestId.ToString(), Current.Step, *Next.Objective.ToString());
	UVaelNoticeSubsystem::Post(GetGameInstance(), Quest->Title, Next.Objective, QuestNoticeColor, 3.5f);
}

void UVaelQuestSubsystem::Finish(const UVaelQuest* Quest)
{
	FVaelQuestProgress& Current = Progress.FindOrAdd(Quest->QuestId);
	Current.bDone = true;
	Current.Step = Quest->Steps.Num();

	++Current.TimesDone;
	UE_LOG(LogVael, Log, TEXT("Quest %s done"), *Quest->QuestId.ToString());
	const FText Reward = Quest->CoinReward > 0 ? FText::Format(LOCTEXT("QuestCoins", "+{0} Gildenmünzen für jeden"), Quest->CoinReward) : FText::GetEmpty();
	UVaelNoticeSubsystem::Post(GetGameInstance(), FText::Format(Quest->bContract ? LOCTEXT("ContractDone", "Kontrakt erfüllt: {0}") : LOCTEXT("QuestDone", "Auftrag erfüllt: {0}"), Quest->Title),
		Reward, QuestNoticeColor, 4.0f);

	// Every player gets the full pay into their own purse
	const UWorld* World = GetGameInstance() != nullptr ? GetGameInstance()->GetWorld() : nullptr;
	if (Quest->CoinReward > 0 && World != nullptr)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const AVaelCharacter* Player = It->Get() != nullptr ? It->Get()->GetPawn<AVaelCharacter>() : nullptr;
			if (Player != nullptr)
			{
				Player->GetInventory()->AddCoins(Quest->CoinReward);
			}
		}
	}

	// A repeatable contract is offered again right away
	if (Quest->bRepeatable)
	{
		Current.bDone = false;
		Current.Step = 0;
		Current.Count = 0;
	}

	for (const UVaelQuest* Follower : Quests)
	{
		if (Follower->Prerequisite == Quest->QuestId && !GetProgress(Follower).bActive)
		{
			Activate(Follower);
		}
	}
}

void UVaelQuestSubsystem::Activate(const UVaelQuest* Quest)
{
	Progress.FindOrAdd(Quest->QuestId).bActive = true;

	UE_LOG(LogVael, Log, TEXT("Quest %s active"), *Quest->QuestId.ToString());
	UVaelNoticeSubsystem::Post(GetGameInstance(), FText::Format(Quest->bContract ? LOCTEXT("ContractNew", "Neuer Kontrakt: {0}") : LOCTEXT("QuestNew", "Neuer Auftrag: {0}"), Quest->Title),
		Quest->Steps[0].Objective, QuestNoticeColor, 4.5f);
}

void UVaelQuestSubsystem::SetStep(FName QuestId, int32 Step)
{
	const UVaelQuest* Quest = FindQuest(QuestId);
	if (Quest == nullptr)
	{
		return;
	}

	FVaelQuestProgress& Current = Progress.FindOrAdd(QuestId);
	Current.bActive = true;
	Current.bDone = false;
	Current.Count = 0;
	Current.Step = FMath::Max(Step, 0);

	if (Current.Step >= Quest->Steps.Num())
	{
		Finish(Quest);
	}
}

FString UVaelQuestSubsystem::Describe() const
{
	TArray<FString> Lines;
	for (const UVaelQuest* Quest : Quests)
	{
		const FVaelQuestProgress Current = GetProgress(Quest);
		const FString State = Current.bDone ? TEXT("done") : !Current.bActive ? TEXT("waiting")
			: FString::Printf(TEXT("step %d/%d (%d/%d)"), Current.Step + 1, Quest->Steps.Num(), Current.Count, Quest->Steps[Current.Step].Count);
		Lines.Add(FString::Printf(TEXT("%s %s"), *Quest->QuestId.ToString(), *State));
	}

	return FString::Join(Lines, TEXT(", "));
}

#undef LOCTEXT_NAMESPACE
