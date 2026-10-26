// Copyright Epic Games, Inc. All Rights Reserved.

#include "Save/VaelSaveSubsystem.h"
#include "Nature/VaelGarden.h"
#include "AbilitySystemComponent.h"
#include "Combat/VaelAttributeSet.h"
#include "Compendium/VaelCompendiumSubsystem.h"
#include "Debug/VaelShowcase.h"
#include "Debug/VaelSmokeTest.h"
#include "Debug/VaelStreamTest.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Items/VaelInventory.h"
#include "Items/VaelMaterialBag.h"
#include "Kismet/GameplayStatics.h"
#include "Magic/VaelElementComponent.h"
#include "Magic/VaelFormula.h"
#include "Magic/VaelGrimoireSubsystem.h"
#include "Misc/CommandLine.h"
#include "Player/VaelCharacter.h"
#include "Player/VaelPlayerController.h"
#include "Save/VaelSaveGame.h"
#include "Story/VaelQuestSubsystem.h"
#include "Vael.h"
#include "World/VaelProgressSubsystem.h"
#include "World/VaelRegion.h"
#include "World/VaelTimeOfDay.h"

namespace
{
	/** Name of the save on disk: Saved/SaveGames/VaelGruppe.sav */
	const TCHAR* const SaveSlotName = TEXT("VaelGruppe");

	/** Seconds after something worth keeping until the game is written, so several things at once make one write */
	constexpr float SaveRequestDelay = 2.0f;

	/** Seconds between two writes when nothing asked for one; keeps bags and purses fresh */
	constexpr float SaveInterval = 45.0f;
}

void UVaelSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// The save is handed to these, so they have to be there first
	Collection.InitializeDependency<UVaelQuestSubsystem>();
	Collection.InitializeDependency<UVaelGrimoireSubsystem>();
	Collection.InitializeDependency<UVaelCompendiumSubsystem>();
	Collection.InitializeDependency<UVaelProgressSubsystem>();
	Collection.InitializeDependency<UVaelGardenSubsystem>();
	Collection.InitializeDependency<UVaelTimeOfDay>();

	bEnabled = !FParse::Param(FCommandLine::Get(), TEXT("VaelNoSave")) && !AVaelSmokeTest::IsRequested() && !AVaelShowcase::IsRequested() && !AVaelStreamTest::IsRequested();

	if (bEnabled && UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0))
	{
		UVaelSaveGame* Loaded = Cast<UVaelSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0));
		if (Loaded != nullptr && Loaded->Version == UVaelSaveGame::CurrentVersion)
		{
			Current = Loaded;
			bLoaded = true;
			UE_LOG(LogVael, Log, TEXT("Save game loaded, written %s"), *Loaded->SavedAt.ToString());
		}
		else
		{
			UE_LOG(LogVael, Warning, TEXT("Save game could not be read or is of an older version, starting anew"));
		}
	}

	if (Current == nullptr)
	{
		Current = NewObject<UVaelSaveGame>(this);
	}

	if (bLoaded)
	{
		ApplyToGroup();
	}

	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UVaelSaveSubsystem::Tick), 0.5f);

	UE_LOG(LogVael, Log, TEXT("Saving is %s"), bEnabled ? TEXT("on") : TEXT("off"));
}

void UVaelSaveSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);

	Super::Deinitialize();
}

UVaelSaveSubsystem* UVaelSaveSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	return GameInstance != nullptr ? GameInstance->GetSubsystem<UVaelSaveSubsystem>() : nullptr;
}

void UVaelSaveSubsystem::RequestSave(const UObject* WorldContext)
{
	if (UVaelSaveSubsystem* Save = Get(WorldContext))
	{
		Save->bDirty = true;
	}
}

bool UVaelSaveSubsystem::Tick(float DeltaSeconds)
{
	TimeSinceSave += DeltaSeconds;

	if ((bDirty && TimeSinceSave >= SaveRequestDelay) || TimeSinceSave >= SaveInterval)
	{
		SaveNow();
	}

	return true;
}

void UVaelSaveSubsystem::SaveNow()
{
	TimeSinceSave = 0.0f;

	const UWorld* World = GetGameInstance() != nullptr ? GetGameInstance()->GetWorld() : nullptr;
	if (!bEnabled || bSuspended || World == nullptr || !World->HasBegunPlay())
	{
		return;
	}

	Collect();
	bDirty = false;

	Current->SavedAt = FDateTime::Now();
	if (!UGameplayStatics::SaveGameToSlot(Current, SaveSlotName, 0))
	{
		UE_LOG(LogVael, Warning, TEXT("Save game could not be written"));
	}
}

void UVaelSaveSubsystem::Collect()
{
	const UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GameInstance->GetWorld();

	GameInstance->GetSubsystem<UVaelQuestSubsystem>()->WriteSave(*Current);
	GameInstance->GetSubsystem<UVaelGrimoireSubsystem>()->WriteSave(*Current);
	GameInstance->GetSubsystem<UVaelCompendiumSubsystem>()->WriteSave(*Current);
	GameInstance->GetSubsystem<UVaelProgressSubsystem>()->WriteSave(*Current);
	GameInstance->GetSubsystem<UVaelGardenSubsystem>()->WriteSave(*Current);
	GameInstance->GetSubsystem<UVaelTimeOfDay>()->WriteSave(*Current);

	// Slots of players who are not in the level right now keep what they had
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const AVaelPlayerController* PlayerController = Cast<AVaelPlayerController>(It->Get());
		const AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
		if (Player != nullptr)
		{
			CapturePlayer(Player, PlayerController->GetPlayerSlot());
		}
	}

	for (TActorIterator<AVaelRegion> It(World); It; ++It)
	{
		FVaelSavedRegion& Saved = Current->Regions.FindOrAdd(GetActorKey(*It));
		Saved.Corruption = It->GetCorruption();
		Saved.ScarredCorruption = It->GetScarredCorruption();
	}
}

void UVaelSaveSubsystem::ApplyToGroup() const
{
	const UGameInstance* GameInstance = GetGameInstance();

	GameInstance->GetSubsystem<UVaelQuestSubsystem>()->ReadSave(*Current);
	GameInstance->GetSubsystem<UVaelGrimoireSubsystem>()->ReadSave(*Current);
	GameInstance->GetSubsystem<UVaelCompendiumSubsystem>()->ReadSave(*Current);
	GameInstance->GetSubsystem<UVaelProgressSubsystem>()->ReadSave(*Current);
	GameInstance->GetSubsystem<UVaelGardenSubsystem>()->ReadSave(*Current);
	GameInstance->GetSubsystem<UVaelTimeOfDay>()->ReadSave(*Current);
}

FName UVaelSaveSubsystem::GetActorKey(const AActor* Actor)
{
	// The name of the map without what the editor puts in front of it while playing
	return FName(*FString::Printf(TEXT("%s.%s"), *UWorld::RemovePIEPrefix(Actor->GetWorld()->GetMapName()), *Actor->GetName()));
}

void UVaelSaveSubsystem::Mark(const AActor* Actor)
{
	if (UVaelSaveSubsystem* Save = Get(Actor))
	{
		Save->Current->WorldMarks.Add(GetActorKey(Actor));
		Save->bDirty = true;
	}
}

bool UVaelSaveSubsystem::IsMarked(const AActor* Actor)
{
	const UVaelSaveSubsystem* Save = Get(Actor);
	return Save != nullptr && Save->Current->WorldMarks.Contains(GetActorKey(Actor));
}

void UVaelSaveSubsystem::MarkUniqueDefeated(FName Kind)
{
	Current->DefeatedUniques.Add(Kind);
	bDirty = true;
}

bool UVaelSaveSubsystem::IsUniqueDefeated(FName Kind) const
{
	return Current->DefeatedUniques.Contains(Kind);
}

void UVaelSaveSubsystem::CapturePlayer(const AVaelCharacter* Player, int32 PlayerSlot)
{
	if (Player == nullptr || PlayerSlot < 0)
	{
		return;
	}

	FVaelSavedPlayer& Saved = Current->Players.FindOrAdd(PlayerSlot);
	Player->GetInventory()->WriteSave(Saved);
	Player->GetMaterialBag()->WriteSave(Saved);

	Saved.QuickSlots.Reset();
	for (int32 SlotIndex = 0; SlotIndex < UVaelElementComponent::NumQuickSlots; ++SlotIndex)
	{
		Saved.QuickSlots.Add(FSoftObjectPath(Player->GetElementComponent()->GetQuickSlotFormula(SlotIndex)));
	}

	Saved.Corruption = Player->GetAbilitySystemComponent()->GetNumericAttributeBase(UVaelAttributeSet::GetCorruptionAttribute());
}

bool UVaelSaveSubsystem::RestorePlayer(AVaelCharacter* Player, int32 PlayerSlot) const
{
	const FVaelSavedPlayer* Saved = Player != nullptr ? Current->Players.Find(PlayerSlot) : nullptr;
	if (Saved == nullptr)
	{
		return false;
	}

	Player->GetInventory()->ReadSave(*Saved);
	Player->GetMaterialBag()->ReadSave(*Saved);

	for (int32 SlotIndex = 0; SlotIndex < UVaelElementComponent::NumQuickSlots; ++SlotIndex)
	{
		UVaelFormula* Formula = Saved->QuickSlots.IsValidIndex(SlotIndex) ? Cast<UVaelFormula>(Saved->QuickSlots[SlotIndex].TryLoad()) : nullptr;
		Player->GetElementComponent()->AssignQuickSlot(SlotIndex, Formula);
	}

	Player->GetAbilitySystemComponent()->SetNumericAttributeBase(UVaelAttributeSet::GetCorruptionAttribute(), Saved->Corruption);

	UE_LOG(LogVael, Log, TEXT("Player slot %d gets back what was saved"), PlayerSlot);
	return true;
}

void UVaelSaveSubsystem::RestoreRegion(AVaelRegion* Region) const
{
	if (const FVaelSavedRegion* Saved = Current->Regions.Find(GetActorKey(Region)))
	{
		Region->RestoreCorruption(Saved->Corruption, Saved->ScarredCorruption);
	}
}

void UVaelSaveSubsystem::StartNewGame()
{
	UWorld* World = GetGameInstance() != nullptr ? GetGameInstance()->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return;
	}

	UE_LOG(LogVael, Log, TEXT("New game: the save is thrown away"));

	UGameplayStatics::DeleteGameInSlot(SaveSlotName, 0);

	// An empty save resets everything the group had reached; nothing is written until the level has started anew
	Current = NewObject<UVaelSaveGame>(this);
	bLoaded = false;
	bDirty = false;
	bSuspended = true;
	ApplyToGroup();

	UGameplayStatics::OpenLevel(World, FName(*UWorld::RemovePIEPrefix(World->GetMapName())));
}

FString UVaelSaveSubsystem::TestRoundTrip(AVaelCharacter* Player, int32 PlayerSlot)
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UVaelQuestSubsystem* Quests = GameInstance->GetSubsystem<UVaelQuestSubsystem>();
	const UVaelCompendiumSubsystem* Compendium = GameInstance->GetSubsystem<UVaelCompendiumSubsystem>();
	const UVaelGrimoireSubsystem* Grimoire = GameInstance->GetSubsystem<UVaelGrimoireSubsystem>();

	const auto Describe = [&]()
	{
		return FString::Printf(TEXT("%s | %s | %d formulas, Mark %d | %d coins, %d potions, %d in the backpack, quick slot %s, corruption %.0f"), *Quests->Describe(), *Compendium->Describe(),
			Grimoire->GetKnownFormulas().Num(), Grimoire->IsMarkAwakened() ? 1 : 0, Player->GetInventory()->GetCoins(), Player->GetPotionCount(), Player->GetInventory()->GetBackpack().Num(),
			*GetNameSafe(Player->GetElementComponent()->GetQuickSlotFormula(0)), Player->GetAbilitySystemComponent()->GetNumericAttributeBase(UVaelAttributeSet::GetCorruptionAttribute()));
	};

	Collect();
	const FString Before = Describe();

	TArray<uint8> Bytes;
	UVaelSaveGame* Loaded = UGameplayStatics::SaveGameToMemory(Current, Bytes) ? Cast<UVaelSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)) : nullptr;
	if (Loaded == nullptr)
	{
		return TEXT("FAILED: the save could not be written to memory or read back");
	}

	// An empty save wipes the group and the player
	UVaelSaveGame* Kept = Current;
	Current = NewObject<UVaelSaveGame>(this);
	Current->Players.Add(PlayerSlot);
	ApplyToGroup();
	RestorePlayer(Player, PlayerSlot);
	const FString Wiped = Describe();

	Current = Loaded;
	ApplyToGroup();
	RestorePlayer(Player, PlayerSlot);
	const FString After = Describe();

	const bool bOk = Before == After && Before != Wiped && Loaded->WorldMarks.Num() == Kept->WorldMarks.Num() && Loaded->Regions.Num() == Kept->Regions.Num();
	return FString::Printf(TEXT("%s%d bytes, %d world marks, %d regions, %d players; same after reading back: %s; wiped in between: %s%s"), bOk ? TEXT("") : TEXT("FAILED: "), Bytes.Num(),
		Loaded->WorldMarks.Num(), Loaded->Regions.Num(), Loaded->Players.Num(), Before == After ? TEXT("yes") : TEXT("NO"), Before != Wiped ? TEXT("yes") : TEXT("NO"),
		Before == After ? TEXT("") : *FString::Printf(TEXT("\n    before: %s\n    after:  %s"), *Before, *After));
}
