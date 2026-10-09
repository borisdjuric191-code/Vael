// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelSpellGust.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Magic/VaelSpellEffects.h"
#include "World/VaelGround.h"

namespace
{
	/** Diameter of the engine sphere mesh in cm */
	constexpr float GustSphereSize = 100.0f;

	/** The ring of pressure at the hand grows from this to that diameter in cm within this time */
	constexpr float GustRingStartSize = 25.0f;
	constexpr float GustRingEndSize = 115.0f;
	constexpr float GustRingTime = 0.16f;

	/** The wall of air starts at this share of the reach and arrives at the end of the cone after this many seconds */
	constexpr float GustWaveStartShare = 0.12f;
	constexpr float GustWaveTime = 0.38f;

	/** Pieces side by side along the arc of the wall, its thickness in cm, its height as share of the reach at the start and the end */
	constexpr int32 GustWaveSegments = 9;
	constexpr float GustWaveThickness = 22.0f;
	constexpr float GustWaveStartHeight = 0.12f;
	constexpr float GustWaveEndHeight = 0.32f;

	/** The faint row lags behind the bright front at this share of its distance */
	constexpr float GustWaveBackShare = 0.82f;

	/** Streaks of wind, seconds each one races out, latest start, thickness in cm */
	constexpr int32 GustStreakCount = 12;
	constexpr float GustStreakTime = 0.26f;
	constexpr float GustStreakLatestStart = 0.12f;
	constexpr float GustStreakThickness = 5.0f;

	/** Grains of dust and puffs, seconds each one flies, fall in cm/s² and slowing by the air */
	constexpr int32 GustDustCount = 20;
	constexpr float GustDustLifetime = 0.55f;
	constexpr float GustDustGravity = 760.0f;
	constexpr float GustDustDrag = 1.6f;

	/** Every this many pieces of dust one is a soft puff that rises and swells instead of falling */
	constexpr int32 GustPuffEvery = 4;

	/** Brightness of the light on the wall of air in candela */
	constexpr float GustLightIntensity = 6.0f;
}

AVaelSpellGust::AVaelSpellGust()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	PrimaryActorTick.bCanEverTick = true;
}

AVaelSpellGust* AVaelSpellGust::Spawn(AActor* Caster, const FVector& Location, const FVector& Direction, float Range, float HalfAngle, const FLinearColor& Color)
{
	UWorld* World = Caster != nullptr ? Caster->GetWorld() : nullptr;
	const FVector BlowDirection = Direction.GetSafeNormal2D();
	if (World == nullptr || BlowDirection.IsNearlyZero())
	{
		return nullptr;
	}

	const FTransform SpawnTransform(BlowDirection.Rotation(), Location);

	AVaelSpellGust* Gust = World->SpawnActorDeferred<AVaelSpellGust>(AVaelSpellGust::StaticClass(), SpawnTransform, Caster, Cast<APawn>(Caster), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Gust != nullptr)
	{
		Gust->Range = FMath::Max(Range, 50.0f);
		Gust->HalfAngle = HalfAngle;
		Gust->GustColor = Color;
		Gust->FinishSpawning(SpawnTransform);
	}

	return Gust;
}

void AVaelSpellGust::BeginPlay()
{
	Super::BeginPlay();

	// The glowing materials of the element orbs; the plain engine material while they don't exist
	UMaterialInterface* CoreMaterial = nullptr;
	UMaterialInterface* GlowMaterial = nullptr;
	VaelEffects::LoadLookMaterials(CoreMaterial, GlowMaterial);

	// The wall of air rolls over the ground below the hand
	const FVector Hand = GetActorLocation();
	FHitResult GroundHit;
	if (VaelGround::TraceGround(GetWorld(), Hand, Hand - FVector(0.0f, 0.0f, 400.0f), GroundHit, GetInstigator()))
	{
		GroundHeight = GroundHit.Location.Z - Hand.Z;
	}

	const FLinearColor Bright = FMath::Lerp(GustColor, FLinearColor::White, 0.3f);
	const FLinearColor Ash(0.42f, 0.36f, 0.28f);

	HandRing = VaelEffects::AddLookShape(this, GlowMaterial, Bright, 2.2f, 1.0f);

	// The wall only shines at its edges, so it reads as clear air pressed together
	for (int32 SegmentIndex = 0; SegmentIndex < GustWaveSegments; ++SegmentIndex)
	{
		WaveFront.Add(VaelEffects::AddLookShape(this, GlowMaterial, Bright, 1.4f, 1.0f));
		WaveBack.Add(VaelEffects::AddLookShape(this, GlowMaterial, GustColor, 0.5f, 0.6f));
	}

	for (int32 StreakIndex = 0; StreakIndex < GustStreakCount; ++StreakIndex)
	{
		Streaks.Add(VaelEffects::AddLookShape(this, GlowMaterial, Bright, 3.5f, 0.0f));
		StreakYaws.Add(FMath::FRandRange(-HalfAngle, HalfAngle) * 0.85f);
		StreakHeights.Add(GroundHeight + FMath::FRandRange(25.0f, Range * GustWaveEndHeight * 0.9f));
		StreakStarts.Add(FMath::FRandRange(0.0f, GustStreakLatestStart));
		StreakLengths.Add(Range * FMath::FRandRange(0.18f, 0.3f));
	}

	// Dust lies where the wall of air will pass and flies off the moment it arrives
	for (int32 DustIndex = 0; DustIndex < GustDustCount; ++DustIndex)
	{
		const bool bPuff = DustIndex % GustPuffEvery == 0;
		const float Yaw = FMath::DegreesToRadians(FMath::FRandRange(-HalfAngle, HalfAngle) * 0.9f);
		const FVector Out(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.0f);
		const FVector Side(-Out.Y, Out.X, 0.0f);
		const float Distance = Range * FMath::FRandRange(0.2f, 0.85f);

		Dust.Add(bPuff ? VaelEffects::AddLookShape(this, GlowMaterial, FLinearColor(0.55f, 0.52f, 0.48f), 0.35f, 0.3f) : VaelEffects::AddLookShape(this, CoreMaterial, Ash, 0.0f, 0.0f));
		DustOrigins.Add(Out * Distance + FVector(0.0f, 0.0f, GroundHeight + 3.0f));
		DustStarts.Add(GetWaveArrival(Distance) + FMath::FRandRange(0.0f, 0.03f));
		DustSizes.Add(bPuff ? FMath::FRandRange(30.0f, 50.0f) : FMath::FRandRange(5.0f, 12.0f));
		DustVelocities.Add(bPuff
			? Out * FMath::FRandRange(120.0f, 220.0f) + FVector(0.0f, 0.0f, FMath::FRandRange(40.0f, 90.0f))
			: Out * FMath::FRandRange(220.0f, 480.0f) + Side * FMath::FRandRange(-60.0f, 60.0f) + FVector(0.0f, 0.0f, FMath::FRandRange(160.0f, 340.0f)));
	}

	WaveLight = NewObject<UPointLightComponent>(this);
	WaveLight->SetupAttachment(RootComponent);
	WaveLight->SetMobility(EComponentMobility::Movable);
	WaveLight->SetIntensityUnits(ELightUnits::Candelas);
	WaveLight->SetIntensity(0.0f);
	WaveLight->SetLightColor(Bright);
	WaveLight->SetAttenuationRadius(320.0f);
	WaveLight->SetCastShadows(false);
	WaveLight->RegisterComponent();

	Tick(0.0f);
}

float AVaelSpellGust::GetWaveDistance(float Time) const
{
	// Fast out of the hand, slowing towards the end of the cone
	const float Progress = FMath::Clamp(Time / GustWaveTime, 0.0f, 1.0f);
	const float Eased = 1.0f - FMath::Square(1.0f - Progress);

	return Range * FMath::Lerp(GustWaveStartShare, 1.0f, Eased);
}

float AVaelSpellGust::GetWaveArrival(float Distance) const
{
	const float Eased = FMath::Clamp((Distance / Range - GustWaveStartShare) / (1.0f - GustWaveStartShare), 0.0f, 1.0f);

	return (1.0f - FMath::Sqrt(1.0f - Eased)) * GustWaveTime;
}

void AVaelSpellGust::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Elapsed += DeltaSeconds;

	// Gone once the wall has blown out and the last dust has settled
	if (Elapsed >= GustWaveTime + GustStreakLatestStart + GustDustLifetime)
	{
		Destroy();
		return;
	}

	AnimateHandRing();
	AnimateWave();
	AnimateStreaks();
	AnimateDust();
}

void AVaelSpellGust::AnimateHandRing()
{
	const float Progress = Elapsed / GustRingTime;
	HandRing->SetVisibility(Progress < 1.0f);
	if (Progress >= 1.0f)
	{
		return;
	}

	// A flat ring facing the aim bursts open in front of the hand
	const float Size = FMath::Lerp(GustRingStartSize, GustRingEndSize, 1.0f - FMath::Square(1.0f - Progress)) / GustSphereSize;

	HandRing->SetRelativeLocation(FVector(30.0f * Progress, 0.0f, 0.0f));
	HandRing->SetRelativeScale3D(FVector(Size * 0.08f, Size, Size));
	VaelEffects::SetLookGlow(HandRing, 2.2f * (1.0f - Progress));
}

void AVaelSpellGust::AnimateWave()
{
	const float Progress = Elapsed / GustWaveTime;
	const bool bVisible = Progress < 1.0f;

	for (int32 SegmentIndex = 0; SegmentIndex < GustWaveSegments; ++SegmentIndex)
	{
		WaveFront[SegmentIndex]->SetVisibility(bVisible);
		WaveBack[SegmentIndex]->SetVisibility(bVisible);
	}

	WaveLight->SetVisibility(bVisible);

	if (!bVisible)
	{
		return;
	}

	// Comes up quickly, fades over the last stretch of the cone
	const float Fade = FMath::Clamp(Elapsed / 0.05f, 0.0f, 1.0f) * (1.0f - FMath::SmoothStep(0.55f, 1.0f, Progress));
	const float Distance = GetWaveDistance(Elapsed);
	const float Height = Range * FMath::Lerp(GustWaveStartHeight, GustWaveEndHeight, FMath::Clamp(Progress, 0.0f, 1.0f));

	const float Spread = HalfAngle * 0.92f;
	const float Step = FMath::DegreesToRadians(2.0f * Spread / (GustWaveSegments - 1));

	for (int32 SegmentIndex = 0; SegmentIndex < GustWaveSegments; ++SegmentIndex)
	{
		// The wall bulges forward in the middle and is lower at its ends, like a rolling crest
		const float Across = SegmentIndex / (GustWaveSegments - 1.0f) * 2.0f - 1.0f;
		const float Yaw = Across * Spread;
		const float Radians = FMath::DegreesToRadians(Yaw);
		const FVector Out(FMath::Cos(Radians), FMath::Sin(Radians), 0.0f);

		const float SegmentDistance = Distance * (1.0f - 0.06f * FMath::Abs(Across));
		const float SegmentHeight = Height * (1.0f - 0.35f * Across * Across);
		const float Width = SegmentDistance * Step * 1.8f + 10.0f;

		UStaticMeshComponent* Front = WaveFront[SegmentIndex];
		Front->SetRelativeLocation(Out * SegmentDistance + FVector(0.0f, 0.0f, GroundHeight + SegmentHeight * 0.5f));
		Front->SetRelativeRotation(FRotator(0.0f, Yaw, 0.0f));
		Front->SetRelativeScale3D(FVector(GustWaveThickness, Width, SegmentHeight) / GustSphereSize);
		VaelEffects::SetLookGlow(Front, 1.4f * Fade);

		UStaticMeshComponent* Back = WaveBack[SegmentIndex];
		Back->SetRelativeLocation(Out * SegmentDistance * GustWaveBackShare + FVector(0.0f, 0.0f, GroundHeight + SegmentHeight * 0.4f));
		Back->SetRelativeRotation(FRotator(0.0f, Yaw, 0.0f));
		Back->SetRelativeScale3D(FVector(GustWaveThickness * 1.6f, Width * GustWaveBackShare, SegmentHeight * 0.8f) / GustSphereSize);
		VaelEffects::SetLookGlow(Back, 0.5f * Fade);
	}

	WaveLight->SetRelativeLocation(FVector(Distance, 0.0f, GroundHeight + Height * 0.5f));
	WaveLight->SetIntensity(GustLightIntensity * Fade);
}

void AVaelSpellGust::AnimateStreaks()
{
	for (int32 StreakIndex = 0; StreakIndex < Streaks.Num(); ++StreakIndex)
	{
		const float Progress = (Elapsed - StreakStarts[StreakIndex]) / GustStreakTime;

		UStaticMeshComponent* Streak = Streaks[StreakIndex];
		Streak->SetVisibility(Progress > 0.0f && Progress < 1.0f);
		if (Progress <= 0.0f || Progress >= 1.0f)
		{
			continue;
		}

		// Each streak races out ahead of the wall, stretching at first and thinning out before it vanishes
		const float Radians = FMath::DegreesToRadians(StreakYaws[StreakIndex]);
		const FVector Out(FMath::Cos(Radians), FMath::Sin(Radians), 0.0f);
		const float Head = Range * FMath::Lerp(0.05f, 1.05f, 1.0f - FMath::Square(1.0f - Progress));
		const float Length = StreakLengths[StreakIndex] * FMath::Min(1.0f, Progress * 4.0f) * (1.0f - Progress * 0.5f);

		Streak->SetRelativeLocation(Out * (Head - Length * 0.5f) + FVector(0.0f, 0.0f, StreakHeights[StreakIndex] + Progress * 20.0f));
		Streak->SetRelativeRotation(FRotator(0.0f, StreakYaws[StreakIndex], 0.0f));
		Streak->SetRelativeScale3D(FVector(Length, GustStreakThickness, GustStreakThickness) / GustSphereSize);
		VaelEffects::SetLookGlow(Streak, 3.5f * FMath::Min(1.0f, Progress * 6.0f) * (1.0f - Progress));
	}
}

void AVaelSpellGust::AnimateDust()
{
	for (int32 DustIndex = 0; DustIndex < Dust.Num(); ++DustIndex)
	{
		const float Age = Elapsed - DustStarts[DustIndex];

		UStaticMeshComponent* Grain = Dust[DustIndex];
		Grain->SetVisibility(Age > 0.0f && Age < GustDustLifetime);
		if (Age <= 0.0f || Age >= GustDustLifetime)
		{
			continue;
		}

		// Flung off by the wall, slowed by the air; grains fall back to the ground, puffs drift up
		const bool bPuff = DustIndex % GustPuffEvery == 0;
		const float Gravity = bPuff ? -60.0f : GustDustGravity;
		const float Carried = (1.0f - FMath::Exp(-GustDustDrag * Age)) / GustDustDrag;

		FVector Location = DustOrigins[DustIndex] + DustVelocities[DustIndex] * Carried - FVector(0.0f, 0.0f, 0.5f * Gravity * Age * Age);
		Location.Z = FMath::Max(Location.Z, GroundHeight + 2.0f);

		const float Life = Age / GustDustLifetime;
		const float Size = bPuff
			? DustSizes[DustIndex] * (0.4f + Life)
			: DustSizes[DustIndex] * FMath::Min(1.0f, (1.0f - Life) * 4.0f);

		Grain->SetRelativeLocation(Location);
		Grain->SetRelativeScale3D(FVector(Size / GustSphereSize));

		if (bPuff)
		{
			VaelEffects::SetLookGlow(Grain, 0.35f * (1.0f - Life));
		}
	}
}
