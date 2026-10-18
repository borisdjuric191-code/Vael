// Copyright Epic Games, Inc. All Rights Reserved.

#include "Floromant/VaelSeed.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Floromant/VaelFloromantComponent.h"
#include "Magic/VaelSpellEffects.h"
#include "Player/VaelCharacter.h"

namespace
{
	/** Height of the arc above the straight line, per metre of the throw, and at least */
	constexpr float SeedArcPerMetre = 22.0f;
	constexpr float SeedArcMin = 60.0f;
}

AVaelSeed::AVaelSeed()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(RootComponent);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(6.0f);
	Light->SetAttenuationRadius(220.0f);
	Light->SetCastShadows(false);

	PrimaryActorTick.bCanEverTick = true;
}

AVaelSeed* AVaelSeed::Throw(AVaelCharacter* InGardener, EVaelSeedKind InKind, const FVector& From, const FVector& GroundTarget)
{
	UWorld* World = InGardener != nullptr ? InGardener->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}

	const FTransform SpawnTransform(From);
	AVaelSeed* Seed = World->SpawnActorDeferred<AVaelSeed>(AVaelSeed::StaticClass(), SpawnTransform, InGardener, InGardener, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Seed != nullptr)
	{
		Seed->Kind = InKind;
		Seed->Gardener = InGardener;
		Seed->Start = From;
		Seed->Target = GroundTarget;
		Seed->ThrowYaw = (GroundTarget - From).Rotation().Yaw;
		Seed->FinishSpawning(SpawnTransform);
	}
	return Seed;
}

void AVaelSeed::BeginPlay()
{
	Super::BeginPlay();

	const FLinearColor Color = UVaelFloromantSettings::Get()->GetPlant(Kind).Color;
	Light->SetLightColor(Color);

	// A dark husk with a glowing kernel in the colour of its plant
	UMaterialInterface* CoreMaterial = nullptr;
	UMaterialInterface* GlowMaterial = nullptr;
	VaelEffects::LoadLookMaterials(CoreMaterial, GlowMaterial);

	UStaticMeshComponent* Husk = VaelEffects::AddLookShape(this, CoreMaterial, FLinearColor(0.09f, 0.06f, 0.04f), 0.0f);
	Husk->SetRelativeScale3D(FVector(0.12f, 0.08f, 0.08f));
	Shapes.Add(Husk);

	UStaticMeshComponent* Kernel = VaelEffects::AddLookShape(this, GlowMaterial, Color, 6.0f);
	Kernel->SetRelativeScale3D(FVector(0.16f));
	Shapes.Add(Kernel);

	for (UStaticMeshComponent* Shape : Shapes)
	{
		Shape->SetVisibility(true);
	}
}

void AVaelSeed::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Elapsed += DeltaSeconds;
	const float FlightTime = UVaelFloromantSettings::Get()->SeedFlightTime;
	const float Along = FMath::Clamp(Elapsed / FlightTime, 0.0f, 1.0f);

	// An arc: higher for longer throws; the seed spins as it flies
	const float Arc = FMath::Max(SeedArcMin, FVector::Dist2D(Start, Target) / 100.0f * SeedArcPerMetre);
	SetActorLocation(FMath::Lerp(Start, Target, Along) + FVector(0.0f, 0.0f, 4.0f * Arc * Along * (1.0f - Along)));
	SetActorRotation(FRotator(-Elapsed * 900.0f, ThrowYaw, 0.0f));

	if (Along >= 1.0f)
	{
		Land();
	}
}

void AVaelSeed::Land()
{
	if (AVaelCharacter* Thrower = Gardener.Get())
	{
		if (UVaelFloromantComponent* Floromant = Thrower->GetFloromant())
		{
			Floromant->PlantSeed(Kind, Target, ThrowYaw);
		}
	}

	Destroy();
}
