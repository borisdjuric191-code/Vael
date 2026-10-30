// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "VaelPoseFollowerAnimInstance.generated.h"

class USkeletalMeshComponent;

/** Worker side of UVaelPoseFollowerAnimInstance: turns the pose of the source into a pose of this mesh */
struct FVaelPoseFollowerProxy : public FAnimInstanceProxy
{
	FVaelPoseFollowerProxy() = default;
	explicit FVaelPoseFollowerProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

private:

	/** Matches the bones of both meshes by name and stores what lies between their reference poses */
	void BuildBoneMap(const USkeletalMesh* SourceMesh, const USkeletalMesh* TargetMesh);

	/** The meshes the bone map was made for */
	TWeakObjectPtr<const USkeletalMesh> MappedSource;
	TWeakObjectPtr<const USkeletalMesh> MappedTarget;

	/** Per bone of this mesh: its bone on the source (INDEX_NONE: it keeps its place, like the corrective bones of a MetaHuman) */
	TArray<int32> SourceBones;

	/** Per bone of this mesh: turns a component space rotation of the source bone into one of this bone */
	TArray<FQuat> RotationOffsets;

	/** Pelvis of this mesh, the only bone that takes over movement; its rest place on both meshes and how they scale */
	int32 PelvisBone = INDEX_NONE;
	FVector SourcePelvisRest = FVector::ZeroVector;
	FVector TargetPelvisRest = FVector::ZeroVector;
	float PelvisScale = 1.0f;

	/** Component space pose of the source this frame */
	TArray<FTransform> SourceTransforms;

	/** Scratch space of Evaluate */
	TArray<FTransform> ComponentSpace;
};

/**
 * Lets a mesh on another skeleton follow a hidden source mesh bone by bone, by name and without an animation blueprint.
 * The new human bodies (MetaHuman) follow the mage's mesh this way, so its animations, cast gestures and sockets stay as they are.
 * Rotations are carried over in component space, bone lengths stay those of the body.
 */
UCLASS(Transient, NotBlueprintable)
class UVaelPoseFollowerAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:

	/** Sets the mesh to follow */
	void SetSource(USkeletalMeshComponent* InSource) { Source = InSource; }

	const USkeletalMeshComponent* GetSource() const { return Source.Get(); }

protected:

	//~Begin UAnimInstance
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FVaelPoseFollowerProxy(this); }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override { delete static_cast<FVaelPoseFollowerProxy*>(InProxy); }
	//~End UAnimInstance

private:

	UPROPERTY(Transient)
	TWeakObjectPtr<USkeletalMeshComponent> Source;
};
