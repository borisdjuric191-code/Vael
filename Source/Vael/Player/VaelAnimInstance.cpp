// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/VaelAnimInstance.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Magic/VaelGameplayTags.h"
#include "Magic/VaelMagicSettings.h"
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
	for (int32 Side = 0; Side < 2; ++Side)
	{
		bMortarArm[Side] = Floromant != nullptr && Floromant->IsMortarHand(Side == 0);
		MortarDraw[Side] = Floromant != nullptr ? Floromant->GetMortarDraw(Side == 0) : 0.0f;
	}
	MortarPitch = UVaelFloromantSettings::Get()->MortarArmPitch;
	const UVaelFloromantSettings* FloromantSettings = UVaelFloromantSettings::Get();
	PlantArmLean = FloromantSettings->FiringLean * FMath::Max(PlantArmStance[0], PlantArmStance[1]);
	PlantArmReach = FloromantSettings->FiringArmReach;

	FlightPose = Character->GetFlightPose();

	CastGesture = Character->GetCastGesture(CastGestureSeconds);

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
	for (int32 Side = 0; Side < 2; ++Side)
	{
		bMortar[Side] = Instance->bMortarArm[Side];
		Draw[Side] = Instance->MortarDraw[Side];
	}
	MortarPitch = Instance->MortarPitch;

	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	Gesture = Instance->CastGesture;
	GestureSeconds = Instance->CastGestureSeconds;
	GestureRelease = MagicSettings->GetGestureRelease(Gesture);
	GestureDuration = MagicSettings->GetGestureDuration(Gesture);
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

	/** Points a bone so its child lies towards a direction, as far as the weight says */
	void AimPoseBone(FCSPose<FCompactPose>& Pose, FCompactPoseBoneIndex Bone, FCompactPoseBoneIndex Child, const FVector& Direction, float Weight)
	{
		const FVector Current = PoseBoneDirection(Pose, Bone, Child);
		if (Current.IsNearlyZero() || Weight <= 0.0f)
		{
			return;
		}
		TurnPoseBone(Pose, Bone, FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(Current, Direction.GetSafeNormal()), Weight));
	}

	/** Axis around which a finger bends towards the palm, from how it is already bent; zero when it lies straight */
	FVector FingerCurlAxis(FCSPose<FCompactPose>& Pose, FCompactPoseBoneIndex First, FCompactPoseBoneIndex Second, FCompactPoseBoneIndex Third)
	{
		const FVector Axis = FVector::CrossProduct(PoseBoneDirection(Pose, First, Second), PoseBoneDirection(Pose, Second, Third));
		return Axis.Size() > 0.05f ? Axis.GetSafeNormal() : FVector::ZeroVector;
	}
}

void FVaelAnimProxy::ApplyTwoFingerPoint(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones) const
{
	const auto Find = [&Bones](const FString& Name) { return FindPoseBone(Bones, *Name); };

	const FCompactPoseBoneIndex Upper = Find(TEXT("upperarm_r"));
	const FCompactPoseBoneIndex Lower = Find(TEXT("lowerarm_r"));
	const FCompactPoseBoneIndex Hand = Find(TEXT("hand_r"));
	const FCompactPoseBoneIndex Middle = Find(TEXT("middle_01_r"));
	if (Upper == INDEX_NONE || Lower == INDEX_NONE || Hand == INDEX_NONE || Middle == INDEX_NONE)
	{
		return;
	}

	// Timing: the arm draws back until half the time to the shot, snaps forward and stays out a moment, then goes back into the walk
	const float T = GestureSeconds;
	const float Release = FMath::Max(GestureRelease, 0.02f);
	const float DrawEnd = Release * 0.5f;
	const float Hold = Release + 0.12f;
	const float End = FMath::Max(GestureDuration, Hold + 0.05f);

	const float Weight = FMath::SmoothStep(0.0f, DrawEnd, T) * (1.0f - FMath::SmoothStep(Hold, End, T));
	const float Snap = 1.0f - FMath::Pow(1.0f - FMath::Clamp((T - DrawEnd) / (Release - DrawEnd), 0.0f, 1.0f), 3.0f);
	const float Recoil = FMath::Sin(PI * FMath::Clamp((T - Release) / 0.14f, 0.0f, 1.0f));
	const float Fingers = FMath::SmoothStep(0.0f, Release * 0.7f, T) * (1.0f - FMath::SmoothStep(Hold + 0.05f, End, T));

	const FCompactPoseBoneIndex Chest = Find(TEXT("spine_05")) != INDEX_NONE ? Find(TEXT("spine_05")) : Find(TEXT("spine_03"));
	const float ChestX = Chest != INDEX_NONE ? Pose.GetComponentSpaceTransform(Chest).GetLocation().X : 0.0f;
	const float Outward = FMath::Sign(Pose.GetComponentSpaceTransform(Upper).GetLocation().X - ChestX);

	// The chest turns the shoulder back while drawing and drives it forward with the shot, leaning a little into it
	// (the body faces +Y in the space of the mesh: turning around +Z by the sign of the side brings that shoulder forward)
	const float Twist = Outward * FMath::Lerp(-10.0f, 18.0f, Snap) * Weight;
	for (const TCHAR* Spine : { TEXT("spine_02"), TEXT("spine_03") })
	{
		TurnPoseBone(Pose, Find(Spine), FQuat(FVector::ZAxisVector, FMath::DegreesToRadians(Twist * 0.5f)));
	}
	TurnPoseBone(Pose, Find(TEXT("spine_03")), FQuat(FVector::XAxisVector, FMath::DegreesToRadians(-5.0f * Snap * Weight)));

	// Drawn: the elbow out to the side, the hand cocked up beside the shoulder. Shot: the whole arm straight towards the aim,
	// a little inwards and down so the spell flies low; the wrist flicks up after the shot
	const FVector Shot = FVector(-Outward * 0.05f, 1.0f, -0.18f).GetSafeNormal();
	const FVector ShotHand = FVector(-Outward * 0.05f, 1.0f, -0.18f + 0.5f * Recoil).GetSafeNormal();
	AimPoseBone(Pose, Upper, Lower, FMath::Lerp(FVector(Outward * 0.75f, 0.15f, -0.3f).GetSafeNormal(), Shot, Snap), Weight);
	AimPoseBone(Pose, Lower, Hand, FMath::Lerp(FVector(-Outward * 0.25f, 0.45f, 0.85f).GetSafeNormal(), Shot, Snap), Weight);
	AimPoseBone(Pose, Hand, Middle, FMath::Lerp(FVector(-Outward * 0.15f, 0.75f, 0.65f).GetSafeNormal(), ShotHand, Snap), Weight);

	if (Fingers <= 0.0f)
	{
		return;
	}

	const auto Finger = [&Find](const TCHAR* Name, int32 Joint) { return Find(FString::Printf(TEXT("%s_0%d_r"), Name, Joint)); };

	// The bend of the middle finger stands in for a finger that lies straight in the animation
	const FVector FallbackAxis = FingerCurlAxis(Pose, Finger(TEXT("middle"), 1), Finger(TEXT("middle"), 2), Finger(TEXT("middle"), 3));

	// Index and middle finger stretched along the hand
	const FVector Pointing = PoseBoneDirection(Pose, Hand, Middle);
	for (const TCHAR* Name : { TEXT("index"), TEXT("middle") })
	{
		AimPoseBone(Pose, Finger(Name, 1), Finger(Name, 2), Pointing, Fingers);
		AimPoseBone(Pose, Finger(Name, 2), Finger(Name, 3), Pointing, Fingers);
	}

	// Ring and little finger curled into the palm, every joint further around the way the finger already bends
	for (const TCHAR* Name : { TEXT("ring"), TEXT("pinky") })
	{
		FVector Axis = FingerCurlAxis(Pose, Finger(Name, 1), Finger(Name, 2), Finger(Name, 3));
		Axis = Axis.IsZero() ? FallbackAxis : Axis;
		if (Axis.IsZero())
		{
			continue;
		}
		const float Degrees[3] = { 75.0f, 95.0f, 60.0f };
		for (int32 Joint = 1; Joint <= 3; ++Joint)
		{
			TurnPoseBone(Pose, Finger(Name, Joint), FQuat(Axis, FMath::DegreesToRadians(Degrees[Joint - 1] * Fingers)));
		}
	}

	// The thumb folds over the curled fingers
	const FCompactPoseBoneIndex Thumb1 = Finger(TEXT("thumb"), 1);
	const FCompactPoseBoneIndex Thumb2 = Finger(TEXT("thumb"), 2);
	const FCompactPoseBoneIndex Thumb3 = Finger(TEXT("thumb"), 3);
	const FCompactPoseBoneIndex Ring2 = Finger(TEXT("ring"), 2);
	if (Thumb1 != INDEX_NONE && Ring2 != INDEX_NONE)
	{
		const FVector ToRing = (Pose.GetComponentSpaceTransform(Ring2).GetLocation() - Pose.GetComponentSpaceTransform(Thumb1).GetLocation()).GetSafeNormal();
		SwingPoseBone(Pose, Thumb1, Thumb2, ToRing, 30.0f * Fingers);
		const FVector ThumbAxis = FingerCurlAxis(Pose, Thumb1, Thumb2, Thumb3);
		if (!ThumbAxis.IsZero())
		{
			TurnPoseBone(Pose, Thumb2, FQuat(ThumbAxis, FMath::DegreesToRadians(35.0f * Fingers)));
			TurnPoseBone(Pose, Thumb3, FQuat(ThumbAxis, FMath::DegreesToRadians(30.0f * Fingers)));
		}
	}
}

void FVaelAnimProxy::ApplyBlowOut(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones) const
{
	const auto Find = [&Bones](const FString& Name) { return FindPoseBone(Bones, *Name); };

	// Timing: breathing in until the release, then a quick blow that is held while the cloud streams out, then back into the walk
	const float T = GestureSeconds;
	const float Release = FMath::Max(GestureRelease, 0.05f);
	const float Hold = Release + 0.55f;
	const float End = FMath::Max(GestureDuration, Hold + 0.1f);

	const float Weight = FMath::SmoothStep(0.0f, 0.15f, T) * (1.0f - FMath::SmoothStep(Hold, End, T));
	const float In = FMath::SmoothStep(0.0f, Release, T);
	const float Blow = 1.0f - FMath::Square(1.0f - FMath::Clamp((T - Release) / 0.25f, 0.0f, 1.0f));
	const float Breath = In * (1.0f - Blow);

	// Chest and head go back while breathing in and come forward with the blow (turning around +X tips back)
	const float ChestTip = Weight * (7.0f * Breath - 9.0f * Blow);
	for (const TCHAR* Spine : { TEXT("spine_03"), TEXT("spine_05") })
	{
		TurnPoseBone(Pose, Find(Spine), FQuat(FVector::XAxisVector, FMath::DegreesToRadians(ChestTip * 0.5f)));
	}
	TurnPoseBone(Pose, Find(TEXT("neck_01")), FQuat(FVector::XAxisVector, FMath::DegreesToRadians(Weight * (8.0f * Breath - 12.0f * Blow))));

	const FCompactPoseBoneIndex Chest = Find(TEXT("spine_05")) != INDEX_NONE ? Find(TEXT("spine_05")) : Find(TEXT("spine_03"));
	const float ArmWeight = Weight * FMath::Min(1.0f, In * 1.5f + Blow);
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const TCHAR* Suffix = Side == 0 ? TEXT("l") : TEXT("r");
		const FCompactPoseBoneIndex Clavicle = Find(FString::Printf(TEXT("clavicle_%s"), Suffix));
		const FCompactPoseBoneIndex Upper = Find(FString::Printf(TEXT("upperarm_%s"), Suffix));
		const FCompactPoseBoneIndex Lower = Find(FString::Printf(TEXT("lowerarm_%s"), Suffix));
		const FCompactPoseBoneIndex Hand = Find(FString::Printf(TEXT("hand_%s"), Suffix));
		const FCompactPoseBoneIndex Middle = Find(FString::Printf(TEXT("middle_01_%s"), Suffix));
		if (Upper == INDEX_NONE || Lower == INDEX_NONE || Hand == INDEX_NONE || Middle == INDEX_NONE)
		{
			continue;
		}

		// The shoulders rise with the breath
		SwingPoseBone(Pose, Clavicle, Upper, FVector::ZAxisVector, 6.0f * Breath * Weight);

		// Breathing in: elbows low by the body, the hands cupped together before the chin. Blowing: forearms and open hands
		// go forward and a little apart, sending the cloud off
		const float ChestX = Chest != INDEX_NONE ? Pose.GetComponentSpaceTransform(Chest).GetLocation().X : 0.0f;
		const float Outward = FMath::Sign(Pose.GetComponentSpaceTransform(Upper).GetLocation().X - ChestX);
		AimPoseBone(Pose, Upper, Lower, FMath::Lerp(FVector(Outward * 0.3f, 0.7f, -0.6f), FVector(Outward * 0.3f, 0.6f, -0.55f), Blow), ArmWeight);
		AimPoseBone(Pose, Lower, Hand, FMath::Lerp(FVector(-Outward * 0.45f, 0.2f, 0.9f), FVector(Outward * 0.05f, 0.95f, 0.15f), Blow), ArmWeight);
		AimPoseBone(Pose, Hand, Middle, FMath::Lerp(FVector(-Outward * 0.3f, 0.35f, 0.9f), FVector(Outward * 0.35f, 0.9f, 0.2f), Blow), ArmWeight);

		// Open hands, not fists: the fingers stretch most of the way along the hand
		const FVector Along = PoseBoneDirection(Pose, Hand, Middle);
		for (const TCHAR* Name : { TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky") })
		{
			const FCompactPoseBoneIndex First = Find(FString::Printf(TEXT("%s_01_%s"), Name, Suffix));
			const FCompactPoseBoneIndex Second = Find(FString::Printf(TEXT("%s_02_%s"), Name, Suffix));
			const FCompactPoseBoneIndex Third = Find(FString::Printf(TEXT("%s_03_%s"), Name, Suffix));
			AimPoseBone(Pose, First, Second, Along, ArmWeight * 0.8f);
			AimPoseBone(Pose, Second, Third, Along, ArmWeight * 0.8f);
		}
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
	const bool bGesture = Gesture != EVaelCastGesture::None;
	if (InRootNode != GetRootNode() || (!bFlying && !bShielding && !bGesture && ArmStance[0] <= KINDA_SMALL_NUMBER && ArmStance[1] <= KINDA_SMALL_NUMBER && LeanDegrees <= KINDA_SMALL_NUMBER))
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
		// A mortar arm (Funkenmohn, Boris 2026-10-26): stretched out and up towards the sky; the longer it charges the further it
		// draws back to the shoulder, the elbow out to the side and the forearm folded up, the bud still pointing along the arc
		if (bMortar[Side])
		{
			const float Pitch = FMath::DegreesToRadians(MortarPitch);
			const FVector Up = FVector(Outward * 0.12f, FMath::Cos(Pitch), FMath::Sin(Pitch)).GetSafeNormal();
			const FVector UpperDrawn = FVector(Outward * 0.8f, 0.35f, 0.1f).GetSafeNormal();
			const FVector LowerDrawn = FVector(-Outward * 0.35f, 0.25f, 0.9f).GetSafeNormal();
			Aim(Upper, Lower, FMath::Lerp(Up, UpperDrawn, Draw[Side]).GetSafeNormal(), Weight);
			Aim(Lower, Hand, FMath::Lerp(Up, LowerDrawn, Draw[Side]).GetSafeNormal(), Weight);
			Aim(Hand, Middle, Up, Weight);
			continue;
		}

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

	if (Gesture == EVaelCastGesture::TwoFingerPoint)
	{
		ApplyTwoFingerPoint(Pose, Bones);
	}
	else if (Gesture == EVaelCastGesture::BlowOut)
	{
		ApplyBlowOut(Pose, Bones);
	}

	FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(Pose), Output.Pose);
	return true;
}
