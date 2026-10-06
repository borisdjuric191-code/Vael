// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "VaelAnimInstance.generated.h"

class AVaelCharacter;

/**
 *  Base of the animation blueprints of the player characters.
 *  Reads what the character is doing once per frame, so the animation graph only has to pick and blend animations.
 *  Twin-stick movement: the body faces the aim direction, so walking sideways or backwards is common.
 */
UCLASS()
class UVaelAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

protected:

	/** Horizontal speed in cm/s */
	UPROPERTY(BlueprintReadOnly, Category="Movement")
	float Speed = 0.0f;

	/** Direction of movement relative to where the body faces, in degrees: 0 forward, 90 right, -90 left, 180 or -180 backwards */
	UPROPERTY(BlueprintReadOnly, Category="Movement")
	float Direction = 0.0f;

	/** True while the character moves faster than the threshold */
	UPROPERTY(BlueprintReadOnly, Category="Movement")
	bool bIsMoving = false;

	/** True while the character falls or is in the air */
	UPROPERTY(BlueprintReadOnly, Category="Movement")
	bool bIsFalling = false;

	/** True during a dodge roll or a spell dash */
	UPROPERTY(BlueprintReadOnly, Category="Movement")
	bool bIsDodging = false;

	/** True while the character plays a cast animation or channels a spell, like the fire beam */
	UPROPERTY(BlueprintReadOnly, Category="Magic")
	bool bIsCasting = false;

	/** True while the player is down and waits for help */
	UPROPERTY(BlueprintReadOnly, Category="Combat")
	bool bIsDowned = false;

	/** Below this speed in cm/s the character counts as standing */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement", meta = (ClampMin = 0))
	float MovingThreshold = 5.0f;

public:

	//~Begin UAnimInstance
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	//~End UAnimInstance

private:

	/** The animated character */
	UPROPERTY(Transient)
	TObjectPtr<AVaelCharacter> Character;
};
