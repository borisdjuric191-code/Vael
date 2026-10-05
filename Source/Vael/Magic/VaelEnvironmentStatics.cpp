// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelEnvironmentStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Magic/VaelGroundArea.h"
#include "Magic/VaelMagicSettings.h"
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
		if (It->GetElement() == Element && It->IsInRange(Location, ExtraDistance))
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

	if (Element == EVaelElement::Earth)
	{
		const FCollisionShape Slab = FCollisionShape::MakeBox(FVector(MagicSettings->EarthSourceDistance, MagicSettings->EarthSourceDistance, EarthSearchHalfHeight));
		const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VaelEarthSource), false, Caster);

		return World->OverlapAnyTestByObjectType(Location, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic), Slab, QueryParams);
	}

	return false;
}
