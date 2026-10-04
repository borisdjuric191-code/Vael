// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelSpellProjectile.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	/** Radius of the engine sphere mesh used as placeholder */
	constexpr float PlaceholderSphereRadius = 50.0f;
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
}

void AVaelSpellProjectile::InitSpell(const FVaelSpellHit& InHit, float Speed, float Radius, float Lifetime, const FLinearColor& Color)
{
	Hit = InHit;

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

void AVaelSpellProjectile::OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bHasHit || OtherActor == nullptr || OtherActor == GetInstigator())
	{
		return;
	}

	// Allies are passed through
	if (UVaelCombatStatics::ApplySpellHit(GetInstigator(), OtherActor, Hit, GetVelocity()))
	{
		bHasHit = true;
		Destroy();
	}
}

void AVaelSpellProjectile::OnStopped(const FHitResult& ImpactResult)
{
	Destroy();
}
