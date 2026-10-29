// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelPfuhlnatter.h"
#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"
#include "Magic/VaelElementSource.h"
#include "Magic/VaelSpellProjectile.h"
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

	/** Path points on each side over which the way along the path is measured (about 60 cm at SnakePathStep) */
	constexpr int32 SnakeDirectionSpan = 8;

	/** How quickly (1/s) the body behind the neck follows its new place, and how many front joints follow at once */
	constexpr float SnakeBodyFollowRate = 14.0f;
	constexpr int32 SnakeFreeFrontJoints = 9;

	/** Sharpest bend between two body joints (degrees): crawling, and wound round prey */
	constexpr float SnakeMaxBend = 26.0f;
	constexpr float SnakeMaxCoilBend = 40.0f;

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
	// It steers itself (Slither): never turned by its movement, which could slide it backwards
	Movement->bOrientRotationToMovement = false;
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
	for (const FName& Bone : SpineBones)
	{
		const int32 Index = Skeleton.FindBoneIndex(Bone);
		RestJoints.Add(Index != INDEX_NONE ? FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, Index).GetTranslation() : FVector::ZeroVector);
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
			// The fin's place along the body sets its phase in the sway wave (flosse_NN_*: piece NN from the tail);
			// crown petals barely sway, each a little apart
			const int32 Piece = Name.StartsWith(TEXT("flosse_")) ? FCString::Atoi(*Name.Mid(7, 2)) : 0;
			FinPhases.Add(Name.StartsWith(TEXT("krone_")) ? FinPhases.Num() * 0.9f : Piece * 0.55f);
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
	// Boris 2026-10-29: coiling round the player didn't look clean enough; it spits and wallows in its poison only
	const UVaelPfuhlnatterData* Data = GetData<UVaelPfuhlnatterData>();
	bNextSpit = !Data->bCanConstrict || FMath::FRand() < Data->SpitChance;
}

void AVaelPfuhlnatter::RearUp(TArrayView<FVector> Joints, int32 InCount, float Weight) const
{
	const int32 NumJoints = Joints.Num();
	const int32 Count = FMath::Clamp(InCount, 2, NumJoints - 1);
	const int32 Base = NumJoints - 1 - Count;
	// The arch rises steeply from its base and bends forward at the top like a cobra's; it leans back to draw breath
	// and forward to strike, and quivers a little
	const FVector Ahead = (LookWeight > 0.1f ? LookPoint - Joints[Base] : Joints[NumJoints - 1] - Joints[Base]).GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Ahead);
	const float Time = GetWorld()->GetTimeSeconds();
	if (RestJoints.Num() != NumJoints)
	{
		return;
	}
	// The arch is the Tripo pose's own front (the shape the crown skin over the neck was modelled on), turned to face
	// ahead; it leans back to draw breath and forward to strike, and quivers a little
	const FTransform& ToWorld = GetMesh()->GetComponentTransform();
	const FVector RestAhead = ToWorld.TransformVectorNoScale(RestJoints[NumJoints - 1] - RestJoints[Base]).GetSafeNormal2D();
	const FQuat Face = FQuat::FindBetweenNormals(RestAhead, Ahead);
	const float Lean = FMath::DegreesToRadians(Thrust * 24.0f);
	for (int32 Step = 1; Step <= Count; ++Step)
	{
		const float Along = static_cast<float>(Step) / Count;
		const FVector Shape = Face.RotateVector(ToWorld.TransformVector(RestJoints[Base + Step] - RestJoints[Base]));
		// Lean in the upright plane through Ahead: positive tips the top forward and down
		const float Forward = FVector::DotProduct(Shape, Ahead);
		const FVector Across = Shape - Ahead * Forward - FVector::UpVector * Shape.Z;
		const FVector Leaned = Ahead * (Forward * FMath::Cos(Lean) + Shape.Z * FMath::Sin(Lean)) + FVector::UpVector * (Shape.Z * FMath::Cos(Lean) - Forward * FMath::Sin(Lean)) + Across;
		const FVector Quiver = Side * FMath::Sin(Time * 2.0f * PI * 11.0f + Step) * 1.2f * Along * Weight;
		Joints[Base + Step] = FMath::Lerp(Joints[Base + Step], Joints[Base] + Leaned + Quiver, FMath::SmoothStep(0.0f, 1.0f, Weight));
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
	// One ball of poison water from each fang, at where the prey will be
	const FVector Head = GetMesh()->GetSocketLocation(TEXT("kopf"));
	const FVector Aim = Target->GetActorLocation() + Target->GetVelocity() * FVector(1.0f, 1.0f, 0.0f) * (GetDistanceTo2D(Target) / Data->SpitSpeed) * Data->SpitLeadShare;
	const FVector Forward = (Aim - Head).GetSafeNormal2D();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	FVaelSpellHit Hit;
	Hit.Damage = Data->SpitDamage;
	Hit.Element = EVaelElement::Water;
	UVaelElementSource* Poison = Data->PoisonSource.LoadSynchronous();
	for (const float Side : { -1.0f, 1.0f })
	{
		const FVector Fang = Head + Forward * Data->FangOffset.X + Right * Data->FangOffset.Y * Side + FVector(0.0f, 0.0f, Data->FangOffset.Z);
		const float Range = FMath::Max(Data->SpitRange.Y * 1.2f, FVector::Dist2D(Fang, Aim) + 200.0f);
		AVaelSpellProjectile::Launch(this, Fang, (Aim + Right * Data->FangOffset.Y * Side - Fang).GetSafeNormal2D(), Hit, Data->SpitSpeed,
			Data->SpitBallRadius, Range / Data->SpitSpeed, Data->PoisonColor, 0.0f, 0.0f, 0.0f, Poison);
	}
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

	// Keep what the body lies on, and plenty more: the body lies in waves on the path and is longer along it than the
	// path's straight length; kept too short, its tail lay on the oldest points and jumped whenever one was dropped
	float Length = 0.0f;
	int32 Keep = Path.Num() - 1;
	while (Keep > 0 && Length < BodyLength * 1.6f + 300.0f)
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
		// The way along the path over a stretch of about 60 cm, and how sharply it bends there: with neighbours only,
		// a kink in the path flipped the side of the wave at once and the body jumped as each joint passed it; in a
		// sharp bend the waves flatten out
		const int32 Span = SnakeDirectionSpan;
		const FVector Here = Path[Point];
		const FVector In = (Here - Path[FMath::Max(Point - Span, 0)]).GetSafeNormal2D();
		const FVector Out = (Path[FMath::Min(Point + Span, Path.Num() - 1)] - Here).GetSafeNormal2D();
		const FVector Along = (In + Out).GetSafeNormal2D().IsNearlyZero() ? Out : (In + Out).GetSafeNormal2D();
		const float Straight = In.IsNearlyZero() || Out.IsNearlyZero() ? 1.0f : FMath::Square(FMath::Clamp(FVector::DotProduct(In, Out), 0.0f, 1.0f));
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Along);
		const float Grow = FMath::SmoothStep(0.0f, Data->WaveRamp * Data->BodyScale, BehindHead) * (1.0f - FMath::Clamp(PathLift[Point] / 20.0f, 0.0f, 1.0f)) * Straight;
		// Lying still it never freezes: the waves breathe a little wider and narrower and slowly shift along the body
		const float Swing = Data->WaveWidth * Data->BodyScale * Grow * (1.0f + 0.12f * FMath::Sin(Breath)) * FMath::Sin(2.0f * PI * PathRun[Point] / WaveLength + IdleShift);
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
		RearUp(Joints, Data->RearJoints, Rear);
	}

	// Its long fangs hang below the jaw: lying or crawling, the head is held up by their length, so it never stands on them
	if (State != EVaelSnakeState::Constrict && State != EVaelSnakeState::Release && !IsDead())
	{
		const float Clearance = Data->FangClearance * GetMesh()->GetComponentScale().Z;
		const float Missing = Clearance - (Joints[NumJoints - 1].Z - GetMesh()->GetComponentLocation().Z);
		if (Missing > 0.0f)
		{
			const float Raise[] = { 1.0f, 0.7f, 0.4f, 0.15f };
			for (int32 Back = 0; Back < 4 && NumJoints - 1 - Back >= 0; ++Back)
			{
				Joints[NumJoints - 1 - Back].Z += Missing * Raise[Back];
			}
		}
	}

	// The tail's tip is never still: it curls slowly to and fro
	const float Time = GetWorld()->GetTimeSeconds();
	const int32 TailJoints = FMath::Min(6, NumJoints - 2);
	for (int32 Joint = 0; Joint < TailJoints; ++Joint)
	{
		const FVector Along = (Joints[Joint + 1] - Joints[Joint]).GetSafeNormal2D();
		const float Tip = FMath::Square(1.0f - static_cast<float>(Joint) / TailJoints);
		Joints[Joint] += FVector::CrossProduct(FVector::UpVector, Along) * FMath::Sin(Time * 1.7f + Joint * 0.5f) * 6.0f * Data->BodyScale * Tip;
	}

	// Coiled round its prey, squeezes run along the coils as waves: each turn presses in and lets up after the one before
	if (State == EVaelSnakeState::Constrict && StateTime > Data->WrapTime)
	{
		const float Hold = FMath::Clamp((StateTime - Data->WrapTime) / 0.4f, 0.0f, 1.0f);
		for (int32 Joint = 0; Joint < NumJoints; ++Joint)
		{
			const float Lift = Joints[Joint].Z - GetMesh()->GetComponentLocation().Z;
			if (Lift > 15.0f)
			{
				const FVector In = (CoilCentre - Joints[Joint]).GetSafeNormal2D();
				const float Press = 0.5f + 0.5f * FMath::Sin(Time * 5.0f - Joint * 0.55f);
				Joints[Joint] += (In * 5.0f + FVector(0.0f, 0.0f, 2.0f)) * Press * Hold * Data->BodyScale;
			}
		}
		// Above the coils the neck rises in its modelled shape and the head looks the prey in the face: a free-wound neck
		// stretched the crown skin over it into straps
		RearUp(Joints, 5, Hold * 0.8f);
	}

	// The body behind the neck follows its new place over a few frames: whatever small kink the path still has, a
	// joint passing it slides instead of twitching (Boris 2026-10-29: the hind body twitched often while slithering)
	{
		const float DeltaSeconds = GetWorld()->GetDeltaSeconds();
		const float Follow = 1.0f - FMath::Exp(-DeltaSeconds * SnakeBodyFollowRate);
		const int32 Smoothed = FMath::Max(NumJoints - SnakeFreeFrontJoints, 0);
		if (SmoothJoints.Num() != NumJoints || FVector::Dist(SmoothJoints[0], Joints[0]) > 300.0f)
		{
			SmoothJoints = TArray<FVector>(Joints.GetData(), NumJoints);
		}
		for (int32 Joint = 0; Joint < NumJoints; ++Joint)
		{
			// Fully smoothed along the body, easing to none over the neck so the front stays quick
			const float Ease = Joint < Smoothed ? 1.0f : 1.0f - static_cast<float>(Joint - Smoothed + 1) / (SnakeFreeFrontJoints + 1);
			// Carried along with the snake's own pace first, so the smoothed body never trails behind the neck
			const FVector Carried = SmoothJoints[Joint] + GetVelocity() * DeltaSeconds;
			SmoothJoints[Joint] = FMath::Lerp(Joints[Joint], FMath::Lerp(Carried, Joints[Joint], Follow), Ease);
			Joints[Joint] = SmoothJoints[Joint];
		}
	}

	// A clean body: from the neck back each joint gets its true distance again (the smoothing pulls some together)
	// and no joint bends sharper than its skin can follow (tight waves and the tail's curl kinked it by up to 90
	// degrees and crumpled the hind body); the reared front keeps its modelled shape
	{
		const int32 Front = FMath::Clamp(NumJoints - SnakeFreeFrontJoints, 0, NumJoints - 2);
		const float MaxBend = FMath::DegreesToRadians(State == EVaelSnakeState::Constrict || State == EVaelSnakeState::Release ? SnakeMaxCoilBend : SnakeMaxBend);
		for (int32 Joint = Front - 1; Joint >= 0; --Joint)
		{
			// Both point tailwards: Straight the way the body runs on from the joint before, Back where this joint lies
			const FVector Straight = (Joints[Joint + 1] - Joints[Joint + 2]).GetSafeNormal();
			FVector Back = (Joints[Joint] - Joints[Joint + 1]).GetSafeNormal();
			if (Back.IsNearlyZero())
			{
				Back = Straight;
			}
			const float Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(Back, Straight), -1.0f, 1.0f));
			// The first joint behind the neck only keeps its length: the neck is raised while hunting, and bending the
			// body towards its slope would lift the hind body off the ground
			if (Joint < Front - 1 && Angle > MaxBend && !Straight.IsNearlyZero())
			{
				Back = FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(Straight, Back), MaxBend / Angle).RotateVector(Straight);
			}
			Joints[Joint] = Joints[Joint + 1] + Back * SpineLengths[Joint];
		}
	}

	// Into the mesh's space: X and Y where the joints go, Z the height above the ground (added to the pose's own)
	const FTransform& ToWorld = GetMesh()->GetComponentTransform();
	TArray<FVector> Targets;
	TArray<FVector> Ups;
	Targets.Reserve(NumJoints);
	Ups.Reserve(NumJoints);
	for (int32 Joint = 0; Joint < NumJoints; ++Joint)
	{
		Targets.Add(ToWorld.InverseTransformPosition(Joints[Joint]));

		// Its back up and its belly on the ground; where the body stands up (the reared neck) the back faces away from
		// where it goes, so the belly faces forward; in the coils the back faces out and the belly lies on the prey
		const FVector Ahead = (Joints[FMath::Min(Joint + 1, NumJoints - 1)] - Joints[FMath::Max(Joint + 1, 1) - 1]).GetSafeNormal2D();
		FVector Up = FVector::UpVector - Ahead;
		const float Lift = Joints[Joint].Z - ToWorld.GetLocation().Z;
		if ((State == EVaelSnakeState::Constrict || State == EVaelSnakeState::Release) && Lift > 10.0f)
		{
			const FVector Out = (Joints[Joint] - CoilCentre).GetSafeNormal2D();
			Up = FMath::Lerp(Up, Out, FMath::Clamp((Lift - 10.0f) / 30.0f, 0.0f, 1.0f));
		}
		Ups.Add(ToWorld.InverseTransformVectorNoScale(Up));
	}
	Animator->SetSpineTargets(SpineBones, Targets, 1.0f, Ups);
}

void AVaelPfuhlnatter::Slither(const FVector& Direction, float Speed, float DeltaSeconds)
{
	MoveInDirection(Direction, Speed);
}

void AVaelPfuhlnatter::MoveInDirection(const FVector& Direction, float Speed)
{
	const float DeltaSeconds = GetWorld()->GetDeltaSeconds();
	// Every way it moves (also confused or afraid) goes through here. A snake only crawls forward and steers: the head turns towards where it wants to go no tighter than its
	// smallest curve, and it slows in sharp turns. Moving straight the wanted way (the movement's own choice) let it
	// slide backwards for a moment whenever that way lay behind it: its head ran back over its own path and the body
	// lying on that path jumped a metre along it
	const UVaelPfuhlnatterData* Data = GetData<UVaelPfuhlnatterData>();
	const FVector Wanted = Direction.GetSafeNormal2D();
	if (Wanted.IsNearlyZero())
	{
		return;
	}
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	const float Radius = FMath::Max(Data->MinTurnRadius * GetMesh()->GetComponentScale().Z, 1.0f);
	const float MaxTurn = FMath::Min(FMath::DegreesToRadians(Data->TurnRate), FMath::Max(Speed, Data->CrawlSpeed * 0.35f) / Radius) * DeltaSeconds;
	const float Angle = FMath::Atan2(FVector::CrossProduct(Forward, Wanted).Z, FVector::DotProduct(Forward, Wanted));
	const float Turn = FMath::Clamp(Angle, -MaxTurn, MaxTurn);
	const FVector Heading = Forward.RotateAngleAxis(FMath::RadiansToDegrees(Turn), FVector::UpVector);
	SetActorRotation(Heading.Rotation());
	const float Sharpness = FMath::Clamp(FMath::Abs(Angle) / PI, 0.0f, 1.0f);
	Super::MoveInDirection(Heading, Speed * FMath::Lerp(1.0f, 0.55f, Sharpness));
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
		CoilEntry = GetNeckLocation();
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
		if (StrikeCooldown <= 0.0f && (bNextSpit || !Data->bCanConstrict) && Distance >= Data->SpitRange.X && Distance <= Data->SpitRange.Y && !IsBlinded())
		{
			EnterState(EVaelSnakeState::Spit);
		}
		else if (StrikeCooldown <= 0.0f && Data->bCanConstrict && (!bNextSpit || Distance < Data->SpitRange.X) && Distance <= Data->RushRange && !IsBlinded())
		{
			EnterState(EVaelSnakeState::Rush);
		}
		else if (StrikeCooldown <= 0.0f && (bNextSpit || !Data->bCanConstrict) && Distance < Data->SpitRange.X)
		{
			// Too close to spit: it curves away until it has room, then rears up
			const FVector Away = -ToPrey + FVector::CrossProduct(FVector::UpVector, ToPrey) * CircleSide * 0.6f;
			Slither(Away, Data->CrawlSpeed, DeltaSeconds);
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
		// The head winds up round the prey and the body follows its path into coils (the squeezing waves are laid on in
		// LayBodyOnPath); it eases into the wrap and out of it, like a body getting going and settling
		const float Linear = FMath::Clamp(StateTime / FMath::Max(Data->WrapTime, 0.05f), 0.0f, 1.0f);
		const float Wound = Linear * Linear * (3.0f - 2.0f * Linear) * 0.6f + Linear * 0.4f;
		const float Radius = Player->GetCapsuleComponent()->GetScaledCapsuleRadius() + Data->CoilGap * Data->BodyScale;
		const float Angle = CoilStartAngle + CoilSense * Wound * Data->CoilTurns * 2.0f * PI;
		// At the top the head draws back a little from its prey and looks it in the eyes, swaying slowly
		const float Back = FMath::SmoothStep(Data->WrapTime, Data->WrapTime + 0.45f, StateTime);
		const float Sway = FMath::Sin(StateTime * 1.4f) * 0.12f * Back;
		const FVector OnCoil = CoilCentre + FVector(FMath::Cos(Angle + Sway), FMath::Sin(Angle + Sway), 0.0f) * (Radius + Data->HeadGap * Data->BodyScale * Back);
		// From where its head was onto the coil, not in one jump
		const float Join = FMath::SmoothStep(0.0f, 0.2f, StateTime);
		Lead = FMath::Lerp(CoilEntry, OnCoil, Join);
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
		// From its hunting arch up into the cobra pose and back down to it
		Rear = StateTime < Down ? FMath::Lerp(Data->HeadRaise, 1.0f, FMath::SmoothStep(0.0f, 1.0f, StateTime / FMath::Max(Up, 0.05f)))
			: FMath::Lerp(1.0f, Data->HeadRaise, FMath::SmoothStep(Down, End, StateTime));
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
			Rear = Data->HeadRaise;
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
		// Slow: turning on the spot swings the neck round and the body behind would fold into a hairpin
		const float Step = 70.0f * DeltaSeconds;
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
		// Let go: the coils sink only slowly, so it crawls out of them along the spiral instead of lying flat in a knot
		// of crossed coils; they are gone from the path once its tail has passed them
		for (float& Lift : PathLift)
		{
			Lift = FMath::FInterpTo(Lift, 0.0f, DeltaSeconds, 0.35f);
		}
	}
	if (!IsDead())
	{
		AddPathPoint(Lead, LeadLift);

		// Breathing, and lying still the waves slowly shift along the body as it settles
		Breath += DeltaSeconds * 0.9f;
		if (GetVelocity().Size2D() < 20.0f && (State == EVaelSnakeState::Lurk || State == EVaelSnakeState::Hunt))
		{
			IdleShift += DeltaSeconds * 0.22f;
		}
	}
	LayBodyOnPath();

	// The head always looks at its prey, whichever way it crawls; coiled, at its face; lying still, it looks about slowly
	const AActor* Watched = Held.IsValid() ? static_cast<AActor*>(Held.Get()) : Prey.Get();
	const bool bWatch = Watched != nullptr && !IsDead() && State != EVaelSnakeState::Lurk;
	const bool bLookAbout = !bWatch && !IsDead() && State == EVaelSnakeState::Lurk;
	LookWeight = FMath::FInterpTo(LookWeight, bWatch ? 1.0f : (bLookAbout ? 0.6f : 0.0f), DeltaSeconds, 4.0f);
	if (Watched != nullptr && bWatch)
	{
		LookPoint = Watched->GetActorLocation() + FVector(0.0f, 0.0f, Held.IsValid() ? 70.0f : 0.0f);
	}
	else if (bLookAbout)
	{
		const float Glance = FMath::Sin(Breath * 0.35f) * 40.0f + FMath::Sin(Breath * 0.9f) * 10.0f;
		LookPoint = GetNeckLocation() + GetActorForwardVector().RotateAngleAxis(Glance, FVector::UpVector) * 400.0f + FVector(0.0f, 0.0f, 30.0f);
	}
	// Crawling the head stays nearly level; reared or coiled it may look down or up at its prey
	const bool bHeadFree = State == EVaelSnakeState::Spit || State == EVaelSnakeState::Constrict;
	// Boris 2026-10-29: it knows where the players are without staring at them; turning the head on the neck stretched
	// the modelled neck. Only a slow glance about while lying still; reared to spit, the whole front faces the prey
	const float HeadTurn = bLookAbout ? LookWeight * 0.5f : 0.0f;
	Animator->SetLookAt(GetMesh()->GetComponentTransform().InverseTransformPosition(LookPoint), Data->HeadLookAngle, HeadTurn, bHeadFree ? 55.0f : Data->HeadLevelPitch);

	// Crest and fins lie flat while it crawls; coiled round prey the crown opens again; reared to spit, everything
	// spreads and quivers
	const bool bReared = State == EVaelSnakeState::Spit && Rear > 0.3f;
	const float DrawTarget = State == EVaelSnakeState::Constrict || bReared ? 0.0f : 1.0f;
	FinDraw = FMath::FInterpTo(FinDraw, DrawTarget, DeltaSeconds, bReared ? 8.0f : 5.0f);
	FinOpen = FMath::FInterpTo(FinOpen, bReared ? 1.0f : (State == EVaelSnakeState::Constrict ? 0.5f : 0.0f), DeltaSeconds, bReared ? 6.0f : 4.0f);
	// Boris 2026-10-29: every fin stays as modelled, spread and full size (folded and drawn in, the skin at their roots
	// shrank and swelled with them); the fins sway calmly in a wave running down the body, the crown petals each apart
	for (int32 Index = 0; Index < FinBones.Num(); ++Index)
	{
		FinScales[Index] = 1.0f;
		FinOpens[Index] = 1.0f;
	}
	const float Sway = IsDead() ? 0.0f : Data->FinSwayAngle * (bReared ? 0.4f : 1.0f);
	Animator->SetBoneScales(FinBones, FinScales, FinOpens, Data->TrembleAngle * FinOpen * (bReared ? 1.0f : 0.0f), GetWorld()->GetTimeSeconds(), Sway, &FinPhases);

	// The throat glows in the poison's colour before it spits
	if (SkinMaterial != nullptr)
	{
		SkinMaterial->SetScalarParameterValue(TEXT("GlutStaerke"), ThroatGlow * 1.8f);
		SkinMaterial->SetVectorParameterValue(TEXT("GlutFarbe"), Data->PoisonColor);
		// Only the throat glows: the yellow belly scales round a point below the neck top (the material masks the rest);
		// the pale crown skin left over the neck is tinted to the body's colour round the same place while crawling
		const float Scale = GetMesh()->GetComponentScale().Z;
		const FVector NeckTop = GetMesh()->GetSocketLocation(SpineBones.Num() > 1 ? SpineBones.Last(1) : NAME_None);
		SkinMaterial->SetVectorParameterValue(TEXT("KehleOrt"), FLinearColor(NeckTop - FVector(0.0f, 0.0f, 10.0f * Scale)));
		SkinMaterial->SetScalarParameterValue(TEXT("KehleRadius"), Data->ThroatRadius * Scale);
		// Round the head nothing is hidden: the crown stands
		SkinMaterial->SetVectorParameterValue(TEXT("KopfOrt"), FLinearColor(GetMesh()->GetSocketLocation(TEXT("kopf"))));
		SkinMaterial->SetScalarParameterValue(TEXT("KopfRadius"), Data->CrownRadius * Scale);
		// Membranes drawn in fade away (M_Pfuhlnatter_Koerper), so no stretched skin is left where they fold
		SkinMaterial->SetScalarParameterValue(TEXT("FlossenVerbergen"), 0.0f);
	}
	if (State != EVaelSnakeState::Spit)
	{
		// Hunting it carries its head a little raised, its neck in a low arch; lying still, almost flat
		const bool bHunting = State == EVaelSnakeState::Hunt || State == EVaelSnakeState::Rush || State == EVaelSnakeState::Wallow;
		Rear = FMath::FInterpTo(Rear, bHunting ? Data->HeadRaise : (State == EVaelSnakeState::Lurk ? Data->HeadRaise * 0.35f : 0.0f), DeltaSeconds, 3.0f);
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
