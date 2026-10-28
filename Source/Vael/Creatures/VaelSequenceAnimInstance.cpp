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

void UVaelSequenceAnimInstance::SetSpineTargets(const TArray<FName>& Bones, const TArray<FVector>& Targets, float Weight, const TArray<FVector>& Ups)
{
	SpineBones = Bones;
	SpineTargets = Targets;
	SpineUps = Ups;
	SpineWeight = Bones.Num() == Targets.Num() ? FMath::Clamp(Weight, 0.0f, 1.0f) : 0.0f;
}

void UVaelSequenceAnimInstance::SetLookAt(const FVector& Point, float MaxAngle, float Weight)
{
	LookPoint = Point;
	LookMaxAngle = MaxAngle;
	LookWeight = FMath::Clamp(Weight, 0.0f, 1.0f);
}

void UVaelSequenceAnimInstance::SetBoneScales(const TArray<FName>& Bones, const TArray<float>& Scales, const TArray<float>& Opens, float Tremble, float Time)
{
	ScaledBones = Bones;
	BoneScales = Scales;
	BoneOpens = Opens;
	TrembleAngle = Tremble;
	TrembleTime = Time;
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
	SpineUps = Instance->SpineUps;
	SpineWeight = Instance->SpineWeight;
	LookPoint = Instance->LookPoint;
	LookMaxAngle = Instance->LookMaxAngle;
	LookWeight = Instance->LookWeight;
	ScaledBones = Instance->ScaledBones;
	BoneScales = Instance->BoneScales;
	BoneOpens = Instance->BoneOpens;
	TrembleAngle = Instance->TrembleAngle;
	TrembleTime = Instance->TrembleTime;
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
		if (!Bone.IsValid())
		{
			continue;
		}
		FTransform& Local = Pose[Bone];
		const float Open = BoneOpens.IsValidIndex(Index) ? BoneOpens[Index] : 0.0f;
		if (Open > 0.0f)
		{
			// Spread back towards the reference pose, scale and all
			const FTransform& Rest = Pose.GetRefPose(Bone);
			Local.SetRotation(FQuat::Slerp(Local.GetRotation(), Rest.GetRotation(), Open).GetNormalized());
			Local.SetScale3D(FMath::Lerp(Local.GetScale3D(), Rest.GetScale3D(), Open));
		}
		if (TrembleAngle > 0.0f)
		{
			// Each fin its own fast, uneven shake
			const float Shake = FMath::DegreesToRadians(TrembleAngle) * (FMath::Sin(TrembleTime * UE_TWO_PI * 13.0f + Index * 1.7f) + 0.5f * FMath::Sin(TrembleTime * UE_TWO_PI * 21.0f + Index));
			Local.SetRotation((Local.GetRotation() * FQuat(FVector::XAxisVector, Shake) * FQuat(FVector::ZAxisVector, Shake * 0.6f)).GetNormalized());
		}
		Local.SetScale3D(Local.GetScale3D() * BoneScales[Index]);
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

	// The bones' back: the local axis that points up in the reference pose of a bone lying along the ground (the bones
	// share their axes); with up hints each bone is rolled about itself so its back faces its hint, its belly away
	FVector LocalBack = FVector::ZAxisVector;
	const bool bRoll = SpineUps.Num() == SpineTargets.Num();
	if (bRoll)
	{
		const FCompactPoseBoneIndex Sample = Chain[FMath::Min(4, Chain.Num() - 2)];
		FTransform RefSample = FTransform::Identity;
		for (FCompactPoseBoneIndex Bone = Sample; Bone.IsValid(); Bone = Pose.GetParentBoneIndex(Bone))
		{
			RefSample = RefSample * Pose.GetRefPose(Bone);
		}
		float Best = -2.0f;
		for (const FVector& Axis : { FVector::XAxisVector, FVector::YAxisVector, FVector::ZAxisVector, -FVector::XAxisVector, -FVector::YAxisVector, -FVector::ZAxisVector })
		{
			const float Up = RefSample.GetRotation().RotateVector(Axis).Z;
			if (Up > Best)
			{
				Best = Up;
				LocalBack = Axis;
			}
		}
	}

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
			if (bRoll)
			{
				const FVector Along = Wanted.GetSafeNormal();
				const FVector Back = FVector::VectorPlaneProject(Bone.GetRotation().RotateVector(LocalBack), Along).GetSafeNormal();
				const FVector Hint = FVector::VectorPlaneProject(SpineUps[Index], Along).GetSafeNormal();
				if (!Back.IsNearlyZero() && !Hint.IsNearlyZero())
				{
					const float Angle = FMath::Atan2(FVector::DotProduct(FVector::CrossProduct(Back, Hint), Along), FVector::DotProduct(Back, Hint));
					Bone.SetRotation((FQuat(Along, Angle * SpineWeight) * Bone.GetRotation()).GetNormalized());
				}
			}
		}
		Pose[Chain[Index]] = Bone.GetRelativeTransform(Parent);
		Parent = Bone;
	}

	// The head looks at its point: its forward is where it points in the reference pose (X)
	if (LookWeight > 0.0f)
	{
		const FCompactPoseBoneIndex Tip = Chain.Last();
		FTransform RefTip = FTransform::Identity;
		for (FCompactPoseBoneIndex Bone = Tip; Bone.IsValid(); Bone = Pose.GetParentBoneIndex(Bone))
		{
			RefTip = RefTip * Pose.GetRefPose(Bone);
		}
		const FVector LocalForward = RefTip.GetRotation().UnrotateVector(FVector::ForwardVector);
		// Its top: the local axis pointing up in the reference pose, where the head lies level
		FVector LocalTop = FVector::ZAxisVector;
		float Highest = -2.0f;
		for (const FVector& Axis : { FVector::XAxisVector, FVector::YAxisVector, FVector::ZAxisVector, -FVector::XAxisVector, -FVector::YAxisVector, -FVector::ZAxisVector })
		{
			const float Up = RefTip.GetRotation().RotateVector(Axis).Z;
			if (Up > Highest)
			{
				Highest = Up;
				LocalTop = Axis;
			}
		}
		FTransform Head = Pose[Tip] * Parent;
		const FVector Forward = Head.GetRotation().RotateVector(LocalForward).GetSafeNormal();
		const FVector ToPoint = (LookPoint - Head.GetTranslation()).GetSafeNormal();
		if (!ToPoint.IsNearlyZero() && !Forward.IsNearlyZero())
		{
			// Turns sideways and then up or down, never by the shortest way in space: that one rolls the head over
			const float MaxTurn = FMath::DegreesToRadians(LookMaxAngle);
			const float YawNow = FMath::Atan2(Forward.Y, Forward.X);
			const float Yaw = YawNow + FMath::Clamp(FMath::FindDeltaAngleRadians(YawNow, FMath::Atan2(ToPoint.Y, ToPoint.X)), -MaxTurn, MaxTurn) * LookWeight;
			const float PitchNow = FMath::Asin(FMath::Clamp(Forward.Z, -1.0f, 1.0f));
			const float PitchTo = FMath::Atan2(ToPoint.Z, FVector2D(ToPoint.X, ToPoint.Y).Size());
			const float Pitch = PitchNow + FMath::Clamp(PitchTo - PitchNow, -0.7f, 0.7f) * LookWeight;
			const FVector Wanted(FMath::Cos(Pitch) * FMath::Cos(Yaw), FMath::Cos(Pitch) * FMath::Sin(Yaw), FMath::Sin(Pitch));
			Head.SetRotation((FQuat::FindBetweenNormals(Forward, Wanted) * Head.GetRotation()).GetNormalized());

			// and keeps its top up, whatever the neck below it does
			const FVector Top = FVector::VectorPlaneProject(Head.GetRotation().RotateVector(LocalTop), Wanted).GetSafeNormal();
			const FVector Up = FVector::VectorPlaneProject(FVector::UpVector, Wanted).GetSafeNormal();
			if (!Top.IsNearlyZero() && !Up.IsNearlyZero())
			{
				const float Roll = FMath::Atan2(FVector::DotProduct(FVector::CrossProduct(Top, Up), Wanted), FVector::DotProduct(Top, Up));
				Head.SetRotation((FQuat(Wanted, Roll * LookWeight) * Head.GetRotation()).GetNormalized());
			}
			Pose[Tip] = Head.GetRelativeTransform(Parent);
		}
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
