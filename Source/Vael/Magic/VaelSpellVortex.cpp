// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelSpellVortex.h"
#include "UObject/ConstructorHelpers.h"
#include "Combat/VaelCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Magic/VaelGroundArea.h"
#include "Magic/VaelSpellEffects.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "UI/VaelCombatTextSubsystem.h"
#include "Vael.h"
#include "World/VaelGround.h"

#define LOCTEXT_NAMESPACE "VaelMagic"

namespace
{
	/** Size of the engine cone mesh used as placeholder */
	constexpr float VortexPlaceholderConeRadius = 50.0f;
	constexpr float VortexPlaceholderConeHeight = 100.0f;

	/** Height of the placeholder funnel in cm */
	constexpr float VortexLookHeight = 300.0f;

	/** Share of the radius the placeholder funnel takes at its top, so the enemies inside stay visible */
	constexpr float VortexLookRadiusShare = 0.6f;

	/** Turns per second of the placeholder */
	constexpr float VortexSpinDegreesPerSecond = 540.0f;

	/** A fire whose edge is this close to the middle of the whirlwind lights it (prototype: 0.8 tiles) */
	constexpr float VortexCatchFireDistance = 112.0f;

	/** Walls are looked for at this height above the ground */
	constexpr float VortexWallSearchHeight = 60.0f;

	/** Share of its speed the whirlwind keeps when it bounces off a wall, as in the prototype */
	constexpr float VortexBounceSpeedShare = 0.5f;

	/** How far above and below the whirlwind the ground is searched */
	constexpr float VortexGroundSearchHeight = 80.0f;
	constexpr float VortexGroundSearchDepth = 500.0f;

	/** Surfaces that face up at least this much are ramps or slopes, not walls (about 45 degrees) */
	constexpr float VortexWalkableSlopeNormalZ = 0.7f;

	/** The placeholder disc of each fire is lifted this much above the ground */
	constexpr float VortexFireHeightAboveGround = 2.0f;
}

AVaelSpellVortex::AVaelSpellVortex()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->bReceivesDecals = false;

	// The cone stands on its tip, which touches the ground
	Mesh->SetRelativeRotation(FRotator(180.0f, 0.0f, 0.0f));
	Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, VortexLookHeight * 0.5f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (ConeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(ConeMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ConeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ConeMaterial.Succeeded())
	{
		Mesh->SetMaterial(0, ConeMaterial.Object);
	}

	PrimaryActorTick.bCanEverTick = true;
}

AVaelSpellVortex* AVaelSpellVortex::Launch(APawn* Caster, const FVector& Location, const FVector& Direction, const FVaelVortexSettings& InSettings)
{
	UWorld* World = Caster != nullptr ? Caster->GetWorld() : nullptr;
	const FVector GroundDirection = Direction.GetSafeNormal2D();
	if (World == nullptr || GroundDirection.IsNearlyZero())
	{
		return nullptr;
	}

	// The whirlwind touches the ground below the given spot
	FVector SpawnLocation = Location;

	FHitResult GroundHit;
	if (VaelGround::TraceGround(World, Location + FVector(0.0f, 0.0f, VortexGroundSearchHeight), Location - FVector(0.0f, 0.0f, VortexGroundSearchDepth), GroundHit, Caster))
	{
		SpawnLocation = GroundHit.Location;
	}

	const FTransform SpawnTransform(GroundDirection.Rotation(), SpawnLocation);

	AVaelSpellVortex* Vortex = World->SpawnActorDeferred<AVaelSpellVortex>(AVaelSpellVortex::StaticClass(), SpawnTransform, Caster, Caster, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Vortex != nullptr)
	{
		Vortex->Settings = InSettings;
		Vortex->Velocity = GroundDirection * InSettings.Speed;
		Vortex->FinishSpawning(SpawnTransform);
	}

	return Vortex;
}

void AVaelSpellVortex::BeginPlay()
{
	Super::BeginPlay();

	SetLifeSpan(Settings.Lifetime);

	const float LookScale = Settings.Radius * VortexLookRadiusShare / VortexPlaceholderConeRadius;
	Mesh->SetRelativeScale3D(FVector(LookScale, LookScale, VortexLookHeight / VortexPlaceholderConeHeight));

	// The effect replaces the placeholder cone as soon as it exists
	VisualComponent = VaelEffects::Attach(Settings.Visual, RootComponent, NAME_None, Settings.Color, Settings.Radius);
	if (VisualComponent != nullptr)
	{
		Mesh->SetVisibility(false);
	}

	RefreshColor();
}

void AVaelSpellVortex::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Move(DeltaSeconds);
	CatchFire();
	HitEnemies(DeltaSeconds);
	LayFires(DeltaSeconds);

	AddActorWorldRotation(FRotator(0.0f, VortexSpinDegreesPerSecond * DeltaSeconds, 0.0f));

#if ENABLE_DRAW_DEBUG
	// Placeholder: the reach on the ground
	if (VisualComponent == nullptr)
	{
		DrawDebugCircle(GetWorld(), GetActorLocation() + FVector(0.0f, 0.0f, 5.0f), Settings.Radius, 32, (bFiery ? Settings.FireColor : Settings.Color).ToFColor(true),
			false, -1.0f, 0, 4.0f, FVector::ForwardVector, FVector::RightVector, false);
	}
#endif
}

void AVaelSpellVortex::Move(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	const FVector Location = GetActorLocation();
	const FVector NextLocation = Location + Velocity * DeltaSeconds;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VaelSpellVortex), false, this);
	QueryParams.AddIgnoredActor(GetInstigator());

	FCollisionObjectQueryParams SolidObjects(ECC_WorldStatic);
	SolidObjects.AddObjectTypesToQuery(ECC_PhysicsBody);

	const FVector WallOffset(0.0f, 0.0f, VortexWallSearchHeight);

	// Walls and rock walls throw it back at half its speed, as does the edge of the ground
	FHitResult WallHit;
	const bool bHitWall = World->LineTraceSingleByObjectType(WallHit, Location + WallOffset, NextLocation + WallOffset, SolidObjects, QueryParams) && WallHit.ImpactNormal.Z < VortexWalkableSlopeNormalZ;

	FHitResult GroundHit;
	const bool bOnGround = !bHitWall && VaelGround::TraceGround(World, NextLocation + FVector(0.0f, 0.0f, VortexGroundSearchHeight), NextLocation - FVector(0.0f, 0.0f, VortexGroundSearchDepth), GroundHit, this);

	if (!bOnGround)
	{
		Velocity *= -VortexBounceSpeedShare;
		return;
	}

	SetActorLocation(GroundHit.Location);
}

void AVaelSpellVortex::CatchFire()
{
	if (bFiery || !AVaelGroundArea::IsFireNear(GetWorld(), GetActorLocation(), VortexCatchFireDistance))
	{
		return;
	}

	bFiery = true;
	RefreshColor();

	UVaelCombatTextSubsystem::PostReaction(this, LOCTEXT("FireWhirl", "Feuerwirbel!"));
	UE_LOG(LogVael, Verbose, TEXT("Whirlwind '%s' turns into a fire whirl"), *GetNameSafe(this));
}

void AVaelSpellVortex::HitEnemies(float DeltaSeconds)
{
	APawn* Caster = GetInstigator();
	const FVector Center = GetActorLocation();
	const float DamagePerSecond = Settings.DamagePerSecond + (bFiery ? Settings.FireDamageBonus : 0.0f);

	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		AVaelCharacterBase* Target = *It;
		if (!UVaelCombatStatics::CanDamage(Caster, Target)
			|| FVector::Dist2D(Target->GetActorLocation(), Center) >= Settings.Radius + Target->GetCapsuleComponent()->GetScaledCapsuleRadius())
		{
			continue;
		}

		Target->ApplyPull(Center, Settings.PullSpeed, DeltaSeconds);

		if (Settings.BlindDuration > 0.0f)
		{
			Target->ApplyBlind(Settings.BlindDuration);
		}

		const float Dealt = CollectedDamage.Add(Target, DamagePerSecond * DeltaSeconds, Settings.DamageStep);
		if (Dealt > 0.0f)
		{
			FVaelSpellHit Hit = bFiery ? Settings.FireHit : Settings.Hit;
			Hit.Damage = Dealt;

			UVaelCombatStatics::ApplySpellHit(Caster, Target, Hit, Center - Target->GetActorLocation());
		}
	}
}

void AVaelSpellVortex::LayFires(float DeltaSeconds)
{
	if (!bFiery || Settings.FireInterval <= 0.0f)
	{
		return;
	}

	NextFireCountdown -= DeltaSeconds;
	if (NextFireCountdown <= 0.0f)
	{
		NextFireCountdown = Settings.FireInterval;

		AVaelGroundArea::SpawnArea(GetWorld(), GetActorLocation() + FVector(0.0f, 0.0f, VortexFireHeightAboveGround), EVaelElement::Fire, Settings.FireRadius, Settings.FireLifetime,
			Settings.FireDamagePerSecond, GetInstigator());
	}
}

void AVaelSpellVortex::RefreshColor()
{
	if (VisualComponent != nullptr)
	{
		VisualComponent->SetVariableLinearColor(VaelEffects::ColorParameter, bFiery ? Settings.FireColor : Settings.Color);
	}

	UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
	if (Material == nullptr)
	{
		Material = Mesh->CreateAndSetMaterialInstanceDynamic(0);
	}

	if (Material != nullptr)
	{
		Material->SetVectorParameterValue(TEXT("Color"), bFiery ? Settings.FireColor : Settings.Color);
	}
}

#undef LOCTEXT_NAMESPACE
