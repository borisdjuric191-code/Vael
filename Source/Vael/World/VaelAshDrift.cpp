// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelAshDrift.h"
#include "Creatures/VaelCreature.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Magic/VaelFlakes.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelSpellEffects.h"
#include "NiagaraSystem.h"
#include "VaelAssets.h"
#include "World/VaelRegion.h"
#include "Vael.h"

AVaelAshDrift::AVaelAshDrift()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
}

FVector AVaelAshDrift::DriftDirection(const FVector& Location, bool& bOutWindy) const
{
	const FVector Wind = AVaelRegion::GetWindAt(GetWorld(), Location);
	bOutWindy = Wind.Size2D() > 50.0f;
	if (bOutWindy)
	{
		return FVector(Wind.X, Wind.Y, 0.0f).GetSafeNormal();
	}
	return FVector(FMath::Cos(BreezeAngle), FMath::Sin(BreezeAngle), 0.0f);
}

void AVaelAshDrift::StartGust(const FVector& PlayerLocation, bool bWindy)
{
	bool bUnused = false;
	FVector Direction = DriftDirection(PlayerLocation, bUnused);
	Direction = Direction.RotateAngleAxis(FMath::FRandRange(-25.0f, 25.0f), FVector::UpVector);

	// Starts upwind of a spot near the player, so it blows past where they can see it
	const float Angle = FMath::FRandRange(0.0f, UE_TWO_PI);
	const FVector Spot = PlayerLocation + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * FMath::FRandRange(StartDistance.X, StartDistance.Y);
	const float Length = FMath::FRandRange(RunLength.X, RunLength.Y) * (bWindy ? 1.5f : 1.0f);
	FVector Ground;
	if (!AVaelCreature::FindGround(GetWorld(), Spot - Direction * Length * 0.5f + FVector(0.0f, 0.0f, 400.0f), Ground))
	{
		return;
	}
	FGust Gust;
	Gust.Location = Ground;
	Gust.Direction = Direction;
	Gust.Left = Length / RunSpeed;
	Gust.Width = FMath::FRandRange(100.0f, 220.0f) * (bWindy ? 1.4f : 1.0f);
	Gusts.Add(Gust);
	UE_LOG(LogVael, Display, TEXT("Ash gust at %s, %.0f m"), *Ground.ToCompactString(), Length / 100.0f);
}

void AVaelAshDrift::GustNow()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (const APawn* Player = It->Get() != nullptr ? It->Get()->GetPawn() : nullptr)
		{
			// Right beside the player, so the test sees it
			const FVector Saved = FVector(StartDistance.X, StartDistance.Y, 0.0f);
			StartDistance = FVector2D(150.0f, 400.0f);
			StartGust(Player->GetActorLocation(), true);
			StartDistance = FVector2D(Saved.X, Saved.Y);
			return;
		}
	}
}

void AVaelAshDrift::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UWorld* World = GetWorld();
	if (World == nullptr || !World->IsPartitionedWorld())
	{
		return;
	}
	if (Flakes == nullptr)
	{
		Flakes = NewObject<UVaelFlakesComponent>(this);
		Flakes->SetupAttachment(RootComponent);
		Flakes->RegisterComponent();
	}
	BreezeAngle += DeltaSeconds * 0.02f;

	// New gusts near the players
	NextGust -= DeltaSeconds;
	if (NextGust <= 0.0f)
	{
		bool bWindy = false;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APawn* Player = It->Get() != nullptr ? It->Get()->GetPawn() : nullptr;
			if (Player != nullptr && Gusts.Num() < 8)
			{
				DriftDirection(Player->GetActorLocation(), bWindy);
				StartGust(Player->GetActorLocation(), bWindy);
			}
		}
		const FVector2D Range = bWindy ? WindyInterval : CalmInterval;
		NextGust = FMath::FRandRange(Range.X, Range.Y);
	}

	// Running gusts: move along, leave low dust behind and send flakes skating
	UNiagaraSystem* Dust = VaelAssets::LoadOptional(UVaelMagicSettings::Get()->DustPuffEffect);
	// The soft cloud of the mist kit carries the lifted ash; the gravel puff adds grit at its foot
	UNiagaraSystem* Cloud = VaelAssets::LoadOptional(UVaelMagicSettings::Get()->SteamEffect);
	for (int32 Index = Gusts.Num() - 1; Index >= 0; --Index)
	{
		FGust& Gust = Gusts[Index];
		Gust.Left -= DeltaSeconds;
		if (Gust.Left <= 0.0f)
		{
			Gusts.RemoveAtSwap(Index);
			continue;
		}
		const FVector Wobble = FVector(-Gust.Direction.Y, Gust.Direction.X, 0.0f) * FMath::Sin(Gust.Left * 3.1f) * 60.0f * DeltaSeconds;
		Gust.Location += Gust.Direction * RunSpeed * DeltaSeconds + Wobble;
		FVector Ground;
		if (AVaelCreature::FindGround(World, Gust.Location + FVector(0.0f, 0.0f, 300.0f), Ground))
		{
			Gust.Location.Z = Ground.Z;
		}
		Gust.NextPuff -= DeltaSeconds;
		if (Gust.NextPuff <= 0.0f)
		{
			Gust.NextPuff = 0.16f;
			const FVector Side = FVector(-Gust.Direction.Y, Gust.Direction.X, 0.0f) * FMath::FRandRange(-Gust.Width, Gust.Width) * 0.5f;
			VaelEffects::SpawnAt(this, Dust, Gust.Location + Side, Gust.Direction.Rotation(), AshColor, Gust.Width * 0.4f);
			if (Cloud != nullptr && FMath::FRand() < 0.3f)
			{
				VaelEffects::SpawnAt(this, Cloud, Gust.Location + Side + FVector(0.0f, 0.0f, 20.0f), Gust.Direction.Rotation(), FLinearColor(AshColor.R * 1.15f, AshColor.G * 1.15f, AshColor.B * 1.15f, 0.28f), Gust.Width * 0.28f);
			}
		}
		// Flakes lifted off the ground, carried along low and fast, then settling again
		for (int32 Flake = 0; Flake < 3; ++Flake)
		{
			const FVector Side = FVector(-Gust.Direction.Y, Gust.Direction.X, 0.0f) * FMath::FRandRange(-Gust.Width, Gust.Width);
			const FVector Start = Gust.Location + Side + FVector(0.0f, 0.0f, FMath::FRandRange(5.0f, 40.0f));
			const FVector Velocity = Gust.Direction * FMath::FRandRange(RunSpeed * 0.7f, RunSpeed * 1.3f) + FVector(0.0f, 0.0f, FMath::FRandRange(40.0f, 140.0f));
			Flakes->Add(EVaelFlake::Ash, Start, Velocity, FMath::FRandRange(4.0f, 8.0f), Gust.Location.Z);
		}
	}
}
