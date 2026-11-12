// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelLifeLayer.h"
#include "AnimationRuntime.h"
#include "Components/SkeletalMeshComponent.h"
#include "Creatures/VaelSequenceAnimInstance.h"
#include "Engine/SkeletalMesh.h"

namespace
{
	/** A soft uneven wave -1..1 */
	float LifeDrift(float Time, float Seed)
	{
		return 0.6f * FMath::Sin(Time * 0.9f + Seed) + 0.3f * FMath::Sin(Time * 2.3f + Seed * 2.7f) + 0.1f * FMath::Sin(Time * 5.1f + Seed * 4.3f);
	}

	/** 0..1..0 over a span of time that started at Start */
	float LifeBump(float Time, float Start, float Length)
	{
		const float Share = (Time - Start) / FMath::Max(Length, 0.01f);
		return Share <= 0.0f || Share >= 1.0f ? 0.0f : FMath::Square(FMath::Sin(Share * UE_PI));
	}
}

void FVaelLifeLayer::Init(const USkeletalMeshComponent* Mesh, const FVaelLifeSetup& InSetup)
{
	Setup = InSetup;
	bReady = false;
	const USkeletalMesh* Asset = Mesh != nullptr ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (Asset == nullptr)
	{
		return;
	}

	const FReferenceSkeleton& Skeleton = Asset->GetRefSkeleton();
	const int32 PelvisIndex = Skeleton.FindBoneIndex(Setup.Pelvis);
	const int32 HeadIndex = Skeleton.FindBoneIndex(Setup.Head);
	if (PelvisIndex == INDEX_NONE || HeadIndex == INDEX_NONE)
	{
		return;
	}

	// Forward runs from the pelvis to the head as the model stands; everything else follows from it
	HeadPlace = FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, HeadIndex).GetLocation();
	Forward = (HeadPlace - FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, PelvisIndex).GetLocation()).GetSafeNormal2D();
	if (Forward.IsNearlyZero())
	{
		Forward = FVector::ForwardVector;
	}
	Right = FVector::CrossProduct(FVector::UpVector, Forward);
	YawSign = FVector::DotProduct(FQuat(FVector::UpVector, 0.1f).RotateVector(Forward), Right) > 0.0f ? 1.0f : -1.0f;
	NodSign = FQuat(Right, 0.1f).RotateVector(Forward).Z < 0.0f ? 1.0f : -1.0f;

	Seed = FMath::FRandRange(0.0f, 100.0f);
	EarNow.Init(0.0f, Setup.Ears.Num());
	EarWanted.Init(0.0f, Setup.Ears.Num());
	EarNext.Init(0.0f, Setup.Ears.Num());
	EarFlick.Init(-10.0f, Setup.Ears.Num());
	bReady = true;
}

FQuat FVaelLifeLayer::Yaw(float Degrees) const
{
	return FQuat(FVector::UpVector, FMath::DegreesToRadians(Degrees * YawSign));
}

FQuat FVaelLifeLayer::Nod(float Degrees) const
{
	return FQuat(Right, FMath::DegreesToRadians(Degrees * NodSign));
}

FQuat FVaelLifeLayer::Roll(float Degrees) const
{
	return FQuat(Forward, FMath::DegreesToRadians(Degrees));
}

void FVaelLifeLayer::Clear(UVaelSequenceAnimInstance* Animator) const
{
	if (Animator != nullptr)
	{
		Animator->SetBoneTurns(TArray<FName>(), TArray<FQuat>());
	}
}

void FVaelLifeLayer::Update(float DeltaSeconds, float Time, const USkeletalMeshComponent* Mesh, UVaelSequenceAnimInstance* Animator, const FVector* LookAt,
	float Alert, float Stride, float Effort)
{
	if (!bReady || Animator == nullptr || Mesh == nullptr)
	{
		return;
	}

	const float Own = Time + Seed;
	const bool bStanding = Stride < 0.0f;
	TArray<FName> Bones;
	TArray<FQuat> Turns;
	TArray<FVector> Scales;
	const auto Add = [&](FName Bone, const FQuat& Turn, const FVector& Scale = FVector::OneVector)
	{
		if (!Bone.IsNone())
		{
			Bones.Add(Bone);
			Turns.Add(Turn);
			Scales.Add(Scale);
		}
	};

	// --- Head: watches its prey, never quite still, and cocks its head now and then
	float WantedYaw = 0.0f;
	float WantedPitch = 0.0f;
	if (LookAt != nullptr)
	{
		const FVector To = Mesh->GetComponentTransform().InverseTransformPosition(*LookAt) - HeadPlace;
		WantedYaw = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(To, Right), FVector::DotProduct(To, Forward))), -Setup.LookYaw, Setup.LookYaw);
		WantedPitch = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(-To.Z, FMath::Max(To.Size2D(), 1.0f))), -Setup.LookPitch, Setup.LookPitch);
	}
	else
	{
		// Nothing to watch: the head wanders, slowly, with long rests
		WantedYaw = Setup.LookYaw * 0.45f * LifeDrift(Own * 0.35f, 1.0f);
		WantedPitch = Setup.LookPitch * 0.3f * LifeDrift(Own * 0.3f, 2.0f);
	}
	WantedYaw += 2.5f * LifeDrift(Own * 1.3f, 3.0f);
	LookYawNow = FMath::FInterpTo(LookYawNow, WantedYaw, DeltaSeconds, 5.0f);
	LookPitchNow = FMath::FInterpTo(LookPitchNow, WantedPitch, DeltaSeconds, 5.0f);

	if (Time >= NextTilt)
	{
		TiltWanted = (FMath::RandBool() ? 1.0f : -1.0f) * Setup.HeadTilt * FMath::FRandRange(0.5f, 1.0f);
		TiltUntil = Time + FMath::FRandRange(0.6f, 1.6f);
		NextTilt = TiltUntil + FMath::FRandRange(2.5f, 7.0f) * (bStanding ? 1.0f : 2.5f);
	}
	TiltNow = FMath::FInterpTo(TiltNow, Time < TiltUntil ? TiltWanted : 0.0f, DeltaSeconds, 7.0f);

	// Stalking: the neck stoops, the head lifts against it
	StoopNow = FMath::FInterpTo(StoopNow, bStanding ? Setup.Stoop * 0.35f * Alert : Setup.Stoop * Alert, DeltaSeconds, 3.0f);

	// The spine swings with the stride, a wave running forward; the head takes none of it
	SwingNow = FMath::FInterpTo(SwingNow, bStanding ? 0.0f : Setup.StrideSwing, DeltaSeconds, 4.0f);
	float Swung = 0.0f;
	if (SwingNow > 0.05f && Setup.Spine.Num() > 0)
	{
		const float Phase = UE_TWO_PI * FMath::Max(Stride, 0.0f);
		for (int32 Index = 0; Index < Setup.Spine.Num(); ++Index)
		{
			const float Angle = SwingNow * FMath::Sin(Phase - Index * 0.9f) * (Index == 0 ? -0.6f : 0.55f);
			Swung += Angle;
			Add(Setup.Spine[Index], Yaw(Angle));
		}
	}

	const int32 NumNeck = Setup.Neck.Num();
	const float NeckShare = NumNeck > 0 ? 0.4f / NumNeck : 0.0f;
	for (const FName& Bone : Setup.Neck)
	{
		Add(Bone, Yaw((LookYawNow - Swung) * NeckShare) * Nod(LookPitchNow * NeckShare + StoopNow / NumNeck));
	}
	Add(Setup.Head, Yaw((LookYawNow - Swung) * (NumNeck > 0 ? 0.6f : 1.0f)) * Nod(LookPitchNow * (NumNeck > 0 ? 0.6f : 1.0f) - StoopNow * 0.85f) * Roll(TiltNow));

	// --- Ears: each one turns to its own sounds and flicks
	for (int32 Index = 0; Index < Setup.Ears.Num(); ++Index)
	{
		if (Time >= EarNext[Index])
		{
			EarWanted[Index] = Setup.EarSwivel * FMath::FRandRange(-1.0f, 1.0f);
			EarNext[Index] = Time + FMath::FRandRange(0.5f, 2.8f) * (1.3f - 0.6f * Alert);
			if (FMath::FRand() < 0.4f)
			{
				EarFlick[Index] = Time;
			}
		}
		EarNow[Index] = FMath::FInterpTo(EarNow[Index], EarWanted[Index], DeltaSeconds, 12.0f);
		const float Side = Index % 2 == 0 ? 1.0f : -1.0f;
		const float Flick = LifeBump(Time, EarFlick[Index], 0.22f);
		Add(Setup.Ears[Index], Yaw(EarNow[Index]) * Roll(Side * (0.5f * Setup.EarSwivel * Flick - 14.0f * Alert * (Time < SnarlUntil ? 1.0f : 0.0f))));
	}

	// --- Jaw: pants after a run, snarls at its prey now and then
	if (Setup.Snarl > 0.0f && Alert > 0.6f && Time >= NextSnarl)
	{
		SnarlUntil = Time + FMath::FRandRange(0.5f, 0.9f);
		NextSnarl = SnarlUntil + FMath::FRandRange(4.0f, 9.0f);
	}
	const float WantedJaw = Time < SnarlUntil ? Setup.Snarl : Setup.Pant * Effort * (0.55f + 0.45f * FMath::Sin(Own * 21.0f));
	JawNow = FMath::FInterpTo(JawNow, WantedJaw, DeltaSeconds, 14.0f);
	if (JawNow > 0.05f)
	{
		Add(Setup.Jaw, Nod(JawNow));
	}

	// --- Nose: works in short bursts while it stands
	if (Time >= NextSniff)
	{
		SniffUntil = Time + FMath::FRandRange(0.5f, 1.1f);
		NextSniff = SniffUntil + FMath::FRandRange(2.0f, 6.0f);
	}
	if (Time < SniffUntil)
	{
		Add(Setup.Nose, Nod(-3.5f * FMath::Abs(FMath::Sin(Own * 26.0f))), FVector(1.0f + 0.07f * FMath::Abs(FMath::Sin(Own * 26.0f))));
	}

	// --- Crest: breathes in a ripple from the neck back, stands up when it is roused
	BristleNow = FMath::FInterpTo(BristleNow, FMath::Max(Alert, Time < SnarlUntil ? 1.0f : 0.0f), DeltaSeconds, 3.0f);
	const float BreathPace = 2.2f + 3.0f * FMath::Max(Effort, Alert * 0.5f);
	for (int32 Index = 0; Index < Setup.Crest.Num(); ++Index)
	{
		const float Breath = 0.5f + 0.5f * FMath::Sin(Own * BreathPace - Index * 0.45f);
		const float Size = 1.0f + 0.06f * Breath + Setup.CrestBristle * BristleNow * (1.0f + 0.08f * FMath::Sin(Own * 17.0f + Index * 2.1f));
		Add(Setup.Crest[Index], Roll(2.5f * LifeDrift(Own * 1.6f, 10.0f + Index * 1.3f) + 4.0f * BristleNow * FMath::Sin(Own * 19.0f + Index)), FVector(Size));
	}

	// --- Petals: each trembles on its own beat, harder when it is roused
	for (int32 Index = 0; Index < Setup.Petals.Num(); ++Index)
	{
		const float Tremble = Setup.PetalTremble * (0.6f * LifeDrift(Own * 1.9f, 30.0f + Index * 2.3f) + (0.3f + 0.7f * Alert) * 0.4f * FMath::Sin(Own * 23.0f + Index * 1.7f));
		Add(Setup.Petals[Index], Nod(Tremble) * Roll(Tremble * 0.6f));
	}

	// --- Tendrils: sway, each in its own time; now and then one lifts and tastes the air
	if (Setup.Tendrils.Num() > 0 && Time >= NextTaste)
	{
		TastingTendril = FMath::RandHelper(Setup.Tendrils.Num());
		TasteStart = Time;
		NextTaste = Time + FMath::FRandRange(3.5f, 8.0f);
	}
	for (int32 Tendril = 0; Tendril < Setup.Tendrils.Num(); ++Tendril)
	{
		const TArray<FName>& Chain = Setup.Tendrils[Tendril];
		const float Taste = Tendril == TastingTendril ? LifeBump(Time, TasteStart, 2.4f) : 0.0f;
		for (int32 Index = 0; Index < Chain.Num(); ++Index)
		{
			const float Along = Chain.Num() > 1 ? static_cast<float>(Index) / (Chain.Num() - 1) : 0.0f;
			const float Sway = Setup.TendrilSway * (0.4f + 0.6f * Along);
			const float Lag = Own - Index * 0.22f;
			Add(Chain[Index], Yaw(Sway * LifeDrift(Lag * 1.1f, 60.0f + Tendril * 3.1f) + 6.0f * Taste * FMath::Sin(Own * 5.0f - Index * 0.7f))
				* Nod(Sway * 0.7f * LifeDrift(Lag * 0.9f, 80.0f + Tendril * 4.7f) - 5.0f * Taste));
		}
	}

	Animator->SetBoneTurns(Bones, Turns, Scales);
}
