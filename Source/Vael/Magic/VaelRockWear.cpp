// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelRockWear.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Magic/VaelMagicSettings.h"

UVaelRockWear* UVaelRockWear::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
	return World != nullptr ? World->GetSubsystem<UVaelRockWear>() : nullptr;
}

bool UVaelRockWear::IsWornOut(const FRockState& State) const
{
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	if (State.Uses < MagicSettings->EarthRockUses)
	{
		return false;
	}

	// A rock left alone for long enough is whole again
	return MagicSettings->EarthRockRecoverSeconds <= 0.0f || GetWorld()->GetTimeSeconds() - State.WornOutTime < MagicSettings->EarthRockRecoverSeconds;
}

bool UVaelRockWear::IsWornOut(const UPrimitiveComponent* Rock, int32 Item) const
{
	const FRockState* State = Rocks.Find(FRockKey{ Rock, Item });
	return State != nullptr && IsWornOut(*State);
}

int32 UVaelRockWear::Wear(const UPrimitiveComponent* Rock, int32 Item)
{
	const int32 MaxUses = UVaelMagicSettings::Get()->EarthRockUses;
	FRockState& State = Rocks.FindOrAdd(FRockKey{ Rock, Item });

	// Recovered since it was worn out: it starts anew
	if (State.Uses >= MaxUses && !IsWornOut(State))
	{
		State.Uses = 0;
	}

	State.Uses = FMath::Min(State.Uses + 1, MaxUses);
	if (State.Uses >= MaxUses)
	{
		State.WornOutTime = GetWorld()->GetTimeSeconds();
	}

	return MaxUses - State.Uses;
}
