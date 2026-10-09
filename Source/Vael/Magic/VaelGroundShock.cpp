// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelGroundShock.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Magic/VaelDebrisBurst.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelSpellEffects.h"
#include "VaelAssets.h"

namespace
{
	/** Diameter of the engine sphere in cm */
	constexpr float ShockSphereSize = 100.0f;

	/** The quake: seconds its front needs to the edge, rocks breaking out, their height and width in cm */
	constexpr float QuakeFrontTime = 0.45f;
	constexpr int32 QuakeRockCount = 16;
	constexpr float QuakeRockMinHeight = 35.0f;
	constexpr float QuakeRockMaxHeight = 75.0f;
	constexpr float QuakeRockMinWidth = 40.0f;
	constexpr float QuakeRockMaxWidth = 80.0f;

	/** Each rock shoots up within this time, stands, then sinks back, in seconds */
	constexpr float QuakeRockRise = 0.1f;
	constexpr float QuakeRockStand = 0.25f;
	constexpr float QuakeRockSink = 0.3f;

	/** The pulse of Mark: seconds its dome needs to the edge, the dome's height as share of its width, the flash in candela */
	constexpr float PulseFrontTime = 0.35f;
	constexpr float PulseDomeHeight = 0.6f;
	constexpr float PulseFlash = 40.0f;

	/** The black hole opens to this diameter in cm within this time and closes again by that time */
	constexpr float PulseHoleSize = 120.0f;
	constexpr float PulseHoleOpen = 0.1f;
	constexpr float PulseHoleClose = 0.5f;
}

AVaelGroundShock::AVaelGroundShock()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	PrimaryActorTick.bCanEverTick = true;
}

AVaelGroundShock* AVaelGroundShock::Spawn(AActor* Caster, const FVector& Feet, float Radius, EVaelNovaLook Look, const FLinearColor& Color)
{
	UWorld* World = Caster != nullptr ? Caster->GetWorld() : nullptr;
	if (World == nullptr || Look == EVaelNovaLook::Plain)
	{
		return nullptr;
	}

	const FTransform SpawnTransform(Feet);

	AVaelGroundShock* Shock = World->SpawnActorDeferred<AVaelGroundShock>(AVaelGroundShock::StaticClass(), SpawnTransform, Caster, Cast<APawn>(Caster), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Shock != nullptr)
	{
		Shock->Look = Look;
		Shock->Radius = FMath::Max(Radius, 50.0f);
		Shock->ShockColor = Color;
		Shock->FinishSpawning(SpawnTransform);
	}

	return Shock;
}

void AVaelGroundShock::BeginPlay()
{
	Super::BeginPlay();

	if (Look == EVaelNovaLook::Quake)
	{
		BuildQuake();
	}
	else
	{
		BuildMarkPulse();
	}

	Tick(0.0f);
}

float AVaelGroundShock::GetFrontDistance(float Time) const
{
	const float Progress = FMath::Clamp(Time / FrontTime, 0.0f, 1.0f);
	return Radius * (1.0f - FMath::Square(1.0f - Progress));
}

float AVaelGroundShock::GetFrontArrival(float Distance) const
{
	return (1.0f - FMath::Sqrt(FMath::Max(0.0f, 1.0f - Distance / Radius))) * FrontTime;
}

void AVaelGroundShock::BuildQuake()
{
	FrontTime = QuakeFrontTime;
	LookTime = FrontTime + QuakeRockRise + QuakeRockStand + QuakeRockSink;

	UMaterialInterface* CoreMaterial = nullptr;
	UMaterialInterface* GlowMaterial = nullptr;
	VaelEffects::LoadLookMaterials(CoreMaterial, GlowMaterial);

	// A ring of dust rolls over the ground
	GroundRing = VaelEffects::AddLookShape(this, GlowMaterial, FLinearColor(0.5f, 0.42f, 0.3f), 0.6f, 1.0f);

	// Rocks of the earth orb break out where the ring passes; dark spheres stand in for them while the pack isn't installed
	UStaticMesh* RockMesh = VaelAssets::LoadOptional(UVaelMagicSettings::Get()->ElementOrbRockMesh);
	const FBoxSphereBounds Bounds = RockMesh != nullptr ? RockMesh->GetBounds() : FBoxSphereBounds(FVector::ZeroVector, FVector(ShockSphereSize * 0.5f), ShockSphereSize * 0.5f);
	const FVector MeshSize = FVector::Max(Bounds.BoxExtent * 2.0f, FVector(1.0f));

	for (int32 RockIndex = 0; RockIndex < QuakeRockCount; ++RockIndex)
	{
		UStaticMeshComponent* Rock = VaelEffects::AddLookShape(this, CoreMaterial, ShockColor * 0.35f, 0.0f);
		if (RockMesh != nullptr)
		{
			Rock->SetStaticMesh(RockMesh);
			Rock->EmptyOverrideMaterials();
		}

		const float Angle = FMath::FRandRange(0.0f, UE_TWO_PI);
		const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
		const float Distance = Radius * FMath::Lerp(0.15f, 0.95f, FMath::Sqrt(FMath::FRand()));
		const float Height = FMath::FRandRange(QuakeRockMinHeight, QuakeRockMaxHeight);
		const float Width = FMath::FRandRange(QuakeRockMinWidth, QuakeRockMaxWidth);

		// Jagged: tilted outwards, partly still in the ground
		const FRotator Rotation = FRotator(FMath::FRandRange(-25.0f, 25.0f), FMath::FRandRange(0.0f, 360.0f), FMath::FRandRange(-25.0f, 25.0f));
		const FVector Scale = FVector(Width, Width * FMath::FRandRange(0.7f, 1.0f), Height) / MeshSize;
		const FVector Center = Out * Distance + FVector(0.0f, 0.0f, Height * 0.3f);

		Rock->SetRelativeRotation(Rotation);
		Rock->SetRelativeScale3D(Scale);

		Rocks.Add(Rock);
		RockPlaces.Add(Center - Rotation.RotateVector(Scale * Bounds.Origin));
		RockHeights.Add(Height);
		RockArrivals.Add(GetFrontArrival(Distance));
		RockBurst.Add(false);
	}
}

void AVaelGroundShock::BuildMarkPulse()
{
	FrontTime = PulseFrontTime;
	LookTime = FMath::Max(FrontTime, PulseHoleClose) + 0.1f;

	UMaterialInterface* CoreMaterial = nullptr;
	UMaterialInterface* GlowMaterial = nullptr;
	VaelEffects::LoadLookMaterials(CoreMaterial, GlowMaterial);

	const FLinearColor Void(0.02f, 0.0f, 0.04f);

	Hole = VaelEffects::AddLookShape(this, CoreMaterial, Void, 0.0f);
	Dome = VaelEffects::AddLookShape(this, GlowMaterial, ShockColor, 2.5f, 1.0f);
	GroundRing = VaelEffects::AddLookShape(this, GlowMaterial, FMath::Lerp(ShockColor, FLinearColor::White, 0.2f), 3.0f, 1.0f);

	Flash = NewObject<UPointLightComponent>(this);
	Flash->SetupAttachment(RootComponent);
	Flash->SetRelativeLocation(FVector(0.0f, 0.0f, 80.0f));
	Flash->SetMobility(EComponentMobility::Movable);
	Flash->SetIntensityUnits(ELightUnits::Candelas);
	Flash->SetLightColor(ShockColor);
	Flash->SetAttenuationRadius(Radius * 1.2f);
	Flash->SetCastShadows(false);
	Flash->RegisterComponent();

	// Black shards and violet motes are torn from the ground and rise
	FVaelDebris Shards;
	Shards.Count = 18;
	Shards.Spread = Radius * 0.3f;
	Shards.Speed = 220.0f;
	Shards.Lift = 120.0f;
	Shards.Gravity = -260.0f;
	Shards.Drag = 1.2f;
	Shards.Lifetime = 0.9f;
	Shards.Color = Void;
	Shards.bLandOnGround = false;
	AVaelDebrisBurst::Spawn(this, GetActorLocation(), Shards);

	FVaelDebris Motes;
	Motes.Count = 14;
	Motes.Spread = Radius * 0.3f;
	Motes.Speed = 300.0f;
	Motes.Lift = 150.0f;
	Motes.Gravity = -200.0f;
	Motes.Drag = 1.5f;
	Motes.Lifetime = 0.8f;
	Motes.MinSize = 3.0f;
	Motes.MaxSize = 6.0f;
	Motes.Color = ShockColor;
	Motes.Glow = 5.0f;
	Motes.bLandOnGround = false;
	AVaelDebrisBurst::Spawn(this, GetActorLocation() + FVector(0.0f, 0.0f, 20.0f), Motes);
}

void AVaelGroundShock::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Elapsed += DeltaSeconds;

	if (Elapsed >= LookTime)
	{
		Destroy();
		return;
	}

	if (Look == EVaelNovaLook::Quake)
	{
		AnimateQuake();
	}
	else
	{
		AnimateMarkPulse();
	}
}

void AVaelGroundShock::AnimateQuake()
{
	// The dust ring runs out over the ground and thins
	const float Progress = Elapsed / FrontTime;
	const float Front = GetFrontDistance(Elapsed);

	GroundRing->SetVisibility(Progress < 1.2f);
	GroundRing->SetRelativeScale3D(FVector(Front * 2.0f, Front * 2.0f, 20.0f) / ShockSphereSize);
	VaelEffects::SetLookGlow(GroundRing, 0.6f * FMath::Max(0.0f, 1.0f - Progress / 1.2f));

	for (int32 RockIndex = 0; RockIndex < Rocks.Num(); ++RockIndex)
	{
		const float Age = Elapsed - RockArrivals[RockIndex];
		const bool bVisible = Age > 0.0f && Age < QuakeRockRise + QuakeRockStand + QuakeRockSink;

		UStaticMeshComponent* Rock = Rocks[RockIndex];
		Rock->SetVisibility(bVisible);
		if (!bVisible)
		{
			continue;
		}

		// Dust and pebbles burst the moment the rock breaks out
		if (!RockBurst[RockIndex])
		{
			RockBurst[RockIndex] = true;

			FVaelDebris Dust;
			Dust.Count = 5;
			Dust.Spread = 25.0f;
			Dust.Speed = 200.0f;
			Dust.Lift = 320.0f;
			Dust.MaxSize = 10.0f;
			Dust.MinSize = 5.0f;
			Dust.RockShare = 0.2f;
			AVaelDebrisBurst::Spawn(this, GetActorTransform().TransformPosition(RockPlaces[RockIndex] * FVector(1.0f, 1.0f, 0.0f)), Dust);
		}

		// Shoots up past its height, stands, sinks back into the ground
		float Out = 1.0f;
		if (Age < QuakeRockRise)
		{
			const float Back = Age / QuakeRockRise - 1.0f;
			Out = 1.0f + 2.7f * Back * Back * Back + 1.7f * Back * Back;
		}
		else if (Age > QuakeRockRise + QuakeRockStand)
		{
			Out = 1.0f - FMath::Square((Age - QuakeRockRise - QuakeRockStand) / QuakeRockSink);
		}

		Rock->SetRelativeLocation(RockPlaces[RockIndex] - FVector(0.0f, 0.0f, RockHeights[RockIndex] * 1.1f * (1.0f - Out)));
	}
}

void AVaelGroundShock::AnimateMarkPulse()
{
	const float Progress = FMath::Clamp(Elapsed / FrontTime, 0.0f, 1.0f);
	const float Front = GetFrontDistance(Elapsed);
	const float Fade = 1.0f - FMath::SmoothStep(0.5f, 1.0f, Progress);

	// The dome swells from the feet, the ring on the ground runs just ahead of it
	Dome->SetVisibility(Progress < 1.0f);
	Dome->SetRelativeScale3D(FVector(Front * 2.0f, Front * 2.0f, Front * 2.0f * PulseDomeHeight) / ShockSphereSize);
	VaelEffects::SetLookGlow(Dome, 2.5f * Fade);

	GroundRing->SetVisibility(Progress < 1.0f);
	GroundRing->SetRelativeScale3D(FVector(Front * 2.04f, Front * 2.04f, 12.0f) / ShockSphereSize);
	VaelEffects::SetLookGlow(GroundRing, 3.0f * Fade);

	// The black hole tears open at once and closes slowly
	const float Opening = Elapsed < PulseHoleOpen
		? Elapsed / PulseHoleOpen
		: 1.0f - FMath::Clamp((Elapsed - PulseHoleOpen) / (PulseHoleClose - PulseHoleOpen), 0.0f, 1.0f);

	Hole->SetVisibility(Opening > 0.0f);
	Hole->SetRelativeScale3D(FVector(PulseHoleSize, PulseHoleSize, PulseHoleSize * 0.3f) * Opening / ShockSphereSize);

	Flash->SetIntensity(PulseFlash * FMath::Max(0.0f, 1.0f - Elapsed / (FrontTime + 0.1f)));
}
