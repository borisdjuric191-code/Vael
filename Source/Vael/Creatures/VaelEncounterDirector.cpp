// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelEncounterDirector.h"
#include "Components/BillboardComponent.h"
#include "Creatures/VaelCreature.h"
#include "Creatures/VaelCreatureData.h"
#include "Creatures/VaelEncounterTable.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Vael.h"
#include "World/VaelGround.h"
#include "World/VaelSafeZone.h"
#include "World/VaelTimeOfDay.h"

namespace
{
	/** Seconds between two looks at the group */
	constexpr float DirectorTickInterval = 0.25f;

	/** Jumps of the group longer than this (teleport, respawn at the camp) don't count as walking, in cm */
	constexpr float MaxStep = 1500.0f;

	/** Spots tried for one pack before it is given up */
	constexpr int32 SpotTries = 8;

	/** Keeps a pack this far from a safe zone, in cm */
	constexpr float SafeMargin = 800.0f;
}

AVaelEncounterDirector::AVaelEncounterDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = DirectorTickInterval;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

#if WITH_EDITORONLY_DATA
	UBillboardComponent* Icon = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Icon"));
	if (Icon != nullptr)
	{
		Icon->SetupAttachment(RootComponent);
	}
#endif
}

AVaelEncounterDirector* AVaelEncounterDirector::Get(const UWorld* World)
{
	if (World == nullptr)
	{
		return nullptr;
	}
	TActorIterator<AVaelEncounterDirector> It(const_cast<UWorld*>(World));
	return It ? *It : nullptr;
}

void AVaelEncounterDirector::BeginPlay()
{
	Super::BeginPlay();

	if (Table == nullptr)
	{
		UE_LOG(LogVael, Warning, TEXT("Encounter director '%s' has no table"), *GetName());
		SetActorTickEnabled(false);
		return;
	}
	NextPackAt = FMath::FRandRange(Table->DistancePerPack.X, Table->DistancePerPack.Y);
}

bool AVaelEncounterDirector::GetGroup(FVector& OutCentre, int32& OutPlayers) const
{
	FVector Sum = FVector::ZeroVector;
	OutPlayers = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APawn* Pawn = It->Get() != nullptr ? It->Get()->GetPawn() : nullptr;
		if (Pawn != nullptr)
		{
			Sum += Pawn->GetActorLocation();
			++OutPlayers;
		}
	}
	if (OutPlayers == 0)
	{
		return false;
	}
	OutCentre = Sum / OutPlayers;
	return true;
}

const FVaelEncounterPack* AVaelEncounterDirector::PickPack(float GroundZ, float Night) const
{
	float Total = 0.0f;
	TArray<TPair<const FVaelEncounterPack*, float>, TInlineAllocator<8>> Fitting;
	for (const FVaelEncounterPack& Pack : Table->Packs)
	{
		if (GroundZ < Pack.GroundHeight.X || GroundZ > Pack.GroundHeight.Y || (Pack.CreatureData == nullptr && Pack.CreatureKind == nullptr))
		{
			continue;
		}
		const float Weight = Pack.Weight * FMath::Lerp(1.0f, Pack.NightWeight, Night);
		if (Weight > 0.0f)
		{
			Fitting.Emplace(&Pack, Weight);
			Total += Weight;
		}
	}
	float Roll = FMath::FRand() * Total;
	for (const TPair<const FVaelEncounterPack*, float>& Entry : Fitting)
	{
		Roll -= Entry.Value;
		if (Roll <= 0.0f)
		{
			return Entry.Key;
		}
	}
	return Fitting.Num() > 0 ? Fitting.Last().Key : nullptr;
}

void AVaelEncounterDirector::CleanUp(const FVector& Centre)
{
	for (int32 Index = Spawned.Num() - 1; Index >= 0; --Index)
	{
		AVaelCreature* Creature = Spawned[Index].Get();
		// A bound or raised one belongs to the players now, not to the director
		if (Creature == nullptr || Creature->IsDead() || Creature->IsOnPlayerSide())
		{
			Spawned.RemoveAtSwap(Index);
			continue;
		}
		// Far from every player: out of sight and out of the fight, so it frees its place. Left behind the walking group
		// it goes sooner, else lurkers beside the way would fill the limit and no new pack would come (Boris' test 2026-10-28).
		float Nearest = UE_BIG_NUMBER;
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			const APawn* Pawn = It->Get() != nullptr ? It->Get()->GetPawn() : nullptr;
			if (Pawn != nullptr)
			{
				Nearest = FMath::Min(Nearest, static_cast<float>(FVector::Dist2D(Pawn->GetActorLocation(), Creature->GetActorLocation())));
			}
		}
		const FVector FromGroup = (Creature->GetActorLocation() - Centre) * FVector(1.0f, 1.0f, 0.0f);
		const bool bBehind = !Heading.IsNearlyZero() && FVector::DotProduct(FromGroup.GetSafeNormal(), Heading.GetSafeNormal()) < -0.2f;
		if (Nearest >= Table->DespawnDistance || (bBehind && Nearest >= Table->BehindDespawnDistance))
		{
			Creature->Destroy();
			Spawned.RemoveAtSwap(Index);
			continue;
		}
		Creature->ProwlTowards(Centre, Nearest);
	}
}

void AVaelEncounterDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	FVector Centre;
	int32 Players = 0;
	if (Table == nullptr || !GetGroup(Centre, Players))
	{
		return;
	}
	Age += DeltaSeconds;
	CleanUp(Centre);

	// Walking counts, standing still and jumps (teleport, respawn) don't
	if (bHasLastCentre)
	{
		const FVector Step = (Centre - LastCentre) * FVector(1.0f, 1.0f, 0.0f);
		if (Step.Size() < MaxStep)
		{
			const float Night = UVaelTimeOfDay::NightFactorOf(this);
			Walked += Step.Size() * FMath::Lerp(1.0f, Table->NightPace, Night);
			if (!Step.IsNearlyZero())
			{
				Heading = FMath::Lerp(Heading, Step.GetSafeNormal(), FMath::Clamp(DeltaSeconds * 1.5f, 0.0f, 1.0f));
			}
		}
	}
	LastCentre = Centre;
	bHasLastCentre = true;

	if (bEnabled && Age >= Table->StartGrace && Walked >= NextPackAt)
	{
		SpawnPackNow();
	}
}

int32 AVaelEncounterDirector::SpawnPackNow()
{
	FVector Centre;
	int32 Players = 0;
	if (Table == nullptr || !GetGroup(Centre, Players))
	{
		return 0;
	}
	Walked = 0.0f;
	NextPackAt = FMath::FRandRange(Table->DistancePerPack.X, Table->DistancePerPack.Y);

	const float GroupShare = 1.0f + Table->PerExtraPlayer * (Players - 1);
	const int32 Limit = FMath::RoundToInt(Table->MaxAlive * GroupShare);
	if (Spawned.Num() >= Limit)
	{
		return 0;
	}

	const float Night = UVaelTimeOfDay::NightFactorOf(this);
	const FVector Forward = Heading.IsNearlyZero() ? FVector(FMath::FRandRange(-1.0f, 1.0f), FMath::FRandRange(-1.0f, 1.0f), 0.0f).GetSafeNormal() : Heading.GetSafeNormal2D();
	// Ahead of the group, a little to the side, out of the camera's view
	auto TraceSpot = [&](float Cone, FHitResult& Hit)
	{
		const FVector Direction = Forward.RotateAngleAxis(FMath::FRandRange(-Cone, Cone), FVector::UpVector);
		const FVector Spot = Centre + Direction * FMath::FRandRange(Table->SpawnDistance.X, Table->SpawnDistance.Y);
		return VaelGround::TraceGround(GetWorld(), Spot + FVector(0.0f, 0.0f, 5000.0f), Spot - FVector(0.0f, 0.0f, 8000.0f), Hit)
			&& Hit.Location.Z >= Table->MinGroundHeight && Hit.ImpactNormal.Z >= Table->MinGroundNormalZ
			&& !AVaelSafeZone::IsSafe(GetWorld(), Hit.Location, SafeMargin);
	};
	for (int32 Try = 0; Try < SpotTries; ++Try)
	{
		FHitResult Hit;
		if (!TraceSpot(Table->SpawnCone, Hit))
		{
			continue;
		}
		const FVaelEncounterPack* Pack = PickPack(Hit.Location.Z, Night);
		if (Pack == nullptr)
		{
			continue;
		}
		UVaelCreatureData* Data = Pack->CreatureData != nullptr ? Pack->CreatureData.Get() : Pack->CreatureKind->GetDefaultObject<UVaelCreatureData>();

		// Lurkers lie right on the group's way, else it would walk past them and they would only fill the limit
		const AVaelCreature* Kind = Data->CreatureClass != nullptr ? Data->CreatureClass->GetDefaultObject<AVaelCreature>() : nullptr;
		if (Kind != nullptr && Kind->IsAmbusher()
			&& (!TraceSpot(Table->AmbushCone, Hit) || Hit.Location.Z < Pack->GroundHeight.X || Hit.Location.Z > Pack->GroundHeight.Y))
		{
			continue;
		}
		const int32 Count = FMath::Min(FMath::RoundToInt(FMath::RandRange(Pack->Count.X, Pack->Count.Y) * GroupShare), Limit - Spawned.Num());

		// Facing the group, so they look up when it comes
		const float FacingYaw = (Centre - Hit.Location).GetSafeNormal2D().Rotation().Yaw;
		int32 NumSpawned = 0;
		for (int32 Member = 0; Member < Count; ++Member)
		{
			const float Angle = UE_TWO_PI * Member / FMath::Max(Count - 1, 1) + FMath::FRandRange(-0.4f, 0.4f);
			const FVector Offset = Member == 0 ? FVector::ZeroVector : FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Pack->Spread * FMath::FRandRange(0.7f, 1.2f);
			FVector Ground;
			if (!AVaelCreature::FindGround(GetWorld(), Hit.Location + Offset, Ground) || Ground.Z < Table->MinGroundHeight)
			{
				continue;
			}
			AVaelCreature* Creature = AVaelCreature::SpawnCreature(GetWorld(), Data, Ground, FRotator(0.0f, FacingYaw + FMath::FRandRange(-40.0f, 40.0f), 0.0f));
			if (Creature != nullptr)
			{
				Spawned.Add(Creature);
				++NumSpawned;
			}
		}
		UE_LOG(LogVael, Log, TEXT("Encounter: %d x %s %.0f m ahead of the group (%d of the director alive)"), NumSpawned, *GetNameSafe(Data),
		       FVector::Dist2D(Centre, Hit.Location) / 100.0f, Spawned.Num());
		return NumSpawned;
	}
	return 0;
}
