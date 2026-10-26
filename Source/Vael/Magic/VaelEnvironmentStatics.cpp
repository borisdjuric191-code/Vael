// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelEnvironmentStatics.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Magic/VaelDebrisBurst.h"
#include "Magic/VaelElementSource.h"
#include "Magic/VaelRockWall.h"
#include "Magic/VaelRockWear.h"
#include "World/VaelFireSpot.h"
#include "World/VaelLocalWeather.h"
#include "Magic/VaelGroundArea.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelWaterPuddle.h"
#include "Nature/VaelHarvestable.h"
#include "World/VaelMarkSource.h"
#include "World/VaelRegion.h"

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

	// Campfires and Brandherde
	if (Element == EVaelElement::Fire && FindFireSpot(Caster) != nullptr)
	{
		return true;
	}

	// Puddles left by balls of water, once enough has gathered, and ash rain that brings water everywhere
	if (Element == EVaelElement::Water)
	{
		if (AVaelWaterPuddle::IsDeepWaterAt(World, Location))
		{
			return true;
		}

		const AVaelRegion* Region = AVaelRegion::GetRegionAt(const_cast<UWorld*>(World), Location);
		if ((Region != nullptr && Region->GetWeather() == EVaelWeather::Rain) || AVaelLocalWeather::IsRainingAt(World, Location))
		{
			return true;
		}
	}

	// Plants and stones that carry an element, like the Glutdistel or the Traenenkelch
	for (TActorIterator<AVaelHarvestable> It(World); It; ++It)
	{
		if (It->ProvidesElement(Element, Location))
		{
			return true;
		}
	}

	// Wind and storms fill the whole region with air, a thermal, a wind front or a thunderstorm the patch they blow on
	if (Element == EVaelElement::Air)
	{
		const AVaelRegion* Region = AVaelRegion::GetRegionAt(const_cast<UWorld*>(World), Location);
		return (Region != nullptr && (Region->GetWeather() == EVaelWeather::Storm || Region->GetWeather() == EVaelWeather::Wind)) || !AVaelLocalWeather::GetWindAt(World, Location).IsNearlyZero();
	}

	// Open Mark sources let mages draw the Mark, once it has awakened in them
	if (Element == EVaelElement::Mark)
	{
		return AVaelMarkSource::FindOpenSourceAt(World, Location) != nullptr;
	}

	// Rock that isn't worn out; plain ground doesn't count
	if (Element == EVaelElement::Earth)
	{
		UPrimitiveComponent* Rock = nullptr;
		int32 Item = INDEX_NONE;
		FVector Point;
		return FindRock(Caster, Rock, Item, Point);
	}

	return false;
}

AVaelFireSpot* UVaelEnvironmentStatics::FindFireSpot(const AActor* Caster)
{
	const UWorld* World = Caster != nullptr ? Caster->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}

	const FVector Location = Caster->GetActorLocation();
	const float Margin = UVaelMagicSettings::Get()->FireSourceMargin;

	AVaelFireSpot* Nearest = nullptr;
	float NearestGap = UE_BIG_NUMBER;
	for (TActorIterator<AVaelFireSpot> It(World); It; ++It)
	{
		if (!It->CanGiveFire() || FMath::Abs(It->GetActorLocation().Z - Location.Z) > Margin + 200.0f)
		{
			continue;
		}

		const float Gap = FVector::Dist2D(It->GetActorLocation(), Location) - It->GetFire()->GetShape().Radius;
		if (Gap <= Margin && Gap < NearestGap)
		{
			Nearest = *It;
			NearestGap = Gap;
		}
	}

	return Nearest;
}

bool UVaelEnvironmentStatics::IsRock(const UPrimitiveComponent* Component)
{
	if (Component == nullptr)
	{
		return false;
	}

	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	const AActor* Owner = Component->GetOwner();
	if (Component->ComponentHasTag(MagicSettings->NoEarthSourceTag) || (Owner != nullptr && Owner->ActorHasTag(MagicSettings->NoEarthSourceTag)))
	{
		return false;
	}

	// Raised rock walls and anything tagged as earth
	if (Component->ComponentHasTag(MagicSettings->EarthSourceTag) || (Owner != nullptr && (Owner->ActorHasTag(MagicSettings->EarthSourceTag) || Owner->IsA<AVaelRockWall>())))
	{
		return true;
	}

	// Placed or scattered rock, known by the name of its mesh
	const UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Component);
	const UStaticMesh* Mesh = MeshComponent != nullptr ? MeshComponent->GetStaticMesh() : nullptr;
	if (Mesh == nullptr)
	{
		return false;
	}

	const FString MeshName = Mesh->GetName();
	for (const FString& Word : MagicSettings->EarthSourceMeshWords)
	{
		if (!Word.IsEmpty() && MeshName.Contains(Word, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

bool UVaelEnvironmentStatics::FindRock(const AActor* Caster, UPrimitiveComponent*& OutRock, int32& OutItem, FVector& OutPoint)
{
	const UWorld* World = Caster != nullptr ? Caster->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return false;
	}

	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	const UVaelRockWear* Wear = UVaelRockWear::Get(Caster);
	const FVector Location = Caster->GetActorLocation();

	// A slab from just above the feet to the hips: low stones count, the ground under the feet doesn't
	const float HalfHeight = Caster->GetSimpleCollisionHalfHeight();
	const FVector SlabCenter = Location - FVector(0.0f, 0.0f, HalfHeight * 0.4f);
	const FCollisionShape Slab = FCollisionShape::MakeBox(FVector(MagicSettings->EarthSourceDistance, MagicSettings->EarthSourceDistance, HalfHeight * 0.5f));
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VaelEarthSource), false, Caster);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, SlabCenter, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic), Slab, QueryParams);

	float NearestDistance = UE_BIG_NUMBER;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		UPrimitiveComponent* Component = Overlap.GetComponent();
		if (!IsRock(Component))
		{
			continue;
		}

		// One rock of a scattered set is its own rock
		const UInstancedStaticMeshComponent* Instances = Cast<UInstancedStaticMeshComponent>(Component);
		const int32 Item = Instances != nullptr ? Overlap.ItemIndex : INDEX_NONE;
		if (Wear != nullptr && Wear->IsWornOut(Component, Item))
		{
			continue;
		}

		// The side of the rock facing the caster, at the height of the hands where it can be reached
		FVector Point;
		FTransform InstanceTransform;
		if (Instances != nullptr && Instances->GetStaticMesh() != nullptr && Instances->GetInstanceTransform(Item, InstanceTransform, true))
		{
			const FBox Box = Instances->GetStaticMesh()->GetBoundingBox().TransformBy(InstanceTransform);
			Point = Box.GetClosestPointTo(FVector(Location.X, Location.Y, FMath::Clamp(Location.Z, Box.Min.Z, Box.Max.Z)));
		}
		else if (Component->GetClosestPointOnCollision(Location, Point) < 0.0f)
		{
			Point = Component->Bounds.GetBox().GetClosestPointTo(Location);
		}

		const float Distance = FVector::DistSquared(Point, Location);
		if (Distance < NearestDistance)
		{
			NearestDistance = Distance;
			OutRock = Component;
			OutItem = Item;
			OutPoint = Point;
		}
	}

	return NearestDistance < UE_BIG_NUMBER;
}

FVaelElementDraw UVaelEnvironmentStatics::DrawFromEnvironment(AActor* Caster, EVaelElement Element)
{
	FVaelElementDraw Draw;
	UWorld* World = Caster != nullptr ? Caster->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return Draw;
	}

	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	const FVector Location = Caster->GetActorLocation();

	switch (Element)
	{
	case EVaelElement::Earth:
	{
		UPrimitiveComponent* Rock = nullptr;
		int32 Item = INDEX_NONE;
		if (!FindRock(Caster, Rock, Item, Draw.From))
		{
			break;
		}
		Draw.bHasPoint = true;

		// Chunks break out of the rock towards the mage; a rock that has given its last earth crumbles in a cloud of dust
		UVaelRockWear* Wear = UVaelRockWear::Get(Caster);
		const bool bWornOut = Wear != nullptr && Wear->Wear(Rock, Item) <= 0;

		FVaelDebris Debris;
		Debris.Count = bWornOut ? 14 : 6;
		Debris.Spread = bWornOut ? 45.0f : 20.0f;
		Debris.Speed = bWornOut ? 260.0f : 160.0f;
		Debris.Lift = bWornOut ? 320.0f : 220.0f;
		Debris.Lifetime = bWornOut ? 1.1f : 0.7f;
		Debris.MinSize = 5.0f;
		Debris.MaxSize = bWornOut ? 18.0f : 12.0f;
		Debris.RockShare = 0.3f;
		Debris.bDust = true;
		AVaelDebrisBurst::Spawn(Caster, Draw.From, Debris);

		if (bWornOut)
		{
			FVaelDebris Dust;
			Dust.Count = 1;
			Dust.Spread = 90.0f;
			Dust.MaxSize = 90.0f;
			Dust.bSoft = true;
			Dust.bDust = true;
			AVaelDebrisBurst::Spawn(Caster, Draw.From, Dust);
		}
		break;
	}

	case EVaelElement::Fire:
		if (AVaelFireSpot* Spot = FindFireSpot(Caster))
		{
			Draw.bHasPoint = true;
			Draw.From = Spot->GetActorLocation() + FVector(0.0f, 0.0f, Spot->GetFire()->GetShape().Height * 0.4f);
			Spot->GiveFire(Location);
		}
		break;

	case EVaelElement::Water:
		if (FindWaterDrawPoint(Caster, Draw.From, Draw.Color))
		{
			Draw.bHasPoint = true;
		}
		else
		{
			// Ash rain: the water gathers from the wet ground ahead of the feet
			Draw.bHasPoint = true;
			Draw.From = Location - FVector(0.0f, 0.0f, Caster->GetSimpleCollisionHalfHeight()) + Caster->GetActorForwardVector().GetSafeNormal2D() * MagicSettings->WaterDrawFootOffset;
			Draw.Color = MagicSettings->WaterDrawPondColor;
		}

		if (AVaelWaterPuddle* Puddle = AVaelWaterPuddle::FindDeepWaterAt(World, Location))
		{
			Puddle->DrainSome(Draw.From, MagicSettings->PuddleDrawWater);
		}
		break;

	default:
		break;
	}

	return Draw;
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
	if ((Region != nullptr && Region->GetWeather() == EVaelWeather::Rain) || AVaelLocalWeather::IsRainingAt(World, Location))
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
