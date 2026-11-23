// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelTravel.h"

#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "LandscapeProxy.h"
#include "TimerManager.h"

void VaelTravel::Land(APawn* Pawn, const FVector2D& Place)
{
	Land(Pawn, Place, 50000.0f, -UE_BIG_NUMBER);
}

void VaelTravel::Land(APawn* Pawn, const FVector2D& Place, float FromZ, float MinZ)
{
	UWorld* World = Pawn != nullptr ? Pawn->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return;
	}

	const FVector Above(Place.X, Place.Y, FromZ);
	const bool bUnderRoof = MinZ > -UE_BIG_NUMBER * 0.5f;
	TWeakObjectPtr<APawn> WeakPawn(Pawn);
	TWeakObjectPtr<UWorld> WeakWorld(World);
	TSharedRef<int32> Tries = MakeShared<int32>(0);
	TSharedRef<FTimerHandle> Handle = MakeShared<FTimerHandle>();
	auto Step = [WeakPawn, WeakWorld, Above, Tries, Handle, MinZ, bUnderRoof]()
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
		// In a level without partition (a cave) not the rock base far below: in the joined caves of the Eisenschlund it lies 40 m deep
		// under the abyss and is ready long before the floor tiles, the group landed on it (2026-11-22)
		const bool bPartitioned = Here->IsPartitionedWorld();
		const bool bTraced = Here->LineTraceSingleByChannel(Hit, Above, Above - FVector(0.0f, 0.0f, 100000.0f), ECC_Visibility, QueryParams);
		const bool bGround = bTraced && (bUnderRoof ? Hit.ImpactPoint.Z >= MinZ
			: (Cast<ALandscapeProxy>(Hit.GetActor()) != nullptr || (!bPartitioned && Hit.ImpactPoint.Z > -1000.0f) || (bPartitioned && Hit.ImpactPoint.Z > 200.0f)));
		UCharacterMovementComponent* Movement = Mover->FindComponentByClass<UCharacterMovementComponent>();
		// Under a roof (the caves inside L_Vael) the floor of a far place can take a while to stream in: wait up to a minute, and
		// never give up below it (once the group fell into nothing after 15 s, walk test 2026-11-23)
		if (bGround || ++(*Tries) > (bUnderRoof || !bPartitioned ? 300 : 75))
		{
			const float GroundZ = bGround ? Hit.ImpactPoint.Z : bUnderRoof ? Above.Z : 0.0f;
			Mover->TeleportTo(FVector(Above.X, Above.Y, GroundZ + Mover->GetSimpleCollisionHalfHeight() + 5.0f), Mover->GetActorRotation());
			UE_LOG(LogTemp, Log, TEXT("VaelTravel: '%s' lands at %s on '%s' after %d tries"), *Mover->GetName(), *Mover->GetActorLocation().ToCompactString(),
				bGround ? *GetNameSafe(Hit.GetActor()) : TEXT("nothing"), *Tries);
			if (Movement != nullptr)
			{
				Movement->SetMovementMode(MOVE_Walking);
			}
			Here->GetTimerManager().ClearTimer(*Handle);
			return;
		}
		// Still waiting: up where the streaming starts loading around it (under a roof: just above the floor that is coming)
		Mover->TeleportTo(FVector(Above.X, Above.Y, bUnderRoof ? Above.Z : 20000.0f), Mover->GetActorRotation());
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
