// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelPfuhlnatter.h"
#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"
#include "Combat/VaelArcShot.h"
#include "Combat/VaelCombatStatics.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Creatures/VaelCreatureData.h"
#include "Creatures/VaelPoisonPuddle.h"
#include "Creatures/VaelSequenceAnimInstance.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/VaelCharacter.h"

namespace
{
	/** A new path point once the lead has moved this far, in cm */
	constexpr float SnakePathStep = 4.0f;

	/** How long the coils take to sink to the ground after letting go, and how long it stays before slithering on */
	constexpr float SnakeReleaseTime = 0.6f;
}

AVaelPfuhlnatter::AVaelPfuhlnatter()
{
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	GetMesh()->SetAnimInstanceClass(UVaelSequenceAnimInstance::StaticClass());
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	// The body trails far behind the actor along its path
	GetMesh()->SetBoundsScale(3.0f);
}

TSubclassOf<UVaelCreatureData> AVaelPfuhlnatter::GetDefaultDataClass() const
{
	return UVaelPfuhlnatterData::StaticClass();
}

void AVaelPfuhlnatter::BeginPlay()
{
	Super::BeginPlay();

	const UVaelPfuhlnatterData* Data = GetData<UVaelPfuhlnatterData>();
	USkeletalMesh* SnakeMesh = Data->NatterMesh.LoadSynchronous();
	if (SnakeMesh == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("'%s': SK_Pfuhlnatter is missing, the snake keeps its placeholder look"), *GetName());
		return;
	}
	GetBody()->SetVisibility(false);
	GetMesh()->SetSkeletalMesh(SnakeMesh);
	GetMesh()->SetRelativeScale3D(FVector(Data->BodyScale));
	// The capsule sits on the front of the body, where it is hit and strikes from; the rest trails behind
	GetMesh()->SetRelativeLocationAndRotation(FVector(-Data->FrontOffset * Data->BodyScale, 0.0f, -GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), FRotator::ZeroRotator);
	GetMesh()->InitAnim(true);

	// The head turns quickly into its waves: the body follows the path, so the turns never show as a stiff swing
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->RotationRate = FRotator(0.0f, Data->TurnRate, 0.0f);
	Movement->MaxAcceleration = Data->Acceleration;
	Animator = Cast<UVaelSequenceAnimInstance>(GetMesh()->GetAnimInstance());

	CrawlSequence = Data->CrawlAnimation.LoadSynchronous();
	DeathSequence = Data->DeathAnimation.LoadSynchronous();

	// Spine rumpf_01 (tail root) .. rumpf_NN (neck) and the head; each joint distance is the next bone's offset
	const FReferenceSkeleton& Skeleton = SnakeMesh->GetRefSkeleton();
	for (int32 Index = 1; ; ++Index)
	{
		const FName Bone(*FString::Printf(TEXT("rumpf_%02d"), Index));
		if (Skeleton.FindBoneIndex(Bone) == INDEX_NONE)
		{
			break;
		}
		SpineBones.Add(Bone);
	}
	SpineBones.Add(TEXT("kopf"));
	// Measured in the mesh's space: the imported skeleton carries a scale of its own, so local offsets are not in cm
	BodyLength = 0.0f;
	for (int32 Index = 1; Index < SpineBones.Num(); ++Index)
	{
		const int32 Bone = Skeleton.FindBoneIndex(SpineBones[Index]);
		const int32 Parent = Skeleton.FindBoneIndex(SpineBones[Index - 1]);
		const float Length = Bone != INDEX_NONE && Parent != INDEX_NONE
			? FVector::Dist(FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, Bone).GetTranslation(),
				FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, Parent).GetTranslation()) : 20.0f;
		SpineLengths.Add(Length * Data->BodyScale);
		BodyLength += SpineLengths.Last();
	}
	UE_LOG(LogTemp, Log, TEXT("'%s': %d spine joints, body %.0f cm"), *GetName(), SpineBones.Num(), BodyLength);
	for (int32 Bone = 0; Bone < Skeleton.GetNum(); ++Bone)
	{
		const FString Name = Skeleton.GetBoneName(Bone).ToString();
		if (Name.StartsWith(TEXT("flosse_")) || Name.StartsWith(TEXT("krone_")))
		{
			FinBones.Add(Skeleton.GetBoneName(Bone));
			FinIsCrown.Add(Name.StartsWith(TEXT("krone_")));
			FinScales.Add(1.0f);
		}
	}

	// It lies straight behind its head (the waves are laid onto the path)
	const FVector Neck = GetNeckLocation();
	const FVector Back = -GetActorForwardVector().GetSafeNormal2D();
	for (float Along = BodyLength + 200.0f; Along > 0.0f; Along -= SnakePathStep * 2.0f)
	{
		AddPathPoint(Neck + Back * Along, 0.0f);
	}
	Lead = Neck;
	LeadLift = 0.0f;
	FinOpens.Init(0.0f, FinBones.Num());
	SkinMaterial = GetMesh()->CreateDynamicMaterialInstance(0);
	PickNextAttack();

	EnterState(EVaelSnakeState::Lurk);
}

void AVaelPfuhlnatter::PickNextAttack()
{
	bNextSpit = FMath::FRand() < GetData<UVaelPfuhlnatterData>()->SpitChance;
}

void AVaelPfuhlnatter::RearUp(TArrayView<FVector> Joints) const
{
	const UVaelPfuhlnatterData* Data = GetData<UVaelPfuhlnatterData>();
	const int32 NumJoints = Joints.Num();
	const int32 Count = FMath::Clamp(Data->RearJoints, 2, NumJoints - 1);
	const int32 Base = NumJoints - 1 - Count;
	// The arch rises steeply from its base and bends forward at the top like a cobra's; it leans back to draw breath
	// and forward to strike, and quivers a little
	const FVector Ahead = (LookWeight > 0.1f ? LookPoint - Joints[Base] : Joints[NumJoints - 1] - Joints[Base]).GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Ahead);
	const float Time = GetWorld()->GetTimeSeconds();
	FVector Point = Joints[Base];
	for (int32 Step = 1; Step <= Count; ++Step)
	{
		const float Along = static_cast<float>(Step) / Count;
		const float Pitch = FMath::Lerp(82.0f, -8.0f, FMath::Pow(Along, 1.1f)) - Thrust * 32.0f * Along;
		const FVector Direction = (Ahead * FMath::Cos(FMath::DegreesToRadians(Pitch)) + FVector::UpVector * FMath::Sin(FMath::DegreesToRadians(Pitch))).GetSafeNormal();
		Point += Direction * SpineLengths[Base + Step - 1];
		const FVector Quiver = Side * FMath::Sin(Time * 2.0f * PI * 11.0f + Step) * 1.2f * Along * Rear;
		Joints[Base + Step] = FMath::Lerp(Joints[Base + Step], Point + Quiver, FMath::SmoothStep(0.0f, 1.0f, Rear));
	}
}

void AVaelPfuhlnatter::SpitPoison()
{
	const UVaelPfuhlnatterData* Data = GetData<UVaelPfuhlnatterData>();
	AActor* Target = Prey.Get();
	if (Target == nullptr)
	{
		return;
	}
	// From the mouth to where the prey will be when the gob comes down
	const FVector Mouth = GetMesh()->GetSocketLocation(TEXT("kopf")) + GetActorForwardVector() * 30.0f;
	const FVector Landing = Target->GetActorLocation() + Target->GetVelocity() * FVector(1.0f, 1.0f, 0.0f) * Data->SpitFlightTime * Data->SpitLeadShare;
	const FLinearColor Color = Data->PoisonColor;
	AVaelArcShot::Lob(GetWorld(), Mouth, Landing, Data->SpitFlightTime, Data->SpitApex, Color, 26.0f);
	AVaelPoisonPuddle::Spit(GetWorld(), Landing, Data->SpitFlightTime, this);
	++SpitCount;
}

void AVaelPfuhlnatter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FreePlayer();
	Super::EndPlay(EndPlayReason);
}

FVector AVaelPfuhlnatter::GetNeckLocation() const
{
	// In the crawl pose the neck lies NeckForward ahead of the mesh's middle; the mesh's origin is on the ground
	return GetMesh()->GetComponentTransform().TransformPosition(FVector(GetData<UVaelPfuhlnatterData>()->NeckForward, 0.0f, 0.0f));
}

void AVaelPfuhlnatter::AddPathPoint(const FVector& Location, float Lift)
{
	if (!Path.IsEmpty() && FVector::Dist(Path.Last() + FVector(0.0f, 0.0f, PathLift.Last()), Location + FVector(0.0f, 0.0f, Lift)) < SnakePathStep)
	{
		return;
	}
	PathRun.Add(Path.IsEmpty() ? 0.0f : PathRun.Last() + FVector::Dist2D(Path.Last(), Location));
	Path.Add(Location);
	PathLift.Add(Lift);

	// Keep what the body lies on, and a little more
	float Length = 0.0f;
	int32 Keep = Path.Num() - 1;
	while (Keep > 0 && Length < BodyLength + 150.0f)
	{
		Length += FVector::Dist(Path[Keep], Path[Keep - 1]);
		--Keep;
	}
	if (Keep > 0)
	{
		Path.RemoveAt(0, Keep);
		PathLift.RemoveAt(0, Keep);
		PathRun.RemoveAt(0, Keep);
	}
}

void AVaelPfuhlnatter::LayBodyOnPath()
{
	const int32 NumJoints = SpineBones.Num();
	if (Animator == nullptr || NumJoints < 2 || SpineLengths.Num() != NumJoints - 1)
	{
		return;
	}

	// The path with its waves: they stand still on the ground (their phase is the distance along the path), so the body
	// glides through them; none right behind the head, none in the coils
	const UVaelPfuhlnatterData* Data = GetData<UVaelPfuhlnatterData>();
	const FVector LeadPoint = Lead + FVector(0.0f, 0.0f, LeadLift);
	const float WaveLength = FMath::Max(Data->WaveLength * Data->BodyScale, 1.0f);
	TArray<FVector, TInlineAllocator<512>> Wavy;
	Wavy.SetNum(Path.Num());
	float BehindHead = FVector::Dist2D(LeadPoint, Path.IsEmpty() ? LeadPoint : Path.Last());
	for (int32 Point = Path.Num() - 1; Point >= 0; --Point)
	{
		if (Point < Path.Num() - 1)
		{
			BehindHead += FVector::Dist2D(Path[Point], Path[Point + 1]);
		}
		const FVector Along = (Path[FMath::Min(Point + 1, Path.Num() - 1)] - Path[FMath::Max(Point - 1, 0)]).GetSafeNormal2D();
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Along);
		const float Grow = FMath::SmoothStep(0.0f, Data->WaveRamp * Data->BodyScale, BehindHead) * (1.0f - FMath::Clamp(PathLift[Point] / 20.0f, 0.0f, 1.0f));
		const float Swing = Data->WaveWidth * Data->BodyScale * Grow * FMath::Sin(2.0f * PI * PathRun[Point] / WaveLength);
		Wavy[Point] = Path[Point] + Side * Swing + FVector(0.0f, 0.0f, PathLift[Point]);
	}

	// From the lead back along the path: each joint where the path is one joint distance away from the one before
	TArray<FVector, TInlineAllocator<32>> Joints;
	Joints.SetNum(NumJoints);
	FVector Current = LeadPoint;
	Joints[NumJoints - 1] = Current;
	int32 Next = Path.Num() - 1;
	for (int32 Joint = NumJoints - 2; Joint >= 0; --Joint)
	{
		const float Distance = SpineLengths[Joint];
		FVector From = Current;
		bool bFound = false;
		while (Next >= 0)
		{
			const FVector To = Wavy[Next];
			if (FVector::Dist(To, Current) >= Distance)
			{
				// Where the segment From..To leaves the sphere of this distance round the last joint
				const FVector Segment = To - From;
				const FVector Offset = From - Current;
				const float A = Segment.SizeSquared();
				const float B = 2.0f * FVector::DotProduct(Offset, Segment);
				const float C = Offset.SizeSquared() - Distance * Distance;
				const float T = A > KINDA_SMALL_NUMBER ? FMath::Clamp((-B + FMath::Sqrt(FMath::Max(B * B - 4.0f * A * C, 0.0f))) / (2.0f * A), 0.0f, 1.0f) : 1.0f;
				Current = From + Segment * T;
				bFound = true;
				break;
			}
			From = To;
			--Next;
		}
		if (!bFound)
		{
			// Past the end of the path: straight on the way the body already points
			const FVector Before = Joint + 2 < NumJoints ? Joints[Joint + 2] : Current + GetActorForwardVector();
			Current = Current + (Current - Before).GetSafeNormal2D() * Distance;
		}
		Joints[Joint] = Current;
	}
	if (Rear > 0.0f)
	{
		RearUp(Joints);
	}

	// Into the mesh's space: X and Y where the joints go, Z the height above the ground (added to the pose's own)
	const FTransform& ToWorld = GetMesh()->GetComponentTransform();
	TArray<FVector> Targets;
	Targets.Reserve(NumJoints);
	for (const FVector& Joint : Joints)
	{
		Targets.Add(ToWorld.InverseTransformPosition(Joint));
	}
	Animator->SetSpineTargets(SpineBones, Targets, 1.0f);
}

void AVaelPfuhlnatter::Slither(const FVector& Direction, float Speed, float DeltaSeconds)
{
	// The head goes straight; the waves are in the body behind it (LayBodyOnPath)
	MoveInDirection(Direction, Speed);
}

void AVaelPfuhlnatter::HoldPlayer(AVaelCharacter* Player)
{
	Held = Player;
	if (Player != nullptr)
	{
		UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}
}

void AVaelPfuhlnatter::FreePlayer()
{
	AVaelCharacter* Player = Held.Get();
	Held = nullptr;
	if (Player != nullptr && Player->GetCharacterMovement()->MovementMode == MOVE_None && !Player->IsDefeated()
		&& !UVaelCombatStatics::HasStatus(Player, EVaelStatus::Frozen))
	{
		Player->GetCharacterMovement()->SetDefaultMovementMode();
	}
}

void AVaelPfuhlnatter::EnterState(EVaelSnakeState NewState)
{
	const UVaelPfuhlnatterData* Data = GetData<UVaelPfuhlnatterData>();
	const EVaelSnakeState OldState = State;
	State = NewState;
	StateTime = 0.0f;

	switch (NewState)
	{
	case EVaelSnakeState::Lurk:
		Prey = nullptr;
		break;

	case EVaelSnakeState::Hunt:
		NoPreyTime = 0.0f;
		break;

	case EVaelSnakeState::Constrict:
	{
		AVaelCharacter* Player = Cast<AVaelCharacter>(Prey.Get());
		HoldPlayer(Player);
		++ConstrictCount;
		DamageWhileHolding = 0.0f;
		SqueezeCountdown = Data->WrapTime;
		// Winds on from where its head is, round the side it came from
		CoilCentre = Player->GetActorLocation() - FVector(0.0f, 0.0f, Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		const FVector FromPrey = GetNeckLocation() - CoilCentre;
		CoilStartAngle = FMath::Atan2(FromPrey.Y, FromPrey.X);
		const float Cross = FVector::CrossProduct(FromPrey.GetSafeNormal2D(), GetActorForwardVector().GetSafeNormal2D()).Z;
		CoilSense = Cross >= 0.0f ? 1.0f : -1.0f;
		break;
	}

	case EVaelSnakeState::Release:
		FreePlayer();
		break;

	case EVaelSnakeState::Spit:
		bSpat = false;
		break;

	case EVaelSnakeState::Wallow:
		++WallowCount;
		WallowAngle = FMath::FRandRange(0.0f, 2.0f * PI);
		break;

	default:
		break;
	}

	if (OldState == EVaelSnakeState::Constrict && NewState != EVaelSnakeState::Release)
	{
		FreePlayer();
	}
}

void AVaelPfuhlnatter::TickBehavior(float DeltaSeconds)
{
	const UVaelPfuhlnatterData* Data = GetData<UVaelPfuhlnatterData>();
	StateTime += DeltaSeconds;
	StrikeCooldown -= DeltaSeconds;

	float Distance = 0.0f;
	switch (State)
	{
	case EVaelSnakeState::Lurk:
		if (FindTarget(Data->AggroRange) != nullptr)
		{
			EnterState(EVaelSnakeState::Hunt);
		}
		break;

	case EVaelSnakeState::Hunt:
	{
		AActor* Target = FindTarget(Data->AggroRange, &Distance);
		Prey = Target;
		if (Target == nullptr)
		{
			// Without prey it slithers home and lies still there
			NoPreyTime += DeltaSeconds;
			const FVector ToHome = (HomeLocation - GetActorLocation()) * FVector(1.0f, 1.0f, 0.0f);
			if (NoPreyTime < Data->LoseInterestTime)
			{
				break;
			}
			if (ToHome.Size() > 150.0f)
			{
				Slither(ToHome, Data->CrawlSpeed * 0.6f, DeltaSeconds);
			}
			else
			{
				EnterState(EVaelSnakeState::Lurk);
			}
			break;
		}
		NoPreyTime = 0.0f;
		const FVector ToPrey = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		if (StrikeCooldown <= 0.0f && GetMaxHealth() > 0.0f && GetHealth() < Data->WallowHealthShare * GetMaxHealth())
		{
			// Hurt: off to a puddle of its poison to heal, if there is one
			if (AVaelPoisonPuddle* Puddle = AVaelPoisonPuddle::FindNearest(GetWorld(), GetActorLocation(), Data->WallowRange))
			{
				WallowPuddle = Puddle;
				EnterState(EVaelSnakeState::Wallow);
				break;
			}
		}
		if (StrikeCooldown <= 0.0f && bNextSpit && Distance >= Data->SpitRange.X && Distance <= Data->SpitRange.Y && !IsBlinded())
		{
			EnterState(EVaelSnakeState::Spit);
		}
		else if (StrikeCooldown <= 0.0f && (!bNextSpit || Distance < Data->SpitRange.X) && Distance <= Data->RushRange && !IsBlinded())
		{
			EnterState(EVaelSnakeState::Rush);
		}
		else if (StrikeCooldown > 0.0f && Distance < Data->CircleDistance * 1.4f)
		{
			// Waits for its next strike circling the prey, in and out a little
			const FVector Around = FVector::CrossProduct(FVector::UpVector, ToPrey) * CircleSide;
			const float InOut = FMath::Clamp((Distance - Data->CircleDistance) / Data->CircleDistance, -1.0f, 1.0f);
			Slither(Around + ToPrey * InOut * 1.5f, Data->CrawlSpeed, DeltaSeconds);
		}
		else
		{
			Slither(ToPrey, Data->CrawlSpeed, DeltaSeconds);
		}
		break;
	}

	case EVaelSnakeState::Rush:
	{
		AActor* Target = Prey.Get();
		if (Target == nullptr || StateTime > Data->RushTime || GetDistanceTo2D(Target) > Data->RushRange * 1.5f)
		{
			StrikeCooldown = RandomInRange(Data->StrikeInterval) * 0.5f;
			EnterState(EVaelSnakeState::Hunt);
			break;
		}
		const UCapsuleComponent* TargetCapsule = Target->FindComponentByClass<UCapsuleComponent>();
		const float Touch = GetCapsuleComponent()->GetScaledCapsuleRadius() + (TargetCapsule != nullptr ? TargetCapsule->GetScaledCapsuleRadius() : 40.0f) + Data->ContactSlack;
		if (GetDistanceTo2D(Target) <= Touch)
		{
			if (Cast<AVaelCharacter>(Target) != nullptr)
			{
				EnterState(EVaelSnakeState::Constrict);
			}
			else
			{
				HitPlayer(Target, Data->SqueezeDamage, EVaelElement::Earth);
				StrikeCooldown = RandomInRange(Data->StrikeInterval);
				EnterState(EVaelSnakeState::Hunt);
			}
			break;
		}
		Slither(Target->GetActorLocation() - GetActorLocation(), Data->RushSpeed, DeltaSeconds);
		break;
	}

	case EVaelSnakeState::Constrict:
	{
		AVaelCharacter* Player = Held.Get();
		if (Player == nullptr || Player->IsDefeated() || StateTime >= Data->WrapTime + Data->HoldTime
			|| DamageWhileHolding >= Data->BreakDamageShare * GetMaxHealth())
		{
			EnterState(EVaelSnakeState::Release);
			break;
		}
		// The head winds up round the prey and the body follows its path into coils; held, they tighten in pulses
		const float Wound = FMath::Clamp(StateTime / FMath::Max(Data->WrapTime, 0.05f), 0.0f, 1.0f);
		const float Squeeze = StateTime > Data->WrapTime ? FMath::Abs(FMath::Sin((StateTime - Data->WrapTime) * PI / FMath::Max(Data->SqueezeInterval, 0.1f))) : 0.0f;
		const float Radius = (Player->GetCapsuleComponent()->GetScaledCapsuleRadius() + Data->CoilGap * Data->BodyScale) * (1.0f - 0.08f * Squeeze);
		const float Angle = CoilStartAngle + CoilSense * Wound * Data->CoilTurns * 2.0f * PI;
		Lead = CoilCentre + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Radius;
		LeadLift = FMath::Lerp(Data->CoilHeight.X, Data->CoilHeight.Y, Wound) * Data->BodyScale;
		FaceTowards(CoilCentre);

		SqueezeCountdown -= DeltaSeconds;
		if (SqueezeCountdown <= 0.0f)
		{
			SqueezeCountdown = Data->SqueezeInterval;
			HitPlayer(Player, Data->SqueezeDamage, EVaelElement::Earth);
		}
		break;
	}

	case EVaelSnakeState::Release:
		if (StateTime >= SnakeReleaseTime)
		{
			StrikeCooldown = RandomInRange(Data->StrikeInterval);
			CircleSide = FMath::RandBool() ? 1.0f : -1.0f;
			PickNextAttack();
			EnterState(EVaelSnakeState::Hunt);
		}
		break;

	case EVaelSnakeState::Spit:
	{
		// Rears up, draws back trembling while its throat glows, strikes forward and spits, holds, lays down
		const float Up = Data->RearTime;
		const float Strike = Up + Data->TellTime;
		const float Down = Strike + 0.5f;
		const float End = Down + 0.45f;
		if (Prey.IsValid() && StateTime < Strike)
		{
			Prey = FindTarget(Data->AggroRange * 1.2f) != nullptr ? Prey : nullptr;
		}
		Rear = StateTime < Down ? FMath::SmoothStep(0.0f, 1.0f, StateTime / FMath::Max(Up, 0.05f)) : 1.0f - FMath::SmoothStep(Down, End, StateTime);
		if (StateTime < Strike)
		{
			Thrust = -0.6f * FMath::SmoothStep(Up * 0.5f, Strike, StateTime);
			ThroatGlow = FMath::SmoothStep(Up * 0.6f, Strike, StateTime);
		}
		else
		{
			Thrust = StateTime < Strike + 0.12f ? FMath::Lerp(-0.6f, 1.0f, (StateTime - Strike) / 0.12f) : FMath::Lerp(1.0f, 0.0f, FMath::Clamp((StateTime - Strike - 0.12f) / 0.4f, 0.0f, 1.0f));
			ThroatGlow = FMath::Max(0.0f, 1.0f - (StateTime - Strike) / 0.3f);
		}
		if (AActor* Target = Prey.Get())
		{
			TurnTowards(Target->GetActorLocation(), DeltaSeconds);
		}
		if (!bSpat && StateTime >= Strike + 0.08f)
		{
			bSpat = true;
			SpitPoison();
		}
		if (StateTime >= End || (!Prey.IsValid() && StateTime < Strike))
		{
			Rear = 0.0f;
			Thrust = 0.0f;
			ThroatGlow = 0.0f;
			StrikeCooldown = RandomInRange(Data->StrikeInterval);
			CircleSide = FMath::RandBool() ? 1.0f : -1.0f;
			PickNextAttack();
			EnterState(EVaelSnakeState::Hunt);
		}
		break;
	}

	case EVaelSnakeState::Wallow:
	{
		AVaelPoisonPuddle* Puddle = WallowPuddle.Get();
		if (Puddle == nullptr || Puddle->GetPoisonState() != EVaelPoisonState::Liquid || StateTime > Data->WallowTime + 4.0f
			|| GetHealth() >= GetMaxHealth())
		{
			StrikeCooldown = RandomInRange(Data->StrikeInterval) * 0.5f;
			EnterState(EVaelSnakeState::Hunt);
			break;
		}
		const FVector Middle = Puddle->GetMiddle();
		if (!Puddle->Covers(GetActorLocation(), 20.0f))
		{
			// On its way there
			Slither(Middle - GetActorLocation(), Data->RushSpeed * 0.8f, DeltaSeconds);
			break;
		}
		// Rolls round through the poison and soaks it up
		WallowAngle += DeltaSeconds * 1.6f;
		const float Round = FMath::Max(Puddle->GetRadius() * 0.45f, 60.0f);
		Slither(Middle + FVector(FMath::Cos(WallowAngle), FMath::Sin(WallowAngle), 0.0f) * Round - GetActorLocation(), Data->CrawlSpeed * 0.6f, DeltaSeconds);
		const float Got = Puddle->Drain(Data->WallowDrainPerSecond * DeltaSeconds);
		if (Got > 0.0f)
		{
			UVaelCombatStatics::Heal(this, Data->HealPerSecond * DeltaSeconds * (Puddle->IsCorrupted() ? Data->CorruptHealMultiplier : 1.0f));
		}
		if (StateTime > Data->WallowTime)
		{
			StrikeCooldown = RandomInRange(Data->StrikeInterval) * 0.5f;
			EnterState(EVaelSnakeState::Hunt);
		}
		break;
	}
	}
}

void AVaelPfuhlnatter::TurnTowards(const FVector& Location, float DeltaSeconds)
{
	const FVector To = (Location - GetActorLocation()).GetSafeNormal2D();
	if (!To.IsNearlyZero())
	{
		const float Step = 180.0f * DeltaSeconds;
		const float Delta = FMath::Clamp(FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, To.Rotation().Yaw), -Step, Step);
		SetActorRotation(FRotator(0.0f, GetActorRotation().Yaw + Delta, 0.0f));
	}
}

void AVaelPfuhlnatter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const UVaelPfuhlnatterData* Data = GetData<UVaelPfuhlnatterData>();
	if (Animator == nullptr)
	{
		return;
	}
	if (Animator->GetCurrent() == nullptr && CrawlSequence != nullptr)
	{
		// The crawl pose, held: the waves come from the path, the pose gives fins and head
		Animator->Play(CrawlSequence, true, 0.0f, 0.0f);
	}

	// Crawling, the neck leads; winding round prey, the head of the coil does (set in TickBehavior)
	if (State != EVaelSnakeState::Constrict)
	{
		Lead = GetNeckLocation();
		LeadLift = 0.0f;
	}
	if (State != EVaelSnakeState::Constrict && !IsDead())
	{
		// Let go: the coils sink down to the ground
		for (float& Lift : PathLift)
		{
			Lift = FMath::FInterpTo(Lift, 0.0f, DeltaSeconds, 1.0f / SnakeReleaseTime * 3.0f);
		}
	}
	if (!IsDead())
	{
		AddPathPoint(Lead, LeadLift);
	}
	LayBodyOnPath();

	// The head always looks at its prey, whichever way it crawls; coiled, at its face
	const AActor* Watched = Held.IsValid() ? static_cast<AActor*>(Held.Get()) : Prey.Get();
	const bool bWatch = Watched != nullptr && !IsDead() && State != EVaelSnakeState::Lurk;
	LookWeight = FMath::FInterpTo(LookWeight, bWatch ? 1.0f : 0.0f, DeltaSeconds, 4.0f);
	if (Watched != nullptr)
	{
		LookPoint = Watched->GetActorLocation() + FVector(0.0f, 0.0f, Held.IsValid() ? 60.0f : 0.0f);
	}
	Animator->SetLookAt(GetMesh()->GetComponentTransform().InverseTransformPosition(LookPoint), Data->HeadLookAngle, LookWeight);

	// Crest and fins lie flat while it crawls; coiled round prey the crown opens again; reared to spit, everything
	// spreads and quivers
	const bool bReared = State == EVaelSnakeState::Spit && Rear > 0.3f;
	const float DrawTarget = State == EVaelSnakeState::Constrict || bReared ? 0.0f : 1.0f;
	FinDraw = FMath::FInterpTo(FinDraw, DrawTarget, DeltaSeconds, bReared ? 8.0f : 5.0f);
	FinOpen = FMath::FInterpTo(FinOpen, bReared ? 1.0f : (State == EVaelSnakeState::Constrict ? 0.5f : 0.0f), DeltaSeconds, bReared ? 6.0f : 4.0f);
	for (int32 Index = 0; Index < FinBones.Num(); ++Index)
	{
		const float Flat = FinIsCrown[Index] ? Data->CrownFlatScale : Data->FinFlatScale;
		const float Spread = bReared || FinIsCrown[Index] ? 1.0f : Data->FinCoiledScale;
		FinScales[Index] = FMath::Lerp(Spread, Flat, FinDraw);
		FinOpens[Index] = FinIsCrown[Index] || bReared ? FinOpen : 0.0f;
	}
	Animator->SetBoneScales(FinBones, FinScales, FinOpens, Data->TrembleAngle * FinOpen * (bReared ? 1.0f : 0.0f), GetWorld()->GetTimeSeconds());

	// The throat glows in the poison's colour before it spits
	if (SkinMaterial != nullptr)
	{
		SkinMaterial->SetScalarParameterValue(TEXT("GlutStaerke"), ThroatGlow * 3.0f);
		SkinMaterial->SetVectorParameterValue(TEXT("GlutFarbe"), Data->PoisonColor);
	}
	if (State != EVaelSnakeState::Spit)
	{
		Rear = FMath::FInterpTo(Rear, 0.0f, DeltaSeconds, 6.0f);
		ThroatGlow = 0.0f;
	}
}

void AVaelPfuhlnatter::OnDamageTaken(float Damage, const FGameplayTagContainer& DamageTags)
{
	Super::OnDamageTaken(Damage, DamageTags);

	if (State == EVaelSnakeState::Constrict)
	{
		// Hits from the friends of its prey make it let go (co-op rescue)
		DamageWhileHolding += Damage;
	}
	else if (!IsDead() && State == EVaelSnakeState::Lurk)
	{
		EnterState(EVaelSnakeState::Hunt);
	}
}

void AVaelPfuhlnatter::Die()
{
	FreePlayer();
	if (Animator != nullptr && DeathSequence != nullptr)
	{
		Animator->Play(DeathSequence, false, 0.2f);
	}
	Super::Die();
}
