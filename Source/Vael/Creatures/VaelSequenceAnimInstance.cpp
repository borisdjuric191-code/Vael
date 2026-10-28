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

void UVaelSequenceAnimInstance::SetSpineTargets(const TArray<FName>& Bones, const TArray<FVector>& Targets, float Weight)
{
	SpineBones = Bones;
	SpineTargets = Targets;
	SpineWeight = Bones.Num() == Targets.Num() ? FMath::Clamp(Weight, 0.0f, 1.0f) : 0.0f;
}

void UVaelSequenceAnimInstance::SetBoneScales(const TArray<FName>& Bones, const TArray<float>& Scales)
{
	ScaledBones = Bones;
	BoneScales = Scales;
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
	SpineBones = Instance->SpineBones;
	SpineTargets = Instance->SpineTargets;
	SpineWeight = Instance->SpineWeight;
	ScaledBones = Instance->ScaledBones;
	BoneScales = Instance->BoneScales;
}

void FVaelSequenceAnimProxy::ApplySpine(FCompactPose& Pose) const
{
	const FBoneContainer& Container = Pose.GetBoneContainer();
	const auto Find = [&Container](const FName& Name)
	{
		const int32 MeshIndex = Container.GetPoseBoneIndexForBoneName(Name);
		return MeshIndex == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Container.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
	};

	for (int32 Index = 0; Index < ScaledBones.Num() && Index < BoneScales.Num(); ++Index)
	{
		const FCompactPoseBoneIndex Bone = Find(ScaledBones[Index]);
		if (Bone.IsValid())
		{
			Pose[Bone].SetScale3D(Pose[Bone].GetScale3D() * BoneScales[Index]);
		}
	}

	if (SpineWeight <= 0.0f || SpineBones.Num() < 2 || SpineBones.Num() != SpineTargets.Num())
	{
		return;
	}
	TArray<FCompactPoseBoneIndex, TInlineAllocator<32>> Chain;
	for (const FName& Name : SpineBones)
	{
		const FCompactPoseBoneIndex Bone = Find(Name);
		if (!Bone.IsValid())
		{
			return;
		}
		Chain.Add(Bone);
	}

	// Component space of the chain's parent, and where the sequence puts each joint
	FTransform Parent = FTransform::Identity;
	for (FCompactPoseBoneIndex Bone = Pose.GetParentBoneIndex(Chain[0]); Bone.IsValid(); Bone = Pose.GetParentBoneIndex(Bone))
	{
		Parent = Parent * Pose[Bone];
	}
	TArray<FVector, TInlineAllocator<32>> Joints;
	FTransform Walk = Parent;
	for (int32 Index = 0; Index < Chain.Num(); ++Index)
	{
		Walk = Pose[Chain[Index]] * Walk;
		Joints.Add(Walk.GetTranslation());
	}
	const auto Target = [&](int32 Index)
	{
		const FVector Wanted(SpineTargets[Index].X, SpineTargets[Index].Y, Joints[Index].Z + SpineTargets[Index].Z);
		return FMath::Lerp(Joints[Index], Wanted, SpineWeight);
	};

	// Each bone turned so its child joint lands on the next point; the lengths stay, so the points must be spaced by them
	for (int32 Index = 0; Index + 1 < Chain.Num(); ++Index)
	{
		FTransform Bone = Pose[Chain[Index]] * Parent;
		if (Index == 0)
		{
			Bone.SetTranslation(Target(0));
		}
		const FVector Child = Bone.TransformPosition(Pose[Chain[Index + 1]].GetTranslation()) - Bone.GetTranslation();
		const FVector Wanted = Target(Index + 1) - Bone.GetTranslation();
		if (!Child.IsNearlyZero() && !Wanted.IsNearlyZero())
		{
			Bone.SetRotation((FQuat::FindBetweenVectors(Child, Wanted) * Bone.GetRotation()).GetNormalized());
		}
		Pose[Chain[Index]] = Bone.GetRelativeTransform(Parent);
		Parent = Bone;
	}
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
	ApplySpine(Output.Pose);
	return true;
}
