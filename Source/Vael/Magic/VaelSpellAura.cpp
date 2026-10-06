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
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	/** Size of the engine cylinder mesh used as placeholder */
	constexpr float AuraPlaceholderCylinderRadius = 50.0f;

	/** The placeholder disc lies this far above the feet of the caster */
	constexpr float AuraDiscHeightAboveFeet = 3.0f;

	/** Turns per second of the placeholder rings */
	constexpr float AuraSpinDegreesPerSecond = 300.0f;
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

	// Follows the feet of the caster
	const FVector Feet = Caster->GetActorLocation() - FVector(0.0f, 0.0f, Caster->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - AuraDiscHeightAboveFeet);
	SetActorLocationAndRotation(Feet, FRotator(0.0f, GetActorRotation().Yaw + AuraSpinDegreesPerSecond * DeltaSeconds, 0.0f));

	HitEnemies(DeltaSeconds);

#if ENABLE_DRAW_DEBUG
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

	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		AVaelCharacterBase* Target = *It;
		if (!UVaelCombatStatics::CanDamage(Caster, Target)
			|| FVector::Dist2D(Target->GetActorLocation(), Center) >= Settings.Radius + Target->GetCapsuleComponent()->GetScaledCapsuleRadius())
		{
			continue;
		}

		Target->ApplyBlind(Settings.BlindDuration);

		const float Dealt = CollectedDamage.Add(Target, Settings.DamagePerSecond * DeltaSeconds, Settings.DamageStep);
		if (Dealt > 0.0f)
		{
			FVaelSpellHit Hit = Settings.Hit;
			Hit.Damage = Dealt;

			UVaelCombatStatics::ApplySpellHit(Caster, Target, Hit, Target->GetActorLocation() - Center);
		}
	}
}
