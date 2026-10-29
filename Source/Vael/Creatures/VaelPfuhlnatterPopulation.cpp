// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelPfuhlnatterPopulation.h"
#include "Creatures/VaelCreatureData.h"
#include "Creatures/VaelPfuhlnatter.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Vael.h"

AVaelPfuhlnatterPopulation::AVaelPfuhlnatterPopulation()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 2.0f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AVaelPfuhlnatterPopulation::BeginPlay()
{
	Super::BeginPlay();
	if (Nests.IsEmpty())
	{
		Nests.Add(GetActorLocation());
	}
	NextBreed = BreedInterval;
}

int32 AVaelPfuhlnatterPopulation::GetSnakeCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AVaelPfuhlnatter>& Snake : Snakes)
	{
		Count += Snake.IsValid() && !Snake->IsDead() ? 1 : 0;
	}
	return Count;
}

int32 AVaelPfuhlnatterPopulation::GetGrownCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AVaelPfuhlnatter>& Snake : Snakes)
	{
		Count += Snake.IsValid() && !Snake->IsDead() && Snake->IsElite() ? 1 : 0;
	}
	return Count;
}

int32 AVaelPfuhlnatterPopulation::FindHiddenNest(int32 Preferred) const
{
	TArray<FVector> Players;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (const APawn* Pawn = It->IsValid() ? (*It)->GetPawn() : nullptr)
		{
			Players.Add(Pawn->GetActorLocation());
		}
	}
	for (int32 Step = 0; Step < Nests.Num(); ++Step)
	{
		const int32 Nest = (Preferred + Step) % Nests.Num();
		bool bSeen = false;
		for (const FVector& Player : Players)
		{
			bSeen |= FVector::Dist2D(Player, Nests[Nest]) < HiddenDistance;
		}
		if (!bSeen)
		{
			return Nest;
		}
	}
	return INDEX_NONE;
}

bool AVaelPfuhlnatterPopulation::Hatch(float Age, int32 Nest)
{
	if (GetSnakeCount() >= MaxCount || !Nests.IsValidIndex(Nest))
	{
		return false;
	}
	const FVector Spot = Nests[Nest] + FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f).Vector() * FMath::FRandRange(0.0f, 400.0f);
	FVector Ground;
	if (!AVaelCreature::FindGround(GetWorld(), Spot, Ground))
	{
		Ground = Nests[Nest];
	}
	AVaelCreature* Creature = AVaelCreature::SpawnCreature(GetWorld(), GetMutableDefault<UVaelPfuhlnatterData>(), Ground,
		FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), nullptr, [Age](AVaelCreature* New)
		{
			if (AVaelPfuhlnatter* Young = Cast<AVaelPfuhlnatter>(New))
			{
				Young->AgeDays = Age;
			}
		});
	if (AVaelPfuhlnatter* Snake = Cast<AVaelPfuhlnatter>(Creature))
	{
		Snakes.Add(Snake);
		return true;
	}
	return false;
}

void AVaelPfuhlnatterPopulation::BreedNow()
{
	const int32 Grown = GetGrownCount();
	int32 Young = 0;
	for (int32 Mother = 0; Mother < Grown; ++Mother)
	{
		if (FMath::FRand() >= BroodChance)
		{
			continue;
		}
		const int32 Nest = FindHiddenNest(FMath::RandHelper(Nests.Num()));
		const int32 Size = FMath::RandRange(BroodSize.X, BroodSize.Y);
		for (int32 Index = 0; Index < Size && Nest != INDEX_NONE; ++Index)
		{
			Young += Hatch(HatchAge, Nest) ? 1 : 0;
		}
	}
	UE_LOG(LogVael, Log, TEXT("Pfuhlnattern of %s: %d grown, %d young hatched, %d snakes now"), *GetActorNameOrLabel(), Grown, Young, GetSnakeCount());
}

void AVaelPfuhlnatterPopulation::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bStarted)
	{
		// Once the ground has loaded: the moor's first snakes, spread over the nests
		FVector Ground;
		if (!AVaelCreature::FindGround(GetWorld(), Nests[0], Ground))
		{
			return;
		}
		bStarted = true;
		const FVector2D Grown = GetDefault<UVaelPfuhlnatterData>()->GrownAge;
		for (int32 Index = 0; Index < StartCount; ++Index)
		{
			Hatch(Index < StartGrown ? FMath::FRandRange(Grown.X, Grown.Y) : -1.0f, Index % Nests.Num());
		}
		UE_LOG(LogVael, Log, TEXT("Pfuhlnattern of %s: %d snakes at %d nests, %d grown"), *GetActorNameOrLabel(), GetSnakeCount(), Nests.Num(), GetGrownCount());
		return;
	}

	Snakes.RemoveAll([](const TWeakObjectPtr<AVaelPfuhlnatter>& Snake) { return !Snake.IsValid(); });
	NextBreed -= DeltaSeconds;
	if (NextBreed <= 0.0f)
	{
		NextBreed = BreedInterval;
		BreedNow();
	}
}
