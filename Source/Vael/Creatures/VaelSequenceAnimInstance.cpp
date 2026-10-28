// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelSequenceAnimInstance.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"

namespace
{
	/** Moves a playing time on, wrapping or stopping at the end */
	float Advance(float Time, float DeltaSeconds, float Rate, bool bLoop, const UAnimSequence* Sequence)
	{
		const float Length = Sequence != nullptr ? Sequence->GetPlayLength() : 0.0f;
		if (Length <= KINDA_SMALL_NUMBER)
		{
			return 0.0f;
		}
		Time += DeltaSeconds * Rate;
		return bLoop ? FMath::Fmod(FMath::Fmod(Time, Length) + Length, Length) : FMath::Clamp(Time, 0.0f, Length);
	}
}

void UVaelSequenceAnimInstance::Play(UAnimSequence* Sequence, bool bLoop, float BlendTime, float Rate, bool bRestart)
{
	if (Sequence == nullptr || (Sequence == Current && !bRestart))
	{
		return;
	}

	Previous = Current;
	PreviousTime = CurrentTime;
	PreviousRate = CurrentRate;
	bPreviousLoop = bCurrentLoop;

	Current = Sequence;
	CurrentTime = 0.0f;
	CurrentRate = Rate;
	bCurrentLoop = bLoop;
	BlendDuration = FMath::Max(BlendTime, 0.0f);
	BlendElapsed = 0.0f;
}

void UVaelSequenceAnimInstance::PlayInStep(UAnimSequence* Sequence, float BlendTime, float Rate)
{
	if (Sequence == nullptr || Sequence == Current)
	{
		return;
	}
	const float OldLength = Current != nullptr ? Current->GetPlayLength() : 0.0f;
	const float Phase = OldLength > KINDA_SMALL_NUMBER ? FMath::Fmod(CurrentTime, OldLength) / OldLength : 0.0f;
	Play(Sequence, true, BlendTime, Rate);
	CurrentTime = Phase * Sequence->GetPlayLength();
}

bool UVaelSequenceAnimInstance::IsFinished() const
{
	return Current != nullptr && !bCurrentLoop && CurrentTime >= Current->GetPlayLength() - KINDA_SMALL_NUMBER;
}

void UVaelSequenceAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	CurrentTime = Advance(CurrentTime, DeltaSeconds, CurrentRate, bCurrentLoop, Current);
	if (Previous != nullptr)
	{
		PreviousTime = Advance(PreviousTime, DeltaSeconds, PreviousRate, bPreviousLoop, Previous);
		BlendElapsed += DeltaSeconds;
		if (BlendElapsed >= BlendDuration)
		{
			Previous = nullptr;
		}
	}
}

void FVaelSequenceAnimProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);

	const UVaelSequenceAnimInstance* Instance = CastChecked<UVaelSequenceAnimInstance>(InAnimInstance);
	Current = Instance->Current.Get();
	Previous = Instance->Previous.Get();
	CurrentTime = Instance->CurrentTime;
	PreviousTime = Instance->PreviousTime;
	Blend = Instance->BlendDuration > KINDA_SMALL_NUMBER ? FMath::Clamp(Instance->BlendElapsed / Instance->BlendDuration, 0.0f, 1.0f) : 1.0f;
}

bool FVaelSequenceAnimProxy::Evaluate(FPoseContext& Output)
{
	if (Current == nullptr)
	{
		Output.ResetToRefPose();
		return true;
	}

	FAnimationPoseData CurrentData(Output);
	Current->GetAnimationPose(CurrentData, FAnimExtractContext(static_cast<double>(CurrentTime), false));

	if (Previous != nullptr && Blend < 1.0f)
	{
		FPoseContext PreviousPose(Output);
		FAnimationPoseData PreviousData(PreviousPose);
		Previous->GetAnimationPose(PreviousData, FAnimExtractContext(static_cast<double>(PreviousTime), false));

		FPoseContext Blended(Output);
		FAnimationPoseData BlendedData(Blended);
		// Smooth in and out
		const float Weight = FMath::SmoothStep(0.0f, 1.0f, Blend);
		FAnimationRuntime::BlendTwoPosesTogether(PreviousData, CurrentData, 1.0f - Weight, BlendedData);
		Output.Pose = Blended.Pose;
		Output.Curve = Blended.Curve;
		Output.CustomAttributes = Blended.CustomAttributes;
	}

	// The actor moves the creature; the root bone stays where it rests
	if (Output.Pose.GetNumBones() > 0)
	{
		const FCompactPoseBoneIndex Root(0);
		Output.Pose[Root].SetTranslation(Output.Pose.GetRefPose(Root).GetTranslation());
	}
	return true;
}
