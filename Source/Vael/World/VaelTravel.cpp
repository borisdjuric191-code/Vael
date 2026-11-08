// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelTravel.h"

#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "LandscapeProxy.h"
#include "TimerManager.h"

void VaelTravel::Land(APawn* Pawn, const FVector2D& Place)
{
	UWorld* World = Pawn != nullptr ? Pawn->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return;
	}

	const FVector Above(Place.X, Place.Y, 50000.0f);
	TWeakObjectPtr<APawn> WeakPawn(Pawn);
	TWeakObjectPtr<UWorld> WeakWorld(World);
	TSharedRef<int32> Tries = MakeShared<int32>(0);
	TSharedRef<FTimerHandle> Handle = MakeShared<FTimerHandle>();
	auto Step = [WeakPawn, WeakWorld, Above, Tries, Handle]()
	{
		APawn* Mover = WeakPawn.Get();
		UWorld* Here = WeakWorld.Get();
		if (Mover == nullptr || Here == nullptr)
		{
			if (Here != nullptr)
			{
				Here->GetTimerManager().ClearTimer(*Handle);
			}
			return;
		}
		FHitResult Hit;
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VaelTravel), false, Mover);
		// Not the sea: it is loaded long before the land under it (land, roofs and rocks above it count)
		const bool bGround = Here->LineTraceSingleByChannel(Hit, Above, Above - FVector(0.0f, 0.0f, 100000.0f), ECC_Visibility, QueryParams)
			&& (Cast<ALandscapeProxy>(Hit.GetActor()) != nullptr || !Here->IsPartitionedWorld() || Hit.ImpactPoint.Z > 200.0f);
		UCharacterMovementComponent* Movement = Mover->FindComponentByClass<UCharacterMovementComponent>();
		if (bGround || ++(*Tries) > 75)
		{
			Mover->TeleportTo(FVector(Above.X, Above.Y, (bGround ? Hit.ImpactPoint.Z : 0.0f) + Mover->GetSimpleCollisionHalfHeight() + 5.0f), Mover->GetActorRotation());
			if (Movement != nullptr)
			{
				Movement->SetMovementMode(MOVE_Walking);
			}
			Here->GetTimerManager().ClearTimer(*Handle);
			return;
		}
		// Still waiting: up where the streaming starts loading around it
		Mover->TeleportTo(FVector(Above.X, Above.Y, 20000.0f), Mover->GetActorRotation());
		if (Movement != nullptr)
		{
			Movement->SetMovementMode(MOVE_None);
		}
	};
	Step();
	if (*Tries > 0)
	{
		World->GetTimerManager().SetTimer(*Handle, FTimerDelegate::CreateLambda(Step), 0.2f, true);
	}
}
