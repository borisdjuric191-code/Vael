// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelFireLight.h"
#include "Components/PointLightComponent.h"

AVaelFireLight::AVaelFireLight()
{
	PrimaryActorTick.bCanEverTick = true;

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	RootComponent = Light;
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(BaseIntensity);
	Light->SetLightColor(FLinearColor(1.0f, 0.55f, 0.25f));
	Light->SetAttenuationRadius(900.0f);
	Light->SetSourceRadius(30.0f);
}

void AVaelFireLight::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	Light->SetIntensity(BaseIntensity);
}

void AVaelFireLight::BeginPlay()
{
	Super::BeginPlay();

	NoiseTime = FMath::FRandRange(0.0f, 1000.0f);
}

void AVaelFireLight::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (FlickerStrength <= 0.0f)
	{
		return;
	}

	// Two layers of noise: a slow breathing of the flames and a quick crackle on top
	NoiseTime += DeltaSeconds * FlickerSpeed;
	const float Slow = FMath::PerlinNoise1D(NoiseTime * 0.37f);
	const float Quick = FMath::PerlinNoise1D(NoiseTime * 1.9f + 51.3f);
	const float Swing = FMath::Clamp(Slow * 0.7f + Quick * 0.3f, -1.0f, 1.0f);

	Light->SetIntensity(BaseIntensity * FMath::Max(0.0f, 1.0f + Swing * FlickerStrength));
}
