// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "VaelGameMode.generated.h"

class AVaelSharedCamera;

/**
 *  Game Mode for local co-op with one shared isometric camera
 *  Sets the default gameplay framework classes
 */
UCLASS()
class AVaelGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	/** Highest number of players on one screen */
	static constexpr int32 MaxLocalPlayers = 4;

	/** Constructor */
	AVaelGameMode();

	/** Gives every joining player a free player slot */
	virtual void PostLogin(APlayerController* NewPlayer) override;

	/** Spawns joining players next to the group instead of at the player start */
	virtual void RestartPlayer(AController* NewPlayer) override;

	/** Returns the camera shared by all players, spawning it on first use */
	AVaelSharedCamera* GetSharedCamera();

	/** Returns the placeholder color of a player slot */
	FLinearColor GetPlayerColor(int32 PlayerSlot) const;

protected:

	/** Camera class shared by all players */
	UPROPERTY(EditDefaultsOnly, Category="Camera")
	TSubclassOf<AVaelSharedCamera> SharedCameraClass;

	/** Placeholder color per player slot */
	UPROPERTY(EditDefaultsOnly, Category="Players")
	TArray<FLinearColor> PlayerColors;

	/** Distance from an existing player at which a joining player appears */
	UPROPERTY(EditDefaultsOnly, Category="Players", meta = (ClampMin = 0))
	float JoinSpawnRadius = 150.0f;

private:

	/** Returns the lowest player slot that no other player uses */
	int32 FindFreePlayerSlot(const APlayerController* ForPlayer) const;

	/** Camera shared by all players */
	UPROPERTY(Transient)
	TObjectPtr<AVaelSharedCamera> SharedCamera;
};
