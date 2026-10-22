// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelFireSpot.h"
#include "Engine/World.h"
#include "Vael.h"

AVaelFireSpot::AVaelFireSpot()
{
	PrimaryActorTick.bCanEverTick = true;

	Fire = CreateDefaultSubobject<UVaelFireComponent>(TEXT("Fire"));
	RootComponent = Fire;

	// A campfire: a broad bed of flames, sparks and light
	Shape.Radius = 45.0f;
	Shape.Height = 110.0f;
	Shape.bScorch = false;
}

AVaelFireSpot* AVaelFireSpot::Spawn(UWorld* World, const FVector& Location, const FVaelFireShape& InShape, float InBurnTime)
{
	if (World == nullptr)
	{
		return nullptr;
	}
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Parameters.bDeferConstruction = true;
	AVaelFireSpot* Spot = World->SpawnActor<AVaelFireSpot>(AVaelFireSpot::StaticClass(), FTransform(Location), Parameters);
	if (Spot != nullptr)
	{
		Spot->Shape = InShape;
		Spot->BurnTime = InBurnTime;
		Spot->FinishSpawning(FTransform(Location));
	}
	return Spot;
}

AVaelFireSpot* AVaelFireSpot::SpawnBurst(UWorld* World, const FVector& Location, float Radius)
{
	FVaelFireShape Shape;
	Shape.Radius = Radius * 0.55f;
	Shape.Height = Radius * 1.3f;
	Shape.Heat = 1.5f;
	Shape.Smoke = 0.9f;
	Shape.Speed = 2.4f;
	Shape.Tongues = 18;
	Shape.bFloating = true;
	AVaelFireSpot* Spot = Spawn(World, Location, Shape, 0.6f);
	if (Spot != nullptr)
	{
		Spot->bBurst = true;
		Spot->Fire->SetSizeScale(0.3f);
		UE_LOG(LogVael, Verbose, TEXT("Fire burst of %.0f cm at %s, strength %.2f"), Radius, *Location.ToCompactString(), Spot->Fire->GetStrength());
	}
	return Spot;
}

void AVaelFireSpot::BeginPlay()
{
	Super::BeginPlay();
	Fire->Ignite(Shape, 0.35f);
}

void AVaelFireSpot::PutOut(float FadeOut)
{
	if (!bPutOut)
	{
		bPutOut = true;
		Fire->Extinguish(FadeOut, false);
	}
}

void AVaelFireSpot::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;

	// A blast blooms out fast and keeps growing a little while it dies
	if (bBurst)
	{
		Fire->SetSizeScale(0.3f + 1.1f * FMath::Sqrt(FMath::Clamp(Age / 0.6f, 0.0f, 1.0f)));
	}
	if (BurnTime > 0.0f && Age >= BurnTime)
	{
		PutOut();
	}

	// Gone once the flames are out and the scorch mark has cooled
	if (bPutOut && !Fire->IsBurning() && Age > BurnTime + UVaelFireSettings::Get()->ScorchLinger + 1.0f)
	{
		Destroy();
	}
}
