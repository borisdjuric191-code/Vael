// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelThistleSpider.h"
#include "Combat/VaelCombatStatics.h"
#include "Creatures/VaelCreatureData.h"
#include "Creatures/VaelLegIKComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Magic/VaelGameplayTags.h"
#include "Player/VaelCharacter.h"
#include "Vael.h"

namespace
{
	/** How much the thistle swells before it bursts */
	constexpr float BurstSwell = 0.18f;

	/** Seconds between two looks for seedlings to set on fire */
	constexpr float FireSpreadInterval = 0.4f;
}

// ---------------------------------------------------------------- Distelspinne

AVaelThistleSpider::AVaelThistleSpider()
{
	// The burst leaves no corpse to look at, the swarm is the show
	CorpseDuration = 0.2f;
}

TSubclassOf<UVaelCreatureData> AVaelThistleSpider::GetDefaultDataClass() const
{
	return UVaelThistleSpiderData::StaticClass();
}

void AVaelThistleSpider::BeginPlay()
{
	Super::BeginPlay();

	// Starts sunk into the undergrowth
	SetFactoryMeshOffset(FVector(0.0f, 0.0f, -GetData<UVaelThistleSpiderData>()->LurkSink));
}

int32 AVaelThistleSpider::GetLegCount() const
{
	return GetFactoryLegs() != nullptr ? GetFactoryLegs()->GetNumLegs() : 0;
}

int32 AVaelThistleSpider::GetStepCount() const
{
	return GetFactoryLegs() != nullptr ? GetFactoryLegs()->GetStepCount() : 0;
}

bool AVaelThistleSpider::IsHiddenFromAim() const
{
	return State == EVaelThistleState::Lurking;
}

void AVaelThistleSpider::OnDamageTaken(float Damage, const FGameplayTagContainer& DamageTags)
{
	Super::OnDamageTaken(Damage, DamageTags);

	if (IsDead())
	{
		return;
	}
	if (State == EVaelThistleState::Lurking)
	{
		State = EVaelThistleState::Rising;
		StateTime = 0.0f;
	}
	// Fire sets the dry thistle off at once
	if (DamageTags.HasTagExact(VaelTags::GetElementTag(EVaelElement::Fire)) && State != EVaelThistleState::Bursting)
	{
		StartBurst();
	}
}

void AVaelThistleSpider::StartBurst()
{
	// Soaked down sticks together: a wet thistle can't burst
	if (UVaelCombatStatics::HasStatus(this, EVaelStatus::Wet))
	{
		return;
	}
	State = EVaelThistleState::Bursting;
	StateTime = 0.0f;
}

void AVaelThistleSpider::TickBehavior(float DeltaSeconds)
{
	const UVaelThistleSpiderData* Data = GetData<UVaelThistleSpiderData>();
	StateTime += DeltaSeconds;
	BiteCooldown -= DeltaSeconds;

	switch (State)
	{
	case EVaelThistleState::Lurking:
	{
		float Distance = 0.0f;
		if (FindTarget(Data->WakeRange * GetTimeOfDayAwareness(), &Distance) != nullptr)
		{
			State = EVaelThistleState::Rising;
			StateTime = 0.0f;
		}
		break;
	}
	case EVaelThistleState::Rising:
	{
		const float Share = FMath::Clamp(StateTime / Data->RiseTime, 0.0f, 1.0f);
		SetFactoryMeshOffset(FVector(0.0f, 0.0f, -Data->LurkSink * (1.0f - FMath::SmoothStep(0.0f, 1.0f, Share))));
		if (Share >= 1.0f)
		{
			State = EVaelThistleState::Hunting;
			StateTime = 0.0f;
			HuntTime = 0.0f;
		}
		break;
	}
	case EVaelThistleState::Hunting:
	{
		float Distance = 0.0f;
		AActor* Target = FindTarget(Data->AggroRange * GetTimeOfDayAwareness(), &Distance);
		if (Target == nullptr)
		{
			break;
		}
		if (Distance < Data->BurstRange)
		{
			HuntTime += DeltaSeconds;
		}
		const bool bHurt = GetMaxHealth() > 0.0f && GetHealth() / GetMaxHealth() < Data->BurstHealthShare;
		if ((HuntTime >= Data->BurstDelay || bHurt) && !UVaelCombatStatics::HasStatus(this, EVaelStatus::Wet))
		{
			StartBurst();
			break;
		}
		if (Distance > Data->BiteRange * 0.8f)
		{
			MoveTowards(Target->GetActorLocation(), Data->MoveSpeed);
		}
		else
		{
			FaceTowards(Target->GetActorLocation());
		}
		if (Distance <= Data->BiteRange && BiteCooldown <= 0.0f && !IsBlinded())
		{
			HitPlayer(Target, Data->BiteDamage, EVaelElement::Earth);
			BiteCooldown = RandomInRange(Data->BiteInterval);
		}
		break;
	}
	case EVaelThistleState::Bursting:
	{
		// Swells and trembles, then bursts
		const float Share = FMath::Clamp(StateTime / Data->BurstWindup, 0.0f, 1.0f);
		const float Tremble = FMath::Sin(StateTime * 60.0f) * 2.0f * Share;
		SetFactoryMeshOffset(FVector(Tremble, 0.0f, 0.0f), 1.0f + BurstSwell * Share);
		if (Share >= 1.0f)
		{
			Burst();
		}
		break;
	}
	}
}

void AVaelThistleSpider::Burst()
{
	const UVaelThistleSpiderData* Data = GetData<UVaelThistleSpiderData>();
	UWorld* World = GetWorld();
	const bool bBurning = UVaelCombatStatics::HasStatus(this, EVaelStatus::Burning);

	// Seed down: everyone close is slowed
	int32 Players = 0;
	ForEachActivePlayer([&](AVaelCharacter* Player)
	{
		++Players;
		if (GetDistanceTo2D(Player) < Data->DownRange)
		{
			UVaelCombatStatics::ApplyStatus(this, Player, EVaelStatus::Slowed, Data->DownSlowTime);
		}
	});

	// The swarm runs out in a ring, more with more players
	UVaelCreatureData* SeedlingData = Data->SeedlingData != nullptr ? Data->SeedlingData.Get() : GetMutableDefault<UVaelThistleSeedlingData>();
	const int32 Count = FMath::RoundToInt(FMath::RandRange(Data->SeedlingCount.X, Data->SeedlingCount.Y) * (1.0f + Data->ExtraPlayerShare * FMath::Max(Players - 1, 0)));
	int32 Spawned = 0;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Angle = UE_TWO_PI * Index / Count + FMath::FRandRange(-0.2f, 0.2f);
		const FVector Spot = GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * FMath::FRandRange(40.0f, 130.0f);
		FVector Ground;
		if (!FindGround(World, Spot, Ground))
		{
			continue;
		}
		if (AVaelCreature* Seedling = SpawnCreature(World, SeedlingData, Ground, FRotator(0.0f, FMath::RadiansToDegrees(Angle), 0.0f)))
		{
			++Spawned;
			if (bBurning)
			{
				UVaelCombatStatics::ApplyStatus(this, Seedling, EVaelStatus::Burning, 3.0f, 4.0f);
			}
		}
	}
	UE_LOG(LogVael, Log, TEXT("'%s' bursts into %d seed spiders%s"), *GetName(), Spawned, bBurning ? TEXT(", burning") : TEXT(""));

	// What is left is an empty husk
	Die();
}

// ---------------------------------------------------------------- Distelsämling

AVaelThistleSeedling::AVaelThistleSeedling()
{
	CorpseDuration = 0.3f;
}

TSubclassOf<UVaelCreatureData> AVaelThistleSeedling::GetDefaultDataClass() const
{
	return UVaelThistleSeedlingData::StaticClass();
}

void AVaelThistleSeedling::BeginPlay()
{
	Super::BeginPlay();

	const float Spread = GetData<UVaelThistleSeedlingData>()->SwarmSpread;
	const float Angle = FMath::FRandRange(0.0f, UE_TWO_PI);
	SwarmOffset = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * FMath::FRandRange(0.3f, 1.0f) * Spread;
	BiteCooldown = FMath::FRandRange(0.2f, 0.8f);
	FireSpreadCountdown = FMath::FRandRange(0.0f, FireSpreadInterval);
}

void AVaelThistleSeedling::TickBehavior(float DeltaSeconds)
{
	const UVaelThistleSeedlingData* Data = GetData<UVaelThistleSeedlingData>();
	BiteCooldown -= DeltaSeconds;

	// A burning seedling sets its neighbours on fire: fire runs through the swarm
	FireSpreadCountdown -= DeltaSeconds;
	if (FireSpreadCountdown <= 0.0f)
	{
		FireSpreadCountdown = FireSpreadInterval;
		if (UVaelCombatStatics::HasStatus(this, EVaelStatus::Burning))
		{
			for (TActorIterator<AVaelThistleSeedling> It(GetWorld()); It; ++It)
			{
				if (*It != this && !It->IsDead() && GetDistanceTo2D(*It) < Data->FireSpreadRange
					&& !UVaelCombatStatics::HasStatus(*It, EVaelStatus::Burning))
				{
					UVaelCombatStatics::ApplyStatus(this, *It, EVaelStatus::Burning, Data->FireSpreadTime, Data->FireSpreadDamage);
				}
			}
		}
	}

	float Distance = 0.0f;
	AActor* Target = FindTarget(Data->AggroRange * GetTimeOfDayAwareness(), &Distance);
	if (Target == nullptr)
	{
		return;
	}
	// From its own side of the target until close, then straight in
	const FVector Goal = Distance > Data->BiteRange * 2.5f ? Target->GetActorLocation() + SwarmOffset : Target->GetActorLocation();
	if (Distance > Data->BiteRange * 0.7f)
	{
		MoveTowards(Goal, Data->MoveSpeed);
	}
	if (Distance <= Data->BiteRange && BiteCooldown <= 0.0f && !IsBlinded())
	{
		HitPlayer(Target, Data->BiteDamage, EVaelElement::Earth);
		BiteCooldown = RandomInRange(Data->BiteInterval);
	}
}
