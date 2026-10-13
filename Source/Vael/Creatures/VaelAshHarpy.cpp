// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelAshHarpy.h"
#include "UObject/ConstructorHelpers.h"
#include "Combat/VaelCombatStatics.h"
#include "AnimationRuntime.h"
#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Creatures/VaelCreatureData.h"
#include "DrawDebugHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Magic/VaelGameplayTags.h"
#include "Player/VaelCharacter.h"
#include "UI/VaelNoticeSubsystem.h"
#include "Vael.h"

#define LOCTEXT_NAMESPACE "VaelCreatures"

namespace
{
	/** Height of the body above the capsule during a dive */
	constexpr float DiveBodyHeight = 15.0f;

	/** Extra reach of a dive beyond the touching capsules, in cm */
	constexpr float DiveReach = 21.0f;

	/** How fast the body follows its target height */
	constexpr float BodyHeightFollowSpeed = 6.0f;

	/** Up and down movement of the body while circling, in cm */
	constexpr float BobAmplitude = 25.0f;

	/** How fast the pose of a factory mesh follows what the harpy is doing */
	constexpr float HarpyPoseSpeed = 7.0f;

	/** Folded all the way, the wings lie this many degrees back, and their outer halves this many more */
	constexpr float HarpyFoldAngle = 72.0f;
	constexpr float HarpyFoldOuterAngle = 38.0f;

	/** Seconds the head stays thrown up in a screech */
	constexpr float HarpyScreechPose = 0.9f;
}

AVaelAshHarpy::AVaelAshHarpy()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (BodyMesh.Succeeded())
	{
		GetBody()->SetStaticMesh(BodyMesh.Object);
	}

	// Flies at the height it was spawned at and passes over the heads of the players
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->DefaultLandMovementMode = MOVE_Flying;
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
}

void AVaelAshHarpy::BeginPlay()
{
	Super::BeginPlay();

	GetCharacterMovement()->SetMovementMode(MOVE_Flying);

	const UVaelAshHarpyData* Data = GetData<UVaelAshHarpyData>();
	BodyHeight = Data->FlightHeight;
	CircleAngle = FMath::FRandRange(0.0f, UE_TWO_PI);
	ScreechCooldown = Data->FirstScreechDelay;

	if (GetFactoryMesh() != nullptr)
	{
		SetupBones();
	}
}

void AVaelAshHarpy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const UVaelAshHarpyData* Data = GetData<UVaelAshHarpyData>();

	if (IsDead())
	{
		// A mesh of the creature factory drops out of the sky and lies where it fell
		if (GetFactoryMesh() != nullptr)
		{
			DeadTime += DeltaSeconds;
			BodyHeight = FMath::FInterpTo(BodyHeight, 25.0f - GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), DeltaSeconds, 3.0f + DeadTime * 6.0f);
			PoseFactoryMesh(DeltaSeconds);
		}
		return;
	}

	// The body glides between flight and dive height and bobs while circling
	const bool bLow = State == EVaelHarpyState::Diving;
	const float Bob = bLow ? 0.0f : FMath::Sin(GetWorld()->GetTimeSeconds() * 3.0f + CircleAngle) * BobAmplitude;
	const float TargetHeight = (bLow ? DiveBodyHeight : Data->FlightHeight) + Bob;

	BodyHeight = FMath::FInterpTo(BodyHeight, TargetHeight, DeltaSeconds, BodyHeightFollowSpeed);
	GetBody()->SetRelativeLocation(FVector(0.0f, 0.0f, BodyHeight));

	if (GetFactoryMesh() != nullptr)
	{
		PoseFactoryMesh(DeltaSeconds);
	}
}

void AVaelAshHarpy::SetupBones()
{
	const USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(GetFactoryMesh()->GetSkinnedAsset());
	if (SkeletalMesh == nullptr)
	{
		return;
	}

	const FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetRefSkeleton();
	const auto RestOf = [&RefSkeleton](FName Bone, FTransform& OutRest)
	{
		const int32 Index = RefSkeleton.FindBoneIndex(Bone);
		if (Index == INDEX_NONE)
		{
			return false;
		}
		OutRest = FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, Index);
		return true;
	};

	const auto Find = [&RestOf](FVaelHarpyLimb& Limb, const TCHAR* Upper, const TCHAR* Lower)
	{
		Limb.Upper = Upper;
		Limb.Lower = Lower;
		return RestOf(Limb.Upper, Limb.RestUpper) && RestOf(Limb.Lower, Limb.RestLower);
	};

	bHasBones = RestOf(TEXT("head"), RestHead) && RestOf(TEXT("tail"), RestTail);
	bHasBones &= Find(Wings[0], TEXT("wing_L_upper"), TEXT("wing_L_lower")) && Find(Wings[1], TEXT("wing_R_upper"), TEXT("wing_R_lower"));
	bHasBones &= Find(Legs[0], TEXT("leg_L_upper"), TEXT("leg_L_lower")) && Find(Legs[1], TEXT("leg_R_upper"), TEXT("leg_R_lower"));

	// The mesh stands on its feet: it leans and rolls around its middle
	MeshMiddle = SkeletalMesh->GetBounds().Origin.Z;

	if (!bHasBones)
	{
		UE_LOG(LogVael, Warning, TEXT("'%s': the mesh has no wings and legs to pose, it keeps its rest pose"), *GetNameSafe(this));
	}
}

void AVaelAshHarpy::PoseLimb(const FVaelHarpyLimb& Limb, const FQuat& Turn, const FQuat& Bend)
{
	UPoseableMeshComponent* HarpyMesh = GetFactoryMesh();

	FTransform Upper = Limb.RestUpper;
	Upper.SetRotation(Turn * Limb.RestUpper.GetRotation());
	HarpyMesh->SetBoneTransformByName(Limb.Upper, Upper, EBoneSpaces::ComponentSpace);

	FTransform Lower = Limb.RestLower;
	Lower.SetLocation(Limb.RestUpper.GetLocation() + Turn.RotateVector(Limb.RestLower.GetLocation() - Limb.RestUpper.GetLocation()));
	Lower.SetRotation(Turn * Bend * Limb.RestLower.GetRotation());
	HarpyMesh->SetBoneTransformByName(Limb.Lower, Lower, EBoneSpaces::ComponentSpace);
}

void AVaelAshHarpy::PoseBone(FName Bone, const FTransform& Rest, const FQuat& Turn)
{
	FTransform Posed = Rest;
	Posed.SetRotation(Turn * Rest.GetRotation());
	GetFactoryMesh()->SetBoneTransformByName(Bone, Posed, EBoneSpaces::ComponentSpace);
}

void AVaelAshHarpy::PoseFactoryMesh(float DeltaSeconds)
{
	UPoseableMeshComponent* HarpyMesh = GetFactoryMesh();
	const UVaelAshHarpyData* Data = GetData<UVaelAshHarpyData>();
	const float Time = GetWorld()->GetTimeSeconds();
	const bool bDeadNow = IsDead();

	// Gliding it lies forward in the air and beats slowly; before a dive it rears up with raised, shivering wings and
	// its claws out; in the dive it shoots head first with folded wings; climbing back it beats hard
	float Lean = 48.0f;
	float Fold = 0.0f;
	float Raise = 0.0f;
	float Beat = 16.0f;
	float BeatsPerSecond = 1.5f;
	float LegTrail = 45.0f;
	float Roll = 8.0f;

	switch (State)
	{
	case EVaelHarpyState::Idle:
		break;
	case EVaelHarpyState::Circling:
		Beat = 20.0f; BeatsPerSecond = 1.9f; Roll = 14.0f;
		break;
	case EVaelHarpyState::Warning:
		Lean = -12.0f; Raise = 32.0f; Beat = 9.0f; BeatsPerSecond = 6.0f; LegTrail = -45.0f; Roll = 0.0f;
		break;
	case EVaelHarpyState::Diving:
		Lean = 100.0f; Fold = 0.85f; Raise = 10.0f; Beat = 2.0f; LegTrail = 70.0f; Roll = 0.0f;
		break;
	case EVaelHarpyState::Recovering:
		Lean = 18.0f; Beat = 38.0f; BeatsPerSecond = 3.2f; LegTrail = 15.0f; Roll = 0.0f;
		break;
	}

	// Thrown about by the wind it tumbles; dead it falls with limp wings and lies on its front
	if (!bDeadNow && LastAirHitTime >= 0.0f && Time - LastAirHitTime < Data->AirStunDuration)
	{
		Lean = 20.0f; Beat = 30.0f; BeatsPerSecond = 5.0f; LegTrail = 0.0f;
		Roll = FMath::Sin(Time * 9.0f) * 70.0f;
	}
	if (bDeadNow)
	{
		Lean = 88.0f; Fold = 0.3f; Raise = FMath::Max(25.0f - DeadTime * 60.0f, -8.0f); Beat = 0.0f; LegTrail = 10.0f; Roll = 20.0f;
	}

	LeanShown = FMath::FInterpTo(LeanShown, Lean, DeltaSeconds, HarpyPoseSpeed);
	FoldShown = FMath::FInterpTo(FoldShown, Fold, DeltaSeconds, HarpyPoseSpeed);
	RaiseShown = FMath::FInterpTo(RaiseShown, Raise, DeltaSeconds, HarpyPoseSpeed);
	BeatShown = FMath::FInterpTo(BeatShown, Beat, DeltaSeconds, HarpyPoseSpeed);
	LegTrailShown = FMath::FInterpTo(LegTrailShown, LegTrail, DeltaSeconds, HarpyPoseSpeed);
	RollShown = FMath::FInterpTo(RollShown, Roll, DeltaSeconds, HarpyPoseSpeed);
	FlapPhase += BeatsPerSecond * DeltaSeconds;

	// The whole mesh: at flight height, leaning forward and rolling around its middle
	const FQuat Tilt = FRotator(-LeanShown, 0.0f, RollShown).Quaternion();
	HarpyMesh->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, BodyHeight) - Tilt.RotateVector(FVector(0.0f, 0.0f, MeshMiddle)),
		Tilt * FRotator(0.0f, Data->MeshYaw, 0.0f).Quaternion());

	if (!bHasBones)
	{
		return;
	}

	// The mesh looks along +Y; a turn around the cross product of a limb and a direction swings the limb towards that direction
	const FVector Back(0.0f, -1.0f, 0.0f);
	const float Wave = FMath::Sin(FlapPhase * UE_TWO_PI);
	const float OuterWave = FMath::Sin(FlapPhase * UE_TWO_PI - 0.9f);

	// Wings: the beat goes up and down at the shoulder, the outer half follows a little later; folded they lie back
	for (const FVaelHarpyLimb& Wing : Wings)
	{
		const FVector Along = (Wing.RestLower.GetLocation() - Wing.RestUpper.GetLocation()).GetSafeNormal();
		const FVector BeatAxis = FVector::CrossProduct(Along, FVector::UpVector).GetSafeNormal();
		const FVector FoldAxis = FVector::CrossProduct(Along, Back).GetSafeNormal();

		const FQuat Turn = FQuat(FoldAxis, FMath::DegreesToRadians(FoldShown * HarpyFoldAngle)) * FQuat(BeatAxis, FMath::DegreesToRadians(RaiseShown + Wave * BeatShown));
		const FQuat Bend = FQuat(FoldAxis, FMath::DegreesToRadians(FoldShown * HarpyFoldOuterAngle)) * FQuat(BeatAxis, FMath::DegreesToRadians(OuterWave * BeatShown * 0.8f));
		PoseLimb(Wing, Turn, Bend);
	}

	// Legs: trailing behind in flight, thrown forward with open claws before a dive, dangling a little
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const FVaelHarpyLimb& Leg = Legs[Side];
		const FVector Along = (Leg.RestLower.GetLocation() - Leg.RestUpper.GetLocation()).GetSafeNormal();
		const FVector TrailAxis = FVector::CrossProduct(Along, Back).GetSafeNormal();
		const float Dangle = bDeadNow ? 0.0f : FMath::Sin(Time * 2.3f + Side * 1.7f) * 5.0f;

		PoseLimb(Leg, FQuat(TrailAxis, FMath::DegreesToRadians(LegTrailShown + Dangle)), FQuat(TrailAxis, FMath::DegreesToRadians(-LegTrailShown * 0.4f)));
	}

	// The tail lifts behind a leaning body; the head lifts to look ahead and is thrown up in a screech.
	// Both hang down in the rest pose, the tail to the back and the head to the front.
	const FVector TailAxis = FVector::CrossProduct(FVector(0.0f, -0.7f, -0.7f), FVector::UpVector).GetSafeNormal();
	const FVector HeadAxis = FVector::CrossProduct(FVector(0.0f, 0.85f, -0.5f), FVector::UpVector).GetSafeNormal();
	const float Screeching = !bDeadNow && Time < ScreechEndTime ? 45.0f : 0.0f;

	PoseBone(TEXT("tail"), RestTail, FQuat(TailAxis, FMath::DegreesToRadians(FMath::Clamp(LeanShown, 0.0f, 90.0f) * 0.45f + Wave * 4.0f)));
	PoseBone(TEXT("head"), RestHead, FQuat(HeadAxis, FMath::DegreesToRadians(bDeadNow ? 0.0f : FMath::Clamp(LeanShown, -20.0f, 90.0f) * 0.55f + Screeching)));
}

void AVaelAshHarpy::ApplyKnockback(const FVector& Direction, float Speed)
{
	if (IsDead())
	{
		return;
	}

	const UVaelAshHarpyData* Data = GetData<UVaelAshHarpyData>();
	float FinalSpeed = Speed * Data->KnockbackMultiplier;

	// Wind throws harpies out of their flight; the hit by air has just landed in this frame
	if (LastAirHitTime == GetWorld()->GetTimeSeconds())
	{
		FinalSpeed *= Data->AirKnockbackMultiplier;
		Stun(Data->AirStunDuration);

		if (State == EVaelHarpyState::Warning || State == EVaelHarpyState::Diving)
		{
			EnterState(EVaelHarpyState::Recovering);
		}
	}

	const FVector GroundDirection = Direction.GetSafeNormal2D();
	if (FinalSpeed > 0.0f && !GroundDirection.IsNearlyZero())
	{
		GetCharacterMovement()->Velocity += GroundDirection * FinalSpeed;
	}
}

void AVaelAshHarpy::StartCircling()
{
	EnterState(EVaelHarpyState::Circling);
	DiveCooldown = RandomInRange(GetData<UVaelAshHarpyData>()->FirstDiveDelay);
}

TSubclassOf<UVaelCreatureData> AVaelAshHarpy::GetDefaultDataClass() const
{
	return UVaelAshHarpyData::StaticClass();
}

void AVaelAshHarpy::OnDamageTaken(float Damage, const FGameplayTagContainer& DamageTags)
{
	Super::OnDamageTaken(Damage, DamageTags);

	if (DamageTags.HasTagExact(VaelTags::Element_Air))
	{
		LastAirHitTime = GetWorld()->GetTimeSeconds();
	}
}

float AVaelAshHarpy::GetStatusTextHeight() const
{
	return Super::GetStatusTextHeight() + GetData<UVaelAshHarpyData>()->FlightHeight;
}

void AVaelAshHarpy::TickBehavior(float DeltaSeconds)
{
	const UVaelAshHarpyData* Data = GetData<UVaelAshHarpyData>();

	StateTime += DeltaSeconds;

	switch (State)
	{
	case EVaelHarpyState::Idle:
	{
		CircleAngle += Data->IdleCircleSpeed * DeltaSeconds;
		const FVector CirclePoint = HomeLocation + FVector(FMath::Cos(CircleAngle), FMath::Sin(CircleAngle), 0.0f) * Data->IdleCircleRadius;
		MoveTowards(CirclePoint, FMath::Min(Data->MoveSpeed, FVector::Dist2D(CirclePoint, GetActorLocation()) * 1.5f));

		if (FindTarget(Data->AggroRange) != nullptr)
		{
			StartCircling();
		}
		break;
	}

	case EVaelHarpyState::Circling:
	{
		AActor* Target = FindTarget(Data->LoseInterestRange);
		if (Target == nullptr)
		{
			EnterState(EVaelHarpyState::Idle);
			break;
		}

		CircleAngle += Data->CombatCircleSpeed * DeltaSeconds;
		MoveTowards(Target->GetActorLocation() + FVector(FMath::Cos(CircleAngle), FMath::Sin(CircleAngle), 0.0f) * Data->CombatCircleRadius, Data->MoveSpeed);

		DiveCooldown -= DeltaSeconds;
		if (DiveCooldown <= 0.0f && !IsBlinded())
		{
			DiveTarget = Target;
			DivePoint = Target->GetActorLocation();
			EnterState(EVaelHarpyState::Warning);
			break;
		}

		if (Data->bCanScreech)
		{
			ScreechCooldown -= DeltaSeconds;
			if (ScreechCooldown <= 0.0f && !IsBlinded())
			{
				ScreechCooldown = Data->ScreechInterval;
				Screech();
			}
		}
		break;
	}

	case EVaelHarpyState::Warning:
		// Hangs in the air and follows its target with the eyes
		if (const AActor* Target = DiveTarget.Get())
		{
			DivePoint = FMath::Lerp(DivePoint, Target->GetActorLocation(), FMath::Min(1.0f, DeltaSeconds * 2.0f));
		}

		FaceTowards(DivePoint);

#if ENABLE_DRAW_DEBUG
		DrawDebugLine(GetWorld(), GetActorLocation(), FVector(DivePoint.X, DivePoint.Y, GetActorLocation().Z), FColor(200, 190, 170), false, -1.0f, 0, 3.0f);
#endif

		if (StateTime >= Data->DiveWarningDuration)
		{
			DiveDirection = (DivePoint - GetActorLocation()).GetSafeNormal2D();
			if (DiveDirection.IsNearlyZero())
			{
				DiveDirection = GetActorForwardVector();
			}

			DiveHits.Reset();
			bDiveWitnessed = false;
			EnterState(EVaelHarpyState::Diving);

			// Full speed from the first moment
			GetCharacterMovement()->Velocity = DiveDirection * Data->DiveSpeed;
		}
		break;

	case EVaelHarpyState::Diving:
	{
		MoveInDirection(DiveDirection, Data->DiveSpeed);

		const float HarpyRadius = GetCapsuleComponent()->GetScaledCapsuleRadius();
		ForEachActivePlayer([this, Data, HarpyRadius](AVaelCharacter* Player)
		{
			const float Distance = GetDistanceTo2D(Player);

			bDiveWitnessed |= Distance < Data->WitnessDistance;

			if (!DiveHits.Contains(Player) && Distance < HarpyRadius + Player->GetCapsuleComponent()->GetScaledCapsuleRadius() + DiveReach)
			{
				DiveHits.Add(Player);
				HitPlayer(Player, Data->DiveDamage, EVaelElement::Air);
			}
		});

		if (StateTime >= Data->DiveDuration)
		{
			if (bDiveWitnessed)
			{
				TeachFormula(Data->FormulaOnWitnessedDive, LOCTEXT("FormulaFromDive", "Du hast den Aschewirbel ihres Sturzflugs aus n\u00E4chster N\u00E4he gesehen."));
			}

			EnterState(EVaelHarpyState::Recovering);
		}
		break;
	}

	case EVaelHarpyState::Recovering:
		if (StateTime >= Data->RecoverDuration)
		{
			EnterState(EVaelHarpyState::Circling);
			DiveCooldown = RandomInRange(Data->DiveInterval);
		}
		break;
	}
}

void AVaelAshHarpy::EnterState(EVaelHarpyState NewState)
{
	State = NewState;
	StateTime = 0.0f;
}

void AVaelAshHarpy::Screech()
{
	const UVaelAshHarpyData* Data = GetData<UVaelAshHarpyData>();

	ScreechEndTime = GetWorld()->GetTimeSeconds() + HarpyScreechPose;

	int32 NumNearby = 0;
	for (TActorIterator<AVaelAshHarpy> It(GetWorld()); It; ++It)
	{
		if (*It != this && !It->IsDead() && GetDistanceTo2D(*It) < Data->ScreechRange)
		{
			++NumNearby;
		}
	}

	UVaelNoticeSubsystem::Post(this, FText::Format(LOCTEXT("Screech", "{0} kreischt"), GetCreatureName()), LOCTEXT("ScreechDetail", "Weitere Harpyien folgen ihrem Ruf."), FLinearColor(FColor(201, 180, 138)));

	if (NumNearby >= Data->ScreechMaxNearby)
	{
		return;
	}

	UVaelCreatureData* SpawnData = Data->ScreechSpawnData != nullptr ? Data->ScreechSpawnData.Get() : GetMutableDefault<UVaelAshHarpyData>();

	for (int32 SpawnIndex = 0; SpawnIndex < Data->ScreechSpawnCount; ++SpawnIndex)
	{
		const FVector Offset = FVector(FMath::FRandRange(-1.0f, 1.0f), FMath::FRandRange(-1.0f, 1.0f), 0.0f) * 140.0f;

		FVector Ground;
		if (!FindGround(GetWorld(), GetActorLocation() + Offset, Ground))
		{
			continue;
		}

		if (AVaelAshHarpy* Harpy = Cast<AVaelAshHarpy>(SpawnCreature(GetWorld(), SpawnData, Ground, GetActorRotation())))
		{
			Harpy->StartCircling();
		}
	}
}

#undef LOCTEXT_NAMESPACE
