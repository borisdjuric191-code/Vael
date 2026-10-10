// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelSafeZone.h"
#include "Components/SphereComponent.h"
#include "EngineUtils.h"

AVaelSafeZone::AVaelSafeZone()
{
	Area = CreateDefaultSubobject<USphereComponent>(TEXT("Area"));
	Area->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Area->SetSphereRadius(Radius);
	Area->ShapeColor = FColor(120, 220, 140);
	Area->SetHiddenInGame(true);
	RootComponent = Area;

	PrimaryActorTick.bCanEverTick = false;
}

#if WITH_EDITOR
void AVaelSafeZone::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Area->SetSphereRadius(Radius);
}
#endif

bool AVaelSafeZone::IsSafe(const UWorld* World, const FVector& Location, float Margin)
{
	if (World == nullptr)
	{
		return false;
	}

	for (TActorIterator<AVaelSafeZone> It(World); It; ++It)
	{
		if (FVector::Dist2D(It->GetActorLocation(), Location) < It->Radius + Margin)
		{
			return true;
		}
	}

	return false;
}

FVector AVaelSafeZone::KeepOut(const UWorld* World, const FVector& Location, const FVector& Move, float Margin)
{
	FVector Result = Move;
	if (World == nullptr)
	{
		return Result;
	}

	for (TActorIterator<AVaelSafeZone> It(World); It; ++It)
	{
		const FVector Outward = (Location - It->GetActorLocation()) * FVector(1.0f, 1.0f, 0.0f);
		const float Distance = Outward.Size();
		const float Edge = It->Radius + Margin;
		if (Distance > Edge + 150.0f)
		{
			continue;
		}

		const FVector Normal = Distance > KINDA_SMALL_NUMBER ? Outward / Distance : FVector::ForwardVector;

		// Inside: straight out; at the edge: no step further in
		if (Distance < Edge)
		{
			Result = Normal * FMath::Max(Result.Size(), 1.0f);
			continue;
		}

		const float Inward = FVector::DotProduct(Result, -Normal);
		if (Inward > 0.0f)
		{
			Result += Normal * Inward;
		}
	}

	return Result;
}
