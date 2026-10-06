// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelSpellBeam.h"
#include "UObject/ConstructorHelpers.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Combat/VaelCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Magic/VaelFormulaAbility.h"
#include "Magic/VaelGameplayTags.h"
#include "Magic/VaelGroundArea.h"
#include "Magic/VaelSpellEffects.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "World/VaelGround.h"

namespace
{
	/** Size of the engine cylinder mesh used as placeholder */
	constexpr float BeamPlaceholderCylinderRadius = 50.0f;
	constexpr float BeamPlaceholderCylinderHeight = 100.0f;

	/** Thickness of the placeholder beam in cm */
	constexpr float BeamLookRadius = 22.0f;

	/** Fires are laid this far before the end of the beam, so they don't end up inside a wall (prototype: 0.3 tiles) */
	constexpr float FireBackOffDistance = 42.0f;

	/** How far below the end of the beam the ground is searched */
	constexpr float GroundSearchDepth = 500.0f;

	/** How far above it the search starts, so rising ground is found too */
	constexpr float GroundSearchHeight = 80.0f;
}

AVaelSpellBeam::AVaelSpellBeam()
{
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->bReceivesDecals = false;
	RootComponent = Mesh;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CylinderMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CylinderMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (CylinderMaterial.Succeeded())
	{
		Mesh->SetMaterial(0, CylinderMaterial.Object);
	}

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
}

AVaelSpellBeam* AVaelSpellBeam::StartBeam(APawn* Caster, const FVaelBeamSettings& InSettings)
{
	UWorld* World = Caster != nullptr ? Caster->GetWorld() : nullptr;
	const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Caster);

	// One beam at a time
	if (World == nullptr || (AbilitySystem != nullptr && AbilitySystem->HasMatchingGameplayTag(VaelTags::State_Channeling)))
	{
		return nullptr;
	}

	const FTransform SpawnTransform(Caster->GetActorLocation());

	AVaelSpellBeam* Beam = World->SpawnActorDeferred<AVaelSpellBeam>(AVaelSpellBeam::StaticClass(), SpawnTransform, Caster, Caster, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Beam != nullptr)
	{
		Beam->Settings = InSettings;
		Beam->FinishSpawning(SpawnTransform);
	}

	return Beam;
}

void AVaelSpellBeam::BeginPlay()
{
	Super::BeginPlay();

	EndTime = GetWorld()->GetTimeSeconds() + Settings.Duration;
	NextFireCountdown = 0.0f;

	if (UMaterialInstanceDynamic* Material = Mesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), Settings.Color);
	}

	// The beam effect replaces the placeholder cylinder as soon as it exists
	VisualComponent = VaelEffects::Attach(Settings.Visual, Mesh, NAME_None, Settings.Color, BeamLookRadius);
	if (VisualComponent != nullptr)
	{
		VisualComponent->SetUsingAbsoluteLocation(true);
		VisualComponent->SetUsingAbsoluteRotation(true);
		Mesh->SetVisibility(false);
	}

	CasterAbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetInstigator());
	if (CasterAbilitySystem.IsValid())
	{
		CasterAbilitySystem->AddLooseGameplayTag(VaelTags::State_Channeling);
	}

	// The first moment already burns and lays the first fire
	UpdateBeam(0.0f);
}

void AVaelSpellBeam::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (CasterAbilitySystem.IsValid())
	{
		CasterAbilitySystem->RemoveLooseGameplayTag(VaelTags::State_Channeling);
	}

	Super::EndPlay(EndPlayReason);
}

void AVaelSpellBeam::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The beam dies with the strength of its caster
	const AVaelCharacterBase* Caster = Cast<AVaelCharacterBase>(GetInstigator());
	if (Caster == nullptr || Caster->IsDefeated() || GetWorld()->GetTimeSeconds() >= EndTime)
	{
		Destroy();
		return;
	}

	UpdateBeam(DeltaSeconds);
}

void AVaelSpellBeam::UpdateBeam(float DeltaSeconds)
{
	APawn* Caster = GetInstigator();
	UWorld* World = GetWorld();
	if (Caster == nullptr || World == nullptr)
	{
		return;
	}

	const FVector Start = Caster->GetActorLocation();
	const FVector Direction = UVaelFormulaAbility::GetAimDirection(Caster);

	// Walls stop the beam
	float Length = Settings.Length;

	FHitResult WallHit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VaelSpellBeam), false, Caster);
	if (World->LineTraceSingleByObjectType(WallHit, Start, Start + Direction * Length, FCollisionObjectQueryParams(ECC_WorldStatic), QueryParams))
	{
		Length = WallHit.Distance;
	}

	UpdateLook(Start, Direction, Length);

	// Damage collects on everyone close to the middle line of the beam
	for (TActorIterator<AVaelCharacterBase> It(World); It; ++It)
	{
		AVaelCharacterBase* Target = *It;
		if (!UVaelCombatStatics::CanDamage(Caster, Target))
		{
			continue;
		}

		const FVector ToTarget = Target->GetActorLocation() - Start;
		const float Along = FVector::DotProduct(FVector(ToTarget.X, ToTarget.Y, 0.0f), Direction);
		if (Along < 0.0f || Along > Length)
		{
			continue;
		}

		const float Across = FMath::Abs(Direction.X * ToTarget.Y - Direction.Y * ToTarget.X);
		if (Across >= Settings.HalfWidth + Target->GetCapsuleComponent()->GetScaledCapsuleRadius() * 0.5f)
		{
			continue;
		}

		const float Dealt = CollectedDamage.Add(Target, Settings.DamagePerSecond * DeltaSeconds, Settings.DamageStep);
		if (Dealt > 0.0f)
		{
			FVaelSpellHit Hit = Settings.Hit;
			Hit.Damage = Dealt;

			UVaelCombatStatics::ApplySpellHit(Caster, Target, Hit, Direction);
		}
	}

	// The ground catches fire where the beam ends
	if (Settings.FireInterval > 0.0f)
	{
		NextFireCountdown -= DeltaSeconds;
		if (NextFireCountdown <= 0.0f)
		{
			NextFireCountdown = Settings.FireInterval;

			const FVector FireLocation = Start + Direction * FMath::Max(Length - FireBackOffDistance, 0.0f);

			FHitResult GroundHit;
			if (VaelGround::TraceGround(World, FireLocation + FVector(0.0f, 0.0f, GroundSearchHeight), FireLocation - FVector(0.0f, 0.0f, GroundSearchDepth), GroundHit, Caster))
			{
				AVaelGroundArea::SpawnArea(World, GroundHit.Location + FVector(0.0f, 0.0f, 2.0f), Settings.Hit.Element, Settings.FireRadius, Settings.FireLifetime,
					Settings.FireDamagePerSecond, Caster);
			}
		}
	}
}

void AVaelSpellBeam::UpdateLook(const FVector& Start, const FVector& Direction, float Length)
{
	const float VisibleLength = FMath::Max(Length, 1.0f);

	if (VisualComponent != nullptr)
	{
		VisualComponent->SetWorldLocationAndRotation(Start, Direction.Rotation());
		VisualComponent->SetVariableVec3(VaelEffects::BeamEndParameter, Start + Direction * VisibleLength);
	}

	// The cylinder stands on its axis, which is turned into the aim direction
	Mesh->SetWorldLocationAndRotation(Start + Direction * VisibleLength * 0.5f, FRotationMatrix::MakeFromZ(Direction).Rotator());
	Mesh->SetWorldScale3D(FVector(BeamLookRadius / BeamPlaceholderCylinderRadius, BeamLookRadius / BeamPlaceholderCylinderRadius, VisibleLength / BeamPlaceholderCylinderHeight));
}
