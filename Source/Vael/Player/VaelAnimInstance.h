// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "BonePose.h"
#include "Floromant/VaelFlightPose.h"
#include "Magic/VaelElementTypes.h"
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
	/** Rooted Floromant: degrees her upper body twists from her rooted legs towards the aim */
	float RootTwist = 0.0f;
	float ArmReach = 1.0f;

	/** Copied from the instance: how the flying body hangs in the air */
	FVaelFlightPose Flight;

	/** Copied from the instance: how far each shield hand is raised (left, right) */
	float Shield[2] = { 0.0f, 0.0f };

	/** Copied from the instance: which arms are mortars, how far each is drawn back, and how far up they aim in degrees */
	bool bMortar[2] = { false, false };
	float Draw[2] = { 0.0f, 0.0f };
	float MortarPitch = 42.0f;

	/** The whip strokes of lasso hands (Schlotwinde): seconds since each began (negative when none), their length, and the side
	 *  each starts high on (+1 her right) */
	float WhipSeconds[2] = { -1.0f, -1.0f };
	float WhipDuration[2] = { 0.34f, 0.34f };
	float WhipStartSide[2] = { 1.0f, 1.0f };

	/** Copied from the instance: the cast gesture of the mage, seconds since it began, when the spell leaves and how long it lasts */
	EVaelCastGesture Gesture = EVaelCastGesture::None;
	float GestureSeconds = 0.0f;

	/** How far the spent pause after a beam has come, 0 to 1 */
	float GestureExhaust = 0.0f;
	float GestureRelease = 0.13f;
	float GestureDuration = 0.5f;

	/** Leg pushing off in the skating stride of the Böe, 0 left, 1 right */
	int32 SkateSide = 0;

	/** The engineer's slingshot (Zwille): how much he holds it up (0 down, 1 aimed) and how far the band is pulled (0..1) */
	float SlingAim = 0.0f;
	float SlingDraw = 0.0f;

	/** Ground IK (goal 9.11.): how far the ground under each foot lies above (+) or below (-) the floor of the capsule
	 *  (cm), the ground's up direction there (component space), how far the hips go down, and how much of it applies */
	float GroundFoot[2] = { 0.0f, 0.0f };
	FVector GroundNormal[2] = { FVector::UpVector, FVector::UpVector };
	float GroundPelvis = 0.0f;
	float GroundWeight = 0.0f;
	bool bHoldsSling = false;
	float SlingKick = 0.0f;
	float SlingTremble = 0.0f;
	float SlingClock = 0.0f;
	void ApplyGroundIK(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones) const;

private:

	/** The whip stroke of one arm (0 left, 1 right): wound up high on its start side, it lashes down across the body, the chest
	 *  turning with it, the forearm and hand trailing like the end of a whip */
	void ApplyWhipStroke(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones, int32 Side) const;

	/** Tips the body, lets the legs dangle and swing, bends the knees and points the toes */
	void ApplyFlight(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones) const;

	/** The Funke gesture: the right arm draws back and snaps forward, index and middle finger stretched, the others curled in */
	void ApplyTwoFingerPoint(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones) const;

	/** Blowing out mist or steam: breathes in with chest and shoulders up, the hands cupped before the chin, then leans in and
	 *  blows, head forward, the hands opening forward as if sending the cloud off */
	void ApplyBlowOut(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones) const;

	/** Scoop and flick: the hand scoops up from the hip in a soft arc, the wrist snaps and the fingers open, then it swings out */
	void ApplyWaterFlick(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones) const;

	/** The left hand takes hold of the right forearm, as far as the weight says */
	void ApplyForearmGrip(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones, float Weight) const;

	/** Beam from the open palm: gathers at the hip, thrusts, shakes under the strain, hangs forward spent */
	void ApplyPalmBeam(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones) const;

	/** Welle: the hand skims low across the body in a wide arc, tips palm forward and the wrist snaps; hangs while spent */
	void ApplyWaveSweep(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones) const;

	/** Flutwelle: both hands scoop low, heave up over the head and throw forward and down; hangs forward while spent */
	void ApplyWaveHeave(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones) const;

	/** Steinbrocken: the fist cocks by the far shoulder and lashes out back of the hand first, dead stop */
	void ApplyBackfist(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones) const;

	/** The slingshot: the left hand holds the fork out before him, the right pulls the band back to the cheek as far as it is drawn */
	void ApplySlingDraw(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones) const;
	void ApplySlingThrow(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones) const;

	/** Felswand: crouched, both fists low with their backs forward climb up heavily and hold */
	void ApplyWallRise(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones) const;

	/** Erdbeben: both fists up together over the head, slammed into the ground at once, crouched while spent */
	void ApplyGroundSlam(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones) const;

	/** Windstoß: the flat hand fans out from before the chest, edge first, and swings on */
	void ApplyFanSlap(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones) const;

	/** Böe: a skating stride, low and leaning, one leg pushed out sideways and back, the arms swinging against it */
	void ApplySkate(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones) const;

	/** Wirbelsturm: the finger stirs circles over the head, then the hand throws the whirl forward */
	void ApplyStir(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones) const;

	/** Weather spells: both arms up to the sky in a V, head back, murmuring with soft nods */
	void ApplySkyCall(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones) const;

	/** Closes a hand (suffix l or r) to a fist, as far as the weight says */
	void MakeFist(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones, const TCHAR* Suffix, float Weight) const;

	/** Lowers the hips by the depth in cm and bends the legs so the feet stay where they stand */
	void Crouch(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones, float Depth) const;

	/** Stretches the fingers of a hand (suffix l or r) along it, as far as the weight says */
	void OpenHand(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones, const TCHAR* Suffix, float Weight) const;
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
	float PlantRootTwist = 0.0f;
	float PlantArmReach = 1.0f;

	/** How the body of a flying Floromant hangs in the air, from the character */
	FVaelFlightPose FlightPose;

	/** How far each shield hand of a Floromant is raised before the face (left, right) */
	float ShieldStance[2] = { 0.0f, 0.0f };

	/** Mortar hands of a Floromant (Funkenmohn): which arms are mortars, how far each is drawn back, and how far up they aim in degrees */
	bool bMortarArm[2] = { false, false };
	float MortarDraw[2] = { 0.0f, 0.0f };
	float MortarPitch = 42.0f;

	/** The whip strokes of lasso hands (Schlotwinde): seconds since each began (negative when none), their length, and the side
	 *  each starts high on (+1 her right) */
	float WhipSeconds[2] = { -1.0f, -1.0f };
	float WhipDuration[2] = { 0.34f, 0.34f };
	float WhipStartSide[2] = { 1.0f, 1.0f };

	/** Cast gesture of the mage and the seconds since it began */
	EVaelCastGesture CastGesture = EVaelCastGesture::None;
	float CastGestureSeconds = 0.0f;

	/** The engineer's slingshot: held up and how far drawn */
	float SlingAim = 0.0f;
	float SlingDraw = 0.0f;

	/** Ground IK, measured on the game thread (see the proxy) */
	float GroundFoot[2] = { 0.0f, 0.0f };
	FVector GroundNormal[2] = { FVector::UpVector, FVector::UpVector };
	float GroundPelvis = 0.0f;
	float GroundWeight = 0.0f;
	bool bHoldsSling = false;
	float SlingKick = 0.0f;
	float SlingTremble = 0.0f;
	float SlingClock = 0.0f;
	float CastExhaustion = 0.0f;
	int32 CastSkateSide = 0;

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
