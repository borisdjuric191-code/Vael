// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/VaelInteractable.h"
#include "Engine/World.h"
#include "Items/VaelItemSettings.h"
#include "Player/VaelCharacter.h"

void UVaelInteractionSubsystem::Register(AActor* Interactable)
{
	UWorld* World = Interactable != nullptr ? Interactable->GetWorld() : nullptr;
	if (UVaelInteractionSubsystem* Subsystem = World != nullptr ? World->GetSubsystem<UVaelInteractionSubsystem>() : nullptr)
	{
		Subsystem->Interactables.AddUnique(Interactable);
	}
}

void UVaelInteractionSubsystem::Unregister(AActor* Interactable)
{
	UWorld* World = Interactable != nullptr ? Interactable->GetWorld() : nullptr;
	if (UVaelInteractionSubsystem* Subsystem = World != nullptr ? World->GetSubsystem<UVaelInteractionSubsystem>() : nullptr)
	{
		Subsystem->Interactables.Remove(Interactable);
	}
}

AActor* UVaelInteractionSubsystem::FindNearest(const AVaelCharacter* Player)
{
	const UWorld* World = Player != nullptr ? Player->GetWorld() : nullptr;
	const UVaelInteractionSubsystem* Subsystem = World != nullptr ? World->GetSubsystem<UVaelInteractionSubsystem>() : nullptr;
	if (Subsystem == nullptr || Player->IsDowned())
	{
		return nullptr;
	}

	AActor* Nearest = nullptr;
	float NearestDistance = UVaelItemSettings::Get()->InteractRange;

	for (const TWeakObjectPtr<AActor>& Weak : Subsystem->Interactables)
	{
		AActor* Actor = Weak.Get();
		const IVaelInteractable* Interactable = Cast<IVaelInteractable>(Actor);
		if (Interactable == nullptr || !Interactable->CanInteract(Player))
		{
			continue;
		}

		const float Distance = FVector::Dist2D(Actor->GetActorLocation(), Player->GetActorLocation());
		if (Distance <= NearestDistance)
		{
			Nearest = Actor;
			NearestDistance = Distance;
		}
	}

	return Nearest;
}
