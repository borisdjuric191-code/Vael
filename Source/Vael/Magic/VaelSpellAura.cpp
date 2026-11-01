// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelSpellAura.h"
#include "UObject/ConstructorHelpers.h"
#include "Combat/VaelCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/PlayerController.h"
#include "Magic/VaelDebrisBurst.h"
#include "Magic/VaelLightningBolt.h"
#include "Magic/VaelSpellEffects.h"
#include "Magic/VaelDebrisWhirl.h"
#include "Magic/VaelMagicSettings.h"
#include "VaelAssets.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"

namespace
{
	/** Size of the engine cylinder mesh used as placeholder */
	constexpr float AuraPlaceholderCylinderRadius = 50.0f;

	/** The placeholder disc lies this far above the feet of the caster */
	constexpr float AuraDiscHeightAboveFeet = 3.0f;

	/** Turns per second of the placeholder rings */
	constexpr float AuraSpinDegreesPerSecond = 300.0f;

	/** Bone splinters of the Knochensturm and their color */
	constexpr int32 AuraBoneShards = 60;
	const FLinearColor AuraBoneColor(0.8f, 0.74f, 0.6f);

	/** The black sun: its width and height over the feet in cm, its light in candela; the engine plane is this wide */
	constexpr float AuraSunSize = 300.0f;
	constexpr float AuraSunHeight = 340.0f;
	constexpr float AuraSunLight = 30.0f;
	constexpr float AuraPlaneSize = 100.0f;
}

AVaelSpellAura::AVaelSpellAura()
{
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->bReceivesDecals = false;
	RootComponent = Mesh;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DiscMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (DiscMesh.Succeeded())
	{
		Mesh->SetStaticMesh(DiscMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DiscMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (DiscMaterial.Succeeded())
	{
		Mesh->SetMaterial(0, DiscMaterial.Object);
	}

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
}

AVaelSpellAura* AVaelSpellAura::StartAura(APawn* Caster, const FVaelAuraSettings& InSettings)
{
	UWorld* World = Caster != nullptr ? Caster->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}

	// A caster has one storm at a time, casting again renews it
	for (TActorIterator<AVaelSpellAura> It(World); It; ++It)
	{
		if (It->GetInstigator() == Caster && !It->IsActorBeingDestroyed())
		{
			It->Restart(InSettings);
			return *It;
		}
	}

	const FTransform SpawnTransform(Caster->GetActorLocation());

	AVaelSpellAura* Aura = World->SpawnActorDeferred<AVaelSpellAura>(AVaelSpellAura::StaticClass(), SpawnTransform, Caster, Caster, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Aura != nullptr)
	{
		Aura->Settings = InSettings;
		Aura->FinishSpawning(SpawnTransform);
	}

	return Aura;
}

void AVaelSpellAura::BeginPlay()
{
	Super::BeginPlay();

	Restart(Settings);
}

void AVaelSpellAura::Restart(const FVaelAuraSettings& InSettings)
{
	Settings = InSettings;
	EndTime = GetWorld()->GetTimeSeconds() + Settings.Duration;

	// A flat disc of sand marks the reach on the ground
	const float DiscScale = Settings.Radius / AuraPlaceholderCylinderRadius;
	Mesh->SetWorldScale3D(FVector(DiscScale, DiscScale, 0.02f));

	if (UMaterialInstanceDynamic* Material = Mesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), Settings.Color);
	}

	// The effect replaces the placeholder disc and rings as soon as it exists
	// A sand storm carries real debris instead (it keeps its own size, not the flat one of the disc)
	if (Settings.bDebrisWhirl && Whirl == nullptr)
	{
		Whirl = NewObject<UVaelDebrisWhirlComponent>(this);
		Whirl->SetupAttachment(Mesh);
		Whirl->SetUsingAbsoluteScale(true);
		Whirl->RegisterComponent();
		Whirl->Build(Settings.Radius, false, Settings.Color);
	}
	else if (Settings.Look == EVaelAuraLook::BoneStorm && Shards.IsEmpty())
	{
		BuildBoneShards();
	}
	else if (Settings.Look == EVaelAuraLook::BlackSun && Sun == nullptr)
	{
		BuildBlackSun();
	}
	else if (VisualComponent == nullptr && !Settings.bDebrisWhirl && Settings.Look == EVaelAuraLook::Plain)
	{
		VisualComponent = VaelEffects::Attach(Settings.Visual, Mesh, NAME_None, Settings.Color, Settings.Radius);
	}

	Mesh->SetVisibility(VisualComponent == nullptr && Whirl == nullptr && Sun == nullptr && Shards.IsEmpty());
}

void AVaelSpellAura::BuildBoneShards()
{
	UMaterialInterface* CoreMaterial = nullptr;
	UMaterialInterface* GlowMaterial = nullptr;
	VaelEffects::LoadLookMaterials(CoreMaterial, GlowMaterial);
	UStaticMesh* Cone = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));

	for (int32 ShardIndex = 0; ShardIndex < AuraBoneShards; ++ShardIndex)
	{
		UStaticMeshComponent* Shard = VaelEffects::AddLookShape(this, CoreMaterial, AuraBoneColor, 0.0f);
		Shard->SetStaticMesh(Cone);
		Shard->SetUsingAbsoluteLocation(true);
		Shard->SetUsingAbsoluteRotation(true);
		Shard->SetUsingAbsoluteScale(true);
		Shard->SetCastShadow(true);
		Shard->SetVisibility(true);
		Shards.Add(Shard);
	}
}

void AVaelSpellAura::AnimateBoneShards(const FVector& Feet, float Time)
{
	for (int32 ShardIndex = 0; ShardIndex < Shards.Num(); ++ShardIndex)
	{
		// Every shard has its own ring, height and speed, drawn from its number so they never line up
		const float Seed = ShardIndex * 2.39996f;
		const float Ring = FMath::Lerp(0.45f, 1.0f, FMath::Frac(Seed * 0.37f));
		const float Height = FMath::Lerp(25.0f, 190.0f, FMath::Frac(Seed * 0.71f));
		const float Speed = FMath::Lerp(5.0f, 9.0f, FMath::Frac(Seed * 0.19f));
		const float Size = FMath::Lerp(0.6f, 1.25f, FMath::Frac(Seed * 0.53f));

		const float Angle = Seed + Time * Speed;
		const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
		const FVector Along(-Out.Y, Out.X, 0.0f);
		const FVector Place = Feet + Out * Settings.Radius * Ring + FVector(0.0f, 0.0f, Height + 14.0f * FMath::Sin(Time * 7.0f + Seed));

		// They fly point first, tipped up and in as if torn along
		Shards[ShardIndex]->SetWorldLocationAndRotation(Place, FRotationMatrix::MakeFromZ(Along + FVector(0.0f, 0.0f, 0.25f * FMath::Sin(Seed)) - Out * 0.2f).Rotator());
		Shards[ShardIndex]->SetWorldScale3D(FVector(0.1f, 0.1f, 0.55f) * Size);
	}
}

void AVaelSpellAura::BuildBlackSun()
{
	UMaterialInterface* SunMaterial = VaelAssets::LoadOptional(UVaelMagicSettings::Get()->BlackSunMaterial);

	Sun = NewObject<UStaticMeshComponent>(this);
	Sun->SetupAttachment(Mesh);
	Sun->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")));
	Sun->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Sun->SetCastShadow(false);
	Sun->SetUsingAbsoluteLocation(true);
	Sun->SetUsingAbsoluteRotation(true);
	Sun->SetUsingAbsoluteScale(true);
	Sun->SetWorldScale3D(FVector(AuraSunSize / AuraPlaneSize));
	if (SunMaterial != nullptr)
	{
		Sun->SetMaterial(0, SunMaterial);
	}
	Sun->RegisterComponent();

	SunLight = NewObject<UPointLightComponent>(this);
	SunLight->SetupAttachment(Sun);
	SunLight->SetMobility(EComponentMobility::Movable);
	SunLight->SetIntensityUnits(ELightUnits::Candelas);
	SunLight->SetIntensity(AuraSunLight);
	SunLight->SetLightColor(FLinearColor(1.0f, 0.42f, 0.14f));
	SunLight->SetAttenuationRadius(Settings.Radius * 2.6f);
	SunLight->SetCastShadows(false);
	SunLight->RegisterComponent();
}

void AVaelSpellAura::AnimateBlackSun(const FVector& Feet, float DeltaSeconds)
{
	const FVector Middle = Feet + FVector(0.0f, 0.0f, AuraSunHeight);
	const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	const FVector Camera = PlayerController != nullptr && PlayerController->PlayerCameraManager != nullptr ? PlayerController->PlayerCameraManager->GetCameraLocation() : Middle + FVector(-600.0f, 0.0f, 900.0f);
	Sun->SetWorldLocationAndRotation(Middle, FRotationMatrix::MakeFromZ(Camera - Middle).Rotator());
	SunLight->SetIntensity(AuraSunLight * (0.8f + 0.3f * FMath::PerlinNoise1D(GetWorld()->GetTimeSeconds() * 7.0f)));

	// Now and then the corona lashes down onto the ground around the caster, and it rains embers all the time
	SunLashCountdown -= DeltaSeconds;
	if (SunLashCountdown > 0.0f)
	{
		return;
	}
	SunLashCountdown = FMath::FRandRange(0.16f, 0.34f);

	const FVector2D Spot = FMath::RandPointInCircle(Settings.Radius);
	AVaelLightningBolt::Spawn(this, Middle, Feet + FVector(Spot.X, Spot.Y, 6.0f), FLinearColor(1.0f, 0.28f, 0.04f), 0.2f, 0.55f, true);

	FVaelDebris Embers;
	Embers.Count = 7;
	Embers.Spread = AuraSunSize * 0.3f;
	Embers.Speed = 90.0f;
	Embers.Lift = 0.0f;
	Embers.Gravity = 620.0f;
	Embers.Lifetime = 0.8f;
	Embers.MinSize = 3.0f;
	Embers.MaxSize = 7.0f;
	Embers.Color = FLinearColor(1.0f, 0.36f, 0.06f);
	Embers.Glow = 6.0f;
	AVaelDebrisBurst::Spawn(this, Middle, Embers);
}

AVaelSpellAura* AVaelSpellAura::FindSandStorm(const APawn* Caster)
{
	UWorld* World = Caster != nullptr ? Caster->GetWorld() : nullptr;
	for (TActorIterator<AVaelSpellAura> It(World); World != nullptr && It; ++It)
	{
		if (It->GetInstigator() == Caster && It->Settings.bDebrisWhirl && It->Settings.Look == EVaelAuraLook::Plain && !It->bGathering && !It->IsActorBeingDestroyed())
		{
			return *It;
		}
	}
	return nullptr;
}

float AVaelSpellAura::GatherInto(const FVector& Target)
{
	const float Now = GetWorld()->GetTimeSeconds();
	const float Left = FMath::Max(EndTime - Now, 0.0f);
	bGathering = true;
	GatherFrom = GetActorLocation();
	GatherTarget = Target;
	GatherStart = Now;
	EndTime = Now + UVaelMagicSettings::Get()->StormGatherSeconds;
	return Left;
}

void AVaelSpellAura::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The storm dies with the strength of its caster
	const AVaelCharacterBase* Caster = Cast<AVaelCharacterBase>(GetInstigator());
	if (Caster == nullptr || Caster->IsDefeated() || GetWorld()->GetTimeSeconds() >= EndTime)
	{
		Destroy();
		return;
	}

	// Drawn into a dust devil: it spirals over to it, shrinking, and hurts nobody any more
	if (bGathering)
	{
		const float Share = FMath::Clamp((GetWorld()->GetTimeSeconds() - GatherStart) / UVaelMagicSettings::Get()->StormGatherSeconds, 0.0f, 1.0f);
		SetActorLocationAndRotation(FMath::Lerp(GatherFrom, GatherTarget, Share * Share), FRotator(0.0f, GetActorRotation().Yaw + AuraSpinDegreesPerSecond * 3.0f * DeltaSeconds, 0.0f));
		if (Whirl != nullptr)
		{
			Whirl->SetGather(1.0f - Share);
		}
		return;
	}

	// Follows the feet of the caster
	const FVector Feet = Caster->GetActorLocation() - FVector(0.0f, 0.0f, Caster->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - AuraDiscHeightAboveFeet);
	SetActorLocationAndRotation(Feet, FRotator(0.0f, GetActorRotation().Yaw + AuraSpinDegreesPerSecond * DeltaSeconds, 0.0f));

	HitEnemies(DeltaSeconds);

	if (!Shards.IsEmpty())
	{
		AnimateBoneShards(Feet, GetWorld()->GetTimeSeconds());
	}
	if (Sun != nullptr)
	{
		AnimateBlackSun(Feet, DeltaSeconds);
	}

#if ENABLE_DRAW_DEBUG
	if (VisualComponent != nullptr || Whirl != nullptr || Sun != nullptr || !Shards.IsEmpty())
	{
		return;
	}

	// Placeholder: whirling rings of sand around the caster
	const FColor RingColor = Settings.Color.ToFColor(true);
	const float Spin = FMath::DegreesToRadians(GetActorRotation().Yaw);

	for (int32 RingIndex = 0; RingIndex < 3; ++RingIndex)
	{
		const float RingRadius = Settings.Radius * (1.0f - RingIndex * 0.25f);
		const FVector RingCenter = Feet + FVector(0.0f, 0.0f, 40.0f + RingIndex * 60.0f);
		const FVector Forward(FMath::Cos(Spin + RingIndex), FMath::Sin(Spin + RingIndex), 0.0f);
		const FVector Right(-Forward.Y, Forward.X, 0.0f);

		DrawDebugCircle(GetWorld(), RingCenter, RingRadius, 12 + RingIndex * 2, RingColor, false, -1.0f, 0, 4.0f, Forward, Right, false);
	}
#endif
}

void AVaelSpellAura::HitEnemies(float DeltaSeconds)
{
	APawn* Caster = GetInstigator();
	const FVector Center = Caster->GetActorLocation();

	// Some storms burn their caster too
	if (Settings.SelfDamagePerSecond > 0.0f)
	{
		const float Burned = CollectedDamage.Add(Caster, Settings.SelfDamagePerSecond * DeltaSeconds, Settings.DamageStep);
		if (Burned > 0.0f)
		{
			UVaelCombatStatics::DealDamage(Caster, Caster, Burned, Settings.Hit.Element);
		}
	}

	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		AVaelCharacterBase* Target = *It;
		if (!UVaelCombatStatics::CanDamage(Caster, Target)
			|| FVector::Dist2D(Target->GetActorLocation(), Center) >= Settings.Radius + Target->GetCapsuleComponent()->GetScaledCapsuleRadius())
		{
			continue;
		}

		if (Settings.BlindDuration > 0.0f)
		{
			Target->ApplyBlind(Settings.BlindDuration);
		}

		const float Dealt = CollectedDamage.Add(Target, Settings.DamagePerSecond * DeltaSeconds, Settings.DamageStep);
		if (Dealt > 0.0f)
		{
			FVaelSpellHit Hit = Settings.Hit;
			Hit.Damage = Dealt;

			UVaelCombatStatics::ApplySpellHit(Caster, Target, Hit, Target->GetActorLocation() - Center);
		}
	}
}
