// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelPfuhlnatter.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Creatures/VaelCreatureData.h"
#include "Creatures/VaelSequenceAnimInstance.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
	/** Seconds for blends between animations */
	constexpr float SnakePoseBlend = 0.35f;

	/** In the bite animation (0.8 s at the model's pace) the head draws back, lunges from SnakeBiteLungeStart and strikes at SnakeBiteStrike */
	constexpr float SnakeBiteLungeStart = 0.3f;
	constexpr float SnakeBiteStrike = 0.4f;
}

AVaelPfuhlnatter::AVaelPfuhlnatter()
{
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	GetMesh()->SetAnimInstanceClass(UVaelSequenceAnimInstance::StaticClass());
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
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
	// The capsule sits on the front of the body, where it is hit and bites from; the rest trails behind
	GetMesh()->SetRelativeLocationAndRotation(FVector(-Data->FrontOffset * Data->BodyScale, 0.0f, -GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), FRotator::ZeroRotator);
	GetMesh()->InitAnim(true);

	// A long body turns slowly and gets going slowly
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->RotationRate = FRotator(0.0f, Data->TurnRate, 0.0f);
	Movement->MaxAcceleration = Data->Acceleration;
	Animator = Cast<UVaelSequenceAnimInstance>(GetMesh()->GetAnimInstance());

	CrawlSequence = Data->CrawlAnimation.LoadSynchronous();
	RearSequence = Data->RearAnimation.LoadSynchronous();
	ThreatenSequence = Data->ThreatenAnimation.LoadSynchronous();
	BiteSequence = Data->BiteAnimation.LoadSynchronous();
	LowerSequence = Data->LowerAnimation.LoadSynchronous();
	DeathSequence = Data->DeathAnimation.LoadSynchronous();

	EnterState(EVaelSnakeState::Lurk);
}

void AVaelPfuhlnatter::PlayAnimation(UAnimSequence* Sequence, bool bLoop, float BlendTime, float Rate)
{
	if (Animator != nullptr)
	{
		Animator->Play(Sequence, bLoop, BlendTime, Rate);
	}
}

void AVaelPfuhlnatter::EnterState(EVaelSnakeState NewState)
{
	State = NewState;
	StateTime = 0.0f;
	StateDuration = 0.0f;

	switch (NewState)
	{
	case EVaelSnakeState::Lurk:
		// Lying flat and still: the crawl's first pose, held
		PlayAnimation(CrawlSequence, true, SnakePoseBlend, 0.0f);
		Prey = nullptr;
		break;

	case EVaelSnakeState::Crawl:
		PlayAnimation(CrawlSequence, true, SnakePoseBlend, 0.0f);
		break;

	case EVaelSnakeState::Rear:
		PlayAnimation(RearSequence, false, 0.2f);
		StateDuration = RearSequence != nullptr ? RearSequence->GetPlayLength() : 1.0f;
		break;

	case EVaelSnakeState::Threaten:
		PlayAnimation(ThreatenSequence, true, SnakePoseBlend);
		break;

	case EVaelSnakeState::Bite:
		if (Animator != nullptr && BiteSequence != nullptr)
		{
			Animator->Play(BiteSequence, false, 0.1f, 1.0f, true);
		}
		StateDuration = BiteSequence != nullptr ? BiteSequence->GetPlayLength() : 0.8f;
		bBitten = false;
		break;

	case EVaelSnakeState::Lower:
		PlayAnimation(LowerSequence, false, 0.2f);
		StateDuration = LowerSequence != nullptr ? LowerSequence->GetPlayLength() : 0.8f;
		break;
	}
}

void AVaelPfuhlnatter::TurnTowards(const FVector& Location, float DeltaSeconds)
{
	const FVector To = (Location - GetActorLocation()).GetSafeNormal2D();
	if (!To.IsNearlyZero())
	{
		const float Step = GetData<UVaelPfuhlnatterData>()->TurnRate * DeltaSeconds;
		const float Delta = FMath::Clamp(FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, To.Rotation().Yaw), -Step, Step);
		SetActorRotation(FRotator(0.0f, GetActorRotation().Yaw + Delta, 0.0f));
	}
}

void AVaelPfuhlnatter::TickBehavior(float DeltaSeconds)
{
	const UVaelPfuhlnatterData* Data = GetData<UVaelPfuhlnatterData>();
	StateTime += DeltaSeconds;
	BiteCooldown -= DeltaSeconds;

	float Distance = 0.0f;
	switch (State)
	{
	case EVaelSnakeState::Lurk:
		if (FindTarget(Data->AggroRange) != nullptr)
		{
			NoPreyTime = 0.0f;
			EnterState(EVaelSnakeState::Crawl);
		}
		break;

	case EVaelSnakeState::Crawl:
	{
		AActor* Target = FindTarget(Data->AggroRange, &Distance);
		Prey = Target;
		if (Target == nullptr)
		{
			// Without prey it crawls home and lies still there
			NoPreyTime += DeltaSeconds;
			const FVector ToHome = (HomeLocation - GetActorLocation()) * FVector(1.0f, 1.0f, 0.0f);
			if (NoPreyTime < Data->LoseInterestTime)
			{
				break;
			}
			if (ToHome.Size() > 120.0f)
			{
				MoveInDirection(ToHome.GetSafeNormal(), Data->CrawlSpeed);
			}
			else
			{
				EnterState(EVaelSnakeState::Lurk);
			}
			break;
		}
		NoPreyTime = 0.0f;
		if (Distance <= Data->RearRange)
		{
			FaceTowards(Target->GetActorLocation());
			EnterState(EVaelSnakeState::Rear);
			break;
		}
		MoveInDirection((Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D(), Data->CrawlSpeed);
		break;
	}

	case EVaelSnakeState::Rear:
		if (AActor* Target = FindTarget(Data->AggroRange))
		{
			TurnTowards(Target->GetActorLocation(), DeltaSeconds);
		}
		if (StateTime >= StateDuration)
		{
			BiteCooldown = FMath::Max(BiteCooldown, 0.3f);
			EnterState(EVaelSnakeState::Threaten);
		}
		break;

	case EVaelSnakeState::Threaten:
	{
		AActor* Target = FindTarget(Data->AggroRange, &Distance);
		Prey = Target;
		if (Target == nullptr || Distance > Data->RearRange * 1.4f)
		{
			// The prey got away: down again to crawl after it (or home)
			EnterState(EVaelSnakeState::Lower);
			break;
		}
		TurnTowards(Target->GetActorLocation(), DeltaSeconds);
		if (Distance <= Data->BiteRange && BiteCooldown <= 0.0f && !IsBlinded())
		{
			EnterState(EVaelSnakeState::Bite);
		}
		else if (Distance > Data->BiteRange * 0.85f)
		{
			// Edges closer while reared, slowly, so it never gets far from its strike
			MoveInDirection((Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D(), Data->RearedSpeed);
		}
		break;
	}

	case EVaelSnakeState::Bite:
	{
		AActor* Target = Prey.Get();
		const float Pace = FMath::Max(StateDuration / 0.8f, KINDA_SMALL_NUMBER);
		if (StateTime >= SnakeBiteLungeStart * Pace && StateTime < SnakeBiteStrike * Pace)
		{
			MoveInDirection(GetActorForwardVector(), Data->BiteLungeSpeed);
		}
		if (!bBitten && StateTime >= SnakeBiteStrike * Pace)
		{
			bBitten = true;
			++BiteCount;
			if (Target != nullptr && GetDistanceTo2D(Target) <= Data->BiteRange * 1.15f)
			{
				HitPlayer(Target, Data->BiteDamage, EVaelElement::Earth);
			}
		}
		if (StateTime >= StateDuration)
		{
			BiteCooldown = RandomInRange(Data->BiteInterval);
			EnterState(EVaelSnakeState::Threaten);
		}
		break;
	}

	case EVaelSnakeState::Lower:
		if (StateTime >= StateDuration)
		{
			NoPreyTime = 0.0f;
			EnterState(EVaelSnakeState::Crawl);
		}
		break;
	}
}

void AVaelPfuhlnatter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The waves run as fast as the body really moves, so it never slides over the ground
	if (State == EVaelSnakeState::Crawl && Animator != nullptr && Animator->GetCurrent() == CrawlSequence)
	{
		const UVaelPfuhlnatterData* Data = GetData<UVaelPfuhlnatterData>();
		const float Speed = GetVelocity().Size2D();
		Animator->SetRate(Speed < 20.0f ? 0.0f : FMath::Clamp(Speed / (Data->CrawlAnimationSpeed * Data->BodyScale), 0.3f, 2.0f));
	}
}

void AVaelPfuhlnatter::OnDamageTaken(float Damage, const FGameplayTagContainer& DamageTags)
{
	Super::OnDamageTaken(Damage, DamageTags);

	// A hit out of nowhere: it comes for whoever is near
	if (!IsDead() && State == EVaelSnakeState::Lurk)
	{
		EnterState(EVaelSnakeState::Crawl);
	}
}

void AVaelPfuhlnatter::Die()
{
	PlayAnimation(DeathSequence, false, 0.2f);
	Super::Die();
}
