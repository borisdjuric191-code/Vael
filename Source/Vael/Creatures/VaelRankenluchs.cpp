// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelRankenluchs.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Creatures/VaelCreatureData.h"
#include "Creatures/VaelSequenceAnimInstance.h"
#include "Player/VaelCharacter.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "World/VaelRegion.h"

namespace
{
	/** Part of the pounce animation (seconds) in which it is in the air; before it crouches, after it lands */
	constexpr float LeapStart = 0.17f;
	constexpr float LeapEnd = 0.55f;

	/** Seconds for blends between animations */
	constexpr float GaitBlend = 0.3f;
	constexpr float PoseBlend = 0.35f;

	/** How often the region's corruption is looked at, in seconds */
	constexpr float FormCheckInterval = 1.0f;
}

AVaelRankenluchs::AVaelRankenluchs()
{
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	GetMesh()->SetAnimInstanceClass(UVaelSequenceAnimInstance::StaticClass());
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
}

TSubclassOf<UVaelCreatureData> AVaelRankenluchs::GetDefaultDataClass() const
{
	return UVaelRankenluchsData::StaticClass();
}

void AVaelRankenluchs::BeginPlay()
{
	Super::BeginPlay();

	const UVaelRankenluchsData* Data = GetData<UVaelRankenluchsData>();
	USkeletalMesh* LynxMesh = Data->LuchsMesh.LoadSynchronous();
	if (LynxMesh == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("'%s': SK_Rankenluchs is missing, the lynx keeps its placeholder look"), *GetName());
		return;
	}
	GetBody()->SetVisibility(false);
	GetMesh()->SetSkeletalMesh(LynxMesh);
	GetMesh()->SetRelativeScale3D(FVector(Data->BodyScale));
	MeshBaseZ = -GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, MeshBaseZ - Data->AmbushSink * Data->BodyScale), FRotator::ZeroRotator);
	GetMesh()->InitAnim(true);

	// A cat eases into a run and runs in curves
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->RotationRate = FRotator(0.0f, Data->TurnRate, 0.0f);
	Movement->MaxAcceleration = Data->Acceleration;
	Movement->BrakingDecelerationWalking = Data->Braking;
	LastYaw = GetActorRotation().Yaw;
	Animator = Cast<UVaelSequenceAnimInstance>(GetMesh()->GetAnimInstance());

	IdleSequence = Data->IdleAnimation.LoadSynchronous();
	CamouflageSequence = Data->CamouflageAnimation.LoadSynchronous();
	WakeSequence = Data->WakeAnimation.LoadSynchronous();
	PounceSequence = Data->PounceAnimation.LoadSynchronous();
	WalkSequence = Data->WalkAnimation.LoadSynchronous();
	RunSequence = Data->RunAnimation.LoadSynchronous();

	for (int32 Slot = 0; Slot < GetMesh()->GetNumMaterials(); ++Slot)
	{
		Materials.Add(GetMesh()->CreateDynamicMaterialInstance(Slot));
	}

	// Starts in ambush, in its form already (no fade at the start)
	const AVaelRegion* Region = AVaelRegion::GetRegionAt(GetWorld(), GetActorLocation());
	ShadowTarget = ShadowForm = Region != nullptr && Region->GetCorruption() >= Data->ShadowCorruption ? 1.0f : 0.0f;
	FormCheckCountdown = FMath::FRandRange(0.0f, FormCheckInterval);
	Sunk = 1.0f;
	EnterState(EVaelLynxState::Ambush);
}

bool AVaelRankenluchs::IsHiddenFromAim() const
{
	return State == EVaelLynxState::Ambush || State == EVaelLynxState::Settle;
}

bool AVaelRankenluchs::IsCapturable() const
{
	return !IsDead() && CaptureEndTime > 0.0f && GetWorld()->GetTimeSeconds() < CaptureEndTime;
}

bool AVaelRankenluchs::IsLastOfPack() const
{
	const UVaelRankenluchsData* Data = GetData<UVaelRankenluchsData>();
	for (TActorIterator<AVaelRankenluchs> It(GetWorld()); It; ++It)
	{
		if (*It != this && !It->IsDead() && GetDistanceTo2D(*It) <= Data->PackRange)
		{
			return false;
		}
	}
	return true;
}

TArray<FVaelLootEntry> AVaelRankenluchs::GetLoot() const
{
	const UVaelRankenluchsData* Data = GetData<UVaelRankenluchsData>();
	return ShadowForm >= 0.5f ? Data->ShadowLoot : Data->LightLoot;
}

void AVaelRankenluchs::PlayAnimation(UAnimSequence* Sequence, bool bLoop, float BlendTime, float Rate)
{
	if (Animator != nullptr)
	{
		Animator->Play(Sequence, bLoop, BlendTime, Rate);
	}
}

void AVaelRankenluchs::EnterState(EVaelLynxState NewState)
{
	const UVaelRankenluchsData* Data = GetData<UVaelRankenluchsData>();
	State = NewState;
	StateTime = 0.0f;
	StateDuration = 0.0f;

	switch (NewState)
	{
	case EVaelLynxState::Ambush:
		PlayAnimation(CamouflageSequence, true, PoseBlend);
		Prey = nullptr;
		break;

	case EVaelLynxState::Waking:
		PlayAnimation(WakeSequence, false, 0.15f);
		StateDuration = WakeSequence != nullptr ? WakeSequence->GetPlayLength() : 1.0f;
		AlarmPack();
		break;

	case EVaelLynxState::Pounce:
		if (Animator != nullptr && PounceSequence != nullptr)
		{
			Animator->Play(PounceSequence, false, 0.1f, 1.0f, true);
		}
		StateDuration = PounceSequence != nullptr ? PounceSequence->GetPlayLength() : 0.8f;
		bLeapHit = false;
		bBitten = false;
		break;

	case EVaelLynxState::Retreat:
		StateDuration = RandomInRange(Data->RetreatTime);
		RetreatSide = FMath::RandBool() ? 1.0f : -1.0f;
		break;

	case EVaelLynxState::Settle:
		PlayAnimation(CamouflageSequence, true, 0.8f);
		StateDuration = 0.9f;
		break;

	case EVaelLynxState::Flee:
	{
		// Away from the players, to a spot it can hide in
		FVector Away = FVector::ZeroVector;
		ForEachActivePlayer([&](AVaelCharacter* Player)
		{
			Away += (GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D();
		});
		Away = Away.IsNearlyZero() ? GetActorForwardVector() : Away.GetSafeNormal2D();
		FleeGoal = GetActorLocation() + Away.RotateAngleAxis(FMath::FRandRange(-30.0f, 30.0f), FVector::UpVector) * Data->FleeDistance;
		bHasFled = true;
		break;
	}

	default:
		break;
	}
}

void AVaelRankenluchs::ProwlTowards(const FVector& GroupCentre, float GroupDistance)
{
	const UVaelRankenluchsData* Data = GetData<UVaelRankenluchsData>();
	if (State == EVaelLynxState::Ambush && WakeCountdown < 0.0f && GroupDistance < Data->AggroRange)
	{
		WakeUp();
	}
	else if (State == EVaelLynxState::Return)
	{
		HomeLocation = GroupCentre;
	}
}

void AVaelRankenluchs::WakeUp(float Delay)
{
	if (State != EVaelLynxState::Ambush && State != EVaelLynxState::Settle)
	{
		return;
	}
	if (Delay <= 0.0f)
	{
		EnterState(EVaelLynxState::Waking);
	}
	else if (WakeCountdown < 0.0f || Delay < WakeCountdown)
	{
		WakeCountdown = Delay;
	}
}

void AVaelRankenluchs::AlarmPack()
{
	const UVaelRankenluchsData* Data = GetData<UVaelRankenluchsData>();
	for (TActorIterator<AVaelRankenluchs> It(GetWorld()); It; ++It)
	{
		if (*It != this && !It->IsDead() && GetDistanceTo2D(*It) <= Data->PackRange)
		{
			It->WakeUp(RandomInRange(Data->PackWakeDelay));
		}
	}
}

int32 AVaelRankenluchs::GetPackSide() const
{
	// Order of the pack by name: the first goes straight in, the others alternate left and right
	const UVaelRankenluchsData* Data = GetData<UVaelRankenluchsData>();
	int32 Before = 0;
	for (TActorIterator<AVaelRankenluchs> It(GetWorld()); It; ++It)
	{
		if (*It != this && !It->IsDead() && GetDistanceTo2D(*It) <= Data->PackRange && It->GetFName().LexicalLess(GetFName()))
		{
			++Before;
		}
	}
	return Before == 0 ? 0 : (Before % 2 == 1 ? -(Before + 1) / 2 : Before / 2);
}

void AVaelRankenluchs::Move(const FVector& Direction, float Speed)
{
	MoveInDirection(Direction, Speed);
}

void AVaelRankenluchs::UpdateGait(float DeltaSeconds)
{
	const UVaelRankenluchsData* Data = GetData<UVaelRankenluchsData>();
	const float Yaw = GetActorRotation().Yaw;
	const float YawRate = FMath::FindDeltaAngleDegrees(LastYaw, Yaw) / FMath::Max(DeltaSeconds, KINDA_SMALL_NUMBER);
	LastYaw = Yaw;

	const bool bMovingState = State == EVaelLynxState::Hunt || State == EVaelLynxState::Retreat
		|| State == EVaelLynxState::Return || State == EVaelLynxState::Flee;
	const float Speed = GetVelocity().Size2D();

	// Leans into curves the faster it runs (a right turn drops the right side)
	const float LeanTarget = bMovingState ? FMath::Clamp(YawRate * 0.045f * Speed / FMath::Max(Data->MoveSpeed, 1.0f), -Data->LeanAngle, Data->LeanAngle) : 0.0f;
	Lean = FMath::FInterpTo(Lean, LeanTarget, DeltaSeconds, 6.0f);
	GetMesh()->SetRelativeRotation(FRotator(0.0f, 0.0f, Lean));

	if (!bMovingState || Animator == nullptr)
	{
		return;
	}
	// Walk, run or stand after the real speed, so starts and stops ease in and out
	const float Middle = (Data->WalkSpeed + Data->MoveSpeed) * 0.5f;
	bRunning = bRunning ? Speed > Middle * 0.85f : Speed > Middle * 1.1f;
	if (Speed < 35.0f)
	{
		PlayAnimation(IdleSequence, true, PoseBlend);
		return;
	}
	UAnimSequence* Gait = bRunning ? RunSequence : WalkSequence;
	const float Rate = FMath::Clamp(Speed / ((bRunning ? Data->RunAnimationSpeed : Data->WalkAnimationSpeed) * Data->BodyScale), 0.35f, 1.8f);
	if (Animator->GetCurrent() == WalkSequence || Animator->GetCurrent() == RunSequence)
	{
		Animator->PlayInStep(Gait, GaitBlend, Rate);
	}
	else
	{
		PlayAnimation(Gait, true, GaitBlend, Rate);
	}
	Animator->SetRate(Rate);
}

void AVaelRankenluchs::TurnTowards(const FVector& Location, float DeltaSeconds)
{
	const FVector To = (Location - GetActorLocation()).GetSafeNormal2D();
	if (!To.IsNearlyZero())
	{
		const float Step = GetData<UVaelRankenluchsData>()->TurnRate * DeltaSeconds;
		const float Delta = FMath::Clamp(FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, To.Rotation().Yaw), -Step, Step);
		SetActorRotation(FRotator(0.0f, GetActorRotation().Yaw + Delta, 0.0f));
	}
}

void AVaelRankenluchs::TickBehavior(float DeltaSeconds)
{
	const UVaelRankenluchsData* Data = GetData<UVaelRankenluchsData>();
	StateTime += DeltaSeconds;
	PounceCooldown -= DeltaSeconds;

	switch (State)
	{
	case EVaelLynxState::Ambush:
	{
		if (WakeCountdown >= 0.0f)
		{
			WakeCountdown -= DeltaSeconds;
			if (WakeCountdown < 0.0f)
			{
				EnterState(EVaelLynxState::Waking);
			}
			break;
		}
		float Distance = 0.0f;
		if (FindTarget(Data->WakeRange, &Distance) != nullptr)
		{
			EnterState(EVaelLynxState::Waking);
		}
		break;
	}

	case EVaelLynxState::Waking:
	{
		if (AActor* Target = FindTarget(Data->AggroRange))
		{
			// Turns towards the prey while it gets up
			const FVector To = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
			SetActorRotation(FMath::RInterpTo(GetActorRotation(), To.Rotation(), DeltaSeconds, 6.0f));
		}
		if (StateTime >= StateDuration)
		{
			PounceCooldown = 0.4f;
			EnterState(EVaelLynxState::Hunt);
		}
		break;
	}

	case EVaelLynxState::Hunt:
	{
		float Distance = 0.0f;
		AActor* Target = FindTarget(Data->AggroRange, &Distance);
		Prey = Target;
		if (Target == nullptr)
		{
			NoPreyTime += DeltaSeconds;
			if (NoPreyTime >= Data->LoseInterestTime)
			{
				EnterState(EVaelLynxState::Return);
			}
			break;
		}
		NoPreyTime = 0.0f;

		if (Distance >= Data->PounceRange.X && Distance <= Data->PounceRange.Y && PounceCooldown <= 0.0f && !IsBlinded())
		{
			FaceTowards(Target->GetActorLocation());
			LeapDirection = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
			EnterState(EVaelLynxState::Pounce);
			break;
		}

		// Comes in on its side of the pack, a little outside pouncing distance, then closes in
		const FVector FromPrey = (GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
		const FVector Side = FromPrey.RotateAngleAxis(Data->FlankAngle * GetPackSide(), FVector::UpVector);
		const float Wanted = PounceCooldown > 0.0f ? Data->PounceRange.Y * 0.9f : Data->PounceRange.X * 1.4f;
		const FVector Goal = Target->GetActorLocation() + Side * Wanted;
		const FVector ToGoal = (Goal - GetActorLocation()) * FVector(1.0f, 1.0f, 0.0f);
		if (ToGoal.Size() > 60.0f)
		{
			Move(ToGoal.GetSafeNormal(), ToGoal.Size() > 400.0f ? Data->MoveSpeed : Data->WalkSpeed);
		}
		else
		{
			// Waits watching for its moment, turning with the prey
			TurnTowards(Target->GetActorLocation(), DeltaSeconds);
		}
		break;
	}

	case EVaelLynxState::Pounce:
	{
		AActor* Target = Prey.Get();
		if (StateTime >= LeapStart && StateTime <= LeapEnd)
		{
			GetCharacterMovement()->Velocity = LeapDirection * Data->PounceSpeed;
			MoveInDirection(LeapDirection, Data->PounceSpeed);
			if (!bLeapHit && Target != nullptr && GetDistanceTo2D(Target) < GetCapsuleComponent()->GetScaledCapsuleRadius() + 70.0f)
			{
				bLeapHit = true;
				HitPlayer(Target, Data->PounceDamage, EVaelElement::Earth);
			}
		}
		else if (StateTime > LeapEnd && !bBitten)
		{
			bBitten = true;
			if (Target != nullptr && GetDistanceTo2D(Target) <= Data->BiteRange)
			{
				FaceTowards(Target->GetActorLocation());
				HitPlayer(Target, Data->BiteDamage, EVaelElement::Earth);
			}
		}
		if (StateTime >= StateDuration)
		{
			PounceCooldown = RandomInRange(Data->PounceInterval);
			EnterState(EVaelLynxState::Retreat);
		}
		break;
	}

	case EVaelLynxState::Retreat:
	{
		AActor* Target = Prey.Get();
		if (Target == nullptr || StateTime >= StateDuration)
		{
			EnterState(EVaelLynxState::Hunt);
			break;
		}
		// Backs off and slinks sideways round the prey
		const FVector FromPrey = (GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
		const float Distance = GetDistanceTo2D(Target);
		const FVector Around = FVector::CrossProduct(FVector::UpVector, FromPrey) * RetreatSide;
		const FVector Direction = (Around + FromPrey * (Distance < Data->RetreatDistance ? 1.0f : 0.0f)).GetSafeNormal();
		Move(Direction, Data->WalkSpeed * 1.4f);
		break;
	}

	case EVaelLynxState::Return:
	{
		if (FindTarget(Data->WakeRange) != nullptr)
		{
			EnterState(EVaelLynxState::Hunt);
			break;
		}
		const FVector ToHome = (HomeLocation - GetActorLocation()) * FVector(1.0f, 1.0f, 0.0f);
		if (ToHome.Size() > 80.0f)
		{
			Move(ToHome.GetSafeNormal(), Data->WalkSpeed);
		}
		else
		{
			EnterState(EVaelLynxState::Settle);
		}
		break;
	}

	case EVaelLynxState::Settle:
		if (StateTime >= StateDuration)
		{
			NoPreyTime = 0.0f;
			EnterState(EVaelLynxState::Ambush);
		}
		break;

	case EVaelLynxState::Flee:
	{
		const FVector ToGoal = (FleeGoal - GetActorLocation()) * FVector(1.0f, 1.0f, 0.0f);
		if (ToGoal.Size() > 80.0f && StateTime < 4.0f)
		{
			Move(ToGoal.GetSafeNormal(), Data->MoveSpeed);
		}
		else
		{
			// Hides where it is; this is where the Tamer can find and bind it
			HomeLocation = GetActorLocation();
			CaptureEndTime = GetWorld()->GetTimeSeconds() + Data->CaptureWindow;
			EnterState(EVaelLynxState::Settle);
		}
		break;
	}
	}
}

void AVaelRankenluchs::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const UVaelRankenluchsData* Data = GetData<UVaelRankenluchsData>();

	// Sunk into the undergrowth while lying in ambush; rises with the waking animation
	float SunkTarget = 0.0f;
	if (State == EVaelLynxState::Ambush || State == EVaelLynxState::Settle)
	{
		SunkTarget = 1.0f;
	}
	else if (State == EVaelLynxState::Waking)
	{
		SunkTarget = 1.0f - FMath::Clamp(StateTime / FMath::Max(StateDuration * 0.6f, 0.1f), 0.0f, 1.0f);
	}
	UpdateGait(DeltaSeconds);

	Sunk = FMath::FInterpTo(Sunk, SunkTarget, DeltaSeconds, State == EVaelLynxState::Waking ? 20.0f : 3.0f);
	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, MeshBaseZ - Data->AmbushSink * Data->BodyScale * Sunk));

	// Light or shadow form after the corruption of its region
	FormCheckCountdown -= DeltaSeconds;
	if (FormCheckCountdown <= 0.0f)
	{
		FormCheckCountdown = FormCheckInterval;
		const AVaelRegion* Region = AVaelRegion::GetRegionAt(GetWorld(), GetActorLocation());
		ShadowTarget = Region != nullptr && Region->GetCorruption() >= Data->ShadowCorruption ? 1.0f : 0.0f;
	}
	if (!bFormShown || !FMath::IsNearlyEqual(ShadowForm, ShadowTarget))
	{
		bFormShown = true;
		ShadowForm = FMath::FInterpConstantTo(ShadowForm, ShadowTarget, DeltaSeconds, 1.0f / FMath::Max(Data->FormChangeTime, KINDA_SMALL_NUMBER));
		for (UMaterialInstanceDynamic* Material : Materials)
		{
			if (Material != nullptr)
			{
				Material->SetScalarParameterValue(TEXT("Verderbnis"), ShadowForm);
			}
		}
	}
}

void AVaelRankenluchs::OnDamageTaken(float Damage, const FGameplayTagContainer& DamageTags)
{
	Super::OnDamageTaken(Damage, DamageTags);

	// Option A of Boris: the last of the pack, badly hurt, runs off and hides
	if (!IsDead() && !bHasFled && State != EVaelLynxState::Flee && GetMaxHealth() > 0.0f
		&& GetHealth() / GetMaxHealth() < GetData<UVaelRankenluchsData>()->FleeHealthShare && IsLastOfPack())
	{
		EnterState(EVaelLynxState::Flee);
		return;
	}

	// A hit out of nowhere wakes it at once, and the pack with it
	if (!IsDead() && (State == EVaelLynxState::Ambush || State == EVaelLynxState::Settle || State == EVaelLynxState::Return))
	{
		if (State == EVaelLynxState::Return)
		{
			AlarmPack();
			EnterState(EVaelLynxState::Hunt);
		}
		else
		{
			EnterState(EVaelLynxState::Waking);
		}
	}
}

void AVaelRankenluchs::Die()
{
	// Sinks down as if lying in ambush, then the base removes it
	PlayAnimation(CamouflageSequence, false, 0.3f);
	Super::Die();
}
