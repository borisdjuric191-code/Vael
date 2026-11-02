// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelAshRain.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "Magic/VaelCloudCards.h"
#include "Magic/VaelDebrisBurst.h"
#include "Magic/VaelFlakes.h"
#include "Magic/VaelGroundArea.h"
#include "Magic/VaelMagicSettings.h"
#include "Materials/MaterialInterface.h"
#include "Vael.h"
#include "VaelAssets.h"
#include "World/VaelGround.h"

namespace
{
	/** Height of the cloud above the ground and its cards; it is low, the ash has not far to fall */
	constexpr float CloudHeight = 420.0f;
	constexpr int32 CloudCards = 26;

	/** How dark the ash cloud is against its storm cloud cards */
	constexpr float CloudDarkness = 0.35f;

	/** Grey haze under the cloud */
	constexpr int32 HazeCards = 8;
	constexpr float HazeSolid = 0.18f;

	/** Seconds the cloud takes to gather, the rain starts after this and stops this long before the end */
	constexpr float GatherSeconds = 0.5f;
	constexpr float RainStart = 0.3f;
	constexpr float RainTail = 0.6f;

	/** Seconds the ash lies after the rain before it is gone */
	constexpr float AshLinger = 2.5f;

	/** Flakes per second on a patch of the Ascheregen's size (radius 392 cm), more or fewer on others; flakes falling at once when it starts */
	constexpr float SootRate = 34.0f;
	constexpr float EmberRate = 22.0f;
	constexpr float RateRadius = 392.0f;
	constexpr int32 FirstFall = 40;

	/** Width of the flakes in cm */
	constexpr float SootSize = 15.0f;
	constexpr float EmberSize = 8.0f;

	/** Small fires lit by embers: chance per hot landing, most per rain, apart from each other, their radius and seconds */
	constexpr float FireChance = 0.3f;
	constexpr int32 MaxFires = 4;
	constexpr float FireSpacing = 140.0f;
	constexpr float FireRadius = 45.0f;
	constexpr float FireSeconds = 3.5f;

	/** Fires only start on the ground of the patch, not on a roof or a fence the ash fell on */
	constexpr float FireGroundTolerance = 60.0f;

	/** Glimmer of the embers in the cloud */
	constexpr float GlimmerIntensity = 2600.0f;
	const FLinearColor GlimmerColor = FLinearColor(1.0f, 0.38f, 0.1f);
}

AVaelAshRain::AVaelAshRain()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Glimmer = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glimmer"));
	Glimmer->SetupAttachment(RootComponent);
	Glimmer->SetRelativeLocation(FVector(0.0f, 0.0f, CloudHeight - 60.0f));
	Glimmer->SetCastShadows(false);
	Glimmer->SetIntensity(0.0f);
	Glimmer->SetLightColor(GlimmerColor);
}

AVaelAshRain* AVaelAshRain::Spawn(UWorld* World, const FVector& Ground, float InRadius, float InSeconds, float InFireDamagePerSecond, APawn* InInstigator)
{
	if (World == nullptr)
	{
		return nullptr;
	}
	FActorSpawnParameters Parameters;
	Parameters.Instigator = InInstigator;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVaelAshRain* Rain = World->SpawnActor<AVaelAshRain>(AVaelAshRain::StaticClass(), FTransform(Ground), Parameters);
	if (Rain != nullptr)
	{
		Rain->Radius = InRadius;
		Rain->Seconds = InSeconds;
		Rain->FireDamagePerSecond = InFireDamagePerSecond;
		Rain->SetLifeSpan(InSeconds + AshLinger);
		Rain->Build();
	}
	return Rain;
}

void AVaelAshRain::Build()
{
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	Glimmer->SetAttenuationRadius(Radius * 2.0f);

	// The cloud: dark storm cards darkened almost to black; the haze: grey ash cards
	UMaterialInterface* StormMaterial = VaelAssets::LoadOptional(MagicSettings->StormCloudMaterial);
	UMaterialInterface* AshMaterial = VaelAssets::LoadOptional(MagicSettings->AshCardMaterial);
	if (StormMaterial != nullptr && AshMaterial != nullptr)
	{
		Cloud = NewObject<UVaelCloudCardsComponent>(this);
		Cloud->SetupAttachment(RootComponent);
		Cloud->RegisterComponent();
		Cloud->Init(StormMaterial);
		for (int32 Index = 0; Index < CloudCards; ++Index)
		{
			const FVector2D Spot = FMath::RandPointInCircle(Radius * 1.1f);
			const FVector Place(Spot.X, Spot.Y, CloudHeight + FMath::FRandRange(-60.0f, 60.0f));
			const float Size = Radius * FMath::FRandRange(0.6f, 0.9f);
			Cloud->SetBrightness(Cloud->AddCard(Place, Size, 0.0f), CloudDarkness * FMath::FRandRange(0.8f, 1.2f));
			CloudPlaces.Add(Place);
			CloudSizes.Add(Size);
		}
		Cloud->Commit();

		Haze = NewObject<UVaelCloudCardsComponent>(this);
		Haze->SetupAttachment(RootComponent);
		Haze->RegisterComponent();
		Haze->Init(AshMaterial);
		for (int32 Index = 0; Index < HazeCards; ++Index)
		{
			const FVector2D Spot = FMath::RandPointInCircle(Radius * 0.75f);
			const FVector Place(Spot.X, Spot.Y, FMath::FRandRange(50.0f, 150.0f));
			const float Size = Radius * FMath::FRandRange(0.45f, 0.65f);
			Haze->AddCard(Place, Size, 0.0f);
			HazePlaces.Add(Place);
			HazeSizes.Add(Size);
		}
		Haze->Commit();
	}

	Flakes = NewObject<UVaelFlakesComponent>(this);
	Flakes->SetupAttachment(RootComponent);
	Flakes->RegisterComponent();
	Flakes->SetWorldLocation(GetActorLocation());
	Flakes->OnHotLanding = [this](const FVector& Ground) { OnEmberLanded(Ground); };
}

void AVaelAshRain::Drop(bool bEmber, float HeightShare)
{
	const FVector2D Spot = FMath::RandPointInCircle(Radius);
	const FVector Center = GetActorLocation();
	const FVector Above = Center + FVector(Spot.X, Spot.Y, 200.0f);
	FHitResult GroundHit;
	const float GroundZ = VaelGround::TraceGround(GetWorld(), Above, Above - FVector(0.0f, 0.0f, 600.0f), GroundHit, GetInstigator()) ? GroundHit.Location.Z : Center.Z;

	// Out of the underside of the cloud, already on its way down
	const FVector Start(Above.X, Above.Y, FMath::Lerp(GroundZ + 30.0f, Center.Z + CloudHeight - 70.0f, HeightShare));
	const FVector Velocity(FMath::FRandRange(-30.0f, 30.0f), FMath::FRandRange(-30.0f, 30.0f), bEmber ? -260.0f : -150.0f);
	Flakes->Add(bEmber ? EVaelFlake::Ember : EVaelFlake::Soot, Start, Velocity, (bEmber ? EmberSize : SootSize) * FMath::FRandRange(0.7f, 1.3f), GroundZ);
}

void AVaelAshRain::OnEmberLanded(const FVector& Ground)
{
	// The ground flares where it falls
	FVaelDebris Sparks;
	Sparks.Count = 3;
	Sparks.Spread = 4.0f;
	Sparks.Speed = 90.0f;
	Sparks.Lift = 160.0f;
	Sparks.Lifetime = 0.35f;
	Sparks.MinSize = 1.5f;
	Sparks.MaxSize = 3.0f;
	Sparks.Color = GlimmerColor;
	Sparks.Glow = 6.0f;
	Sparks.bSoft = true;
	AVaelDebrisBurst::Spawn(this, Ground + FVector(0.0f, 0.0f, 3.0f), Sparks);

	// Now and then into a small fire, a few apart from each other
	if (FireSpots.Num() >= MaxFires || FMath::FRand() > FireChance || FMath::Abs(Ground.Z - GetActorLocation().Z) > FireGroundTolerance)
	{
		return;
	}
	for (const FVector& Spot : FireSpots)
	{
		if (FVector::Dist2D(Spot, Ground) < FireSpacing)
		{
			return;
		}
	}
	if (AVaelGroundArea::SpawnArea(GetWorld(), Ground + FVector(0.0f, 0.0f, 2.0f), EVaelElement::Fire, FireRadius, FireSeconds, FireDamagePerSecond, GetInstigator()) != nullptr)
	{
		FireSpots.Add(Ground);
		UE_LOG(LogVael, Log, TEXT("Ascheregen: an ember lights a small fire at %s"), *Ground.ToCompactString());
	}
}

void AVaelAshRain::Stop()
{
	if (Age < Seconds)
	{
		Seconds = Age;
		SetLifeSpan(RainTail + AshLinger);
	}
}

void AVaelAshRain::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;

	// The cloud gathers, churns slowly and thins away at the end of the rain
	const float Shown = FMath::Min(FMath::Clamp(Age / GatherSeconds, 0.0f, 1.0f), FMath::Clamp((Seconds - Age) / GatherSeconds + 0.5f, 0.0f, 1.0f));
	if (Cloud != nullptr)
	{
		for (int32 Index = 0; Index < CloudPlaces.Num(); ++Index)
		{
			const float Phase = Age * 0.45f + Index * 1.7f;
			const FVector Churn = FVector(FMath::Sin(Phase), FMath::Cos(Phase * 0.8f), 0.0f) * 25.0f;
			Cloud->SetCard(Index, CloudPlaces[Index] * FMath::Lerp(0.6f, 1.0f, Shown) + Churn, CloudSizes[Index], 0.85f * Shown);
		}
		Cloud->Commit();
	}
	if (Haze != nullptr)
	{
		// The haze thickens as the ash comes down
		const float Thick = FMath::Clamp((Age - RainStart) / 1.2f, 0.0f, 1.0f) * Shown;
		for (int32 Index = 0; Index < HazePlaces.Num(); ++Index)
		{
			const float Phase = Age * 0.3f + Index * 2.3f;
			Haze->SetCard(Index, HazePlaces[Index] + FVector(FMath::Sin(Phase), FMath::Cos(Phase), 0.0f) * 20.0f, HazeSizes[Index], HazeSolid * Thick);
		}
		Haze->Commit();
	}

	// Embers glimmer in the cloud, flickering
	const float Flicker = 0.75f + 0.25f * FMath::Sin(Age * 11.0f) * FMath::Sin(Age * 4.3f + 1.0f);
	Glimmer->SetIntensity(GlimmerIntensity * Shown * Flicker);

	// The rain: soot and embers all over the patch, a first fall at once so it reaches the ground soon
	if (Flakes == nullptr)
	{
		return;
	}
	if (Age >= RainStart && Age < Seconds - RainTail)
	{
		if (!bRaining)
		{
			bRaining = true;
			for (int32 Index = 0; Index < FirstFall; ++Index)
			{
				Drop(Index % 3 == 0, FMath::FRandRange(0.25f, 1.0f));
			}
		}
		const float Share = FMath::Square(Radius / RateRadius);
		SootCredit += SootRate * Share * DeltaSeconds;
		EmberCredit += EmberRate * Share * DeltaSeconds;
		for (; SootCredit >= 1.0f; SootCredit -= 1.0f)
		{
			Drop(false, 1.0f);
		}
		for (; EmberCredit >= 1.0f; EmberCredit -= 1.0f)
		{
			Drop(true, 1.0f);
		}
	}

	// The ash lies on and fades after the rain
	Flakes->SetFade(FMath::Clamp((Seconds + AshLinger - Age) / AshLinger, 0.0f, 1.0f));
}
