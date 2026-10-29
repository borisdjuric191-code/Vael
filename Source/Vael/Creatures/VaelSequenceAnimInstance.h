// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "VaelSequenceAnimInstance.generated.h"

class UAnimSequence;

/** A chain of bones flung out to a point on top of the sequence (a vine striking): see UVaelSequenceAnimInstance::SetReachChains */
struct FVaelReachChain
{
	/** From the chain's root to its tip piece; the tip piece (a vine's bud) is carried along, turned with the last joint */
	TArray<FName> Bones;

	/** Where the tip goes, in component space */
	FVector Target = FVector::ZeroVector;

	/** 0 as the sequence has it .. 1 laid out all the way to the target */
	float Weight = 0.0f;

	/** How far the path bulges up on its way (cm): a strike over the head */
	float Arc = 0.0f;

	/** A sideways ripple along the chain (cm) and where it is in its swing (radians) */
	float Wave = 0.0f;
	float WavePhase = 0.0f;

	/** Two jaw bones on the tip piece, opened apart by JawOpen degrees (negative presses them shut) */
	FName UpperJaw;
	FName LowerJaw;
	float JawOpen = 0.0f;
};

/** Evaluates the current sequence and, while a change is blending in, the one before it (on the animation worker thread) */
struct FVaelSequenceAnimProxy : public FAnimInstanceProxy
{
	FVaelSequenceAnimProxy() = default;
	explicit FVaelSequenceAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

	/** Lays the spine chain onto its targets (see UVaelSequenceAnimInstance::SetSpineTargets) */
	void ApplySpine(FCompactPose& Pose) const;

	/** Flings the reach chains out to their targets (see UVaelSequenceAnimInstance::SetReachChains) */
	void ApplyReach(FCompactPose& Pose) const;

	TArray<FVaelReachChain> ReachChains;

	UAnimSequence* Current = nullptr;
	UAnimSequence* Previous = nullptr;
	float CurrentTime = 0.0f;
	float PreviousTime = 0.0f;

	/** 0 = all the previous sequence, 1 = all the current one */
	float Blend = 1.0f;

	TArray<FName> SpineBones;
	TArray<FVector> SpineTargets;
	TArray<FVector> SpineUps;
	float SpineWeight = 0.0f;
	FVector LookPoint = FVector::ZeroVector;
	float LookMaxAngle = 0.0f;
	float HeadMaxPitch = 90.0f;
	float LookWeight = 0.0f;
	TArray<FName> ScaledBones;
	TArray<float> BoneScales;
	TArray<float> BoneOpens;
	float TrembleAngle = 0.0f;
	float TrembleTime = 0.0f;
	float SwayAngle = 0.0f;
	TArray<float> SwayPhases;
};

/**
 *  Plays one animation sequence at a time on a skeletal mesh and blends softly from one to the next, without an
 *  animation blueprint: creatures with baked animations (like the Rankenluchs) call Play from their behavior.
 *  The root bone is kept on its reference pose, so root motion in a sequence never moves the mesh; the code moves the actor.
 */
UCLASS(Transient, NotBlueprintable)
class UVaelSequenceAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:

	/** Starts a sequence, blending over the given seconds from what plays now. Playing the sequence that plays already does nothing unless bRestart. */
	void Play(UAnimSequence* Sequence, bool bLoop, float BlendTime = 0.25f, float Rate = 1.0f, bool bRestart = false);

	/** Changes the speed of the current sequence, for example to match the walking speed */
	void SetRate(float Rate) { CurrentRate = Rate; }

	/** Plays a looping sequence from the same share of its length the current one has reached, so gaits hand over in step */
	void PlayInStep(UAnimSequence* Sequence, float BlendTime, float Rate);

	/** Sequence playing now */
	UAnimSequence* GetCurrent() const { return Current.Get(); }

	/** Seconds into the current sequence */
	float GetCurrentTime() const { return CurrentTime; }

	/** True once a sequence that doesn't loop has reached its end */
	bool IsFinished() const;

	/**
	 *  Lays a chain of bones onto points instead of where the sequence puts them (a snake's spine onto the path its head
	 *  took). Bones run from the chain's root to its tip, whose own bone only gives the last joint; Targets are one per
	 *  bone, in component space: X and Y are where each joint goes, Z is added to the height the sequence gives it.
	 *  Weight 0 leaves the sequence alone.
	 */
	void SetSpineTargets(const TArray<FName>& Bones, const TArray<FVector>& Targets, float Weight, const TArray<FVector>& Ups = TArray<FVector>());

	/** Turns the tip of the spine chain (the head) towards a point in component space, by at most MaxAngle degrees;
	 *  its forward is the way it points in the reference pose's X. Weight 0 turns it nowhere. Whenever the chain is laid
	 *  out the head is kept upright (its top up) and tilted no further than MaxPitch degrees up or down */
	void SetLookAt(const FVector& Point, float MaxAngle, float Weight, float MaxPitch = 90.0f);

	/** Scales bones on top of the sequence (fins drawn in further) and turns them back towards their reference pose by
	 *  Opens (0 as the sequence has them, 1 as in the reference pose: fins spread), quivering by TrembleAngle degrees
	 *  (each bone its own uneven shake at Time seconds); an empty list changes nothing */
	void SetBoneScales(const TArray<FName>& Bones, const TArray<float>& Scales, const TArray<float>& Opens, float Tremble, float Time,
		float Sway = 0.0f, const TArray<float>* Phases = nullptr);

	/**
	 *  Lays chains of bones along a curve from where their root is to a target (vines striking far out): the bones
	 *  stretch apart as far as the path is long, so a chain may reach several times its own length; the skin between
	 *  the joints stretches along, pieces weighted to one bone (thorns, a bud) keep their shape. An empty list or
	 *  weight 0 leaves the sequence alone.
	 */
	void SetReachChains(const TArray<FVaelReachChain>& Chains) { ReachChains = Chains; }

protected:

	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FVaelSequenceAnimProxy(this); }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override { delete static_cast<FVaelSequenceAnimProxy*>(InProxy); }

	/** Advances both sequences and the blend */
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:

	friend struct FVaelSequenceAnimProxy;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> Current;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> Previous;

	float CurrentTime = 0.0f;
	float PreviousTime = 0.0f;
	float CurrentRate = 1.0f;
	float PreviousRate = 1.0f;
	bool bCurrentLoop = true;
	bool bPreviousLoop = true;
	float BlendDuration = 0.25f;
	float BlendElapsed = 0.0f;

	TArray<FName> SpineBones;
	TArray<FVector> SpineTargets;
	TArray<FVector> SpineUps;
	float SpineWeight = 0.0f;
	FVector LookPoint = FVector::ZeroVector;
	float LookMaxAngle = 0.0f;
	float HeadMaxPitch = 90.0f;
	float LookWeight = 0.0f;
	TArray<FName> ScaledBones;
	TArray<float> BoneScales;
	TArray<float> BoneOpens;
	float TrembleAngle = 0.0f;
	float TrembleTime = 0.0f;
	float SwayAngle = 0.0f;
	TArray<float> SwayPhases;
	TArray<FVaelReachChain> ReachChains;
};
