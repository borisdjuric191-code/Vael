// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VaelGroundStrike.h"
#include "UObject/ConstructorHelpers.h"
#include "Combat/VaelCharacterBase.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	/** Size of the engine basic shapes used as placeholders */
	constexpr float StrikeShapeSize = 100.0f;

	/** Height of the warning disc in cm */
	constexpr float WarningDiscHeight = 2.0f;
}

AVaelGroundStrike::AVaelGroundStrike()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	WarningDisc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WarningDisc"));
	WarningDisc->SetupAttachment(RootComponent);
	WarningDisc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WarningDisc->SetCastShadow(false);

	Spike = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Spike"));
	Spike->SetupAttachment(RootComponent);
	Spike->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Spike->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DiscMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (DiscMesh.Succeeded())
	{
		WarningDisc->SetStaticMesh(DiscMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SpikeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (SpikeMesh.Succeeded())
	{
		Spike->SetStaticMesh(SpikeMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMaterial.Succeeded())
	{
		WarningDisc->SetMaterial(0, ShapeMaterial.Object);
		Spike->SetMaterial(0, ShapeMaterial.Object);
	}

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

AVaelGroundStrike* AVaelGroundStrike::SpawnStrike(AActor* Attacker, const FVector& GroundLocation, const FVaelSpellHit& InHit, float InRadius, float InDelay, const FLinearColor& Color)
{
	UWorld* World = Attacker != nullptr ? Attacker->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}

	const FTransform SpawnTransform(GroundLocation);

	AVaelGroundStrike* GroundStrike = World->SpawnActorDeferred<AVaelGroundStrike>(AVaelGroundStrike::StaticClass(), SpawnTransform, Attacker, Cast<APawn>(Attacker), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (GroundStrike == nullptr)
	{
		return nullptr;
	}

	GroundStrike->Hit = InHit;
	GroundStrike->Radius = InRadius;
	GroundStrike->Delay = InDelay;

	// The warning is a dark version of the color, the spike the color itself
	const float Diameter = InRadius * 2.0f / StrikeShapeSize;
	GroundStrike->WarningDisc->SetRelativeLocation(FVector(0.0f, 0.0f, WarningDiscHeight * 0.5f));
	GroundStrike->WarningDisc->SetRelativeScale3D(FVector(Diameter, Diameter, WarningDiscHeight / StrikeShapeSize));
	GroundStrike->Spike->SetRelativeLocation(FVector(0.0f, 0.0f, GroundStrike->SpikeHeight * 0.5f));
	GroundStrike->Spike->SetRelativeScale3D(FVector(Diameter * 0.5f, Diameter * 0.5f, GroundStrike->SpikeHeight / StrikeShapeSize));

	if (UMaterialInstanceDynamic* WarningMaterial = GroundStrike->WarningDisc->CreateAndSetMaterialInstanceDynamic(0))
	{
		WarningMaterial->SetVectorParameterValue(TEXT("Color"), Color * 0.2f);
	}

	if (UMaterialInstanceDynamic* SpikeMaterial = GroundStrike->Spike->CreateAndSetMaterialInstanceDynamic(0))
	{
		SpikeMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	}

	GroundStrike->FinishSpawning(SpawnTransform);
	return GroundStrike;
}

void AVaelGroundStrike::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Elapsed += DeltaSeconds;

	if (!bStruck && Elapsed >= Delay)
	{
		Strike();
	}
	else if (bStruck && Elapsed >= Delay + SpikeDuration)
	{
		Destroy();
	}
}

void AVaelGroundStrike::Strike()
{
	bStruck = true;

	WarningDisc->SetVisibility(false);
	Spike->SetVisibility(true);

	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		if (FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) <= Radius)
		{
			// Pawns hurt their enemies; anything else, like a Mark source, only hurts players
			UVaelCombatStatics::ApplySpellHit(GetOwner(), *It, Hit, It->GetActorLocation() - GetActorLocation());
		}
	}
}
