// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelCreatureSpawner.h"
#include "Components/BillboardComponent.h"
#include "Creatures/VaelCreature.h"
#include "Creatures/VaelCreatureData.h"
#include "Save/VaelSaveSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Vael.h"
#include "VaelGameMode.h"

AVaelCreatureSpawner::AVaelCreatureSpawner()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

#if WITH_EDITORONLY_DATA
	// Visible in the editor only
	UBillboardComponent* Icon = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Icon"));
	if (Icon != nullptr)
	{
		Icon->SetupAttachment(RootComponent);
	}
#endif

	CreatureKind = UVaelEmberCrawlerData::StaticClass();
}

void AVaelCreatureSpawner::BeginPlay()
{
	Super::BeginPlay();

	UVaelCreatureData* Data = CreatureData;
	if (Data == nullptr && CreatureKind != nullptr)
	{
		Data = CreatureKind->GetDefaultObject<UVaelCreatureData>();
	}

	AVaelGameMode* GameMode = GetWorld()->GetAuthGameMode<AVaelGameMode>();
	if (Data == nullptr || GameMode == nullptr)
	{
		UE_LOG(LogVael, Warning, TEXT("Spawner '%s' has no creature to spawn or runs without the Vael game mode"), *GetNameSafe(this));
		return;
	}

	// A creature that exists once and was defeated in an earlier session stays away
	const UVaelSaveSubsystem* Save = UVaelSaveSubsystem::Get(this);
	if (Data->bUnique && Save != nullptr && Save->IsUniqueDefeated(Data->KindId))
	{
		return;
	}

	SpawnWhenGroundIsThere();
}

void AVaelCreatureSpawner::SpawnWhenGroundIsThere()
{
	// Gives the ground of an open world a few seconds to stream in
	constexpr int32 SpawnerMaxGroundTries = 20;
	constexpr float SpawnerGroundRetryInterval = 0.5f;

	FVector Ground;
	if (!AVaelCreature::FindGround(GetWorld(), GetActorLocation(), Ground) && ++GroundTries < SpawnerMaxGroundTries)
	{
		FTimerHandle RetryTimer;
		GetWorldTimerManager().SetTimer(RetryTimer, this, &AVaelCreatureSpawner::SpawnWhenGroundIsThere, SpawnerGroundRetryInterval, false);
		return;
	}

	UVaelCreatureData* Data = CreatureData != nullptr ? CreatureData.Get() : CreatureKind->GetDefaultObject<UVaelCreatureData>();
	AVaelGameMode* GameMode = GetWorld()->GetAuthGameMode<AVaelGameMode>();
	if (GameMode != nullptr)
	{
		// In an open world they belong to the spawner's cell: they go when it unloads and come anew when it loads again
		GameMode->SpawnCreatureGroup(Data, GetActorLocation(), Count, Spread, true, GetWorld()->IsPartitionedWorld() ? GetLevel() : nullptr);
	}
}

bool AVaelCreatureSpawner::LevelHasSpawners(const UWorld* World)
{
	// In an open world the spawners may not have streamed in yet when the first player arrives
	return World != nullptr && (World->IsPartitionedWorld() || TActorIterator<AVaelCreatureSpawner>(World));
}
