// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelSpellProjectile.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Combat/VaelHitFeedbackSubsystem.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Magic/VaelGroundArea.h"
#include "Magic/VaelMagicSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	/** Radius of the engine sphere mesh used as placeholder */
	constexpr float PlaceholderSphereRadius = 50.0f;

	/** Camera shake of a spell explosion, as in the prototype */
	constexpr float ExplosionShake = 0.2f;
}

AVaelSpellProjectile::AVaelSpellProjectile()
{
	// Walls stop the projectile, characters are passed through and checked in OnOverlap
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(30.0f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetGenerateOverlapEvents(true);
	Collision->CanCharacterStepUpOn = ECB_No;
	RootComponent = Collision;

	Collision->OnComponentBeginOverlap.AddDynamic(this, &AVaelSpellProjectile::OnOverlap);

	// Placeholder look from engine assets
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->bReceivesDecals = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SphereMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (SphereMaterial.Succeeded())
	{
		Mesh->SetMaterial(0, SphereMaterial.Object);
	}

	// Flies straight, no gravity
	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->ProjectileGravityScale = 0.0f;
	Movement->bRotationFollowsVelocity = true;
	Movement->InitialSpeed = 1500.0f;
	Movement->MaxSpeed = 1500.0f;

	Movement->OnProjectileStop.AddDynamic(this, &AVaelSpellProjectile::OnStopped);

	InitialLifeSpan = 1.1f;

	// Only water projectiles tick, see InitSpell
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AVaelSpellProjectile::InitSpell(const FVaelSpellHit& InHit, float Speed, float Radius, float Lifetime, int32 Pierce, const FLinearColor& Color)
{
	Hit = InHit;
	RemainingPierce = Pierce;

	// Water puts out the fires it flies over, which has to be checked every frame
	PrimaryActorTick.bStartWithTickEnabled = Hit.Element == EVaelElement::Water;

	Collision->SetSphereRadius(Radius);
	Mesh->SetRelativeScale3D(FVector(Radius / PlaceholderSphereRadius));

	if (UMaterialInstanceDynamic* Material = Mesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), Color);
	}

	Movement->InitialSpeed = Speed;
	Movement->MaxSpeed = Speed;

	InitialLifeSpan = Lifetime;

	// The caster never blocks or triggers their own spell
	if (AActor* Caster = GetInstigator())
	{
		Collision->IgnoreActorWhenMoving(Caster, true);
	}
}

void AVaelSpellProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Hit.Element == EVaelElement::Water)
	{
		AVaelGroundArea::ExtinguishFires(GetWorld(), GetActorLocation(), Collision->GetScaledSphereRadius());
	}
}

void AVaelSpellProjectile::OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor == nullptr || OtherActor == GetInstigator() || HitActors.Contains(OtherActor))
	{
		return;
	}

	// Allies are passed through
	if (UVaelCombatStatics::ApplySpellHit(GetInstigator(), OtherActor, Hit, GetVelocity()))
	{
		HitActors.Add(OtherActor);

		if (RemainingPierce-- <= 0)
		{
			// Later overlaps of the same move must not hit anything else
			Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			EndFlight();
		}
	}
}

void AVaelSpellProjectile::OnStopped(const FHitResult& ImpactResult)
{
	EndFlight();
}

void AVaelSpellProjectile::LifeSpanExpired()
{
	EndFlight();
}

void AVaelSpellProjectile::SetImpactArea(EVaelElement Element, float Radius, float Lifetime, float DamagePerSecond, EVaelGroundEffect Effect)
{
	ImpactEffect = Effect;
	ImpactElement = Element;
	ImpactRadius = Radius;
	ImpactLifetime = Lifetime;
	ImpactDamagePerSecond = DamagePerSecond;
}

void AVaelSpellProjectile::SetExplosion(const FVaelSpellHit& InExplosionHit, float Radius)
{
	ExplosionHit = InExplosionHit;
	ExplosionRadius = Radius;
}

AVaelSpellProjectile* AVaelSpellProjectile::Launch(APawn* Attacker, const FVector& Location, const FVector& Direction, const FVaelSpellHit& InHit, float Speed, float Radius, float Lifetime, const FLinearColor& Color, float FireRadius, float FireLifetime, float FireDamagePerSecond)
{
	UWorld* World = Attacker != nullptr ? Attacker->GetWorld() : nullptr;
	const FVector FlightDirection = Direction.GetSafeNormal2D();
	if (World == nullptr || FlightDirection.IsNearlyZero())
	{
		return nullptr;
	}

	const FTransform SpawnTransform(FlightDirection.Rotation(), Location);

	AVaelSpellProjectile* Projectile = World->SpawnActorDeferred<AVaelSpellProjectile>(AVaelSpellProjectile::StaticClass(), SpawnTransform, Attacker, Attacker, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Projectile != nullptr)
	{
		Projectile->InitSpell(InHit, Speed, Radius, Lifetime, 0, Color);
		Projectile->SetImpactArea(EVaelElement::Fire, FireRadius, FireLifetime, FireDamagePerSecond);
		Projectile->FinishSpawning(SpawnTransform);
	}

	return Projectile;
}

void AVaelSpellProjectile::EndFlight()
{
	if (IsActorBeingDestroyed())
	{
		return;
	}

	if (ExplosionRadius > 0.0f)
	{
		Explode();
	}

	if (ImpactRadius > 0.0f)
	{
		FVector Location = GetActorLocation();

		// The patch lies on the ground below the projectile
		FHitResult GroundHit;
		if (GetWorld()->LineTraceSingleByObjectType(GroundHit, Location, Location - FVector(0.0f, 0.0f, 500.0f), FCollisionObjectQueryParams(ECC_WorldStatic)))
		{
			AVaelGroundArea::SpawnArea(GetWorld(), GroundHit.Location + FVector(0.0f, 0.0f, 2.0f), ImpactElement, ImpactRadius, ImpactLifetime, ImpactDamagePerSecond, GetInstigator(), true, ImpactEffect);
		}
	}

	Destroy();
}

void AVaelSpellProjectile::Explode()
{
	UWorld* World = GetWorld();
	const FVector Center = GetActorLocation();

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VaelSpellExplosion), false, this);
	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(ExplosionRadius), QueryParams);

	// A pawn can overlap with several components, it is hit only once
	TSet<AActor*> BurstActors;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		if (Target != nullptr && !BurstActors.Contains(Target))
		{
			BurstActors.Add(Target);
			UVaelCombatStatics::ApplySpellHit(GetInstigator(), Target, ExplosionHit, Target->GetActorLocation() - Center);
		}
	}

	UVaelHitFeedbackSubsystem::Shake(this, ExplosionShake);

#if ENABLE_DRAW_DEBUG
	// Placeholder look until the formulas get real effects
	DrawDebugSphere(World, Center, ExplosionRadius, 24, UVaelMagicSettings::Get()->GetElementColor(ExplosionHit.Element).ToFColor(true), false, 0.3f);
#endif
}
