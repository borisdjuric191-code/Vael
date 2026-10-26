// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/VaelAnimInstance.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Magic/VaelGameplayTags.h"
#include "Player/VaelCharacter.h"
#include "Floromant/VaelFloromantComponent.h"
#include "Floromant/VaelFloromantSettings.h"
#include "Animation/AnimNodeBase.h"
#include "BonePose.h"

void UVaelAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	Character = Cast<AVaelCharacter>(TryGetPawnOwner());
}

void UVaelAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (Character == nullptr)
	{
		Character = Cast<AVaelCharacter>(TryGetPawnOwner());
		if (Character == nullptr)
		{
			return;
		}
	}

	const FVector Velocity = Character->GetVelocity();
	const FVector GroundVelocity(Velocity.X, Velocity.Y, 0.0f);

	Speed = GroundVelocity.Size();
	bIsMoving = Speed > MovingThreshold;

	// The plant arms of a Floromant: it stretches out the firing arms and leans back
	const UVaelFloromantComponent* Floromant = Character->IsFloromant() ? Character->GetFloromant() : nullptr;
	PlantArmStance[0] = Floromant != nullptr ? Floromant->GetStance(true) : 0.0f;
	PlantArmStance[1] = Floromant != nullptr ? Floromant->GetStance(false) : 0.0f;
	ShieldStance[0] = Floromant != nullptr ? Floromant->GetShieldStance(true) : 0.0f;
	ShieldStance[1] = Floromant != nullptr ? Floromant->GetShieldStance(false) : 0.0f;
	const UVaelFloromantSettings* FloromantSettings = UVaelFloromantSettings::Get();
	PlantArmLean = FloromantSettings->FiringLean * FMath::Max(PlantArmStance[0], PlantArmStance[1]);
	PlantArmReach = FloromantSettings->FiringArmReach;

	FlightPose = Character->GetFlightPose();

	// The Floromant hovers instead of walking: its legs stay still while it glides; once it has landed to fire, it walks
	const bool bHovering = Character->IsFloromant() && FMath::Max(PlantArmStance[0], PlantArmStance[1]) < 0.5f;
	if (bHovering)
	{
		Speed = 0.0f;
		bIsMoving = false;
	}

	// Keep the last direction while standing, so the blend doesn't jump back to forward
	if (bIsMoving)
	{
		Direction = (GroundVelocity.Rotation() - Character->GetActorRotation()).GetNormalized().Yaw;
	}

	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	bIsFalling = Movement != nullptr && Movement->IsFalling();

	bIsDodging = Character->IsDodging() && !bHovering;
	bIsDowned = Character->IsDowned();

	const UAbilitySystemComponent* AbilitySystem = Character->GetAbilitySystemComponent();
	bIsCasting = AbilitySystem != nullptr && (AbilitySystem->HasMatchingGameplayTag(VaelTags::State_Casting) || AbilitySystem->HasMatchingGameplayTag(VaelTags::State_Channeling));
}

// ---------------------------------------------------------------- Proxy

void FVaelAnimProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);

	const UVaelAnimInstance* Instance = CastChecked<UVaelAnimInstance>(InAnimInstance);
	ArmStance[0] = Instance->PlantArmStance[0];
	ArmStance[1] = Instance->PlantArmStance[1];
	LeanDegrees = Instance->PlantArmLean;
	ArmReach = Instance->PlantArmReach;
	Flight = Instance->FlightPose;
	Shield[0] = Instance->ShieldStance[0];
	Shield[1] = Instance->ShieldStance[1];
}

namespace
{
	/** Index of a bone in the evaluated pose, none when the skeleton lacks it */
	FCompactPoseBoneIndex FindPoseBone(const FBoneContainer& Bones, const TCHAR* Name)
	{
		const int32 MeshIndex = Bones.GetPoseBoneIndexForBoneName(FName(Name));
		return MeshIndex == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
	}

	/** Turns one bone by a rotation in component space, around its own joint; its children follow */
	void TurnPoseBone(FCSPose<FCompactPose>& Pose, FCompactPoseBoneIndex Bone, const FQuat& Rotation)
	{
		if (Bone == INDEX_NONE)
		{
			return;
		}
		FTransform Transform = Pose.GetComponentSpaceTransform(Bone);
		Transform.SetRotation((Rotation * Transform.GetRotation()).GetNormalized());
		TArray<FBoneTransform> Changed = { FBoneTransform(Bone, Transform) };
		Pose.LocalBlendCSBoneTransforms(Changed, 1.0f);
	}

	/** Way from a bone to its child in component space, zero when either is missing */
	FVector PoseBoneDirection(FCSPose<FCompactPose>& Pose, FCompactPoseBoneIndex Bone, FCompactPoseBoneIndex Child)
	{
		if (Bone == INDEX_NONE || Child == INDEX_NONE)
		{
			return FVector::ZeroVector;
		}
		return (Pose.GetComponentSpaceTransform(Child).GetLocation() - Pose.GetComponentSpaceTransform(Bone).GetLocation()).GetSafeNormal();
	}

	/** Swings a bone so the end of its child moves towards a direction by the degrees */
	void SwingPoseBone(FCSPose<FCompactPose>& Pose, FCompactPoseBoneIndex Bone, FCompactPoseBoneIndex Child, const FVector& Towards, float Degrees)
	{
		const FVector Current = PoseBoneDirection(Pose, Bone, Child);
		const FVector Axis = FVector::CrossProduct(Current, Towards);
		if (Current.IsNearlyZero() || Axis.IsNearlyZero(0.001f) || FMath::IsNearlyZero(Degrees))
		{
			return;
		}
		TurnPoseBone(Pose, Bone, FQuat(Axis.GetSafeNormal(), FMath::DegreesToRadians(Degrees)));
	}
}

void FVaelAnimProxy::ApplyFlight(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones) const
{
	// The whole body tips around the pelvis into the way she flies
	TurnPoseBone(Pose, FindPoseBone(Bones, TEXT("pelvis")), Flight.Tilt);

	// The body faces +Y in the space of the mesh; after the tip
	const FVector Forward = Flight.Tilt.RotateVector(FVector::YAxisVector);
	const FVector Down = Flight.Tilt.RotateVector(-FVector::ZAxisVector);

	for (int32 Side = 0; Side < 2; ++Side)
	{
		const TCHAR* Suffix = Side == 0 ? TEXT("l") : TEXT("r");
		const FCompactPoseBoneIndex Thigh = FindPoseBone(Bones, *FString::Printf(TEXT("thigh_%s"), Suffix));
		const FCompactPoseBoneIndex Calf = FindPoseBone(Bones, *FString::Printf(TEXT("calf_%s"), Suffix));
		const FCompactPoseBoneIndex Foot = FindPoseBone(Bones, *FString::Printf(TEXT("foot_%s"), Suffix));
		const FCompactPoseBoneIndex Ball = FindPoseBone(Bones, *FString::Printf(TEXT("ball_%s"), Suffix));
		if (Thigh == INDEX_NONE)
		{
			continue;
		}

		// Dangling: the legs swing out the way the pendulum says and paddle slowly in turn
		const FVector Swing = Flight.LegSwing + Forward * Flight.LegKick[Side] * Flight.Weight;
		SwingPoseBone(Pose, Thigh, Calf, Swing.GetSafeNormal(), Swing.Size());

		// Knees a little bent, the shins hang back; the toes point down like a dancer's
		SwingPoseBone(Pose, Calf, Foot, -Forward, Flight.KneeBend);
		SwingPoseBone(Pose, Foot, Ball, Down, Flight.FootPoint);
	}
}

bool FVaelAnimProxy::Evaluate_WithRoot(FPoseContext& Output, FAnimNode_Base* InRootNode)
{
	// The graph of the animation blueprint as always
	EvaluateAnimationNode_WithRoot(Output, InRootNode);

	const bool bFlying = Flight.Weight > KINDA_SMALL_NUMBER;
	const bool bShielding = Shield[0] > KINDA_SMALL_NUMBER || Shield[1] > KINDA_SMALL_NUMBER;
	if (InRootNode != GetRootNode() || (!bFlying && !bShielding && ArmStance[0] <= KINDA_SMALL_NUMBER && ArmStance[1] <= KINDA_SMALL_NUMBER && LeanDegrees <= KINDA_SMALL_NUMBER))
	{
		return true;
	}

	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	const auto Find = [&Bones](const TCHAR* Name) { return FindPoseBone(Bones, Name); };

	FCSPose<FCompactPose> Pose;
	Pose.InitPose(Output.Pose);

	if (bFlying)
	{
		ApplyFlight(Pose, Bones);
	}

	const auto Turn = [&Pose](FCompactPoseBoneIndex Bone, const FQuat& Rotation) { TurnPoseBone(Pose, Bone, Rotation); };

	// Points a bone towards a direction, as far as the weight says
	const auto Aim = [&Pose, &Turn](FCompactPoseBoneIndex Bone, FCompactPoseBoneIndex Child, const FVector& Direction, float Weight)
	{
		if (Bone == INDEX_NONE || Child == INDEX_NONE || Weight <= 0.0f)
		{
			return;
		}
		const FVector Current = PoseBoneDirection(Pose, Bone, Child);
		if (Current.IsNearlyZero())
		{
			return;
		}
		const FQuat Full = FQuat::FindBetweenNormals(Current, Direction);
		Turn(Bone, FQuat::Slerp(FQuat::Identity, Full, Weight));
	};

	// Leaning back over the three bones of the spine; the body faces +Y in the space of the mesh, so turning around +X tips it back
	const FQuat LeanStep(FVector::XAxisVector, FMath::DegreesToRadians(LeanDegrees / 3.0f));
	for (const TCHAR* Spine : { TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03") })
	{
		Turn(Find(Spine), LeanStep);
	}

	// The firing arms straight out towards the aim, a little apart, upper arm, forearm and hand in turn
	const FCompactPoseBoneIndex Chest = Find(TEXT("spine_05")) != INDEX_NONE ? Find(TEXT("spine_05")) : Find(TEXT("spine_03"));
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const float Weight = FMath::SmoothStep(0.0f, 1.0f, ArmStance[Side]) * ArmReach;
		if (Weight <= 0.0f)
		{
			continue;
		}

		const TCHAR* Suffix = Side == 0 ? TEXT("l") : TEXT("r");
		const FCompactPoseBoneIndex Upper = Find(*FString::Printf(TEXT("upperarm_%s"), Suffix));
		const FCompactPoseBoneIndex Lower = Find(*FString::Printf(TEXT("lowerarm_%s"), Suffix));
		const FCompactPoseBoneIndex Hand = Find(*FString::Printf(TEXT("hand_%s"), Suffix));
		const FCompactPoseBoneIndex Middle = Find(*FString::Printf(TEXT("middle_01_%s"), Suffix));
		if (Upper == INDEX_NONE)
		{
			continue;
		}

		const float ChestX = Chest != INDEX_NONE ? Pose.GetComponentSpaceTransform(Chest).GetLocation().X : 0.0f;
		const float Outward = FMath::Sign(Pose.GetComponentSpaceTransform(Upper).GetLocation().X - ChestX);
		const FVector Direction = FVector(Outward * 0.12f, 1.0f, -0.04f).GetSafeNormal();

		Aim(Upper, Lower, Direction, Weight);
		Aim(Lower, Hand, Direction, Weight);
		Aim(Hand, Middle, Direction, Weight);
	}

	// A shield hand (Glimmklee): the elbow comes forward, the forearm stands up before face and chest, the back of the hand forward
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const float Weight = FMath::SmoothStep(0.0f, 1.0f, Shield[Side]);
		if (Weight <= 0.0f)
		{
			continue;
		}

		const TCHAR* Suffix = Side == 0 ? TEXT("l") : TEXT("r");
		const FCompactPoseBoneIndex Upper = Find(*FString::Printf(TEXT("upperarm_%s"), Suffix));
		const FCompactPoseBoneIndex Lower = Find(*FString::Printf(TEXT("lowerarm_%s"), Suffix));
		const FCompactPoseBoneIndex Hand = Find(*FString::Printf(TEXT("hand_%s"), Suffix));
		const FCompactPoseBoneIndex Middle = Find(*FString::Printf(TEXT("middle_01_%s"), Suffix));
		if (Upper == INDEX_NONE)
		{
			continue;
		}

		const float ChestX = Chest != INDEX_NONE ? Pose.GetComponentSpaceTransform(Chest).GetLocation().X : 0.0f;
		const float Outward = FMath::Sign(Pose.GetComponentSpaceTransform(Upper).GetLocation().X - ChestX);
		Aim(Upper, Lower, FVector(Outward * 0.3f, 0.75f, -0.5f).GetSafeNormal(), Weight);
		Aim(Lower, Hand, FVector(-Outward * 0.45f, 0.3f, 0.85f).GetSafeNormal(), Weight);
		Aim(Hand, Middle, FVector(-Outward * 0.2f, 0.15f, 1.0f).GetSafeNormal(), Weight);
	}

	FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(Pose), Output.Pose);
	return true;
}
