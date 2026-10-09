// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelClayGolem.h"
#include "AIController.h"
#include "Combat/VaelHitFeedbackSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Magic/VaelDebrisBurst.h"
#include "Magic/VaelGroundArea.h"
#include "Magic/VaelSpellEffects.h"

namespace
{
	/** Size of the capsule in cm */
	constexpr float GolemCapsuleRadius = 45.0f;
	constexpr float GolemCapsuleHalfHeight = 90.0f;

	/** Health of the golem */
	constexpr float GolemHealth = 160.0f;

	/** Seconds the golem needs to rise out of the ground */
	constexpr float GolemRiseTime = 0.35f;

	/** How far the arms lift before a stamp, in cm, and the share of the interval they start lifting at */
	constexpr float GolemArmLift = 45.0f;
	constexpr float GolemWindupStart = 0.5f;

	/** Camera shake of a stamp */
	constexpr float GolemStampShake = 0.08f;

	/** Diameter of the engine sphere in cm */
	constexpr float GolemSphereSize = 100.0f;
}

AVaelClayGolem::AVaelClayGolem()
{
	GetCapsuleComponent()->InitCapsuleSize(GolemCapsuleRadius, GolemCapsuleHalfHeight);

	// Stands where it was called; the AI controller only keeps the movement running, so it lands on the ground
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	GetCharacterMovement()->MaxWalkSpeed = 0.0f;

	StartingHealth = GolemHealth;
	StartingMana = 0.0f;

	PrimaryActorTick.bCanEverTick = true;
}

AVaelClayGolem* AVaelClayGolem::Summon(TSubclassOf<AVaelClayGolem> GolemClass, APawn* Caster, const FVector& GroundLocation, const FVaelSpellHit& StampHit, float Lifetime)
{
	UWorld* World = Caster != nullptr ? Caster->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}

	if (GolemClass == nullptr)
	{
		GolemClass = AVaelClayGolem::StaticClass();
	}

	const FTransform SpawnTransform(FRotator(0.0f, Caster->GetActorRotation().Yaw, 0.0f), GroundLocation + FVector(0.0f, 0.0f, GolemCapsuleHalfHeight + 2.0f));

	AVaelClayGolem* Golem = World->SpawnActorDeferred<AVaelClayGolem>(GolemClass, SpawnTransform, Caster, Caster, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (Golem != nullptr)
	{
		Golem->StampHit = StampHit;
		Golem->InitialLifeSpan = Lifetime;
		Golem->FinishSpawning(SpawnTransform);
	}

	return Golem;
}

void AVaelClayGolem::BeginPlay()
{
	Super::BeginPlay();

	UMaterialInterface* CoreMaterial = nullptr;
	UMaterialInterface* GlowMaterial = nullptr;
	VaelEffects::LoadLookMaterials(CoreMaterial, GlowMaterial);

	const FLinearColor Clay(0.45f, 0.33f, 0.22f);
	const FLinearColor WetClay(0.36f, 0.26f, 0.17f);

	// Legs, body, head and two arms, each a lump of clay: place, size in cm, color
	struct FPart { FVector Place; FVector Size; FLinearColor Color; };
	const FPart Parts[] =
	{
		{ FVector(0.0f, -25.0f, -62.0f), FVector(38.0f, 38.0f, 56.0f), WetClay },
		{ FVector(0.0f, 25.0f, -62.0f), FVector(38.0f, 38.0f, 56.0f), WetClay },
		{ FVector(0.0f, 0.0f, 10.0f), FVector(95.0f, 80.0f, 105.0f), Clay },
		{ FVector(8.0f, 0.0f, 78.0f), FVector(46.0f, 46.0f, 42.0f), Clay },
		{ FVector(0.0f, -62.0f, 5.0f), FVector(36.0f, 36.0f, 85.0f), WetClay },
		{ FVector(0.0f, 62.0f, 5.0f), FVector(36.0f, 36.0f, 85.0f), WetClay },
	};

	for (const FPart& Part : Parts)
	{
		UStaticMeshComponent* Shape = VaelEffects::AddLookShape(this, CoreMaterial, Part.Color, 0.0f);
		Shape->SetRelativeScale3D(Part.Size / GolemSphereSize);
		Shape->SetCastShadow(true);
		Shape->SetVisibility(true);

		BodyParts.Add(Shape);
		PartPlaces.Add(Part.Place);
	}

	StampCountdown = StampInterval;

	// Clay and pebbles burst where it breaks out of the ground
	FVaelDebris Dust;
	Dust.Count = 10;
	Dust.Spread = GolemCapsuleRadius;
	Dust.Speed = 240.0f;
	Dust.Lift = 360.0f;
	Dust.Color = WetClay;
	Dust.RockShare = 0.2f;
	AVaelDebrisBurst::Spawn(this, GetActorLocation() - FVector(0.0f, 0.0f, GolemCapsuleHalfHeight), Dust);

	Tick(0.0f);
}

void AVaelClayGolem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bCrumbled)
	{
		return;
	}

	Age += DeltaSeconds;

	// It only winds up while an enemy is close enough; otherwise the arms rest
	bool bEnemyClose = false;
	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It && !bEnemyClose; ++It)
	{
		bEnemyClose = UVaelCombatStatics::CanDamage(this, *It) && GetHorizontalDistanceTo(*It) <= StampRadius;
	}

	StampCountdown -= DeltaSeconds;
	if (!bEnemyClose)
	{
		StampCountdown = FMath::Max(StampCountdown, StampInterval * GolemWindupStart);
	}
	else if (StampCountdown <= 0.0f)
	{
		Stamp();
		StampCountdown = StampInterval;
	}

	// Rises out of the ground; the arms lift before a stamp and come down with it
	const float Rise = FMath::Clamp(Age / GolemRiseTime, 0.0f, 1.0f);
	const float Sunk = GolemCapsuleHalfHeight * 2.0f * FMath::Square(1.0f - Rise);
	const float Windup = FMath::SmoothStep(GolemWindupStart, 0.95f, 1.0f - StampCountdown / StampInterval);

	for (int32 PartIndex = 0; PartIndex < BodyParts.Num(); ++PartIndex)
	{
		const bool bArm = PartIndex >= 4;
		BodyParts[PartIndex]->SetRelativeLocation(PartPlaces[PartIndex] + FVector(0.0f, 0.0f, (bArm ? GolemArmLift * Windup : 0.0f) - Sunk));
	}
}

void AVaelClayGolem::Stamp()
{
	// The hit counts as the caster's, so it hurts their enemies
	AActor* Attacker = GetInstigator() != nullptr ? static_cast<AActor*>(GetInstigator()) : this;
	const FVector Feet = GetActorLocation() - FVector(0.0f, 0.0f, GolemCapsuleHalfHeight);

	UVaelCombatStatics::ApplySpellHitInRadius(Attacker, Feet, StampRadius, StampHit);
	UVaelHitFeedbackSubsystem::Shake(this, GolemStampShake);

	FVaelDebris Dust;
	Dust.Count = 8;
	Dust.Spread = StampRadius * 0.5f;
	Dust.Speed = 300.0f;
	Dust.Lift = 260.0f;
	Dust.Color = FLinearColor(0.36f, 0.26f, 0.17f);
	AVaelDebrisBurst::Spawn(this, Feet, Dust);
}

void AVaelClayGolem::OnHealthChanged(float OldValue, float NewValue)
{
	Super::OnHealthChanged(OldValue, NewValue);

	if (NewValue <= 0.0f)
	{
		Crumble();
	}
}

void AVaelClayGolem::LifeSpanExpired()
{
	Crumble();
}

void AVaelClayGolem::Crumble()
{
	if (bCrumbled)
	{
		return;
	}

	bCrumbled = true;

	// Falls apart into lumps of clay that leave a patch of mud slowing the enemies of the caster
	const FVector Feet = GetActorLocation() - FVector(0.0f, 0.0f, GolemCapsuleHalfHeight);

	AVaelGroundArea::SpawnArea(GetWorld(), Feet + FVector(0.0f, 0.0f, 2.0f), EVaelElement::Earth, MudRadius, MudLifetime, 0.0f, GetInstigator(), true, EVaelGroundEffect::Slow);

	FVaelDebris Lumps;
	Lumps.Count = 16;
	Lumps.Spread = GolemCapsuleRadius;
	Lumps.Speed = 260.0f;
	Lumps.Lift = 220.0f;
	Lumps.Lifetime = 0.8f;
	Lumps.MinSize = 12.0f;
	Lumps.MaxSize = 24.0f;
	Lumps.Color = FLinearColor(0.45f, 0.33f, 0.22f);
	AVaelDebrisBurst::Spawn(this, GetActorLocation(), Lumps);

	Destroy();
}
