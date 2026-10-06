// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelCreatureSpawner.h"
#include "Components/BillboardComponent.h"
#include "Creatures/VaelCreatureData.h"
#include "Engine/World.h"
#include "EngineUtils.h"
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

	GameMode->SpawnCreatureGroup(Data, GetActorLocation(), Count, Spread);
}

bool AVaelCreatureSpawner::LevelHasSpawners(const UWorld* World)
{
	return World != nullptr && TActorIterator<AVaelCreatureSpawner>(World);
}
