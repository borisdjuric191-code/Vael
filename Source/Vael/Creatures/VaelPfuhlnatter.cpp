// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelPfuhlnatter.h"
#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"
#include "Combat/VaelCombatStatics.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Creatures/VaelCreatureData.h"
#include "Creatures/VaelSequenceAnimInstance.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
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

	EnterState(EVaelSnakeState::Lurk);
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
		if (StrikeCooldown <= 0.0f && Distance <= Data->RushRange && !IsBlinded())
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
			EnterState(EVaelSnakeState::Hunt);
		}
		break;
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

	// Crest and fins lie flat while it crawls; coiled round prey the crown opens again
	const float DrawTarget = State == EVaelSnakeState::Constrict ? 0.0f : 1.0f;
	FinDraw = FMath::FInterpTo(FinDraw, DrawTarget, DeltaSeconds, 5.0f);
	for (int32 Index = 0; Index < FinBones.Num(); ++Index)
	{
		const float Flat = FinIsCrown[Index] ? Data->CrownFlatScale : Data->FinFlatScale;
		FinScales[Index] = FMath::Lerp(FinIsCrown[Index] ? 1.0f : Data->FinCoiledScale, Flat, FinDraw);
	}
	Animator->SetBoneScales(FinBones, FinScales);
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
