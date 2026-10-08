// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelSpellProjectile.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Combat/VaelHitFeedbackSubsystem.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Magic/VaelGroundArea.h"
#include "Magic/VaelMagicSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraSystem.h"
#include "VaelAssets.h"
#include "World/VaelGround.h"

namespace
{
	/** Radius of the engine sphere mesh used as placeholder */
	constexpr float PlaceholderSphereRadius = 50.0f;

	/** Camera shake of a spell explosion, as in the prototype */
	constexpr float ExplosionShake = 0.2f;

	/** The spark is this many times longer and thinner than the collision radius of its projectile */
	constexpr float SparkLengthShare = 2.4f;
	constexpr float SparkThicknessShare = 0.42f;

	/** Pieces of the tail behind the spark */
	constexpr int32 SparkTailCount = 3;

	/** Embers a spark can have in the air at once, seconds between two and seconds each one glows */
	constexpr int32 SparkEmberCount = 12;
	constexpr float SparkEmberInterval = 0.055f;
	constexpr float SparkEmberLifetime = 0.55f;

	/** Embers start this fast sideways, keep a share of the flight speed and fall, in cm/s and cm/s² */
	constexpr float SparkEmberSpread = 110.0f;
	constexpr float SparkEmberForwardShare = 0.12f;
	constexpr float SparkEmberGravity = 520.0f;

	/** Material parameters of the glow materials */
	const FName SparkColorParameter(TEXT("Color"));
	const FName SparkGlowParameter(TEXT("Glow"));
	const FName SparkRimParameter(TEXT("Rim"));
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
	SpellColor = Color;

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

	if (!SparkShapes.IsEmpty())
	{
		AnimateSpark(DeltaSeconds);
	}
}

UStaticMeshComponent* AVaelSpellProjectile::AddSparkShape(UMaterialInterface* Material, const FLinearColor& ShapeColor, float Glow, bool bStaysBehind)
{
	UStaticMeshComponent* Shape = NewObject<UStaticMeshComponent>(this);
	Shape->SetupAttachment(RootComponent);
	Shape->SetStaticMesh(Mesh->GetStaticMesh());
	Shape->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Shape->SetCastShadow(false);
	Shape->bReceivesDecals = false;

	if (bStaysBehind)
	{
		Shape->SetUsingAbsoluteLocation(true);
		Shape->SetUsingAbsoluteRotation(true);
		Shape->SetVisibility(false);
	}

	Shape->RegisterComponent();
	Shape->SetMaterial(0, Material);

	if (UMaterialInstanceDynamic* Dynamic = Shape->CreateAndSetMaterialInstanceDynamic(0))
	{
		Dynamic->SetVectorParameterValue(SparkColorParameter, ShapeColor);
		Dynamic->SetScalarParameterValue(SparkGlowParameter, Glow);
		Dynamic->SetScalarParameterValue(SparkRimParameter, 0.0f);
	}

	return Shape;
}

void AVaelSpellProjectile::BuildSpark()
{
	// The glowing materials of the element orbs; the plain engine material while they don't exist
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	UMaterialInterface* Plain = Mesh->GetMaterial(0);
	UMaterialInterface* CoreMaterial = VaelAssets::LoadOptional(MagicSettings->ElementOrbCoreMaterial);
	UMaterialInterface* GlowMaterial = VaelAssets::LoadOptional(MagicSettings->ElementOrbGlowMaterial);
	CoreMaterial = CoreMaterial != nullptr ? CoreMaterial : Plain;
	GlowMaterial = GlowMaterial != nullptr ? GlowMaterial : Plain;

	const FLinearColor Hot = FMath::Lerp(SpellColor, FLinearColor::White, 0.45f);
	const FLinearColor Dying = SpellColor * FLinearColor(1.0f, 0.45f, 0.3f);

	// A white-hot streak in a halo of its color, with a tail that thins out behind it
	SparkShapes.Add(AddSparkShape(CoreMaterial, Hot, 18.0f));
	SparkShapes.Add(AddSparkShape(GlowMaterial, SpellColor, 3.0f));

	for (int32 TailIndex = 0; TailIndex < SparkTailCount; ++TailIndex)
	{
		SparkShapes.Add(AddSparkShape(GlowMaterial, FMath::Lerp(SpellColor, Dying, (TailIndex + 1.0f) / SparkTailCount), 2.6f - TailIndex * 0.5f));
	}

	for (int32 EmberIndex = 0; EmberIndex < SparkEmberCount; ++EmberIndex)
	{
		Embers.Add(AddSparkShape(GlowMaterial, FMath::Lerp(SpellColor, Dying, 0.5f), 5.0f, true));
	}

	EmberVelocities.Init(FVector::ZeroVector, SparkEmberCount);
	EmberAges.Init(-1.0f, SparkEmberCount);

	SparkLight = NewObject<UPointLightComponent>(this);
	SparkLight->SetupAttachment(RootComponent);
	SparkLight->SetMobility(EComponentMobility::Movable);
	SparkLight->SetIntensityUnits(ELightUnits::Candelas);
	SparkLight->SetIntensity(9.0f);
	SparkLight->SetLightColor(SpellColor);
	SparkLight->SetAttenuationRadius(320.0f);
	SparkLight->SetSourceRadius(4.0f);
	SparkLight->SetCastShadows(false);
	SparkLight->RegisterComponent();

	Mesh->SetVisibility(false);
	SetActorTickEnabled(true);
	AnimateSpark(0.0f);
}

void AVaelSpellProjectile::AnimateSpark(float DeltaSeconds)
{
	const float Time = GetWorld()->GetTimeSeconds();
	const float Radius = Collision->GetScaledSphereRadius();
	const float Length = Radius * SparkLengthShare / (PlaceholderSphereRadius * 2.0f);
	const float Thickness = Radius * SparkThicknessShare / (PlaceholderSphereRadius * 2.0f);
	const float Flicker = FMath::PerlinNoise1D(Time * 14.0f);

	// The streak lies along the flight; core and halo flicker against each other
	SparkShapes[0]->SetRelativeScale3D(FVector(Length, Thickness, Thickness) * (1.0f + 0.1f * Flicker));
	SparkShapes[1]->SetRelativeScale3D(FVector(Length * 1.5f, Thickness * 2.6f, Thickness * 2.6f) * (1.0f - 0.15f * Flicker));

	for (int32 TailIndex = 0; TailIndex < SparkTailCount; ++TailIndex)
	{
		const float Thin = 1.0f - (TailIndex + 1.0f) / (SparkTailCount + 1.5f);
		const float Waver = FMath::Sin(Time * 26.0f + TailIndex * 1.9f);

		UStaticMeshComponent* Piece = SparkShapes[2 + TailIndex];
		Piece->SetRelativeLocation(FVector(-Radius * SparkLengthShare * 0.55f * (TailIndex + 1.0f), Waver * Radius * 0.07f, 0.0f));
		Piece->SetRelativeScale3D(FVector(Length * 0.9f, Thickness * 2.0f * Thin, Thickness * 2.0f * Thin) * (1.0f + 0.12f * Waver));
	}

	SparkLight->SetIntensity(9.0f * (1.0f + 0.3f * Flicker));

	// Shed the next ember: it leaves the spark sideways, keeps a little of the flight speed and falls
	NextEmberCountdown -= DeltaSeconds;
	if (NextEmberCountdown <= 0.0f)
	{
		NextEmberCountdown = SparkEmberInterval * FMath::FRandRange(0.6f, 1.5f);

		const FVector Forward = GetActorForwardVector();
		const FVector Sideways = FMath::VRand() * SparkEmberSpread * FMath::FRandRange(0.4f, 1.0f);

		EmberAges[NextEmber] = 0.0f;
		EmberVelocities[NextEmber] = Forward * Movement->Velocity.Size() * SparkEmberForwardShare + Sideways + FVector(0.0f, 0.0f, 60.0f);
		Embers[NextEmber]->SetWorldLocation(GetActorLocation() - Forward * Radius * FMath::FRandRange(0.2f, 1.2f));
		Embers[NextEmber]->SetVisibility(true);

		NextEmber = (NextEmber + 1) % SparkEmberCount;
	}

	for (int32 EmberIndex = 0; EmberIndex < Embers.Num(); ++EmberIndex)
	{
		if (EmberAges[EmberIndex] < 0.0f)
		{
			continue;
		}

		EmberAges[EmberIndex] += DeltaSeconds;
		if (EmberAges[EmberIndex] >= SparkEmberLifetime)
		{
			EmberAges[EmberIndex] = -1.0f;
			Embers[EmberIndex]->SetVisibility(false);
			continue;
		}

		// Embers fall, slow down in the air and shrink as they die
		EmberVelocities[EmberIndex].Z -= SparkEmberGravity * DeltaSeconds;
		EmberVelocities[EmberIndex] *= FMath::Max(0.0f, 1.0f - 2.2f * DeltaSeconds);

		const float Dying = 1.0f - EmberAges[EmberIndex] / SparkEmberLifetime;
		Embers[EmberIndex]->SetWorldLocation(Embers[EmberIndex]->GetComponentLocation() + EmberVelocities[EmberIndex] * DeltaSeconds);
		Embers[EmberIndex]->SetRelativeScale3D(FVector(Thickness * 0.9f * Dying * (0.8f + 0.4f * FMath::Sin(Time * 40.0f + EmberIndex * 2.7f))));
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

		if (RemainingPierce > 0)
		{
			// Flying on: each pierced target gets its own impact, the last one comes with the end of the flight
			VaelEffects::PlayImpact(this, Effects, OtherActor->GetActorLocation(), Collision->GetScaledSphereRadius());
		}

		if (RemainingPierce-- <= 0)
		{
			// Later overlaps of the same move must not hit anything else
			Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			EndFlight(true);
		}
	}
}

void AVaelSpellProjectile::OnStopped(const FHitResult& ImpactResult)
{
	EndFlight(true);
}

void AVaelSpellProjectile::LifeSpanExpired()
{
	EndFlight(false);
}

void AVaelSpellProjectile::SetEffects(const FVaelLoadedEffects& InEffects, UNiagaraSystem* InImpactAreaVisual)
{
	Effects = InEffects;
	ImpactAreaVisual = InImpactAreaVisual;
}

void AVaelSpellProjectile::BeginPlay()
{
	Super::BeginPlay();

	// The trail replaces the placeholder sphere as soon as it exists
	if (VaelEffects::Attach(Effects.Trail, Collision, NAME_None, Effects.Color, Collision->GetScaledSphereRadius()) != nullptr)
	{
		Mesh->SetVisibility(false);
	}
	else if (Look == EVaelProjectileLook::Spark)
	{
		BuildSpark();
	}
}

void AVaelSpellProjectile::SetImpactArea(EVaelElement Element, float Radius, float Lifetime, float DamagePerSecond, EVaelGroundEffect Effect)
{
	ImpactEffect = Effect;
	ImpactElement = Element;
	ImpactRadius = Radius;
	ImpactLifetime = Lifetime;
	ImpactDamagePerSecond = DamagePerSecond;
}

void AVaelSpellProjectile::SetPassesWalls()
{
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
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
		Projectile->SetEffects(VaelEffects::Load(InHit.Element));
		Projectile->FinishSpawning(SpawnTransform);
	}

	return Projectile;
}

void AVaelSpellProjectile::EndFlight(bool bHitSomething)
{
	if (IsActorBeingDestroyed())
	{
		return;
	}

	if (ExplosionRadius > 0.0f)
	{
		Explode();
	}

	// A projectile that just runs out in the air fizzles without an impact
	if (bHitSomething || ExplosionRadius > 0.0f)
	{
		VaelEffects::PlayImpact(this, Effects, GetActorLocation(), ExplosionRadius > 0.0f ? ExplosionRadius : Collision->GetScaledSphereRadius());
	}

	if (ImpactRadius > 0.0f)
	{
		FVector Location = GetActorLocation();

		// The patch lies on the ground below the projectile
		FHitResult GroundHit;
		if (VaelGround::TraceGround(GetWorld(), Location, Location - FVector(0.0f, 0.0f, 500.0f), GroundHit, this))
		{
			AVaelGroundArea::SpawnArea(GetWorld(), GroundHit.Location + FVector(0.0f, 0.0f, 2.0f), ImpactElement, ImpactRadius, ImpactLifetime, ImpactDamagePerSecond, GetInstigator(), true, ImpactEffect, ImpactAreaVisual);
		}
	}

	Destroy();
}

void AVaelSpellProjectile::Explode()
{
	UWorld* World = GetWorld();
	const FVector Center = GetActorLocation();

	UVaelCombatStatics::ApplySpellHitInRadius(GetInstigator(), Center, ExplosionRadius, ExplosionHit);

	UVaelHitFeedbackSubsystem::Shake(this, ExplosionShake);

#if ENABLE_DRAW_DEBUG
	// Placeholder look while no impact effect exists
	if (Effects.Impact == nullptr)
	{
		DrawDebugSphere(World, Center, ExplosionRadius, 24, UVaelMagicSettings::Get()->GetElementColor(ExplosionHit.Element).ToFColor(true), false, 0.3f);
	}
#endif
}
