// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelLightningBolt.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Magic/VaelSpellEffects.h"

namespace
{
	/** Diameter of the engine sphere mesh in cm */
	constexpr float BoltSphereSize = 100.0f;

	/** Each piece of the main line is about this long in cm, and the line has this many pieces at least and at most */
	constexpr float BoltSegmentLength = 38.0f;
	constexpr int32 BoltMinSegments = 5;
	constexpr int32 BoltMaxSegments = 22;

	/** Forks branching off the main line and pieces in each */
	constexpr int32 BoltForks = 3;
	constexpr int32 BoltForkSegments = 3;

	/** The line strays sideways by up to this share of its length, never more than the cap in cm */
	constexpr float BoltJagShare = 0.09f;
	constexpr float BoltJagCap = 70.0f;

	/** Seconds between two shapes of the bolt */
	constexpr float BoltRejagInterval = 0.05f;

	/** Width of the white-hot line and of its halo in cm */
	constexpr float BoltCoreWidth = 4.0f;
	constexpr float BoltHaloWidth = 18.0f;

	/** Glow of line and halo */
	constexpr float BoltCoreGlow = 14.0f;
	constexpr float BoltHaloGlow = 2.2f;

	/** Sparks where it strikes, seconds they fly, how they fall in cm/s², their size in cm */
	constexpr int32 BoltSparkCount = 8;
	constexpr float BoltSparkLifetime = 0.35f;
	constexpr float BoltSparkGravity = 900.0f;
	constexpr float BoltSparkSize = 5.0f;

	/** Brightness of the flash at the ends in candela */
	constexpr float BoltLightIntensity = 20.0f;
}

AVaelLightningBolt::AVaelLightningBolt()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	PrimaryActorTick.bCanEverTick = true;
}

AVaelLightningBolt* AVaelLightningBolt::Spawn(AActor* Caster, const FVector& From, const FVector& To, const FLinearColor& Color, float Duration, float Thickness, bool bSparks)
{
	UWorld* World = Caster != nullptr ? Caster->GetWorld() : nullptr;
	if (World == nullptr || FVector::DistSquared(From, To) < 1.0f)
	{
		return nullptr;
	}

	const FTransform SpawnTransform(From);

	AVaelLightningBolt* Bolt = World->SpawnActorDeferred<AVaelLightningBolt>(AVaelLightningBolt::StaticClass(), SpawnTransform, Caster, Cast<APawn>(Caster), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Bolt != nullptr)
	{
		Bolt->From = From;
		Bolt->To = To;
		Bolt->BoltColor = Color;
		Bolt->Duration = FMath::Max(Duration, 0.05f);
		Bolt->Thickness = Thickness;
		Bolt->bSparks = bSparks;
		Bolt->FinishSpawning(SpawnTransform);
	}

	return Bolt;
}

void AVaelLightningBolt::BeginPlay()
{
	Super::BeginPlay();

	UMaterialInterface* CoreMaterial = nullptr;
	UMaterialInterface* GlowMaterial = nullptr;
	VaelEffects::LoadLookMaterials(CoreMaterial, GlowMaterial);

	const FLinearColor WhiteHot = FMath::Lerp(BoltColor, FLinearColor::White, 0.75f);

	// The main line, then the forks; every piece lies in the world by itself
	const int32 MainSegments = FMath::Clamp(FMath::RoundToInt(FVector::Dist(From, To) / BoltSegmentLength), BoltMinSegments, BoltMaxSegments);
	const int32 NumSegments = MainSegments + BoltForks * BoltForkSegments;

	for (int32 SegmentIndex = 0; SegmentIndex < NumSegments; ++SegmentIndex)
	{
		for (TArray<TObjectPtr<UStaticMeshComponent>>* Pieces : { &Cores, &Halos })
		{
			const bool bCore = Pieces == &Cores;
			UStaticMeshComponent* Piece = VaelEffects::AddLookShape(this, GlowMaterial, bCore ? WhiteHot : BoltColor, bCore ? BoltCoreGlow : BoltHaloGlow);
			Piece->SetUsingAbsoluteLocation(true);
			Piece->SetUsingAbsoluteRotation(true);
			Piece->SetUsingAbsoluteScale(true);
			Pieces->Add(Piece);
		}
	}

	SegmentFlicker.Init(1.0f, NumSegments);

	// Sparks spray from where it strikes, upwards more than down
	for (int32 SparkIndex = 0; bSparks && SparkIndex < BoltSparkCount; ++SparkIndex)
	{
		UStaticMeshComponent* Spark = VaelEffects::AddLookShape(this, GlowMaterial, WhiteHot, 8.0f);
		Spark->SetUsingAbsoluteLocation(true);
		Spark->SetUsingAbsoluteScale(true);
		Sparks.Add(Spark);

		FVector Direction = FMath::VRand();
		Direction.Z = FMath::Abs(Direction.Z);
		SparkVelocities.Add(Direction * FMath::FRandRange(300.0f, 700.0f) + FVector(0.0f, 0.0f, 150.0f));
	}

	for (TObjectPtr<UPointLightComponent>* Light : { &StartLight, &EndLight })
	{
		*Light = NewObject<UPointLightComponent>(this);
		(*Light)->SetupAttachment(RootComponent);
		(*Light)->SetUsingAbsoluteLocation(true);
		(*Light)->SetMobility(EComponentMobility::Movable);
		(*Light)->SetIntensityUnits(ELightUnits::Candelas);
		(*Light)->SetIntensity(0.0f);
		(*Light)->SetLightColor(WhiteHot);
		(*Light)->SetAttenuationRadius(450.0f);
		(*Light)->SetCastShadows(false);
		(*Light)->RegisterComponent();
	}

	StartLight->SetWorldLocation(From);
	EndLight->SetWorldLocation(To);

	Rejag();
	Tick(0.0f);
}

void AVaelLightningBolt::PlaceSegment(int32 Index, const FVector& Start, const FVector& End, float Width)
{
	const FVector Middle = (Start + End) * 0.5f;
	const FRotator Rotation = (End - Start).Rotation();
	const float Length = FVector::Dist(Start, End) * 1.1f;

	Cores[Index]->SetWorldLocationAndRotation(Middle, Rotation);
	Cores[Index]->SetWorldScale3D(FVector(Length, BoltCoreWidth * Width, BoltCoreWidth * Width) / BoltSphereSize);

	Halos[Index]->SetWorldLocationAndRotation(Middle, Rotation);
	Halos[Index]->SetWorldScale3D(FVector(Length, BoltHaloWidth * Width, BoltHaloWidth * Width) / BoltSphereSize);
}

void AVaelLightningBolt::Rejag()
{
	const FVector Line = To - From;
	const float Length = Line.Size();
	const FVector Direction = Line / Length;

	FVector Side = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();
	if (Side.IsNearlyZero())
	{
		Side = FVector::RightVector;
	}

	const FVector Lift = FVector::CrossProduct(Side, Direction);
	const float Jag = FMath::Min(Length * BoltJagShare, BoltJagCap);
	const int32 MainSegments = Cores.Num() - BoltForks * BoltForkSegments;

	// The ends stay put, the middle strays furthest
	TArray<FVector> Points;
	for (int32 PointIndex = 0; PointIndex <= MainSegments; ++PointIndex)
	{
		const float Along = PointIndex / static_cast<float>(MainSegments);
		const float Stray = FMath::Sin(Along * UE_PI) * Jag;

		Points.Add(From + Line * Along + (Side * FMath::FRandRange(-1.0f, 1.0f) + Lift * FMath::FRandRange(-0.5f, 0.5f)) * Stray);
	}

	for (int32 SegmentIndex = 0; SegmentIndex < MainSegments; ++SegmentIndex)
	{
		PlaceSegment(SegmentIndex, Points[SegmentIndex], Points[SegmentIndex + 1], Thickness);
	}

	// Forks split off somewhere along the line and get thinner
	const float ForkStep = FMath::Min(Length * 0.08f, 40.0f);
	for (int32 ForkIndex = 0; ForkIndex < BoltForks; ++ForkIndex)
	{
		FVector Start = Points[FMath::RandRange(1, MainSegments - 1)];
		const FVector ForkDirection = (Direction + Side * FMath::FRandRange(-1.2f, 1.2f) + Lift * FMath::FRandRange(-0.3f, 0.5f)).GetSafeNormal();

		for (int32 Step = 0; Step < BoltForkSegments; ++Step)
		{
			const FVector End = Start + (ForkDirection + FMath::VRand() * 0.4f).GetSafeNormal() * ForkStep;
			PlaceSegment(MainSegments + ForkIndex * BoltForkSegments + Step, Start, End, Thickness * (0.55f - 0.12f * Step));
			Start = End;
		}
	}

	for (float& Flicker : SegmentFlicker)
	{
		Flicker = FMath::FRandRange(0.5f, 1.0f);
	}
}

void AVaelLightningBolt::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Elapsed += DeltaSeconds;

	if (Elapsed >= Duration + (bSparks ? BoltSparkLifetime : 0.0f))
	{
		Destroy();
		return;
	}

	// The bolt jumps into new shapes while it lasts and fades over its second half
	const bool bStriking = Elapsed < Duration;
	if (bStriking)
	{
		RejagCountdown -= DeltaSeconds;
		if (RejagCountdown <= 0.0f)
		{
			RejagCountdown = BoltRejagInterval * FMath::FRandRange(0.7f, 1.3f);
			Rejag();
		}
	}

	const float Fade = bStriking ? 1.0f - FMath::SmoothStep(0.5f, 1.0f, Elapsed / Duration) : 0.0f;

	for (int32 SegmentIndex = 0; SegmentIndex < Cores.Num(); ++SegmentIndex)
	{
		Cores[SegmentIndex]->SetVisibility(bStriking);
		Halos[SegmentIndex]->SetVisibility(bStriking);

		VaelEffects::SetLookGlow(Cores[SegmentIndex], BoltCoreGlow * Fade * SegmentFlicker[SegmentIndex]);
		VaelEffects::SetLookGlow(Halos[SegmentIndex], BoltHaloGlow * Fade * SegmentFlicker[SegmentIndex]);
	}

	const float Flash = BoltLightIntensity * Fade * FMath::FRandRange(0.7f, 1.0f);
	StartLight->SetIntensity(Flash * 0.5f);
	EndLight->SetIntensity(Flash);

	// Sparks fly off, slow down and fall, shrinking as they die
	const float Carried = (1.0f - FMath::Exp(-2.0f * Elapsed)) / 2.0f;
	const float SparkLife = FMath::Clamp(Elapsed / BoltSparkLifetime, 0.0f, 1.0f);

	for (int32 SparkIndex = 0; SparkIndex < Sparks.Num(); ++SparkIndex)
	{
		Sparks[SparkIndex]->SetVisibility(SparkLife < 1.0f);
		Sparks[SparkIndex]->SetWorldLocation(To + SparkVelocities[SparkIndex] * Carried - FVector(0.0f, 0.0f, 0.5f * BoltSparkGravity * Elapsed * Elapsed));
		Sparks[SparkIndex]->SetWorldScale3D(FVector(BoltSparkSize * (1.0f - SparkLife) / BoltSphereSize));
	}
}
