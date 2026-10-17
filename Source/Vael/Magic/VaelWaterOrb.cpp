// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelWaterOrb.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Magic/VaelElementSource.h"
#include "NiagaraComponent.h"
#include "UObject/ConstructorHelpers.h"

const FName AVaelWaterOrb::WaterColorParameter(TEXT("User.WaterColor"));

AVaelWaterOrb::AVaelWaterOrb()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	Pivot->SetupAttachment(RootComponent);

	// The engine sphere is 1 m across
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Ball = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ball"));
	Ball->SetupAttachment(Pivot);
	Ball->SetStaticMesh(SphereMesh.Object);
	Ball->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ball->SetCastShadow(false);

	Particles = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Particles"));
	Particles->SetupAttachment(Pivot);

	// Ball and particles share their middle, so the see-through ball would often be drawn over its own bubbles
	Particles->TranslucencySortPriority = 1;

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Pivot);
	Glow->SetAttenuationRadius(400.0f);
	Glow->SetCastShadows(false);
	Glow->SetIntensity(GlowIntensity);
}

void AVaelWaterOrb::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplySource();
}

void AVaelWaterOrb::SetSource(UVaelElementSource* NewSource)
{
	Source = NewSource;
	ApplySource();
}

void AVaelWaterOrb::ApplySource()
{
	Glow->SetIntensity(GlowIntensity);

	if (Source == nullptr)
	{
		return;
	}

	if (Source->Material != nullptr)
	{
		Ball->SetMaterial(0, Source->Material);
	}

	Glow->SetLightColor(Source->GlowColor);
	Particles->SetVariableLinearColor(WaterColorParameter, Source->ParticleColor);
}

void AVaelWaterOrb::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Thick water moves slower
	const float Speed = 1.0f - 0.6f * (Source != nullptr ? Source->Viscosity : 0.0f);
	MotionTime += DeltaSeconds * Speed;

	const float Bob = FMath::Sin(MotionTime * BobFrequency * UE_TWO_PI) * BobHeight;
	Pivot->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, Bob), FRotator(0.0f, MotionTime * SpinSpeed, 0.0f));
}
