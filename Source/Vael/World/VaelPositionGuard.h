// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VaelPositionGuard.generated.h"

class ACharacter;

/**
 * Watches where a player character goes (Boris 2026-11-27: "im Eisenschlund wird man hinausgeschleudert, das Teleportieren ist immer
 * noch da"). Remembers the last safe spot on solid ground and
 *  - logs every jump of the character that no teleport explains (with movement mode, speed and the floor), so the cause shows in the log,
 *  - brings a player who falls through the floor (floor not streamed in yet, a gap in the cave tiles) back to the last safe spot
 *    instead of letting them fall out of the world,
 *  - caps flings: a speed far above anything the game gives (a depenetration push out of a mesh) is cut down.
 */
UCLASS(ClassGroup=(Vael), meta=(BlueprintSpawnableComponent))
class UVaelPositionGuard : public UActorComponent
{
	GENERATED_BODY()

public:
	UVaelPositionGuard();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Called by every intended teleport (travel, respawn, cheats): the next jump is not a fault */
	void NoteTeleport(const FVector& To);

	/** The last spot where the character stood on solid ground */
	FVector GetLastSafeSpot() const { return LastSafe; }

	/** Seconds of falling after which a player far below the last safe spot is brought back */
	UPROPERTY(EditAnywhere, Category="Guard")
	float RescueAfterFalling = 1.6f;

	/** How far below the last safe spot a falling player must be before the rescue, in cm */
	UPROPERTY(EditAnywhere, Category="Guard")
	float RescueDepth = 900.0f;

	/** A jump longer than this in one frame without a teleport is logged, in cm */
	UPROPERTY(EditAnywhere, Category="Guard")
	float JumpReport = 500.0f;

	/** Highest horizontal speed allowed; above it the character is slowed (no flinging), cm/s */
	UPROPERTY(EditAnywhere, Category="Guard")
	float MaxSpeed = 4200.0f;

private:
	TWeakObjectPtr<ACharacter> Character;
	FVector LastLocation = FVector::ZeroVector;
	FVector LastSafe = FVector::ZeroVector;
	bool bHasLast = false;
	bool bHasSafe = false;
	bool bTeleported = false;
	float FallingTime = 0.0f;
	float SafeClock = 0.0f;
	int32 Rescues = 0;
};
