// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelEnvironmentStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Magic/VaelElementSource.h"
#include "Magic/VaelGroundArea.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelWaterPuddle.h"
#include "Nature/VaelHarvestable.h"
#include "World/VaelMarkSource.h"
#include "World/VaelRegion.h"

namespace
{
	/** Half height of the slab around the caster that is searched for rock; low enough to miss the floor */
	constexpr float EarthSearchHalfHeight = 30.0f;
}

bool UVaelEnvironmentStatics::IsElementInEnvironment(const AActor* Caster, EVaelElement Element)
{
	const UWorld* World = Caster != nullptr ? Caster->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return false;
	}

	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	const FVector Location = Caster->GetActorLocation();

	// Fire reaches a bit beyond its edge, everything else has to be stood in
	const float ExtraDistance = Element == EVaelElement::Fire ? MagicSettings->FireSourceMargin : 0.0f;

	for (TActorIterator<AVaelGroundArea> It(World); It; ++It)
	{
		if (It->ProvidesElement(Element) && It->IsInRange(Location, ExtraDistance))
		{
			return true;
		}
	}

	// Puddles left by balls of water, once enough has gathered
	if (Element == EVaelElement::Water && AVaelWaterPuddle::IsDeepWaterAt(World, Location))
	{
		return true;
	}

	// Plants and stones that carry an element, like the Glutdistel or the Traenenkelch
	for (TActorIterator<AVaelHarvestable> It(World); It; ++It)
	{
		if (It->ProvidesElement(Element, Location))
		{
			return true;
		}
	}

	// A storm fills the whole region with air
	if (Element == EVaelElement::Air)
	{
		const AVaelRegion* Region = AVaelRegion::GetRegionAt(const_cast<UWorld*>(World), Location);
		return Region != nullptr && Region->GetWeather() == EVaelWeather::Storm;
	}

	// Open Mark sources let mages draw the Mark, once it has awakened in them
	if (Element == EVaelElement::Mark)
	{
		return AVaelMarkSource::FindOpenSourceAt(World, Location) != nullptr;
	}

	if (Element == EVaelElement::Earth)
	{
		const FCollisionShape Slab = FCollisionShape::MakeBox(FVector(MagicSettings->EarthSourceDistance, MagicSettings->EarthSourceDistance, EarthSearchHalfHeight));
		const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VaelEarthSource), false, Caster);

		return World->OverlapAnyTestByObjectType(Location, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic), Slab, QueryParams);
	}

	return false;
}

float UVaelEnvironmentStatics::DrawWaveWater(const AActor* Caster)
{
	UWorld* World = Caster != nullptr ? Caster->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return 0.0f;
	}

	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	const FVector Location = Caster->GetActorLocation();
	const float Reach = MagicSettings->WaveSwellRadius;

	// Puddles give all their water within reach, and lose it
	float Water = AVaelWaterPuddle::DrainAround(World, Location, Reach);

	// Ponds and other water on the ground give by their size
	for (TActorIterator<AVaelGroundArea> It(World); It; ++It)
	{
		if (It->ProvidesElement(EVaelElement::Water) && It->IsInRange(Location, Reach))
		{
			Water += UE_PI * FMath::Square(It->GetRadius() / 100.0f) * MagicSettings->WaveSwellPondWaterPerSquareMeter;
		}
	}

	// Water plants like the Traenenkelch
	for (TActorIterator<AVaelHarvestable> It(World); It; ++It)
	{
		if (FVector::Dist2D(It->GetActorLocation(), Location) <= Reach && It->ProvidesElement(EVaelElement::Water, It->GetActorLocation()))
		{
			Water += MagicSettings->WaveSwellPlantWater;
		}
	}

	// Rain fills the air with water
	const AVaelRegion* Region = AVaelRegion::GetRegionAt(World, Location);
	if (Region != nullptr && Region->GetWeather() == EVaelWeather::Rain)
	{
		Water += MagicSettings->WaveSwellRainWater;
	}

	return FMath::Clamp(Water / MagicSettings->WaveSwellFullWater, 0.0f, 1.0f);
}

bool UVaelEnvironmentStatics::FindWaterDrawPoint(const AActor* Caster, FVector& OutLocation, FLinearColor& OutColor)
{
	const UWorld* World = Caster != nullptr ? Caster->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return false;
	}

	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	const FVector Location = Caster->GetActorLocation();
	const FVector Feet = Location - FVector(0.0f, 0.0f, Caster->GetSimpleCollisionHalfHeight());
	const FVector Ahead = Feet + Caster->GetActorForwardVector().GetSafeNormal2D() * MagicSettings->WaterDrawFootOffset;
	OutColor = MagicSettings->WaterDrawPondColor;

	// A water plant is the most visible place to draw from: the strand leaves its cup
	for (TActorIterator<AVaelHarvestable> It(World); It; ++It)
	{
		if (It->ProvidesElement(EVaelElement::Water, Location))
		{
			OutLocation = It->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f);
			return true;
		}
	}

	// A pond: ahead of the feet if the water reaches that far, else towards its middle
	for (TActorIterator<AVaelGroundArea> It(World); It; ++It)
	{
		if (It->ProvidesElement(EVaelElement::Water) && It->IsInRange(Location))
		{
			const FVector Middle(It->GetActorLocation().X, It->GetActorLocation().Y, Feet.Z);
			const FVector Inward = Feet + (Middle - Feet).GetClampedToMaxSize2D(MagicSettings->WaterDrawFootOffset);
			OutLocation = It->IsInRange(Ahead) ? Ahead : Inward;
			return true;
		}
	}

	// A puddle shows its own water, darkened like water lying on the ground
	if (const AVaelWaterPuddle* Puddle = AVaelWaterPuddle::FindDeepWaterAt(World, Location))
	{
		OutLocation = Puddle->Covers(Ahead) ? Ahead : Feet;
		if (const UVaelElementSource* Source = Puddle->GetSource())
		{
			OutColor = Source->ParticleColor * 0.15f;
			OutColor.A = 1.0f;
		}
		return true;
	}

	return false;
}
