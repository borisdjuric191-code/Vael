// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "VaelSequenceAnimInstance.generated.h"

class UAnimSequence;

/** Evaluates the current sequence and, while a change is blending in, the one before it (on the animation worker thread) */
struct FVaelSequenceAnimProxy : public FAnimInstanceProxy
{
	FVaelSequenceAnimProxy() = default;
	explicit FVaelSequenceAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

	UAnimSequence* Current = nullptr;
	UAnimSequence* Previous = nullptr;
	float CurrentTime = 0.0f;
	float PreviousTime = 0.0f;

	/** 0 = all the previous sequence, 1 = all the current one */
	float Blend = 1.0f;
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
};
