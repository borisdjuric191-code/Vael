// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VaelGroundStrike.h"
#include "UObject/ConstructorHelpers.h"
#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelHitFeedbackSubsystem.h"
#include "Components/PointLightComponent.h"
#include "Magic/VaelDebrisBurst.h"
#include "Magic/VaelGroundArea.h"
#include "Magic/VaelLightningBolt.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelSpellEffects.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	/** Lightning comes down from this high above the ground, in cm */
	constexpr float StrikeLightningHeight = 900.0f;

	/** Size of the engine basic shapes used as placeholders */
	constexpr float StrikeShapeSize = 100.0f;

	/** Height of the warning disc in cm */
	constexpr float WarningDiscHeight = 2.0f;

	/** Seconds the spike needs to shoot out of the ground, and the share of its time it spends sinking back at the end */
	constexpr float SpikeRiseTime = 0.07f;
	constexpr float SpikeSinkShare = 0.45f;

	/** Spikes in a cluster of bone or rock, glow of the ground at their foot and its light in candela */
	constexpr int32 ClusterSpikeCount = 4;
	constexpr float ClusterBoneGlow = 2.5f;
	constexpr float ClusterMagmaGlow = 4.0f;
	constexpr float ClusterLightIntensity = 15.0f;
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

		// The spikes of a cluster grow along their lean, the glow at their foot dies as they sink
		for (int32 SpikeIndex = 0; SpikeIndex < ClusterSpikes.Num(); ++SpikeIndex)
		{
			const float SpikeLength = FMath::Max(ClusterHeights[SpikeIndex] * Height, 1.0f);

			ClusterSpikes[SpikeIndex]->SetRelativeLocation(ClusterBases[SpikeIndex] + ClusterLeans[SpikeIndex].RotateVector(FVector(0.0f, 0.0f, SpikeLength * 0.5f)));
			ClusterSpikes[SpikeIndex]->SetRelativeScale3D(FVector(ClusterWidths[SpikeIndex], ClusterWidths[SpikeIndex], SpikeLength) / StrikeShapeSize);
		}

		if (ClusterGlow != nullptr)
		{
			VaelEffects::SetLookGlow(ClusterGlow, (Look == EVaelStrikeLook::BoneSpikes ? ClusterBoneGlow : ClusterMagmaGlow) * Sink);
			ClusterLight->SetIntensity(ClusterLightIntensity * Sink * FMath::FRandRange(0.8f, 1.0f));
		}
	}
}

void AVaelGroundStrike::BuildSpikeCluster()
{
	UMaterialInterface* CoreMaterial = nullptr;
	UMaterialInterface* GlowMaterial = nullptr;
	VaelEffects::LoadLookMaterials(CoreMaterial, GlowMaterial);

	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	const bool bBone = Look == EVaelStrikeLook::BoneSpikes;
	const FLinearColor GlowColor = MagicSettings->GetElementColor(bBone ? EVaelElement::Mark : EVaelElement::Fire);
	const FLinearColor Bone(0.85f, 0.8f, 0.68f);
	const FLinearColor Rock(0.1f, 0.07f, 0.06f);

	// A big spike in the middle, smaller ones leaning out around it; bones are slender, rock is thick and its edges glow
	const float FirstAngle = FMath::FRandRange(0.0f, 360.0f);
	for (int32 SpikeIndex = 0; SpikeIndex < ClusterSpikeCount; ++SpikeIndex)
	{
		const bool bMain = SpikeIndex == 0;
		const float Angle = FMath::DegreesToRadians(FirstAngle + SpikeIndex * 360.0f / (ClusterSpikeCount - 1) + FMath::FRandRange(-20.0f, 20.0f));
		const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);

		const float Height = SpikeHeight * (bMain ? 1.0f : FMath::FRandRange(0.45f, 0.75f)) * (bBone ? 1.1f : 0.9f);
		const float Width = Radius * (bMain ? 0.55f : FMath::FRandRange(0.25f, 0.4f)) * (bBone ? 0.45f : 1.0f);
		const float Lean = bMain ? 5.0f : FMath::FRandRange(bBone ? 15.0f : 10.0f, bBone ? 30.0f : 20.0f);
		const FVector Base = bMain ? FVector::ZeroVector : Out * Radius * FMath::FRandRange(0.25f, 0.55f);
		const FRotator Leaning = FRotationMatrix::MakeFromZ(Out * FMath::Tan(FMath::DegreesToRadians(Lean)) + FVector::UpVector).Rotator();

		const auto AddSpike = [&](UMaterialInterface* Material, const FLinearColor& Color, float Glow, float Rim, float Grow)
		{
			UStaticMeshComponent* Shape = VaelEffects::AddLookShape(this, Material, Color, Glow, Rim);
			Shape->SetStaticMesh(Spike->GetStaticMesh());
			Shape->SetRelativeRotation(Leaning);
			Shape->SetVisibility(true);

			ClusterSpikes.Add(Shape);
			ClusterBases.Add(Base);
			ClusterLeans.Add(Leaning);
			ClusterHeights.Add(Height * Grow);
			ClusterWidths.Add(Width * Grow);
		};

		AddSpike(CoreMaterial, bBone ? Bone : Rock, bBone ? 0.15f : 0.0f, 0.0f, 1.0f);

		if (!bBone)
		{
			AddSpike(GlowMaterial, GlowColor, 2.0f, 1.0f, 1.1f);
		}
	}

	// The ground at the foot glows: violet for bone, molten for rock
	ClusterGlow = VaelEffects::AddLookShape(this, GlowMaterial, GlowColor, bBone ? ClusterBoneGlow : ClusterMagmaGlow, bBone ? 1.0f : 0.4f);
	ClusterGlow->SetRelativeLocation(FVector(0.0f, 0.0f, 3.0f));
	ClusterGlow->SetRelativeScale3D(FVector(Radius * 1.6f, Radius * 1.6f, 8.0f) / StrikeShapeSize);
	ClusterGlow->SetVisibility(true);

	ClusterLight = NewObject<UPointLightComponent>(this);
	ClusterLight->SetupAttachment(RootComponent);
	ClusterLight->SetRelativeLocation(FVector(0.0f, 0.0f, 40.0f));
	ClusterLight->SetMobility(EComponentMobility::Movable);
	ClusterLight->SetIntensityUnits(ELightUnits::Candelas);
	ClusterLight->SetLightColor(GlowColor);
	ClusterLight->SetAttenuationRadius(Radius * 4.0f);
	ClusterLight->SetCastShadows(false);
	ClusterLight->RegisterComponent();

	// Bone splinters fly and violet motes rise; or lava splashes up among chunks of rock
	FVaelDebris Pieces;
	Pieces.Count = bBone ? 6 : 5;
	Pieces.Spread = Radius * 0.4f;
	Pieces.Speed = 220.0f;
	Pieces.Lift = 300.0f;
	Pieces.MinSize = 4.0f;
	Pieces.MaxSize = 9.0f;
	Pieces.Color = bBone ? Bone : Rock;
	Pieces.RockShare = bBone ? 0.0f : 0.4f;
	AVaelDebrisBurst::Spawn(this, GetActorLocation(), Pieces);

	FVaelDebris Glowing;
	Glowing.Count = bBone ? 6 : 9;
	Glowing.Spread = Radius * 0.4f;
	Glowing.Speed = bBone ? 120.0f : 260.0f;
	Glowing.Lift = bBone ? 150.0f : 480.0f;
	Glowing.Gravity = bBone ? -200.0f : 1100.0f;
	Glowing.Lifetime = 0.75f;
	Glowing.MinSize = bBone ? 3.0f : 5.0f;
	Glowing.MaxSize = bBone ? 6.0f : 10.0f;
	Glowing.Color = bBone ? GlowColor : FMath::Lerp(GlowColor, FLinearColor(1.0f, 0.85f, 0.4f), 0.4f);
	Glowing.Glow = bBone ? 5.0f : 6.0f;
	Glowing.bLandOnGround = !bBone;
	AVaelDebrisBurst::Spawn(this, GetActorLocation() + FVector(0.0f, 0.0f, 10.0f), Glowing);
}

void AVaelGroundStrike::Strike()
{
	bStruck = true;

	WarningDisc->SetVisibility(false);
	WarningFill->SetVisibility(false);

	// Lightning comes down from the sky instead of the spike rising
	if (Hit.bLightning)
	{
		const FVector Ground = GetActorLocation();
		const FVector Sky = Ground + FVector(FMath::FRandRange(-150.0f, 150.0f), FMath::FRandRange(-150.0f, 150.0f), StrikeLightningHeight);

		AVaelLightningBolt::Spawn(GetOwner(), Sky, Ground, UVaelMagicSettings::Get()->GetElementColor(Hit.Element), 0.3f, 1.6f, true);
	}
	else if (Look != EVaelStrikeLook::Spike)
	{
		BuildSpikeCluster();
	}
	else
	{
		Spike->SetVisibility(true);
	}

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
