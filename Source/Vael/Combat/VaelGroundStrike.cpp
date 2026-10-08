// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VaelGroundStrike.h"
#include "UObject/ConstructorHelpers.h"
#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelHitFeedbackSubsystem.h"
#include "Magic/VaelGroundArea.h"
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

	/** Seconds the spike needs to shoot out of the ground, and the share of its time it spends sinking back at the end */
	constexpr float SpikeRiseTime = 0.07f;
	constexpr float SpikeSinkShare = 0.45f;
}

AVaelGroundStrike::AVaelGroundStrike()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	WarningDisc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WarningDisc"));
	WarningDisc->SetupAttachment(RootComponent);
	WarningDisc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WarningDisc->SetCastShadow(false);

	WarningFill = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WarningFill"));
	WarningFill->SetupAttachment(RootComponent);
	WarningFill->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WarningFill->SetCastShadow(false);

	Spike = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Spike"));
	Spike->SetupAttachment(RootComponent);
	Spike->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Spike->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DiscMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (DiscMesh.Succeeded())
	{
		WarningDisc->SetStaticMesh(DiscMesh.Object);
		WarningFill->SetStaticMesh(DiscMesh.Object);
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
		WarningFill->SetMaterial(0, ShapeMaterial.Object);
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

	// The fill starts as a point and lies just above the warning
	GroundStrike->WarningScale = FVector(Diameter, Diameter, WarningDiscHeight / StrikeShapeSize);
	GroundStrike->SpikeScale = GroundStrike->Spike->GetRelativeScale3D();
	GroundStrike->WarningFill->SetRelativeLocation(FVector(0.0f, 0.0f, WarningDiscHeight * 1.5f));
	GroundStrike->WarningFill->SetRelativeScale3D(FVector(0.0f, 0.0f, GroundStrike->WarningScale.Z));

	if (UMaterialInstanceDynamic* FillMaterial = GroundStrike->WarningFill->CreateAndSetMaterialInstanceDynamic(0))
	{
		FillMaterial->SetVectorParameterValue(TEXT("Color"), Color * 0.55f);
	}

	if (UMaterialInstanceDynamic* SpikeMaterial = GroundStrike->Spike->CreateAndSetMaterialInstanceDynamic(0))
	{
		SpikeMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	}

	GroundStrike->FinishSpawning(SpawnTransform);
	return GroundStrike;
}

void AVaelGroundStrike::SetAftermath(const FVaelStrikeAftermath& InAftermath, float InShake)
{
	Aftermath = InAftermath;
	Shake = InShake;
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
		return;
	}

	if (!bStruck)
	{
		// The fill closes in on the edge of the warning, faster towards the end, so the moment of the strike can be read
		const float Progress = Delay > 0.0f ? FMath::Clamp(Elapsed / Delay, 0.0f, 1.0f) : 1.0f;
		const float Fill = FMath::Square(Progress);
		WarningFill->SetRelativeScale3D(FVector(WarningScale.X * Fill, WarningScale.Y * Fill, WarningScale.Z));
	}
	else
	{
		// The spike shoots out of the ground, stands and sinks back
		const float SinceStrike = Elapsed - Delay;
		const float Rise = SpikeRiseTime > 0.0f ? FMath::Clamp(SinceStrike / SpikeRiseTime, 0.0f, 1.0f) : 1.0f;
		const float SinkStart = SpikeDuration * (1.0f - SpikeSinkShare);
		const float Sink = SinceStrike > SinkStart ? 1.0f - FMath::Clamp((SinceStrike - SinkStart) / FMath::Max(SpikeDuration - SinkStart, KINDA_SMALL_NUMBER), 0.0f, 1.0f) : 1.0f;
		const float Height = FMath::Max(Rise * Sink, 0.01f);

		Spike->SetRelativeScale3D(FVector(SpikeScale.X, SpikeScale.Y, SpikeScale.Z * Height));
		Spike->SetRelativeLocation(FVector(0.0f, 0.0f, SpikeHeight * 0.5f * Height));
	}
}

void AVaelGroundStrike::Strike()
{
	bStruck = true;

	WarningDisc->SetVisibility(false);
	WarningFill->SetVisibility(false);
	Spike->SetVisibility(true);

	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		if (FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) <= Radius)
		{
			// Pawns hurt their enemies; anything else, like a Mark source, only hurts players
			UVaelCombatStatics::ApplySpellHit(GetOwner(), *It, Hit, It->GetActorLocation() - GetActorLocation());
		}
	}

	// Water puts out the fires it lands on
	if (Hit.Element == EVaelElement::Water)
	{
		AVaelGroundArea::ExtinguishFires(GetWorld(), GetActorLocation(), Radius);
	}

	if (Aftermath.Radius > 0.0f)
	{
		AVaelGroundArea::SpawnArea(GetWorld(), GetActorLocation() + FVector(0.0f, 0.0f, 2.0f), Aftermath.Element, Aftermath.Radius, Aftermath.Lifetime,
			Aftermath.DamagePerSecond, GetInstigator(), true, Aftermath.Effect);
	}

	if (Shake > 0.0f)
	{
		UVaelHitFeedbackSubsystem::Shake(this, Shake);
	}
}
