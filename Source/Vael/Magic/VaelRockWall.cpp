// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelRockWall.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Magic/VaelDebrisBurst.h"
#include "Magic/VaelMagicSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "VaelAssets.h"

namespace
{
	/** Edge length of the engine cube mesh used as placeholder */
	constexpr float PlaceholderCubeSize = 100.0f;

	/** Seconds the rocks need to crumble and sink when the block ends */
	constexpr float RockWallCrumbleDuration = 0.4f;

	/** How far the rocks shoot past their place when they rise, as in an ease-out-back curve */
	constexpr float RockWallOvershoot = 1.7f;

	/** A wall of flesh hurts those touching it every this many seconds, and reaches this far past its edge in cm */
	constexpr float RockWallContactInterval = 0.5f;
	constexpr float RockWallContactReach = 25.0f;
}

AVaelRockWall::AVaelRockWall()
{
	// Static like the rest of the level: stops characters and projectiles and counts as rock for the earth element
	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	Collision->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Collision->SetCollisionObjectType(ECC_WorldStatic);
	Collision->SetMobility(EComponentMobility::Movable);
	Collision->CanCharacterStepUpOn = ECB_No;
	RootComponent = Collision;

	// Placeholder look from engine assets
	Block = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Block"));
	Block->SetupAttachment(RootComponent);
	Block->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Block->bReceivesDecals = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Block->SetStaticMesh(CubeMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CubeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (CubeMaterial.Succeeded())
	{
		Block->SetMaterial(0, CubeMaterial.Object);
	}

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AVaelRockWall::BeginPlay()
{
	Super::BeginPlay();

	// A wall of bone and flesh keeps its block, dark red with a shimmer of the Mark; it pulses in Tick
	if (bHurtsOnContact)
	{
		if (UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Block->GetMaterial(0)))
		{
			Material->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(FLinearColor(0.35f, 0.04f, 0.08f), UVaelMagicSettings::Get()->GetElementColor(EVaelElement::Mark), 0.25f));
		}
	}

	UStaticMesh* RockMesh = bHurtsOnContact ? nullptr : VaelAssets::LoadOptional(UVaelMagicSettings::Get()->ElementOrbRockMesh);
	if (RockMesh != nullptr)
	{
		Block->SetVisibility(false);

		// A big rock at the bottom and a smaller one leaning on top, each turned at random, together filling the block
		const FBoxSphereBounds Bounds = RockMesh->GetBounds();
		const FVector MeshSize = FVector::Max(Bounds.BoxExtent * 2.0f, FVector(1.0f));

		for (const FVector& Layer : { FVector(1.05f, 0.7f, -0.15f), FVector(0.8f, 0.55f, 0.22f) })
		{
			UStaticMeshComponent* Rock = NewObject<UStaticMeshComponent>(this);
			Rock->SetupAttachment(RootComponent);
			Rock->SetStaticMesh(RockMesh);
			Rock->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Rock->bReceivesDecals = false;
			Rock->RegisterComponent();

			const FVector Scale = FVector(BlockSize * Layer.X * FMath::FRandRange(0.9f, 1.1f), BlockSize * Layer.X * FMath::FRandRange(0.9f, 1.1f), FullHeight * Layer.Y) / MeshSize;
			const FRotator Rotation(FMath::FRandRange(-8.0f, 8.0f), FMath::FRandRange(0.0f, 360.0f), FMath::FRandRange(-8.0f, 8.0f));
			const FVector Center(FMath::FRandRange(-0.1f, 0.1f) * BlockSize, FMath::FRandRange(-0.1f, 0.1f) * BlockSize, FullHeight * Layer.Z);

			// The pivot of the rock may not be its middle
			Rock->SetRelativeRotation(Rotation);
			Rock->SetRelativeScale3D(Scale);

			Rocks.Add(Rock);
			RockPlaces.Add(Center - Rotation.RotateVector(Scale * Bounds.Origin));
			RockScales.Add(Scale);
		}

		Tick(0.0f);
	}

	// Dust and pebbles burst where the block breaks out of the ground
	FVaelDebris Dust;
	Dust.Count = 10;
	Dust.Spread = BlockSize * 0.5f;
	Dust.Speed = 260.0f;
	Dust.Lift = 380.0f;
	Dust.Lifetime = 0.7f;
	Dust.RockShare = 0.2f;
	AVaelDebrisBurst::Spawn(this, GetActorLocation() - FVector(0.0f, 0.0f, FullHeight * 0.5f), Dust);
}

void AVaelRockWall::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bCrumbling)
	{
		// The rocks break, sink back into the ground and are gone
		CrumbleTime += DeltaSeconds;
		const float Sink = FMath::Clamp(CrumbleTime / RockWallCrumbleDuration, 0.0f, 1.0f);

		for (int32 RockIndex = 0; RockIndex < Rocks.Num(); ++RockIndex)
		{
			Rocks[RockIndex]->SetRelativeLocation(RockPlaces[RockIndex] - FVector(0.0f, 0.0f, FullHeight * 0.8f * Sink * Sink));
			Rocks[RockIndex]->SetRelativeScale3D(RockScales[RockIndex] * (1.0f - 0.5f * Sink));
		}

		if (Sink >= 1.0f)
		{
			Destroy();
		}

		return;
	}

	// The look rises out of the ground, the collision is there right away
	RiseTime += DeltaSeconds;
	const float Rise = RiseDuration > 0.0f ? FMath::Clamp(RiseTime / RiseDuration, 0.0f, 1.0f) : 1.0f;

	if (!Rocks.IsEmpty())
	{
		// The rocks shoot a little past their place and settle back
		const float Back = Rise - 1.0f;
		const float Settle = 1.0f + (RockWallOvershoot + 1.0f) * Back * Back * Back + RockWallOvershoot * Back * Back;

		for (int32 RockIndex = 0; RockIndex < Rocks.Num(); ++RockIndex)
		{
			Rocks[RockIndex]->SetRelativeLocation(RockPlaces[RockIndex] - FVector(0.0f, 0.0f, FullHeight * (1.0f - Settle)));
		}
	}
	else
	{
		const FVector Scale = Block->GetRelativeScale3D();
		Block->SetRelativeScale3D(FVector(Scale.X, Scale.Y, FMath::Max(0.01f, FullHeight * Rise / PlaceholderCubeSize)));
		Block->SetRelativeLocation(FVector(0.0f, 0.0f, -FullHeight * 0.5f * (1.0f - Rise)));
	}

	if (bHurtsOnContact)
	{
		// Flesh pulses like a heart and hurts whoever touches it
		const FVector Scale = Block->GetRelativeScale3D();
		const float Beat = 1.0f + 0.05f * FMath::Max(0.0f, FMath::Sin(RiseTime * 7.0f));
		Block->SetRelativeScale3D(FVector(BlockSize / PlaceholderCubeSize * Beat, BlockSize / PlaceholderCubeSize * Beat, Scale.Z));

		ContactCountdown -= DeltaSeconds;
		if (ContactCountdown <= 0.0f)
		{
			ContactCountdown = RockWallContactInterval;
			HurtTouchingEnemies();
		}

		return;
	}

	if (Rise >= 1.0f)
	{
		SetActorTickEnabled(false);
	}
}

void AVaelRockWall::HurtTouchingEnemies()
{
	APawn* Caster = GetInstigator();
	if (Caster == nullptr)
	{
		return;
	}

	// Everyone pressed against the block, a hand's breadth around it
	const FVector Extent = Collision->GetScaledBoxExtent() + FVector(RockWallContactReach, RockWallContactReach, 0.0f);

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VaelWallContact), false, this);
	GetWorld()->OverlapMultiByObjectType(Overlaps, GetActorLocation(), GetActorQuat(), FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeBox(Extent), QueryParams);

	TSet<AActor*> Hurt;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		if (Target != nullptr && !Hurt.Contains(Target))
		{
			Hurt.Add(Target);
			UVaelCombatStatics::ApplySpellHit(Caster, Target, ContactHit, Target->GetActorLocation() - GetActorLocation());
		}
	}
}

void AVaelRockWall::LifeSpanExpired()
{
	// Without rocks the box just goes
	if (Rocks.IsEmpty())
	{
		Super::LifeSpanExpired();
		return;
	}

	if (bCrumbling)
	{
		return;
	}

	// Nothing is blocked any more while the rocks break apart
	bCrumbling = true;
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetActorTickEnabled(true);

	FVaelDebris Rubble;
	Rubble.Count = 14;
	Rubble.Spread = BlockSize * 0.45f;
	Rubble.Speed = 180.0f;
	Rubble.Lift = 200.0f;
	Rubble.Lifetime = 0.8f;
	Rubble.MinSize = 8.0f;
	Rubble.MaxSize = 16.0f;
	Rubble.RockShare = 0.35f;
	AVaelDebrisBurst::Spawn(this, GetActorLocation(), Rubble);
}

AVaelRockWall* AVaelRockWall::RaiseBlock(UWorld* World, const FVector& GroundLocation, float Yaw, float Size, float Height, float Lifetime, APawn* InInstigator, const FVaelSpellHit* InContactHit)
{
	if (World == nullptr)
	{
		return nullptr;
	}

	const FTransform SpawnTransform(FRotator(0.0f, Yaw, 0.0f), GroundLocation + FVector(0.0f, 0.0f, Height * 0.5f));

	AVaelRockWall* Wall = World->SpawnActorDeferred<AVaelRockWall>(AVaelRockWall::StaticClass(), SpawnTransform, InInstigator, InInstigator, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Wall != nullptr)
	{
		if (InContactHit != nullptr)
		{
			Wall->bHurtsOnContact = true;
			Wall->ContactHit = *InContactHit;
		}

		Wall->SetBlockSize(Size, Height);
		Wall->InitialLifeSpan = Lifetime;
		Wall->FinishSpawning(SpawnTransform);
	}

	return Wall;
}

void AVaelRockWall::SetBlockSize(float Size, float Height)
{
	FullHeight = Height;
	BlockSize = Size;

	Collision->SetBoxExtent(FVector(Size * 0.5f, Size * 0.5f, Height * 0.5f));
	Block->SetRelativeScale3D(FVector(Size / PlaceholderCubeSize, Size / PlaceholderCubeSize, 0.01f));

	if (UMaterialInstanceDynamic* Material = Block->CreateAndSetMaterialInstanceDynamic(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), UVaelMagicSettings::Get()->GetElementColor(EVaelElement::Earth));
	}
}
