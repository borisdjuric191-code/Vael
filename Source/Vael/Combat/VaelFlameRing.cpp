// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VaelFlameRing.h"
#include "UObject/ConstructorHelpers.h"
#include "Combat/VaelCharacterBase.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	/** Scale of a single sphere of the ring */
	constexpr float FlameScale = 0.55f;
}

AVaelFlameRing::AVaelFlameRing()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Flames = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Flames"));
	Flames->SetupAttachment(RootComponent);
	Flames->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Flames->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> FlameMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (FlameMesh.Succeeded())
	{
		Flames->SetStaticMesh(FlameMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FlameMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (FlameMaterial.Succeeded())
	{
		Flames->SetMaterial(0, FlameMaterial.Object);
	}

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

AVaelFlameRing* AVaelFlameRing::SpawnRing(APawn* Attacker, const FVector& GroundLocation, const FVaelSpellHit& InHit, float InSpeed, float InMaxRadius, float InHalfWidth, const FLinearColor& Color)
{
	UWorld* World = Attacker != nullptr ? Attacker->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}

	const FTransform SpawnTransform(GroundLocation);

	AVaelFlameRing* Ring = World->SpawnActorDeferred<AVaelFlameRing>(AVaelFlameRing::StaticClass(), SpawnTransform, Attacker, Attacker, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Ring == nullptr)
	{
		return nullptr;
	}

	Ring->Hit = InHit;
	Ring->Speed = InSpeed;
	Ring->MaxRadius = InMaxRadius;
	Ring->HalfWidth = InHalfWidth;

	if (UMaterialInstanceDynamic* FlameMaterial = Ring->Flames->CreateAndSetMaterialInstanceDynamic(0))
	{
		FlameMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	}

	for (int32 FlameIndex = 0; FlameIndex < Ring->FlameCount; ++FlameIndex)
	{
		Ring->Flames->AddInstance(FTransform::Identity);
	}

	Ring->FinishSpawning(SpawnTransform);
	Ring->RefreshFlames();

	return Ring;
}

void AVaelFlameRing::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Radius += Speed * DeltaSeconds;
	if (Radius > MaxRadius)
	{
		Destroy();
		return;
	}

	RefreshFlames();

	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		AVaelCharacterBase* Target = *It;
		if (HitActors.Contains(Target))
		{
			continue;
		}

		const float Distance = FVector::Dist2D(Target->GetActorLocation(), GetActorLocation());

		// Only a hit that lands counts: a dodging target is still caught if it stops inside the line
		if (FMath::Abs(Distance - Radius) <= HalfWidth && UVaelCombatStatics::ApplySpellHit(GetInstigator(), Target, Hit, Target->GetActorLocation() - GetActorLocation()))
		{
			HitActors.Add(Target);
		}
	}
}

void AVaelFlameRing::RefreshFlames()
{
	const int32 NumFlames = Flames->GetInstanceCount();

	for (int32 FlameIndex = 0; FlameIndex < NumFlames; ++FlameIndex)
	{
		const float Angle = UE_TWO_PI * FlameIndex / NumFlames;
		const FVector Location(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, FlameHeight);

		Flames->UpdateInstanceTransform(FlameIndex, FTransform(FQuat::Identity, Location, FVector(FlameScale)), false, FlameIndex == NumFlames - 1, true);
	}
}
