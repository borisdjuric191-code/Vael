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
	CastExhaustion = Character->GetCastExhaustion();

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
	GestureExhaust = Instance->CastExhaustion;
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

	/** Degrees the forearm turns so the palm looks up while it scoops water; its sign follows the side of the arm */
	constexpr float WaterFlickPalmTurn = 110.0f;

	/** Lays an arm so its wrist reaches a place, the elbow towards the pole; a place out of reach is reached for as far as the arm goes */
	void ReachPoseArm(FCSPose<FCompactPose>& Pose, FCompactPoseBoneIndex Upper, FCompactPoseBoneIndex Lower, FCompactPoseBoneIndex Hand, const FVector& Target, const FVector& Pole)
	{
		const FVector Shoulder = Pose.GetComponentSpaceTransform(Upper).GetLocation();
		const float UpperLength = FVector::Dist(Shoulder, Pose.GetComponentSpaceTransform(Lower).GetLocation());
		const float LowerLength = FVector::Dist(Pose.GetComponentSpaceTransform(Lower).GetLocation(), Pose.GetComponentSpaceTransform(Hand).GetLocation());
		const float Reach = FMath::Clamp(FVector::Dist(Shoulder, Target), FMath::Abs(UpperLength - LowerLength) + 1.0f, UpperLength + LowerLength - 0.5f);
		const FVector Toward = (Target - Shoulder).GetSafeNormal();

		// Cosine rule: how far along the line to the target the elbow lies, the rest of its way goes to the pole
		const float Along = (FMath::Square(UpperLength) + FMath::Square(Reach) - FMath::Square(LowerLength)) / (2.0f * Reach);
		const float Out = FMath::Sqrt(FMath::Max(FMath::Square(UpperLength) - FMath::Square(Along), 0.0f));
		const FVector Side = (Pole - Toward * FVector::DotProduct(Pole, Toward)).GetSafeNormal();
		const FVector Elbow = Shoulder + Toward * Along + Side * Out;

		AimPoseBone(Pose, Upper, Lower, Elbow - Shoulder, 1.0f);
		AimPoseBone(Pose, Lower, Hand, Target - Pose.GetComponentSpaceTransform(Lower).GetLocation(), 1.0f);
	}
}

void FVaelAnimProxy::ApplyForearmGrip(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones, float Weight) const
{
	const auto Find = [&Bones](const FString& Name) { return FindPoseBone(Bones, *Name); };

	const FCompactPoseBoneIndex Upper = Find(TEXT("upperarm_l"));
	const FCompactPoseBoneIndex Lower = Find(TEXT("lowerarm_l"));
	const FCompactPoseBoneIndex Hand = Find(TEXT("hand_l"));
	const FCompactPoseBoneIndex Middle = Find(TEXT("middle_01_l"));
	const FCompactPoseBoneIndex OtherLower = Find(TEXT("lowerarm_r"));
	const FCompactPoseBoneIndex OtherHand = Find(TEXT("hand_r"));
	if (Upper == INDEX_NONE || Lower == INDEX_NONE || Hand == INDEX_NONE || Middle == INDEX_NONE || OtherLower == INDEX_NONE || OtherHand == INDEX_NONE || Weight <= 0.0f)
	{
		return;
	}

	const FVector Elbow = Pose.GetComponentSpaceTransform(OtherLower).GetLocation();
	const FVector Wrist = Pose.GetComponentSpaceTransform(OtherHand).GetLocation();
	const FVector Forearm = (Wrist - Elbow).GetSafeNormal();
	const float Outward = FMath::Sign(Pose.GetComponentSpaceTransform(Upper).GetLocation().X - Elbow.X);

	// The hand comes from below and its fingers close over the far side of the forearm
	const FVector OverArm = (FVector::CrossProduct(Forearm, FVector::ZAxisVector).GetSafeNormal() * FMath::Sign(FVector::CrossProduct(Forearm, FVector::ZAxisVector).X) * -Outward + FVector::ZAxisVector * 0.45f).GetSafeNormal();
	const FVector Grip = FMath::Lerp(Elbow, Wrist, 0.62f) - OverArm * 8.0f - FVector::ZAxisVector * 3.0f;

	const FVector Rest = Pose.GetComponentSpaceTransform(Hand).GetLocation();
	ReachPoseArm(Pose, Upper, Lower, Hand, FMath::Lerp(Rest, Grip, Weight), FVector(Outward * 0.7f, -0.25f, -0.7f));
	AimPoseBone(Pose, Hand, Middle, OverArm, Weight);

	// The fingers close around the arm, every joint further around the way the finger already bends
	const auto Finger = [&Find](const TCHAR* Name, int32 Joint) { return Find(FString::Printf(TEXT("%s_0%d_l"), Name, Joint)); };
	const FVector FallbackAxis = FingerCurlAxis(Pose, Finger(TEXT("middle"), 1), Finger(TEXT("middle"), 2), Finger(TEXT("middle"), 3));
	for (const TCHAR* Name : { TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky") })
	{
		FVector Axis = FingerCurlAxis(Pose, Finger(Name, 1), Finger(Name, 2), Finger(Name, 3));
		Axis = Axis.IsZero() ? FallbackAxis : Axis;
		if (Axis.IsZero())
		{
			continue;
		}
		const float Degrees[3] = { 38.0f, 48.0f, 32.0f };
		for (int32 Joint = 1; Joint <= 3; ++Joint)
		{
			TurnPoseBone(Pose, Finger(Name, Joint), FQuat(Axis, FMath::DegreesToRadians(Degrees[Joint - 1] * Weight)));
		}
	}
}

void FVaelAnimProxy::ApplyWaterFlick(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones) const
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

	// Timing: all of the scoop lies before the release and is over in a blink; everything after it only swings out and never stops hard
	const float T = GestureSeconds;
	const float Release = FMath::Max(GestureRelease, 0.02f);
	const float End = FMath::Max(GestureDuration, Release + 0.1f);
	const float Weight = FMath::SmoothStep(0.0f, Release * 0.45f, T) * (1.0f - FMath::SmoothStep(Release + 0.08f, End, T));
	const float Scoop = FMath::SmoothStep(0.0f, Release, T);
	const float Flick = FMath::Sin(UE_HALF_PI * FMath::Clamp((T - Release * 0.75f) / 0.12f, 0.0f, 1.0f));

	const FCompactPoseBoneIndex Chest = Find(TEXT("spine_05")) != INDEX_NONE ? Find(TEXT("spine_05")) : Find(TEXT("spine_03"));
	const float ChestX = Chest != INDEX_NONE ? Pose.GetComponentSpaceTransform(Chest).GetLocation().X : 0.0f;
	const float Outward = FMath::Sign(Pose.GetComponentSpaceTransform(Upper).GetLocation().X - ChestX);

	// The chest turns softly with the arc
	const float Twist = Outward * FMath::Lerp(-9.0f, 11.0f, Scoop) * Weight;
	for (const TCHAR* Spine : { TEXT("spine_02"), TEXT("spine_03") })
	{
		TurnPoseBone(Pose, Find(Spine), FQuat(FVector::ZAxisVector, FMath::DegreesToRadians(Twist * 0.5f)));
	}

	// From low beside the hip forward and up, the arm stays a little bent so the way is a bow, not a line
	AimPoseBone(Pose, Upper, Lower, FMath::Lerp(FVector(Outward * 0.32f, 0.1f, -0.95f).GetSafeNormal(), FVector(Outward * 0.1f, 0.92f, -0.1f).GetSafeNormal(), Scoop), Weight);
	AimPoseBone(Pose, Lower, Hand, FMath::Lerp(FVector(Outward * 0.08f, 0.8f, -0.6f).GetSafeNormal(), FVector(-Outward * 0.05f, 0.95f, 0.28f).GetSafeNormal(), Scoop), Weight);

	// The palm looks up while it carries the water; the flick turns it over and the fingers follow the wrist down
	const FVector Forearm = PoseBoneDirection(Pose, Lower, Hand);
	TurnPoseBone(Pose, Lower, FQuat(Forearm, FMath::DegreesToRadians(Outward * WaterFlickPalmTurn * (1.0f - Flick) * Weight)));
	const FVector Carried = FVector(-Outward * 0.05f, 0.75f, 0.65f).GetSafeNormal();
	const FVector Flung = FVector(-Outward * 0.05f, 0.8f, -0.6f).GetSafeNormal();
	AimPoseBone(Pose, Hand, Middle, FMath::Lerp(Carried, Flung, Flick), Weight);

	// The fingers lie open and soft, and stretch with the flick
	const FVector Along = PoseBoneDirection(Pose, Hand, Middle);
	for (const TCHAR* Name : { TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky") })
	{
		const FCompactPoseBoneIndex First = Find(FString::Printf(TEXT("%s_01_r"), Name));
		const FCompactPoseBoneIndex Second = Find(FString::Printf(TEXT("%s_02_r"), Name));
		const FCompactPoseBoneIndex Third = Find(FString::Printf(TEXT("%s_03_r"), Name));
		const float Open = Weight * FMath::Lerp(0.5f, 0.95f, Flick);
		AimPoseBone(Pose, First, Second, Along, Open);
		AimPoseBone(Pose, Second, Third, Along, Open);
	}
}

void FVaelAnimProxy::OpenHand(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones, const TCHAR* Suffix, float Weight) const
{
	const auto Find = [&Bones](const FString& Name) { return FindPoseBone(Bones, *Name); };
	const FVector Along = PoseBoneDirection(Pose, Find(FString::Printf(TEXT("hand_%s"), Suffix)), Find(FString::Printf(TEXT("middle_01_%s"), Suffix)));
	if (Along.IsZero() || Weight <= 0.0f)
	{
		return;
	}
	for (const TCHAR* Name : { TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky") })
	{
		const FCompactPoseBoneIndex First = Find(FString::Printf(TEXT("%s_01_%s"), Name, Suffix));
		const FCompactPoseBoneIndex Second = Find(FString::Printf(TEXT("%s_02_%s"), Name, Suffix));
		const FCompactPoseBoneIndex Third = Find(FString::Printf(TEXT("%s_03_%s"), Name, Suffix));
		AimPoseBone(Pose, First, Second, Along, Weight);
		AimPoseBone(Pose, Second, Third, Along, Weight);
	}
}

void FVaelAnimProxy::ApplyWaveSweep(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones) const
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

	// Timing: the hand reaches across to the far hip, skims low round the front in a wide arc, tips palm forward just before the
	// release and the wrist snaps; a swollen wave leaves the arm hanging as long as the mage is spent (the pose then holds its end)
	const float T = GestureSeconds;
	const float Release = FMath::Max(GestureRelease, 0.05f);
	const float End = FMath::Max(GestureDuration, Release + 0.2f);
	const bool bSpent = GestureExhaust > 0.0f;
	const float Shown = bSpent ? FMath::Min(T, Release + 0.12f) : T;
	const float Weight = FMath::SmoothStep(0.0f, Release * 0.35f, T) * (bSpent ? 1.0f - FMath::SmoothStep(0.7f, 1.0f, GestureExhaust) : 1.0f - FMath::SmoothStep(Release + 0.15f, End, T));
	const float Sweep = FMath::SmoothStep(Release * 0.3f, Release, Shown);
	const float Tip = FMath::Sin(UE_HALF_PI * FMath::Clamp((Shown - Release * 0.8f) / (Release * 0.2f + 0.04f), 0.0f, 1.0f));
	const float Snap = FMath::Clamp((Shown - Release) / 0.12f, 0.0f, 1.0f);
	const float Spent = FMath::SmoothStep(0.0f, 0.3f, GestureExhaust);

	const FCompactPoseBoneIndex Chest = Find(TEXT("spine_05")) != INDEX_NONE ? Find(TEXT("spine_05")) : Find(TEXT("spine_03"));
	const float ChestX = Chest != INDEX_NONE ? Pose.GetComponentSpaceTransform(Chest).GetLocation().X : 0.0f;
	const float Outward = FMath::Sign(Pose.GetComponentSpaceTransform(Upper).GetLocation().X - ChestX);

	// The chest winds towards the far side and turns out with the sweep, bent a little over the water (turning around +X tips back)
	const float Twist = Outward * FMath::Lerp(18.0f, -12.0f, Sweep) * Weight * (1.0f - Spent);
	const float Bend = Weight * FMath::Lerp(-10.0f + 4.0f * Sweep, -24.0f, Spent);
	for (const TCHAR* Spine : { TEXT("spine_02"), TEXT("spine_03") })
	{
		TurnPoseBone(Pose, Find(Spine), FQuat(FVector::ZAxisVector, FMath::DegreesToRadians(Twist * 0.5f)));
		TurnPoseBone(Pose, Find(Spine), FQuat(FVector::XAxisVector, FMath::DegreesToRadians(Bend * 0.5f)));
	}

	// The arc round the front, low over the ground: from across the body (far side) out past the own side, rising a little
	const float Angle = FMath::DegreesToRadians(FMath::Lerp(-70.0f, 40.0f, Sweep));
	const FVector Arc = FVector(Outward * FMath::Sin(Angle), FMath::Cos(Angle), FMath::Lerp(-0.75f, -0.3f, Sweep)).GetSafeNormal();
	const FVector Hanging = FVector(Outward * 0.1f, 0.55f, -0.85f).GetSafeNormal();
	AimPoseBone(Pose, Upper, Lower, FMath::Lerp(Arc + FVector(0.0f, 0.0f, -0.15f), Hanging, Spent).GetSafeNormal(), Weight);
	AimPoseBone(Pose, Lower, Hand, FMath::Lerp(Arc + FVector(0.0f, 0.0f, 0.1f * Tip), Hanging, Spent).GetSafeNormal(), Weight);

	// The palm skims face down, tips forward with the fingers up at the end of the arc, and the wrist snaps them down
	const FVector Forearm = PoseBoneDirection(Pose, Lower, Hand);
	TurnPoseBone(Pose, Lower, FQuat(Forearm, FMath::DegreesToRadians(-Outward * 60.0f * (1.0f - Tip) * Weight * (1.0f - Spent))));
	const FVector Skimming = Arc + FVector(0.0f, 0.0f, -0.15f);
	const FVector Raised = FVector(Outward * 0.05f, 0.45f, 0.9f);
	const FVector Snapped = FVector(Outward * 0.05f, 0.9f, -0.4f);
	const FVector HandAim = FMath::Lerp(FMath::Lerp(FMath::Lerp(Skimming, Raised, Tip), Snapped, Snap), Hanging, Spent);
	AimPoseBone(Pose, Hand, Middle, HandAim.GetSafeNormal(), Weight);

	OpenHand(Pose, Bones, TEXT("r"), Weight * FMath::Lerp(0.85f, 0.4f, Spent));
}

void FVaelAnimProxy::ApplyWaveHeave(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones) const
{
	const auto Find = [&Bones](const FString& Name) { return FindPoseBone(Bones, *Name); };

	// Timing: both hands scoop low at the sides, heave up over the head, throw forward and down with the release, the wrists
	// snap; then the mage hangs forward spent, the arms down, and straightens up at the end of the pause
	const float T = GestureSeconds;
	const float Release = FMath::Max(GestureRelease, 0.05f);
	const float End = FMath::Max(GestureDuration, Release + 0.3f);
	const bool bSpent = GestureExhaust > 0.0f;
	const float Shown = bSpent ? FMath::Min(T, Release + 0.15f) : T;
	const float Weight = FMath::SmoothStep(0.0f, 0.15f, T) * (bSpent ? 1.0f - FMath::SmoothStep(0.7f, 1.0f, GestureExhaust) : 1.0f - FMath::SmoothStep(End - 0.3f, End, T));
	const float Scoop = FMath::SmoothStep(0.0f, Release * 0.3f, Shown);
	const float Lift = FMath::SmoothStep(Release * 0.3f, Release * 0.82f, Shown);
	const float Throw = 1.0f - FMath::Pow(1.0f - FMath::Clamp((Shown - Release * 0.82f) / (Release * 0.18f + 0.04f), 0.0f, 1.0f), 3.0f);
	const float Snap = FMath::Clamp((Shown - Release) / 0.12f, 0.0f, 1.0f);
	const float Spent = FMath::SmoothStep(0.0f, 0.3f, GestureExhaust);

	// Bent down to the water, arched back under the weight overhead, thrown forward into the throw, hanging spent (+X tips back)
	const float Arch = FMath::Lerp(-15.0f * Scoop * (1.0f - Lift) + 10.0f * Lift * (1.0f - Throw) - 22.0f * Throw, -30.0f, Spent) * Weight;
	for (const TCHAR* Spine : { TEXT("spine_02"), TEXT("spine_03"), TEXT("spine_05") })
	{
		TurnPoseBone(Pose, Find(Spine), FQuat(FVector::XAxisVector, FMath::DegreesToRadians(Arch / 3.0f)));
	}
	TurnPoseBone(Pose, Find(TEXT("neck_01")), FQuat(FVector::XAxisVector, FMath::DegreesToRadians((8.0f * Lift * (1.0f - Throw) - 18.0f * Spent) * Weight)));

	const FCompactPoseBoneIndex Chest = Find(TEXT("spine_05")) != INDEX_NONE ? Find(TEXT("spine_05")) : Find(TEXT("spine_03"));
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
		const float ChestX = Chest != INDEX_NONE ? Pose.GetComponentSpaceTransform(Chest).GetLocation().X : 0.0f;
		const float Outward = FMath::Sign(Pose.GetComponentSpaceTransform(Upper).GetLocation().X - ChestX);

		// The shoulders rise with the load overhead
		SwingPoseBone(Pose, Clavicle, Upper, FVector::ZAxisVector, 12.0f * Lift * (1.0f - Throw) * Weight);

		// Low beside and a little behind the hips, up through the front to over the head, thrown forward and down, hanging spent
		const FVector Low = FVector(Outward * 0.35f, -0.25f, -0.9f);
		const FVector Front = FVector(Outward * 0.35f, 0.95f, 0.0f);
		const FVector High = FVector(Outward * 0.3f, 0.25f, 0.92f);
		const FVector Thrown = FVector(Outward * 0.18f, 0.9f, -0.38f);
		const FVector Hanging = FVector(Outward * 0.12f, 0.45f, -0.88f);
		const FVector Heaved = Lift < 0.5f ? FMath::Lerp(Low, Front, Lift * 2.0f) : FMath::Lerp(Front, High, Lift * 2.0f - 1.0f);
		const FVector Arm = FMath::Lerp(FMath::Lerp(Heaved.GetSafeNormal(), Thrown.GetSafeNormal(), Throw), Hanging.GetSafeNormal(), Spent);

		// The forearms bend up while carrying, the open palms face up under the water and turn forward with the throw
		AimPoseBone(Pose, Upper, Lower, Arm.GetSafeNormal(), Weight);
		AimPoseBone(Pose, Lower, Hand, (Arm + FVector(0.0f, 0.0f, 0.35f * Lift * (1.0f - Throw) * (1.0f - Spent))).GetSafeNormal(), Weight);
		const FVector Forearm = PoseBoneDirection(Pose, Lower, Hand);
		TurnPoseBone(Pose, Lower, FQuat(Forearm, FMath::DegreesToRadians(Outward * WaterFlickPalmTurn * 0.6f * Scoop * (1.0f - Throw) * Weight)));
		const FVector Carried = Arm + FVector(0.0f, 0.0f, 0.5f);
		const FVector Flung = FVector(Outward * 0.08f, 0.8f, -0.6f);
		AimPoseBone(Pose, Hand, Middle, FMath::Lerp(FMath::Lerp(Carried, Thrown, Throw), Flung, Snap * (1.0f - Spent)).GetSafeNormal(), Weight);

		OpenHand(Pose, Bones, Suffix, Weight * FMath::Lerp(0.85f, 0.4f, Spent));
	}
}

void FVaelAnimProxy::MakeFist(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones, const TCHAR* Suffix, float Weight) const
{
	if (Weight <= 0.0f)
	{
		return;
	}
	const auto Finger = [&Bones, Suffix](const TCHAR* Name, int32 Joint) { return FindPoseBone(Bones, *FString::Printf(TEXT("%s_0%d_%s"), Name, Joint, Suffix)); };

	// Every finger curled tight into the palm, every joint further around the way the finger already bends
	const FVector FallbackAxis = FingerCurlAxis(Pose, Finger(TEXT("middle"), 1), Finger(TEXT("middle"), 2), Finger(TEXT("middle"), 3));
	for (const TCHAR* Name : { TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky") })
	{
		FVector Axis = FingerCurlAxis(Pose, Finger(Name, 1), Finger(Name, 2), Finger(Name, 3));
		Axis = Axis.IsZero() ? FallbackAxis : Axis;
		if (Axis.IsZero())
		{
			continue;
		}
		const float Degrees[3] = { 80.0f, 95.0f, 65.0f };
		for (int32 Joint = 1; Joint <= 3; ++Joint)
		{
			TurnPoseBone(Pose, Finger(Name, Joint), FQuat(Axis, FMath::DegreesToRadians(Degrees[Joint - 1] * Weight)));
		}
	}

	// The thumb closes over the curled fingers
	const FCompactPoseBoneIndex Thumb1 = Finger(TEXT("thumb"), 1);
	const FCompactPoseBoneIndex Thumb2 = Finger(TEXT("thumb"), 2);
	const FCompactPoseBoneIndex Thumb3 = Finger(TEXT("thumb"), 3);
	const FCompactPoseBoneIndex Middle2 = Finger(TEXT("middle"), 2);
	if (Thumb1 != INDEX_NONE && Middle2 != INDEX_NONE)
	{
		const FVector ToMiddle = (Pose.GetComponentSpaceTransform(Middle2).GetLocation() - Pose.GetComponentSpaceTransform(Thumb1).GetLocation()).GetSafeNormal();
		SwingPoseBone(Pose, Thumb1, Thumb2, ToMiddle, 30.0f * Weight);
		const FVector ThumbAxis = FingerCurlAxis(Pose, Thumb1, Thumb2, Thumb3);
		if (!ThumbAxis.IsZero())
		{
			TurnPoseBone(Pose, Thumb2, FQuat(ThumbAxis, FMath::DegreesToRadians(35.0f * Weight)));
			TurnPoseBone(Pose, Thumb3, FQuat(ThumbAxis, FMath::DegreesToRadians(30.0f * Weight)));
		}
	}
}

void FVaelAnimProxy::Crouch(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones, float Depth) const
{
	const FCompactPoseBoneIndex Pelvis = FindPoseBone(Bones, TEXT("pelvis"));
	if (Pelvis == INDEX_NONE || Depth <= 0.0f)
	{
		return;
	}

	// The feet stay where they stand: remember them, lower the hips, then bend each leg back down to its foot, knees forward
	FTransform Feet[2];
	FCompactPoseBoneIndex Thighs[2], Calves[2], FootBones[2];
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const TCHAR* Suffix = Side == 0 ? TEXT("l") : TEXT("r");
		Thighs[Side] = FindPoseBone(Bones, *FString::Printf(TEXT("thigh_%s"), Suffix));
		Calves[Side] = FindPoseBone(Bones, *FString::Printf(TEXT("calf_%s"), Suffix));
		FootBones[Side] = FindPoseBone(Bones, *FString::Printf(TEXT("foot_%s"), Suffix));
		if (FootBones[Side] != INDEX_NONE)
		{
			Feet[Side] = Pose.GetComponentSpaceTransform(FootBones[Side]);
		}
	}

	FTransform Hips = Pose.GetComponentSpaceTransform(Pelvis);
	Hips.AddToTranslation(FVector(0.0f, 0.0f, -Depth));
	TArray<FBoneTransform> Changed = { FBoneTransform(Pelvis, Hips) };
	Pose.LocalBlendCSBoneTransforms(Changed, 1.0f);

	for (int32 Side = 0; Side < 2; ++Side)
	{
		if (Thighs[Side] == INDEX_NONE || Calves[Side] == INDEX_NONE || FootBones[Side] == INDEX_NONE)
		{
			continue;
		}
		const float Outward = FMath::Sign(Pose.GetComponentSpaceTransform(Thighs[Side]).GetLocation().X - Hips.GetLocation().X);
		ReachPoseArm(Pose, Thighs[Side], Calves[Side], FootBones[Side], Feet[Side].GetLocation(), FVector(Outward * 0.25f, 1.0f, 0.0f));
		// The foot keeps lying flat as it was
		const FQuat Now = Pose.GetComponentSpaceTransform(FootBones[Side]).GetRotation();
		TurnPoseBone(Pose, FootBones[Side], Feet[Side].GetRotation() * Now.Inverse());
	}
}

void FVaelAnimProxy::ApplyBackfist(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones) const
{
	const auto Find = [&Bones](const FString& Name) { return FindPoseBone(Bones, *Name); };

	const FCompactPoseBoneIndex Upper = Find(TEXT("upperarm_r"));
	const FCompactPoseBoneIndex Lower = Find(TEXT("lowerarm_r"));
	const FCompactPoseBoneIndex Hand = Find(TEXT("hand_r"));
	if (Upper == INDEX_NONE || Lower == INDEX_NONE || Hand == INDEX_NONE)
	{
		return;
	}

	// Timing: the fist cocks at the far shoulder, lashes out back of the hand first and stops dead with a small kick back
	const float T = GestureSeconds;
	const float Release = FMath::Max(GestureRelease, 0.02f);
	const float Cock = Release * 0.55f;
	const float End = FMath::Max(GestureDuration, Release + 0.15f);
	const float Weight = FMath::SmoothStep(0.0f, Cock, T) * (1.0f - FMath::SmoothStep(Release + 0.12f, End, T));
	const float Strike = 1.0f - FMath::Pow(1.0f - FMath::Clamp((T - Cock) / (Release - Cock), 0.0f, 1.0f), 4.0f);
	const float Recoil = FMath::Sin(UE_PI * FMath::Clamp((T - Release) / 0.1f, 0.0f, 1.0f));

	const FCompactPoseBoneIndex Chest = Find(TEXT("spine_05")) != INDEX_NONE ? Find(TEXT("spine_05")) : Find(TEXT("spine_03"));
	const float ChestX = Chest != INDEX_NONE ? Pose.GetComponentSpaceTransform(Chest).GetLocation().X : 0.0f;
	const float Outward = FMath::Sign(Pose.GetComponentSpaceTransform(Upper).GetLocation().X - ChestX);

	// The shoulder winds forward across the body and the chest whips out with the blow
	const float Twist = Outward * FMath::Lerp(16.0f, -12.0f, Strike) * Weight;
	for (const TCHAR* Spine : { TEXT("spine_02"), TEXT("spine_03") })
	{
		TurnPoseBone(Pose, Find(Spine), FQuat(FVector::ZAxisVector, FMath::DegreesToRadians(Twist * 0.5f)));
	}

	// Cocked: elbow before the chest, the fist folded up by the far shoulder. Struck: the arm straight out to the front of the
	// own side, so the stone leaves along the aim; the recoil kicks it back a hair
	const FVector CockedUpper = FVector(-Outward * 0.45f, 0.6f, -0.1f).GetSafeNormal();
	const FVector CockedLower = FVector(-Outward * 0.35f, -0.1f, 0.93f).GetSafeNormal();
	const FVector Struck = FVector(Outward * 0.38f, 0.92f, -0.08f).GetSafeNormal();
	const FVector Kicked = FMath::Lerp(Struck, CockedLower, 0.1f * Recoil).GetSafeNormal();
	AimPoseBone(Pose, Upper, Lower, FMath::Lerp(CockedUpper, Struck, Strike).GetSafeNormal(), Weight);
	AimPoseBone(Pose, Lower, Hand, FMath::Lerp(CockedLower, Kicked, Strike).GetSafeNormal(), Weight);
	AimPoseBone(Pose, Hand, Find(TEXT("middle_01_r")), FMath::Lerp(CockedLower, Kicked, Strike).GetSafeNormal(), Weight);

	MakeFist(Pose, Bones, TEXT("r"), FMath::SmoothStep(0.0f, Cock * 0.8f, T) * (1.0f - FMath::SmoothStep(Release + 0.15f, End, T)));
}

void FVaelAnimProxy::ApplyWallRise(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones) const
{
	const auto Find = [&Bones](const FString& Name) { return FindPoseBone(Bones, *Name); };

	// Timing: down into a crouch, fists low; with the release they climb heavily, the body rising with them, and hold at the top
	const float T = GestureSeconds;
	const float Release = FMath::Max(GestureRelease, 0.05f);
	const float End = FMath::Max(GestureDuration, Release + 0.4f);
	const float Weight = FMath::SmoothStep(0.0f, 0.12f, T) * (1.0f - FMath::SmoothStep(End - 0.25f, End, T));
	const float Down = FMath::SmoothStep(0.0f, Release * 0.65f, T);
	const float Rise = FMath::SmoothStep(Release * 0.6f, Release + 0.3f, T);

	// The load of the wall shakes the arms while they lift it
	const float Strain = Rise * (1.0f - Rise) * 4.0f;
	const FVector Tremble = FVector(FMath::PerlinNoise1D(T * 31.0f), 0.0f, FMath::PerlinNoise1D(T * 37.0f + 2.0f)) * 0.06f * Strain;

	Crouch(Pose, Bones, 34.0f * Down * (1.0f - 0.7f * Rise) * Weight);
	const float Lean = -18.0f * Down * (1.0f - Rise) * Weight;
	for (const TCHAR* Spine : { TEXT("spine_02"), TEXT("spine_03") })
	{
		TurnPoseBone(Pose, Find(Spine), FQuat(FVector::XAxisVector, FMath::DegreesToRadians(Lean * 0.5f)));
	}

	const FCompactPoseBoneIndex Chest = Find(TEXT("spine_05")) != INDEX_NONE ? Find(TEXT("spine_05")) : Find(TEXT("spine_03"));
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const TCHAR* Suffix = Side == 0 ? TEXT("l") : TEXT("r");
		const FCompactPoseBoneIndex Upper = Find(FString::Printf(TEXT("upperarm_%s"), Suffix));
		const FCompactPoseBoneIndex Lower = Find(FString::Printf(TEXT("lowerarm_%s"), Suffix));
		const FCompactPoseBoneIndex Hand = Find(FString::Printf(TEXT("hand_%s"), Suffix));
		if (Upper == INDEX_NONE || Lower == INDEX_NONE || Hand == INDEX_NONE)
		{
			continue;
		}
		const float ChestX = Chest != INDEX_NONE ? Pose.GetComponentSpaceTransform(Chest).GetLocation().X : 0.0f;
		const float Outward = FMath::Sign(Pose.GetComponentSpaceTransform(Upper).GetLocation().X - ChestX);

		// Low: fists down before the knees, backs forward. Risen: elbows forward at chest height, forearms upright, backs of the fists to the front
		const FVector UpperLow = FVector(Outward * 0.3f, 0.55f, -0.78f).GetSafeNormal();
		const FVector LowerLow = FVector(Outward * 0.08f, 0.6f, -0.8f).GetSafeNormal();
		const FVector UpperHigh = FVector(Outward * 0.28f, 0.85f, 0.15f).GetSafeNormal();
		const FVector LowerHigh = FVector(Outward * 0.05f, 0.25f, 0.97f).GetSafeNormal();
		AimPoseBone(Pose, Upper, Lower, FMath::Lerp(UpperLow, UpperHigh, Rise).GetSafeNormal() + Tremble, Weight);
		AimPoseBone(Pose, Lower, Hand, FMath::Lerp(LowerLow, LowerHigh, Rise).GetSafeNormal() + Tremble, Weight);
		AimPoseBone(Pose, Hand, Find(FString::Printf(TEXT("middle_01_%s"), Suffix)), FMath::Lerp(LowerLow, LowerHigh, Rise).GetSafeNormal(), Weight);
		MakeFist(Pose, Bones, Suffix, Weight);
	}
}

void FVaelAnimProxy::ApplyGroundSlam(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones) const
{
	const auto Find = [&Bones](const FString& Name) { return FindPoseBone(Bones, *Name); };

	// Timing: both fists go up together over the head, come down at once into the ground with the release, and stay down,
	// crouched, while the mage is spent; then up again
	const float T = GestureSeconds;
	const float Release = FMath::Max(GestureRelease, 0.05f);
	const float End = FMath::Max(GestureDuration, Release + 0.2f);
	const bool bSpent = GestureExhaust > 0.0f;
	const float Shown = bSpent ? FMath::Min(T, Release + 0.05f) : T;
	const float Weight = FMath::SmoothStep(0.0f, 0.12f, T) * (bSpent ? 1.0f - FMath::SmoothStep(0.4f, 1.0f, GestureExhaust) : 1.0f - FMath::SmoothStep(Release + 0.1f, End, T));
	const float Raise = FMath::SmoothStep(0.0f, Release * 0.72f, Shown);
	const float Slam = FMath::Pow(FMath::Clamp((Shown - Release * 0.72f) / (Release * 0.28f), 0.0f, 1.0f), 2.0f);

	// Stretched up on the toes of the back, then thrown down into a crouch over the fists (turning around +X tips back)
	Crouch(Pose, Bones, 40.0f * Slam * Weight);
	const float Bend = Weight * (10.0f * Raise * (1.0f - Slam) - 38.0f * Slam);
	for (const TCHAR* Spine : { TEXT("spine_02"), TEXT("spine_03"), TEXT("spine_05") })
	{
		TurnPoseBone(Pose, Find(Spine), FQuat(FVector::XAxisVector, FMath::DegreesToRadians(Bend / 3.0f)));
	}
	TurnPoseBone(Pose, Find(TEXT("neck_01")), FQuat(FVector::XAxisVector, FMath::DegreesToRadians(-10.0f * Slam * Weight)));

	const FCompactPoseBoneIndex Chest = Find(TEXT("spine_05")) != INDEX_NONE ? Find(TEXT("spine_05")) : Find(TEXT("spine_03"));
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const TCHAR* Suffix = Side == 0 ? TEXT("l") : TEXT("r");
		const FCompactPoseBoneIndex Clavicle = Find(FString::Printf(TEXT("clavicle_%s"), Suffix));
		const FCompactPoseBoneIndex Upper = Find(FString::Printf(TEXT("upperarm_%s"), Suffix));
		const FCompactPoseBoneIndex Lower = Find(FString::Printf(TEXT("lowerarm_%s"), Suffix));
		const FCompactPoseBoneIndex Hand = Find(FString::Printf(TEXT("hand_%s"), Suffix));
		if (Upper == INDEX_NONE || Lower == INDEX_NONE || Hand == INDEX_NONE)
		{
			continue;
		}
		const float ChestX = Chest != INDEX_NONE ? Pose.GetComponentSpaceTransform(Chest).GetLocation().X : 0.0f;
		const float Outward = FMath::Sign(Pose.GetComponentSpaceTransform(Upper).GetLocation().X - ChestX);

		SwingPoseBone(Pose, Clavicle, Upper, FVector::ZAxisVector, 14.0f * Raise * (1.0f - Slam) * Weight);

		// Raised: both fists together above the head. Slammed: the arms down before the body, the fists side by side on the ground
		const FVector UpperHigh = FVector(Outward * 0.12f, 0.3f, 0.95f).GetSafeNormal();
		const FVector LowerHigh = FVector(-Outward * 0.4f, 0.1f, 0.9f).GetSafeNormal();
		const FVector UpperDown = FVector(Outward * 0.1f, 0.75f, -0.66f).GetSafeNormal();
		const FVector LowerDown = FVector(-Outward * 0.2f, 0.55f, -0.81f).GetSafeNormal();
		AimPoseBone(Pose, Upper, Lower, FMath::Lerp(UpperHigh, UpperDown, Slam).GetSafeNormal(), Weight * Raise);
		AimPoseBone(Pose, Lower, Hand, FMath::Lerp(LowerHigh, LowerDown, Slam).GetSafeNormal(), Weight * Raise);
		AimPoseBone(Pose, Hand, Find(FString::Printf(TEXT("middle_01_%s"), Suffix)), FMath::Lerp(LowerHigh, LowerDown, Slam).GetSafeNormal(), Weight * Raise);
		MakeFist(Pose, Bones, Suffix, Weight);
	}
}

void FVaelAnimProxy::ApplyPalmBeam(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones) const
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

	// Timing: the arm gathers at the hip, thrusts forward with the release and holds while the beam burns;
	// then the mage hangs forward, spent, and only at the end of the pause straightens up
	const float T = GestureSeconds;
	const float Release = FMath::Max(GestureRelease, 0.05f);
	const float Thrust = 1.0f - FMath::Pow(1.0f - FMath::Clamp((T - Release * 0.6f) / (Release * 0.4f), 0.0f, 1.0f), 3.0f);
	const float Spent = FMath::SmoothStep(0.0f, 0.3f, GestureExhaust);
	const float Weight = FMath::SmoothStep(0.0f, 0.18f, T) * (1.0f - FMath::SmoothStep(0.7f, 1.0f, GestureExhaust));

	const FCompactPoseBoneIndex Chest = Find(TEXT("spine_05")) != INDEX_NONE ? Find(TEXT("spine_05")) : Find(TEXT("spine_03"));
	const float ChestX = Chest != INDEX_NONE ? Pose.GetComponentSpaceTransform(Chest).GetLocation().X : 0.0f;
	const float Outward = FMath::Sign(Pose.GetComponentSpaceTransform(Upper).GetLocation().X - ChestX);

	// The strain shakes the arm while the beam burns
	const float Strain = Thrust * (1.0f - Spent);
	const FVector Tremble = FVector(FMath::PerlinNoise1D(T * 37.0f), 0.0f, FMath::PerlinNoise1D(T * 41.0f + 3.0f)) * 0.05f * Strain;

	// Turned into the thrust and bent into the beam; spent, the back curls forward over the arm (turning around +X tips back)
	const float Tip = Weight * FMath::Lerp(-8.0f * Thrust, -30.0f, Spent);
	for (const TCHAR* Spine : { TEXT("spine_02"), TEXT("spine_03"), TEXT("spine_05") })
	{
		TurnPoseBone(Pose, Find(Spine), FQuat(FVector::XAxisVector, FMath::DegreesToRadians(Tip / 3.0f)));
		TurnPoseBone(Pose, Find(Spine), FQuat(FVector::ZAxisVector, FMath::DegreesToRadians(Outward * Weight * FMath::Lerp(-18.0f, 14.0f, Thrust) * (1.0f - Spent) / 3.0f)));
	}
	TurnPoseBone(Pose, Find(TEXT("neck_01")), FQuat(FVector::XAxisVector, FMath::DegreesToRadians(-20.0f * Spent * Weight)));

	const FVector UpperAim = FMath::Lerp(FMath::Lerp(FVector(Outward * 0.55f, -0.35f, -0.75f), FVector(-Outward * 0.12f, 1.0f, -0.02f), Thrust), FVector(Outward * 0.1f, 0.75f, -0.65f), Spent);
	const FVector LowerAim = FMath::Lerp(FMath::Lerp(FVector(-Outward * 0.1f, 0.9f, 0.1f), FVector(-Outward * 0.12f, 1.0f, -0.02f), Thrust), FVector(-Outward * 0.1f, 0.6f, -0.8f), Spent);
	const FVector HandAim = FMath::Lerp(FMath::Lerp(FVector(-Outward * 0.1f, 0.9f, 0.1f), FVector(-Outward * 0.05f, 0.32f, 0.95f), Thrust), FVector(0.0f, 0.5f, -0.85f), Spent);
	AimPoseBone(Pose, Upper, Lower, UpperAim.GetSafeNormal() + Tremble, Weight);
	AimPoseBone(Pose, Lower, Hand, LowerAim.GetSafeNormal() + Tremble, Weight);
	AimPoseBone(Pose, Hand, Middle, HandAim.GetSafeNormal() + Tremble, Weight);

	// The open hand: fingers stretched along it, loose once it is spent
	const FVector Along = PoseBoneDirection(Pose, Hand, Middle);
	for (const TCHAR* Name : { TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky") })
	{
		const FCompactPoseBoneIndex First = Find(FString::Printf(TEXT("%s_01_r"), Name));
		const FCompactPoseBoneIndex Second = Find(FString::Printf(TEXT("%s_02_r"), Name));
		const FCompactPoseBoneIndex Third = Find(FString::Printf(TEXT("%s_03_r"), Name));
		AimPoseBone(Pose, First, Second, Along, Weight * (1.0f - 0.6f * Spent));
		AimPoseBone(Pose, Second, Third, Along, Weight * (1.0f - 0.6f * Spent));
	}

	ApplyForearmGrip(Pose, Bones, Weight * FMath::SmoothStep(Release * 0.2f, Release * 0.9f, T));
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
	const float DrawEnd = Release * (Gesture == EVaelCastGesture::GripPoint ? 0.72f : 0.5f);
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
	else if (Gesture == EVaelCastGesture::GripPoint)
	{
		// The Funke with more behind it: the other hand steadies the forearm, as long as the arm stays out
		ApplyTwoFingerPoint(Pose, Bones);
		const float Hold = GestureRelease + 0.12f;
		ApplyForearmGrip(Pose, Bones, FMath::SmoothStep(0.0f, GestureRelease * 0.7f, GestureSeconds) * (1.0f - FMath::SmoothStep(Hold, FMath::Max(GestureDuration, Hold + 0.05f), GestureSeconds)));
	}
	else if (Gesture == EVaelCastGesture::PalmBeam)
	{
		ApplyPalmBeam(Pose, Bones);
	}
	else if (Gesture == EVaelCastGesture::WaterFlick)
	{
		ApplyWaterFlick(Pose, Bones);
	}
	else if (Gesture == EVaelCastGesture::BlowOut)
	{
		ApplyBlowOut(Pose, Bones);
	}
	else if (Gesture == EVaelCastGesture::WaveSweep)
	{
		ApplyWaveSweep(Pose, Bones);
	}
	else if (Gesture == EVaelCastGesture::WaveHeave)
	{
		ApplyWaveHeave(Pose, Bones);
	}
	else if (Gesture == EVaelCastGesture::Backfist)
	{
		ApplyBackfist(Pose, Bones);
	}
	else if (Gesture == EVaelCastGesture::WallRise)
	{
		ApplyWallRise(Pose, Bones);
	}
	else if (Gesture == EVaelCastGesture::GroundSlam)
	{
		ApplyGroundSlam(Pose, Bones);
	}

	FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(Pose), Output.Pose);
	return true;
}
