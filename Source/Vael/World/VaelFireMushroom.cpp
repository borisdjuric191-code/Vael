// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelFireMushroom.h"
#include "Engine/World.h"
#include "Magic/VaelSpellEffects.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "VaelAssets.h"
#include "World/VaelFireComponent.h"

AVaelFireMushroom::AVaelFireMushroom()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AVaelFireMushroom* AVaelFireMushroom::Spawn(UWorld* World, const FVector& Location, float BurstRadius)
{
	if (World == nullptr || UVaelFireSettings::Get()->MushroomSmoke.IsNull())
	{
		return nullptr;
	}
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVaelFireMushroom* Mushroom = World->SpawnActor<AVaelFireMushroom>(AVaelFireMushroom::StaticClass(), FTransform(Location), Parameters);
	if (Mushroom != nullptr)
	{
		Mushroom->Radius = FMath::Max(BurstRadius, 60.0f);
		Mushroom->SetLifeSpan(UVaelFireSettings::Get()->MushroomRiseSeconds + 1.5f);
	}
	return Mushroom;
}

void AVaelFireMushroom::Puff(const FVector& Place, const FVector& Wind, float Size, bool bBurning)
{
	const UVaelFireSettings* Settings = UVaelFireSettings::Get();
	UNiagaraComponent* Smoke = VaelEffects::SpawnAt(this, VaelAssets::LoadOptional(Settings->MushroomSmoke), Place, FRotator::ZeroRotator, Settings->MushroomSmokeColor, Size);
	if (Smoke == nullptr)
	{
		return;
	}
	VaelFire::SetWind(Smoke, Wind, Settings->MushroomWindSpeed);
	if (!bBurning)
	{
		// The cap is only smoke: its flames stay off
		for (const TCHAR* Flame : { TEXT("Main_Fire"), TEXT("Small_Fire"), TEXT("Small_Fire001"), TEXT("Sparks"), TEXT("Fire_Light") })
		{
			Smoke->SetEmitterEnable(Flame, false);
		}
	}
}

void AVaelFireMushroom::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const UVaelFireSettings* Settings = UVaelFireSettings::Get();
	Age += DeltaSeconds;
	const float Height = Radius * Settings->MushroomHeightShare;
	const float Rise = FMath::Clamp(Age / Settings->MushroomRiseSeconds, 0.0f, 1.0f);
	const FVector Base = GetActorLocation();

	// The stem: the head climbs fast and slows near the top, leaving burning smoke behind it that rises on
	NextStemCountdown -= DeltaSeconds;
	if (Rise < 1.0f && NextStemCountdown <= 0.0f)
	{
		NextStemCountdown = Settings->MushroomPuffInterval;
		const float Up = 1.0f - FMath::Square(1.0f - Rise);
		const FVector Head = Base + FVector(FMath::FRandRange(-0.08f, 0.08f) * Radius, FMath::FRandRange(-0.08f, 0.08f) * Radius, Up * Height);
		Puff(Head, FVector(0.0f, 0.0f, 100.0f), Radius * FMath::Lerp(0.5f, 0.38f, Up), Up < 0.55f);
	}

	// The cap: rings of smoke roll out from the top and curl down, each wider than the one before
	const int32 RingsDue = Rise < 0.75f ? 0 : FMath::Min(Settings->MushroomCapRings, 1 + FMath::FloorToInt((Age - Settings->MushroomRiseSeconds * 0.75f) / 0.12f));
	for (; CapRingsDone < RingsDue; ++CapRingsDone)
	{
		const float Spread = Radius * (0.2f + 0.25f * CapRingsDone);
		const int32 NumPuffs = 5 + CapRingsDone * 2;
		const float Turn = FMath::FRandRange(0.0f, UE_TWO_PI);
		for (int32 Index = 0; Index < NumPuffs; ++Index)
		{
			const float Angle = Turn + Index * UE_TWO_PI / NumPuffs;
			const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
			const FVector Place = Base + Out * Spread + FVector(0.0f, 0.0f, Height - CapRingsDone * Radius * 0.12f);
			Puff(Place, (Out - FVector(0.0f, 0.0f, 0.35f + 0.15f * CapRingsDone)).GetSafeNormal() * 100.0f, Radius * 0.5f, false);
		}
	}
}
