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

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickInterval = 1.0f;
}

namespace
{
	/** When a defeated near-player group may come back, by spawner path; survives the spawner unloading with its cell */
	TMap<FString, double> GClearedUntil;
	TWeakObjectPtr<UWorld> GClearedWorld;
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

	if (bNearPlayersOnly)
	{
		if (GClearedWorld.Get() != GetWorld())
		{
			GClearedUntil.Reset();
			GClearedWorld = GetWorld();
		}
		SetActorTickEnabled(true);
		return;
	}

	SpawnWhenGroundIsThere();
}

float AVaelCreatureSpawner::NearestPlayerDistance() const
{
	float Nearest = TNumericLimits<float>::Max();
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APawn* Player = It->Get() != nullptr ? It->Get()->GetPawn() : nullptr;
		if (Player != nullptr)
		{
			Nearest = FMath::Min(Nearest, static_cast<float>(FVector::Dist2D(Player->GetActorLocation(), GetActorLocation())));
		}
	}
	return Nearest;
}

void AVaelCreatureSpawner::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Distance = NearestPlayerDistance();
	const double Now = GetWorld()->GetTimeSeconds();
	const FString Key = GetPathName();

	if (!Group.IsEmpty())
	{
		int32 Alive = 0;
		for (const TWeakObjectPtr<AVaelCreature>& Creature : Group)
		{
			Alive += Creature.IsValid() && !Creature->IsDead() ? 1 : 0;
		}
		if (Alive == 0)
		{
			// Defeated: the place stays quiet for a while
			GClearedUntil.Add(Key, Now + RespawnSeconds);
			Group.Reset();
		}
		else if (Distance > SleepDistance)
		{
			for (const TWeakObjectPtr<AVaelCreature>& Creature : Group)
			{
				if (Creature.IsValid() && !Creature->IsDead())
				{
					Creature->Destroy();
				}
			}
			Group.Reset();
		}
		return;
	}

	const double* Until = GClearedUntil.Find(Key);
	// Wakes when someone comes near, but never right in front of them (the iso camera shows about 25 m round the group)
	if (Distance < WakeDistance && Distance > 2500.0f && (Until == nullptr || Now >= *Until))
	{
		SpawnGroup();
	}
}

void AVaelCreatureSpawner::SpawnGroup()
{
	UVaelCreatureData* Data = CreatureData != nullptr ? CreatureData.Get() : (CreatureKind != nullptr ? CreatureKind->GetDefaultObject<UVaelCreatureData>() : nullptr);
	AVaelGameMode* GameMode = GetWorld()->GetAuthGameMode<AVaelGameMode>();
	FVector Ground;
	if (Data == nullptr || GameMode == nullptr || !AVaelCreature::FindGround(GetWorld(), GetActorLocation(), Ground))
	{
		return;
	}
	TArray<AVaelCreature*> Spawned;
	GameMode->SpawnCreatureGroup(Data, GetActorLocation(), Count, Spread, true, GetWorld()->IsPartitionedWorld() ? GetLevel() : nullptr, &Spawned);
	for (int32 Index = 0; Index < Spawned.Num(); ++Index)
	{
		AVaelCreature* Creature = Spawned[Index];
		Creature->SetStrength(Strength);
		if (Index == 0 && EliteChance > 0.0f && FMath::FRand() < EliteChance)
		{
			Creature->SetElite();
		}
		Group.Add(Creature);
	}
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
