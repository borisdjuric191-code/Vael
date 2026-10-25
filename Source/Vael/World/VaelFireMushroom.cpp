// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelFireMushroom.h"
#include "Engine/World.h"
#include "Magic/VaelSpellEffects.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "VaelAssets.h"
#include "World/VaelFireComponent.h"
#include "World/VaelFireSpot.h"
#include "World/VaelGround.h"

namespace
{
	/** How far below the burst the ground is searched for the patch, in cm */
	constexpr float MushroomGroundSearch = 600.0f;
}

AVaelFireMushroom::AVaelFireMushroom()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AVaelFireMushroom* AVaelFireMushroom::Spawn(UWorld* World, const FVector& Location, float BurstRadius)
{
	if (World == nullptr)
	{
		return nullptr;
	}

	// It rises from the ground below the burst, even if the ball burst in the air or against a wall
	FVector Ground = Location;
	FHitResult GroundHit;
	if (VaelGround::TraceGround(World, Location + FVector(0.0f, 0.0f, 50.0f), Location - FVector(0.0f, 0.0f, MushroomGroundSearch), GroundHit))
	{
		Ground = GroundHit.Location;
	}

	// One burning patch where it fell
	const UVaelFireSettings* Settings = UVaelFireSettings::Get();
	FVaelFireShape Patch;
	Patch.Radius = FMath::Max(BurstRadius, 60.0f) * Settings->MushroomPatchShare;
	Patch.Height = Patch.Radius * 2.0f;
	Patch.bScorch = true;
	AVaelFireSpot::SpawnOrJoin(World, Ground, Patch, Settings->MushroomPatchSeconds, false);

	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVaelFireMushroom* Mushroom = World->SpawnActor<AVaelFireMushroom>(AVaelFireMushroom::StaticClass(), FTransform(Ground), Parameters);
	if (Mushroom != nullptr)
	{
		Mushroom->Radius = FMath::Max(BurstRadius, 60.0f);
		Mushroom->SetLifeSpan(Settings->MushroomRiseSeconds + 1.0f);
	}
	return Mushroom;
}

void AVaelFireMushroom::Surge(float Height, float Size)
{
	const UVaelFireSettings* Settings = UVaelFireSettings::Get();
	if (UNiagaraComponent* Cloud = VaelEffects::SpawnAt(this, VaelAssets::LoadOptional(Settings->MushroomSmoke), GetActorLocation() + FVector(0.0f, 0.0f, Height),
		FRotator::ZeroRotator, Settings->MushroomSmokeColor, Size))
	{
		// Straight up, the wind only stirring it
		VaelFire::SetWind(Cloud, FVector(0.0f, 0.0f, 100.0f), Settings->MushroomWindSpeed);
	}
}

void AVaelFireMushroom::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Surges one after the other, each a little higher and wider: the cloud rears up
	const UVaelFireSettings* Settings = UVaelFireSettings::Get();
	Age += DeltaSeconds;
	const int32 Due = FMath::Min(Settings->MushroomSurges, 1 + FMath::FloorToInt(Age / FMath::Max(Settings->MushroomRiseSeconds / Settings->MushroomSurges, 0.05f)));
	for (; SurgesDone < Due; ++SurgesDone)
	{
		const float Share = Settings->MushroomSurges > 1 ? float(SurgesDone) / (Settings->MushroomSurges - 1) : 0.0f;
		Surge(Radius * Settings->MushroomHeightShare * Share, Radius * FMath::Lerp(0.8f, 1.15f, Share));
	}
}
