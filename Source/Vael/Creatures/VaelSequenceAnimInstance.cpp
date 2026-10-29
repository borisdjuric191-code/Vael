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

void UVaelSequenceAnimInstance::SetLookAt(const FVector& Point, float MaxAngle, float Weight, float MaxPitch)
{
	LookPoint = Point;
	LookMaxAngle = MaxAngle;
	HeadMaxPitch = MaxPitch;
	LookWeight = FMath::Clamp(Weight, 0.0f, 1.0f);
}

void UVaelSequenceAnimInstance::SetBoneScales(const TArray<FName>& Bones, const TArray<float>& Scales, const TArray<float>& Opens, float Tremble, float Time,
	float Sway, const TArray<float>* Phases)
{
	SwayAngle = Sway;
	SwayPhases = Phases != nullptr ? *Phases : TArray<float>();
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
	HeadMaxPitch = Instance->HeadMaxPitch;
	LookWeight = Instance->LookWeight;
	ScaledBones = Instance->ScaledBones;
	BoneScales = Instance->BoneScales;
	BoneOpens = Instance->BoneOpens;
	TrembleAngle = Instance->TrembleAngle;
	TrembleTime = Instance->TrembleTime;
	SwayAngle = Instance->SwayAngle;
	SwayPhases = Instance->SwayPhases;
	ReachChains = Instance->ReachChains;
	GroundLimbs = Instance->GroundLimbs;
	GroundPelvis = Instance->GroundPelvis;
	GroundPelvisShift = Instance->GroundPelvisShift;
}

void FVaelSequenceAnimProxy::ApplyGrounding(FCompactPose& Pose) const
{
	if (GroundLimbs.Num() == 0)
	{
		return;
	}
	const FBoneContainer& Container = Pose.GetBoneContainer();
	const auto Find = [&Container](const FName& Name)
	{
		const int32 MeshIndex = Name.IsNone() ? INDEX_NONE : Container.GetPoseBoneIndexForBoneName(Name);
		return MeshIndex == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Container.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
	};
	const auto ComponentSpace = [&Pose](FCompactPoseBoneIndex Bone)
	{
		FTransform Result = FTransform::Identity;
		for (; Bone.IsValid(); Bone = Pose.GetParentBoneIndex(Bone))
		{
			Result = Result * Pose[Bone];
		}
		return Result;
	};

	// The whole body up or down (a slope: down so the lowest limb still reaches)
	const FCompactPoseBoneIndex Pelvis = Find(GroundPelvis);
	if (Pelvis.IsValid() && !FMath::IsNearlyZero(GroundPelvisShift))
	{
		const FTransform Parent = ComponentSpace(Pose.GetParentBoneIndex(Pelvis));
		FTransform Moved = Pose[Pelvis] * Parent;
		Moved.AddToTranslation(FVector(0.0f, 0.0f, GroundPelvisShift));
		Pose[Pelvis] = Moved.GetRelativeTransform(Parent);
	}

	for (const FVaelGroundLimb& Limb : GroundLimbs)
	{
		const FCompactPoseBoneIndex Upper = Find(Limb.Upper);
		const FCompactPoseBoneIndex Lower = Find(Limb.Lower);
		const FCompactPoseBoneIndex End = Find(Limb.End);
		const float Delta = Limb.Lift - GroundPelvisShift;
		if (!Upper.IsValid() || !Lower.IsValid() || !End.IsValid() || FMath::Abs(Delta) < 0.5f)
		{
			continue;
		}
		const FTransform Parent = ComponentSpace(Pose.GetParentBoneIndex(Upper));
		const FTransform UpperBone = Pose[Upper] * Parent;
		const FTransform LowerBone = Pose[Lower] * UpperBone;
		const FTransform EndBone = Pose[End] * LowerBone;
		const FVector A = UpperBone.GetTranslation();
		const FVector B = LowerBone.GetTranslation();
		const FVector C = EndBone.GetTranslation();
		const float UpperLength = FVector::Dist(A, B);
		const float LowerLength = FVector::Dist(B, C);
		if (UpperLength < 1.0f || LowerLength < 1.0f)
		{
			continue;
		}

		// Two-bone IK in the plane the limb bends in now: the joint keeps pointing the way it does
		const FVector Goal = C + FVector(0.0f, 0.0f, Delta);
		const FVector ToGoal = Goal - A;
		const FVector Along = ToGoal.GetSafeNormal();
		const float Reach = FMath::Clamp(ToGoal.Size(), FMath::Abs(UpperLength - LowerLength) + 1.0f, UpperLength + LowerLength - 0.5f);
		FVector Bend = (B - A) - Along * FVector::DotProduct(B - A, Along);
		if (Bend.IsNearlyZero())
		{
			continue;
		}
		Bend.Normalize();
		const float CosAngle = FMath::Clamp((UpperLength * UpperLength + Reach * Reach - LowerLength * LowerLength) / (2.0f * UpperLength * Reach), -1.0f, 1.0f);
		const float Angle = FMath::Acos(CosAngle);
		const FVector NewB = A + Along * UpperLength * CosAngle + Bend * UpperLength * FMath::Sin(Angle);
		const FVector NewC = A + Along * Reach;

		FTransform NewUpper = UpperBone;
		const FQuat UpperTurn = FQuat::FindBetweenVectors(B - A, NewB - A);
		NewUpper.SetRotation((UpperTurn * UpperBone.GetRotation()).GetNormalized());
		FTransform NewLower = LowerBone;
		NewLower.SetTranslation(NewB);
		const FQuat LowerTurn = FQuat::FindBetweenVectors(UpperTurn.RotateVector(C - B), NewC - NewB) * UpperTurn;
		NewLower.SetRotation((LowerTurn * LowerBone.GetRotation()).GetNormalized());
		FTransform NewEnd = EndBone;
		NewEnd.SetTranslation(NewC);

		Pose[Upper] = NewUpper.GetRelativeTransform(Parent);
		Pose[Lower] = NewLower.GetRelativeTransform(NewUpper);
		Pose[End] = NewEnd.GetRelativeTransform(NewLower);
	}
}

void FVaelSequenceAnimProxy::ApplyReach(FCompactPose& Pose) const
{
	const FBoneContainer& Container = Pose.GetBoneContainer();
	const auto Find = [&Container](const FName& Name)
	{
		const int32 MeshIndex = Name.IsNone() ? INDEX_NONE : Container.GetPoseBoneIndexForBoneName(Name);
		return MeshIndex == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Container.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
	};

	for (const FVaelReachChain& Reach : ReachChains)
	{
		TArray<FCompactPoseBoneIndex, TInlineAllocator<24>> Chain;
		for (const FName& Name : Reach.Bones)
		{
			const FCompactPoseBoneIndex Bone = Find(Name);
			if (!Bone.IsValid())
			{
				Chain.Reset();
				break;
			}
			Chain.Add(Bone);
		}
		const int32 Count = Chain.Num();
		if (Count < 2)
		{
			continue;
		}

		// Component space of the chain's parent and of each bone as the sequence has them
		FTransform Parent = FTransform::Identity;
		for (FCompactPoseBoneIndex Bone = Pose.GetParentBoneIndex(Chain[0]); Bone.IsValid(); Bone = Pose.GetParentBoneIndex(Bone))
		{
			Parent = Parent * Pose[Bone];
		}
		TArray<FTransform, TInlineAllocator<24>> Sequence;
		FTransform Walk = Parent;
		for (const FCompactPoseBoneIndex Bone : Chain)
		{
			Walk = Pose[Bone] * Walk;
			Sequence.Add(Walk);
		}

		const float Weight = FMath::Clamp(Reach.Weight, 0.0f, 1.0f);
		if (Weight > 0.0f)
		{
			// The path: out of the root the way the chain grows, up by the arc, and down onto the target from above
			const FVector Start = Sequence[0].GetTranslation();
			const FVector Out = (Sequence[1].GetTranslation() - Start).GetSafeNormal();
			const float Distance = FVector::Dist(Start, Reach.Target);
			const FVector Lift(0.0f, 0.0f, Reach.Arc);
			const FVector B1 = Start + Out * Distance * 0.35f + Lift;
			const FVector B2 = Reach.Target + Lift * 0.6f;
			constexpr int32 Samples = 40;
			TArray<FVector, TInlineAllocator<Samples + 1>> Curve;
			TArray<float, TInlineAllocator<Samples + 1>> Along;
			for (int32 Index = 0; Index <= Samples; ++Index)
			{
				const float T = static_cast<float>(Index) / Samples;
				const float U = 1.0f - T;
				Curve.Add(U * U * U * Start + 3.0f * U * U * T * B1 + 3.0f * U * T * T * B2 + T * T * T * Reach.Target);
				Along.Add(Index == 0 ? 0.0f : Along.Last() + FVector::Dist(Curve[Index - 1], Curve[Index]));
			}
			const float Length = FMath::Max(Along.Last(), 1.0f);
			const FVector Side = FVector::CrossProduct((Reach.Target - Start).GetSafeNormal(), FVector::UpVector).GetSafeNormal();

			// The joints spread evenly along it, the tip piece at its end
			TArray<FVector, TInlineAllocator<24>> Wanted;
			int32 Segment = 0;
			for (int32 Index = 0; Index < Count; ++Index)
			{
				const float Share = static_cast<float>(Index) / (Count - 1);
				const float Goal = Share * Length;
				while (Segment < Samples - 1 && Along[Segment + 1] < Goal)
				{
					++Segment;
				}
				const float Part = FMath::Clamp((Goal - Along[Segment]) / FMath::Max(Along[Segment + 1] - Along[Segment], KINDA_SMALL_NUMBER), 0.0f, 1.0f);
				const FVector OnPath = FMath::Lerp(Curve[Segment], Curve[Segment + 1], Part)
					+ Side * Reach.Wave * FMath::Sin(Share * UE_PI) * FMath::Sin(Share * UE_PI * 3.0f + Reach.WavePhase);
				Wanted.Add(FMath::Lerp(Sequence[Index].GetTranslation(), OnPath, Weight));
			}

			// Every bone moved onto its point and turned towards the next; the gaps between them are the stretch
			FTransform Before = Parent;
			for (int32 Index = 0; Index < Count; ++Index)
			{
				const bool bLast = Index + 1 == Count;
				const FVector Was = bLast ? Sequence[Index].GetTranslation() - Sequence[Index - 1].GetTranslation() : Sequence[Index + 1].GetTranslation() - Sequence[Index].GetTranslation();
				const FVector Now = bLast ? Wanted[Index] - Wanted[Index - 1] : Wanted[Index + 1] - Wanted[Index];
				FTransform Bone = Sequence[Index];
				Bone.SetTranslation(Wanted[Index]);
				if (!Was.IsNearlyZero() && !Now.IsNearlyZero())
				{
					Bone.SetRotation((FQuat::FindBetweenVectors(Was, Now) * Sequence[Index].GetRotation()).GetNormalized());
				}
				Pose[Chain[Index]] = Bone.GetRelativeTransform(Before);
				Sequence[Index] = Bone;
				Before = Bone;
			}
		}

		// The jaws on the tip piece part along the slit between them
		const FCompactPoseBoneIndex Upper = Find(Reach.UpperJaw);
		const FCompactPoseBoneIndex Lower = Find(Reach.LowerJaw);
		if (FMath::IsNearlyZero(Reach.JawOpen) || !Upper.IsValid() || !Lower.IsValid())
		{
			continue;
		}
		const FTransform Tip = Sequence.Last();
		const FTransform UpperBone = Pose[Upper] * Tip;
		const FTransform LowerBone = Pose[Lower] * Tip;
		const FVector Axis = ((UpperBone.GetTranslation() + LowerBone.GetTranslation()) * 0.5f - Tip.GetTranslation()).GetSafeNormal();
		const FVector Slit = (UpperBone.GetTranslation() - LowerBone.GetTranslation()).GetSafeNormal();
		const FVector Hinge = FVector::CrossProduct(Axis, Slit).GetSafeNormal();
		if (Hinge.IsNearlyZero())
		{
			continue;
		}
		const float Angle = FMath::DegreesToRadians(Reach.JawOpen);
		FTransform Opened = UpperBone;
		Opened.SetRotation((FQuat(Hinge, Angle) * UpperBone.GetRotation()).GetNormalized());
		Pose[Upper] = Opened.GetRelativeTransform(Tip);
		Opened = LowerBone;
		Opened.SetRotation((FQuat(Hinge, -Angle) * LowerBone.GetRotation()).GetNormalized());
		Pose[Lower] = Opened.GetRelativeTransform(Tip);
	}
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
		if (SwayAngle > 0.0f)
		{
			// A slow, calm sway, each bone at its own phase (a wave running along a row of fins)
			const float Phase = SwayPhases.IsValidIndex(Index) ? SwayPhases[Index] : Index * 0.6f;
			const float Swing = FMath::DegreesToRadians(SwayAngle) * FMath::Sin(TrembleTime * UE_TWO_PI * 0.55f - Phase);
			const float Lean = FMath::DegreesToRadians(SwayAngle) * 0.5f * FMath::Sin(TrembleTime * UE_TWO_PI * 0.37f - Phase * 0.7f + 1.3f);
			Local.SetRotation((Local.GetRotation() * FQuat(FVector::XAxisVector, Swing) * FQuat(FVector::ZAxisVector, Lean)).GetNormalized());
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
	// (every time the chain is laid out: the head kept upright and level enough, turned towards the look point by its weight)
	if (SpineWeight > 0.0f)
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
		const FVector Towards = (LookPoint - Head.GetTranslation()).GetSafeNormal();
		const FVector ToPoint = LookWeight > 0.0f && !Towards.IsNearlyZero() ? Towards : Forward;
		if (!ToPoint.IsNearlyZero() && !Forward.IsNearlyZero())
		{
			// Turns sideways and then up or down, never by the shortest way in space: that one rolls the head over
			const float MaxTurn = FMath::DegreesToRadians(LookMaxAngle);
			const float YawNow = FMath::Atan2(Forward.Y, Forward.X);
			const float Yaw = YawNow + FMath::Clamp(FMath::FindDeltaAngleRadians(YawNow, FMath::Atan2(ToPoint.Y, ToPoint.X)), -MaxTurn, MaxTurn) * LookWeight;
			const float PitchNow = FMath::Asin(FMath::Clamp(Forward.Z, -1.0f, 1.0f));
			const float PitchTo = FMath::Atan2(ToPoint.Z, FVector2D(ToPoint.X, ToPoint.Y).Size());
			const float MaxPitch = FMath::DegreesToRadians(HeadMaxPitch);
			const float Pitch = FMath::Clamp(PitchNow + FMath::Clamp(PitchTo - PitchNow, -0.7f, 0.7f) * LookWeight, -MaxPitch, MaxPitch);
			const FVector Wanted(FMath::Cos(Pitch) * FMath::Cos(Yaw), FMath::Cos(Pitch) * FMath::Sin(Yaw), FMath::Sin(Pitch));
			Head.SetRotation((FQuat::FindBetweenNormals(Forward, Wanted) * Head.GetRotation()).GetNormalized());

			// and keeps its top up, whatever the neck below it does
			const FVector Top = FVector::VectorPlaneProject(Head.GetRotation().RotateVector(LocalTop), Wanted).GetSafeNormal();
			const FVector Up = FVector::VectorPlaneProject(FVector::UpVector, Wanted).GetSafeNormal();
			if (!Top.IsNearlyZero() && !Up.IsNearlyZero())
			{
				const float Roll = FMath::Atan2(FVector::DotProduct(FVector::CrossProduct(Top, Up), Wanted), FVector::DotProduct(Top, Up));
				Head.SetRotation((FQuat(Wanted, Roll) * Head.GetRotation()).GetNormalized());
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
	ApplyGrounding(Output.Pose);
	ApplyReach(Output.Pose);
	return true;
}
