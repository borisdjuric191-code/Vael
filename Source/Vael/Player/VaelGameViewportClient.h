// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "VaelGameViewportClient.generated.h"

/**
 *  Game viewport that lets additional controllers join local co-op.
 *  Input from a controller without a player only arrives here, so joining can't be handled by a player controller.
 */
UCLASS()
class UVaelGameViewportClient : public UGameViewportClient
{
	GENERATED_BODY()

public:

	/** Creates a new local player when Options / Start is pressed on a controller that has none */
	virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override;

protected:

	/** Adds a local player for the given platform user. Returns false if that user already plays or the game is full. */
	bool TryJoinLocalPlayer(FPlatformUserId PlatformUser);
};
