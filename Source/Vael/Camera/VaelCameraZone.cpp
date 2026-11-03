// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/VaelCameraZone.h"
#include "Components/SceneComponent.h"

AVaelCameraZone::AVaelCameraZone()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	PrimaryActorTick.bCanEverTick = false;
}

float AVaelCameraZone::GetWeight(const FVector& Location) const
{
	const float Away = FVector::Dist2D(Location, GetActorLocation());
	if (Away <= Radius)
	{
		return 1.0f;
	}
	return EdgeWidth > 0.0f ? FMath::SmoothStep(0.0f, 1.0f, 1.0f - FMath::Clamp((Away - Radius) / EdgeWidth, 0.0f, 1.0f)) : 0.0f;
}
