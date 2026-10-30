// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/VaelPoseFollowerAnimInstance.h"
#include "Animation/AnimNodeBase.h"
#include "BonePose.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "ReferenceSkeleton.h"

namespace
{
	const FName PelvisBoneName(TEXT("pelvis"));

	/** Reference pose of a mesh in component space */
	TArray<FTransform> GetReferencePose(const USkeletalMesh* Mesh)
	{
		const FReferenceSkeleton& Skeleton = Mesh->GetRefSkeleton();
		const TArray<FTransform>& Local = Skeleton.GetRefBonePose();

		TArray<FTransform> Pose;
		Pose.SetNum(Local.Num());
		for (int32 Bone = 0; Bone < Local.Num(); ++Bone)
		{
			const int32 Parent = Skeleton.GetParentIndex(Bone);
			Pose[Bone] = Parent != INDEX_NONE ? Local[Bone] * Pose[Parent] : Local[Bone];
		}
		return Pose;
	}
}

void FVaelPoseFollowerProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);

	const UVaelPoseFollowerAnimInstance* Instance = CastChecked<UVaelPoseFollowerAnimInstance>(InAnimInstance);
	const USkeletalMeshComponent* Source = Instance->GetSource();
	const USkeletalMesh* SourceMesh = Source != nullptr ? Cast<USkeletalMesh>(Source->GetSkinnedAsset()) : nullptr;
	const USkeletalMesh* TargetMesh = Cast<USkeletalMesh>(InAnimInstance->GetSkelMeshComponent()->GetSkinnedAsset());
	if (SourceMesh == nullptr || TargetMesh == nullptr)
	{
		SourceTransforms.Reset();
		return;
	}

	if (MappedSource != SourceMesh || MappedTarget != TargetMesh)
	{
		BuildBoneMap(SourceMesh, TargetMesh);
	}

	SourceTransforms = Source->GetComponentSpaceTransforms();
}

void FVaelPoseFollowerProxy::BuildBoneMap(const USkeletalMesh* SourceMesh, const USkeletalMesh* TargetMesh)
{
	MappedSource = SourceMesh;
	MappedTarget = TargetMesh;

	const FReferenceSkeleton& SourceSkeleton = SourceMesh->GetRefSkeleton();
	const FReferenceSkeleton& TargetSkeleton = TargetMesh->GetRefSkeleton();
	const TArray<FTransform> SourcePose = GetReferencePose(SourceMesh);
	const TArray<FTransform> TargetPose = GetReferencePose(TargetMesh);

	SourceBones.Init(INDEX_NONE, TargetPose.Num());
	RotationOffsets.Init(FQuat::Identity, TargetPose.Num());
	PelvisBone = INDEX_NONE;

	for (int32 Bone = 0; Bone < TargetPose.Num(); ++Bone)
	{
		const FName Name = TargetSkeleton.GetBoneName(Bone);
		const int32 SourceBone = SourceSkeleton.FindBoneIndex(Name);
		if (SourceBone == INDEX_NONE)
		{
			continue;
		}

		SourceBones[Bone] = SourceBone;
		RotationOffsets[Bone] = SourcePose[SourceBone].GetRotation().Inverse() * TargetPose[Bone].GetRotation();

		if (Name == PelvisBoneName)
		{
			PelvisBone = Bone;
			SourcePelvisRest = SourcePose[SourceBone].GetTranslation();
			TargetPelvisRest = TargetPose[Bone].GetTranslation();
			PelvisScale = SourcePelvisRest.Z > UE_KINDA_SMALL_NUMBER ? TargetPelvisRest.Z / SourcePelvisRest.Z : 1.0f;
		}
	}
}

bool FVaelPoseFollowerProxy::Evaluate(FPoseContext& Output)
{
	FCompactPose& Pose = Output.Pose;
	Pose.ResetToRefPose();
	if (SourceTransforms.IsEmpty())
	{
		return true;
	}

	const FBoneContainer& Bones = Pose.GetBoneContainer();
	ComponentSpace.SetNumUninitialized(Pose.GetNumBones(), EAllowShrinking::No);

	for (const FCompactPoseBoneIndex Index : Pose.ForEachBoneIndex())
	{
		const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(Index);
		const FTransform ParentSpace = Parent.IsValid() ? ComponentSpace[Parent.GetInt()] : FTransform::Identity;
		FTransform& Bone = ComponentSpace[Index.GetInt()];
		Bone = Pose[Index] * ParentSpace;

		const int32 MeshBone = Bones.MakeMeshPoseIndex(Index).GetInt();
		const int32 SourceBone = SourceBones.IsValidIndex(MeshBone) ? SourceBones[MeshBone] : INDEX_NONE;
		if (!SourceTransforms.IsValidIndex(SourceBone))
		{
			continue;
		}

		const FTransform& Followed = SourceTransforms[SourceBone];
		Bone.SetRotation((Followed.GetRotation() * RotationOffsets[MeshBone]).GetNormalized());
		if (MeshBone == PelvisBone)
		{
			Bone.SetTranslation(TargetPelvisRest + (Followed.GetTranslation() - SourcePelvisRest) * PelvisScale);
		}
		Pose[Index] = Bone.GetRelativeTransform(ParentSpace);
	}

	return true;
}
