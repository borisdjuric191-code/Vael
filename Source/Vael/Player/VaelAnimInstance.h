// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "BonePose.h"
#include "Floromant/VaelFlightPose.h"
#include "VaelAnimInstance.generated.h"

class AVaelCharacter;

/** Runs the animation graph and then bends the body for the flight and the plant arms of the Floromant (on the animation worker thread) */
struct FVaelAnimProxy : public FAnimInstanceProxy
{
	FVaelAnimProxy() = default;
	explicit FVaelAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate_WithRoot(FPoseContext& Output, FAnimNode_Base* InRootNode) override;

	/** Copied from the instance: how far each arm is stretched out (left, right), lean back in degrees, reach of the arms */
	float ArmStance[2] = { 0.0f, 0.0f };
	float LeanDegrees = 0.0f;
	float ArmReach = 1.0f;

	/** Copied from the instance: how the flying body hangs in the air */
	FVaelFlightPose Flight;

	/** Copied from the instance: how far each shield hand is raised (left, right) */
	float Shield[2] = { 0.0f, 0.0f };

private:

	/** Tips the body, lets the legs dangle and swing, bends the knees and points the toes */
	void ApplyFlight(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones) const;
};

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

	/** How far each arm of a Floromant is stretched out to fire (0 left, 1 right), 0 to 1, and how far it leans back in degrees */
	float PlantArmStance[2] = { 0.0f, 0.0f };
	float PlantArmLean = 0.0f;
	float PlantArmReach = 1.0f;

	/** How the body of a flying Floromant hangs in the air, from the character */
	FVaelFlightPose FlightPose;

	/** How far each shield hand of a Floromant is raised before the face (left, right) */
	float ShieldStance[2] = { 0.0f, 0.0f };

public:

	//~Begin UAnimInstance
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FVaelAnimProxy(this); }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override { delete static_cast<FVaelAnimProxy*>(InProxy); }
	//~End UAnimInstance

	friend struct FVaelAnimProxy;

private:

	/** The animated character */
	UPROPERTY(Transient)
	TObjectPtr<AVaelCharacter> Character;
};
