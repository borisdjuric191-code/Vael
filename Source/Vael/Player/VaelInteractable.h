// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/Interface.h"
#include "VaelInteractable.generated.h"

class AVaelCharacter;

UINTERFACE(MinimalAPI)
class UVaelInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 *  Something a player can use by walking up to it and pressing interact (E, L1 / LB), like a chest.
 *  Actors with this interface register themselves with the interaction subsystem in BeginPlay.
 */
class IVaelInteractable
{
	GENERATED_BODY()

public:

	/** True if the player can use it right now */
	virtual bool CanInteract(const AVaelCharacter* Player) const = 0;

	/** Uses it */
	virtual void Interact(AVaelCharacter* Player) = 0;

	/** What pressing the button does, shown over it, like "Truhe öffnen" */
	virtual FText GetInteractPrompt() const = 0;
};

/** Knows everything players can interact with in the level */
UCLASS()
class UVaelInteractionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	/** Adds an interactable actor, called by the actor itself */
	static void Register(AActor* Interactable);

	/** Removes an interactable actor, called by the actor itself */
	static void Unregister(AActor* Interactable);

	/** The closest thing the player can use within the interact range, null if there is none */
	static AActor* FindNearest(const AVaelCharacter* Player);

private:

	/** Registered interactable actors */
	TArray<TWeakObjectPtr<AActor>> Interactables;
};
