// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nature/VaelWaterable.h"
#include "Engine/World.h"

namespace VaelWatering
{
	namespace
	{
		/** Every plant that can be watered, in all worlds */
		TArray<TWeakObjectPtr<AActor>> Plants;
	}

	void Register(AActor* Plant)
	{
		Plants.AddUnique(Plant);
	}

	void Unregister(AActor* Plant)
	{
		Plants.RemoveAll([Plant](const TWeakObjectPtr<AActor>& Entry) { return !Entry.IsValid() || Entry.Get() == Plant; });
	}

	int32 WaterAround(UWorld* World, const FVector& Location, float Radius, float Amount, AActor* Giver)
	{
		return WaterAlong(World, Location, Location, Radius, Amount, Giver);
	}

	int32 WaterAlong(UWorld* World, const FVector& Start, const FVector& End, float HalfWidth, float Amount, AActor* Giver)
	{
		if (World == nullptr || Amount <= 0.0f)
		{
			return 0;
		}

		// Plants stand on the ground, so the line counts flat
		const FVector2D From(Start);
		const FVector2D Way = FVector2D(End) - From;
		const float WayLengthSquared = Way.SizeSquared();
		int32 Watered = 0;
		// A copy: a plant may wither and leave the list while it is watered
		const TArray<TWeakObjectPtr<AActor>> Candidates = Plants;
		for (const TWeakObjectPtr<AActor>& Entry : Candidates)
		{
			AActor* Actor = Entry.Get();
			IVaelWaterable* Plant = Cast<IVaelWaterable>(Actor);
			if (Plant == nullptr || Actor->GetWorld() != World)
			{
				continue;
			}
			const FVector2D At(Actor->GetActorLocation());
			const float Share = WayLengthSquared > 1.0f ? FMath::Clamp(FVector2D::DotProduct(At - From, Way) / WayLengthSquared, 0.0f, 1.0f) : 0.0f;
			if (FVector2D::DistSquared(At, From + Way * Share) <= FMath::Square(HalfWidth))
			{
				Plant->ReceiveWater(Amount, Giver);
				++Watered;
			}
		}
		return Watered;
	}
}
