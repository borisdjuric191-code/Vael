// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compendium/VaelCompendiumSubsystem.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UI/VaelNoticeSubsystem.h"
#include "Vael.h"

#define LOCTEXT_NAMESPACE "VaelCompendium"

namespace
{
	/** Color of the notices of a book, as on its cover */
	FLinearColor GetBookColor(EVaelCompendiumBook Book)
	{
		switch (Book)
		{
		case EVaelCompendiumBook::Herbarium:	return FLinearColor(FColor(132, 168, 96));
		case EVaelCompendiumBook::Stones:		return FLinearColor(FColor(160, 162, 172));
		case EVaelCompendiumBook::Fungi:		return FLinearColor(FColor(176, 142, 204));
		default:								return FLinearColor(FColor(196, 112, 82));
		}
	}
}

FText UVaelCompendiumSubsystem::GetStageWord(EVaelResearchStage Stage, EVaelCompendiumBook Book)
{
	switch (Stage)
	{
	case EVaelResearchStage::Sighted:		return LOCTEXT("StageSighted", "gesichtet");
	case EVaelResearchStage::Observed:		return LOCTEXT("StageObserved", "beobachtet");
	case EVaelResearchStage::Researched:	return LOCTEXT("StageResearched", "erforscht");
	case EVaelResearchStage::Defeated:
		switch (Book)
		{
		case EVaelCompendiumBook::Herbarium:
		case EVaelCompendiumBook::Fungi:	return LOCTEXT("StageHarvested", "geerntet");
		case EVaelCompendiumBook::Stones:	return LOCTEXT("StageMined", "abgebaut");
		default:							return LOCTEXT("StageDefeated", "bezwungen");
		}
	default:								return LOCTEXT("StageUnknown", "unbekannt");
	}
}

void UVaelCompendiumSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UAssetManager& AssetManager = UAssetManager::Get();

	TArray<FPrimaryAssetId> EntryIds;
	AssetManager.GetPrimaryAssetIdList(UVaelCompendiumEntry::PrimaryAssetType, EntryIds);

	for (const FPrimaryAssetId& EntryId : EntryIds)
	{
		UVaelCompendiumEntry* Entry = Cast<UVaelCompendiumEntry>(AssetManager.GetPrimaryAssetPath(EntryId).TryLoad());
		if (Entry == nullptr || Entry->SubjectId.IsNone())
		{
			UE_LOG(LogVael, Warning, TEXT("Compendium entry '%s' could not be loaded or names no subject"), *EntryId.ToString());
			continue;
		}

		Entries.Add(Entry);
	}

	Entries.Sort([](const UVaelCompendiumEntry& A, const UVaelCompendiumEntry& B)
	{
		return A.Book != B.Book ? A.Book < B.Book : A.SortOrder < B.SortOrder;
	});

	UE_LOG(LogVael, Log, TEXT("Compendium loaded %d entries"), Entries.Num());
}

UVaelCompendiumSubsystem* UVaelCompendiumSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	return GameInstance != nullptr ? GameInstance->GetSubsystem<UVaelCompendiumSubsystem>() : nullptr;
}

const UVaelCompendiumEntry* UVaelCompendiumSubsystem::FindEntry(FName SubjectId) const
{
	for (const UVaelCompendiumEntry* Entry : Entries)
	{
		if (Entry->SubjectId == SubjectId)
		{
			return Entry;
		}
	}

	return nullptr;
}

TConstArrayView<EVaelCompendiumBook> UVaelCompendiumSubsystem::GetBooks()
{
	static const EVaelCompendiumBook Books[] = { EVaelCompendiumBook::Bestiary, EVaelCompendiumBook::Herbarium, EVaelCompendiumBook::Fungi, EVaelCompendiumBook::Stones };
	return Books;
}

TArray<const UVaelCompendiumEntry*> UVaelCompendiumSubsystem::GetEntries(EVaelCompendiumBook Book) const
{
	TArray<const UVaelCompendiumEntry*> InBook;
	for (const UVaelCompendiumEntry* Entry : Entries)
	{
		if (Entry->Book == Book)
		{
			InBook.Add(Entry);
		}
	}

	return InBook;
}

FVaelResearchProgress UVaelCompendiumSubsystem::GetProgress(FName SubjectId) const
{
	const FVaelResearchProgress* Found = Progress.Find(SubjectId);
	return Found != nullptr ? *Found : FVaelResearchProgress();
}

float UVaelCompendiumSubsystem::GetObservationShare(FName SubjectId) const
{
	const UVaelCompendiumEntry* Entry = FindEntry(SubjectId);
	if (Entry == nullptr)
	{
		return 0.0f;
	}

	const FVaelResearchProgress Current = GetProgress(SubjectId);
	return Current.Stage >= EVaelResearchStage::Observed ? 1.0f : FMath::Clamp(Current.ObservedSeconds / FMath::Max(Entry->ObserveSeconds, 0.01f), 0.0f, 1.0f);
}

void UVaelCompendiumSubsystem::Reach(FName SubjectId, EVaelResearchStage Stage)
{
	const UVaelCompendiumEntry* Entry = FindEntry(SubjectId);
	if (Entry == nullptr)
	{
		return;
	}

	FVaelResearchProgress& Current = Progress.FindOrAdd(SubjectId);
	if (Current.Stage >= Stage)
	{
		return;
	}

	Current.Stage = Stage;

	const FText StageWord = GetStageWord(Stage, Entry->Book);
	UE_LOG(LogVael, Log, TEXT("Compendium: %s %s"), *Entry->DisplayName.ToString(), *StageWord.ToString());
	UVaelNoticeSubsystem::Post(GetGameInstance(), FText::Format(LOCTEXT("StageNotice", "{0} {1}"), Entry->DisplayName, StageWord),
		LOCTEXT("StageNoticeDetail", "Neuer Eintrag im Kompendium"), GetBookColor(Entry->Book), 3.0f);
}

void UVaelCompendiumSubsystem::Sight(FName SubjectId)
{
	if (!SubjectId.IsNone() && GetProgress(SubjectId).Stage == EVaelResearchStage::Unknown)
	{
		Reach(SubjectId, EVaelResearchStage::Sighted);
	}
}

void UVaelCompendiumSubsystem::Observe(FName SubjectId, float Seconds)
{
	const UVaelCompendiumEntry* Entry = FindEntry(SubjectId);
	if (Entry == nullptr || Seconds <= 0.0f)
	{
		return;
	}

	Sight(SubjectId);

	FVaelResearchProgress& Current = Progress.FindOrAdd(SubjectId);
	Current.ObservedSeconds += Seconds;

	if (Current.ObservedSeconds >= Entry->ObserveSeconds)
	{
		Reach(SubjectId, EVaelResearchStage::Observed);
	}
}

void UVaelCompendiumSubsystem::AddSample(FName SubjectId)
{
	if (FindEntry(SubjectId) == nullptr)
	{
		return;
	}

	Sight(SubjectId);
	++Progress.FindOrAdd(SubjectId).Samples;
	Reach(SubjectId, EVaelResearchStage::Defeated);
}

int32 UVaelCompendiumSubsystem::StudyAtCamp()
{
	int32 NumResearched = 0;

	for (const UVaelCompendiumEntry* Entry : Entries)
	{
		const FVaelResearchProgress Current = GetProgress(Entry->SubjectId);
		if (Current.Stage == EVaelResearchStage::Defeated && Current.Samples >= Entry->StudyCount)
		{
			Reach(Entry->SubjectId, EVaelResearchStage::Researched);
			++NumResearched;
		}
	}

	return NumResearched;
}

void UVaelCompendiumSubsystem::SetStage(FName SubjectId, EVaelResearchStage Stage)
{
	Reach(SubjectId, Stage);
}

FString UVaelCompendiumSubsystem::Describe() const
{
	int32 NumPerStage[5] = { 0, 0, 0, 0, 0 };
	for (const UVaelCompendiumEntry* Entry : Entries)
	{
		++NumPerStage[static_cast<int32>(GetProgress(Entry->SubjectId).Stage)];
	}

	return FString::Printf(TEXT("%d entries: %d unknown, %d sighted, %d observed, %d defeated, %d researched"),
		Entries.Num(), NumPerStage[0], NumPerStage[1], NumPerStage[2], NumPerStage[3], NumPerStage[4]);
}

#undef LOCTEXT_NAMESPACE
