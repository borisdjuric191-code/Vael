// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelOreShard.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Magic/VaelDebrisBurst.h"

AVaelOreShard::AVaelOreShard()
{
	PrimaryActorTick.bCanEverTick = true;
	Crystal = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Crystal"));
	Crystal->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Crystal->SetCanEverAffectNavigation(false);
	RootComponent = Crystal;
}

AVaelOreShard* AVaelOreShard::Throw(UWorld* World, const FVector& InStart, const FVector& InEnd, float InFlightTime, float InApex,
	UStaticMesh* Mesh, float InSize, EVaelShardImpact InImpact, const FLinearColor& InColor)
{
	if (World == nullptr || Mesh == nullptr)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVaelOreShard* Shard = World->SpawnActor<AVaelOreShard>(AVaelOreShard::StaticClass(), InStart, FRotator::ZeroRotator, Params);
	if (Shard == nullptr)
	{
		return nullptr;
	}
	Shard->Start = InStart;
	Shard->End = InEnd;
	Shard->FlightTime = FMath::Max(0.1f, InFlightTime);
	Shard->Apex = InApex;
	Shard->Size = InSize;
	Shard->Impact = InImpact;
	Shard->Color = InColor;
	Shard->Spin = FRotator(FMath::FRandRange(-900.0f, 900.0f), FMath::FRandRange(-600.0f, 600.0f), FMath::FRandRange(-400.0f, 400.0f));
	Shard->Crystal->SetStaticMesh(Mesh);
	Shard->Crystal->SetWorldScale3D(FVector(InSize / FMath::Max(1.0f, 2.0f * Mesh->GetBounds().SphereRadius)));
	return Shard;
}

void AVaelOreShard::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const float Alpha = FMath::Clamp(Age / FlightTime, 0.0f, 1.0f);
	const float Height = FMath::Max(Start.Z, End.Z) + Apex - FMath::Lerp(Start.Z, End.Z, Alpha);
	SetActorLocation(FMath::Lerp(Start, End, Alpha) + FVector(0.0f, 0.0f, 4.0f * Height * Alpha * (1.0f - Alpha)));
	SetActorRotation(Spin * Age);
	if (Alpha >= 1.0f)
	{
		ShowImpact(GetWorld(), End, Crystal->GetStaticMesh(), Size, Impact, Color);
		Destroy();
	}
}

void AVaelOreShard::ShowImpact(UWorld* World, const FVector& Location, UStaticMesh* Mesh, float InSize, EVaelShardImpact InImpact, const FLinearColor& InColor)
{
	// The crystal crumbles into a few small pieces of its own ore that tumble and lie still
	if (Mesh != nullptr)
	{
		FVaelDebris Crumbs;
		Crumbs.Meshes = {Mesh};
		Crumbs.Count = 5;
		Crumbs.Spread = 15.0f;
		Crumbs.Speed = 260.0f;
		Crumbs.Lift = 320.0f;
		Crumbs.Lifetime = 1.4f;
		Crumbs.MinSize = InSize * 0.2f;
		Crumbs.MaxSize = InSize * 0.4f;
		AVaelDebrisBurst::Spawn(World, Location + FVector(0.0f, 0.0f, 10.0f), Crumbs);
	}

	FVaelDebris Look;
	switch (InImpact)
	{
	case EVaelShardImpact::SulphurSparks:
	{
		// Yellow fumes ...
		Look.bSoft = true;
		Look.bDust = true;
		Look.Count = 7;
		Look.Spread = 45.0f;
		Look.Speed = 120.0f;
		Look.Lift = 90.0f;
		Look.Gravity = -60.0f;
		Look.Lifetime = 1.3f;
		Look.MinSize = 30.0f;
		Look.MaxSize = 55.0f;
		Look.Color = FLinearColor(0.85f, 0.80f, 0.25f);
		Look.Glow = 0.6f;
		AVaelDebrisBurst::Spawn(World, Location, Look);
		// ... that sparks set alight
		FVaelDebris Sparks;
		Sparks.Count = 14;
		Sparks.Spread = 30.0f;
		Sparks.Speed = 520.0f;
		Sparks.Lift = 420.0f;
		Sparks.Lifetime = 0.7f;
		Sparks.MinSize = 3.0f;
		Sparks.MaxSize = 6.0f;
		Sparks.Color = FLinearColor(1.0f, 0.45f, 0.08f);
		Sparks.Glow = 9.0f;
		AVaelDebrisBurst::Spawn(World, Location + FVector(0.0f, 0.0f, 20.0f), Sparks);
		return;
	}
	case EVaelShardImpact::RustSalt:
	{
		Look.bSoft = true;
		Look.bDust = true;
		Look.Count = 6;
		Look.Spread = 40.0f;
		Look.Speed = 140.0f;
		Look.Lift = 70.0f;
		Look.Lifetime = 1.0f;
		Look.MinSize = 25.0f;
		Look.MaxSize = 45.0f;
		Look.Color = FLinearColor(0.55f, 0.25f, 0.10f);
		AVaelDebrisBurst::Spawn(World, Location, Look);
		FVaelDebris Salt;
		Salt.Count = 12;
		Salt.Spread = 30.0f;
		Salt.Speed = 300.0f;
		Salt.Lift = 380.0f;
		Salt.Lifetime = 0.9f;
		Salt.MinSize = 2.5f;
		Salt.MaxSize = 5.0f;
		Salt.Color = FLinearColor(1.0f, 0.75f, 0.85f);
		Salt.Glow = 4.0f;
		AVaelDebrisBurst::Spawn(World, Location + FVector(0.0f, 0.0f, 15.0f), Salt);
		return;
	}
	case EVaelShardImpact::Lightning:
		Look.Count = 16;
		Look.Spread = 25.0f;
		Look.Speed = 700.0f;
		Look.Lift = 250.0f;
		Look.Gravity = 200.0f;
		Look.Drag = 4.0f;
		Look.Lifetime = 0.45f;
		Look.MinSize = 2.5f;
		Look.MaxSize = 6.0f;
		Look.Color = FLinearColor(0.45f, 0.95f, 1.0f);
		Look.Glow = 14.0f;
		Look.bLandOnGround = false;
		AVaelDebrisBurst::Spawn(World, Location + FVector(0.0f, 0.0f, 25.0f), Look);
		return;
	case EVaelShardImpact::Quicksilver:
		// Silver beads roll out and lie on the ground for a moment
		Look.Count = 12;
		Look.Spread = 20.0f;
		Look.Speed = 320.0f;
		Look.Lift = 160.0f;
		Look.Drag = 2.5f;
		Look.Lifetime = 1.8f;
		Look.MinSize = 3.0f;
		Look.MaxSize = 7.0f;
		Look.Color = FLinearColor(0.80f, 0.82f, 0.88f);
		Look.Glow = 1.2f;
		AVaelDebrisBurst::Spawn(World, Location + FVector(0.0f, 0.0f, 10.0f), Look);
		return;
	case EVaelShardImpact::Ribs:
		Look.Count = 8;
		Look.Spread = 20.0f;
		Look.Speed = 300.0f;
		Look.Lift = 350.0f;
		Look.Lifetime = 1.0f;
		Look.MinSize = 4.0f;
		Look.MaxSize = 9.0f;
		Look.Color = FLinearColor(0.95f, 0.88f, 0.75f);
		AVaelDebrisBurst::Spawn(World, Location + FVector(0.0f, 0.0f, 10.0f), Look);
		return;
	default:
		return;
	}
}
