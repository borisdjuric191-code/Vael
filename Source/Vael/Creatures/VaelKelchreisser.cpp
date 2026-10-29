// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelKelchreisser.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimSequence.h"
#include "Combat/VaelAttributeSet.h"
#include "Combat/VaelCombatStatics.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Creatures/VaelCreatureData.h"
#include "Creatures/VaelSequenceAnimInstance.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Magic/VaelElementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/VaelCharacter.h"
#include "World/VaelLocalWeather.h"
#include "World/VaelRegion.h"

namespace
{
	/** Seconds into the attack animations (Tools/Kelchreisser/06_animationen.py) where the blow lands */
	constexpr float ClawHitTime = 0.6f;
	constexpr float SlapHitTime = 0.62f;
	constexpr float VineHitTime = 0.66f;
	constexpr float ScreamPeakTime = 0.55f;

	/** Seconds for blends between animations */
	constexpr float GaitBlend = 0.35f;
	constexpr float PoseBlend = 0.3f;

	/** How often it listens, in seconds */
	constexpr float ListenInterval = 0.25f;
}

AVaelKelchreisser::AVaelKelchreisser()
{
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	GetMesh()->SetAnimInstanceClass(UVaelSequenceAnimInstance::StaticClass());
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	// A carcass this big stays a moment
	CorpseDuration = 2.2f;
}

TSubclassOf<UVaelCreatureData> AVaelKelchreisser::GetDefaultDataClass() const
{
	return UVaelKelchreisserData::StaticClass();
}

void AVaelKelchreisser::BeginPlay()
{
	Super::BeginPlay();

	const UVaelKelchreisserData* Data = GetData<UVaelKelchreisserData>();

	// Stronger the more corrupted its region is when it appears
	CorruptionStrength = 1.0f + GetCorruptionShare() * Data->CorruptionStrength;
	if (CorruptionStrength > 1.0f)
	{
		const float Health = GetMaxHealth() * CorruptionStrength;
		UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent();
		AbilitySystem->SetNumericAttributeBase(UVaelAttributeSet::GetMaxHealthAttribute(), Health);
		AbilitySystem->SetNumericAttributeBase(UVaelAttributeSet::GetHealthAttribute(), Health);
	}

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->RotationRate = FRotator(0.0f, Data->TurnRate, 0.0f);
	Movement->MaxAcceleration = Data->Acceleration;
	Movement->BrakingDecelerationWalking = Data->Acceleration;

	USkeletalMesh* KelchMesh = Data->KelchMesh.LoadSynchronous();
	if (KelchMesh == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("'%s': SK_Kelchreisser is missing, it keeps its placeholder look"), *GetName());
		EnterState(EVaelKelchreisserState::Territory);
		return;
	}
	GetBody()->SetVisibility(false);
	GetMesh()->SetSkeletalMesh(KelchMesh);
	MeshBaseZ = -GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, MeshBaseZ), FRotator::ZeroRotator);
	GetMesh()->InitAnim(true);
	Animator = Cast<UVaelSequenceAnimInstance>(GetMesh()->GetAnimInstance());

	IdleSequence = Data->IdleAnimation.LoadSynchronous();
	GallopSequence = Data->GallopAnimation.LoadSynchronous();
	WalkSequence = Data->WalkAnimation.LoadSynchronous();
	SlapSequence = Data->SlapAnimation.LoadSynchronous();
	ClawSequence = Data->ClawAnimation.LoadSynchronous();
	VineSequence = Data->VineBiteAnimation.LoadSynchronous();
	ScreamSequence = Data->ScreamAnimation.LoadSynchronous();

	for (int32 Slot = 0; Slot < GetMesh()->GetNumMaterials(); ++Slot)
	{
		Materials.Add(GetMesh()->CreateDynamicMaterialInstance(Slot));
	}
	RefreshNidGlow();

	RoamGoal = HomeLocation;
	RoamPause = FMath::FRandRange(0.0f, 4.0f);
	ListenCountdown = FMath::FRandRange(0.0f, ListenInterval);
	EnterState(EVaelKelchreisserState::Territory);
}

float AVaelKelchreisser::GetCorruptionShare() const
{
	const AVaelRegion* Region = AVaelRegion::GetRegionAt(GetWorld(), GetActorLocation());
	return Region != nullptr ? FMath::Clamp(Region->GetCorruption() / 100.0f, 0.0f, 1.0f) : 0.0f;
}

void AVaelKelchreisser::RefreshNidGlow()
{
	const UVaelKelchreisserData* Data = GetData<UVaelKelchreisserData>();
	const float Glow = bHitFlash ? 1.0f : FMath::Lerp(Data->NidGlow.X, Data->NidGlow.Y, GetCorruptionShare());
	const FLinearColor Color = bHitFlash ? FLinearColor(1.0f, 0.92f, 0.8f) : FLinearColor(0.55f, 0.12f, 1.0f);
	for (UMaterialInstanceDynamic* Material : Materials)
	{
		if (Material != nullptr)
		{
			Material->SetScalarParameterValue(TEXT("MarkStaerke"), Glow);
			Material->SetVectorParameterValue(TEXT("MarkFarbe"), Color);
		}
	}
}

void AVaelKelchreisser::OnBodyColorShown(const FLinearColor& Color, bool bInHitFlash)
{
	if (bHitFlash != bInHitFlash)
	{
		bHitFlash = bInHitFlash;
		RefreshNidGlow();
	}
}

float AVaelKelchreisser::GetStatusTextHeight() const
{
	// Above the shoulder hump, under the vines
	return GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + GetData<UVaelKelchreisserData>()->StatusBarAbove;
}

float AVaelKelchreisser::GetOutgoingDamageMultiplier() const
{
	return Super::GetOutgoingDamageMultiplier() * CorruptionStrength;
}

void AVaelKelchreisser::PlayAnimation(UAnimSequence* Sequence, bool bLoop, float BlendTime, float Rate, bool bRestart)
{
	if (Animator != nullptr && Sequence != nullptr)
	{
		Animator->Play(Sequence, bLoop, BlendTime, Rate, bRestart);
	}
}

void AVaelKelchreisser::EnterState(EVaelKelchreisserState NewState)
{
	const UVaelKelchreisserData* Data = GetData<UVaelKelchreisserData>();
	State = NewState;
	StateTime = 0.0f;
	StateDuration = 0.0f;
	bStruck = false;

	const auto Length = [](const UAnimSequence* Sequence, float Fallback)
	{
		return Sequence != nullptr ? Sequence->GetPlayLength() : Fallback;
	};

	switch (NewState)
	{
	case EVaelKelchreisserState::Listen:
		PlayAnimation(IdleSequence, true, PoseBlend);
		StateDuration = Data->ListenTime;
		break;

	case EVaelKelchreisserState::Scream:
		PlayAnimation(ScreamSequence, false, 0.2f, 1.0f, true);
		StateDuration = Length(ScreamSequence, 2.0f);
		bHasScreamed = true;
		break;

	case EVaelKelchreisserState::Claw:
		++AttackCount;
		PlayAnimation(ClawSequence, false, 0.15f, 1.0f, true);
		StateDuration = Length(ClawSequence, 1.0f);
		ClawReadyTime = GetWorld()->GetTimeSeconds() + RandomInRange(Data->ClawInterval);
		break;

	case EVaelKelchreisserState::Slap:
		++AttackCount;
		PlayAnimation(SlapSequence, false, 0.15f, 1.0f, true);
		StateDuration = Length(SlapSequence, 1.2f);
		SlapReadyTime = GetWorld()->GetTimeSeconds() + RandomInRange(Data->SlapInterval);
		break;

	case EVaelKelchreisserState::VineBite:
		++AttackCount;
		PlayAnimation(VineSequence, false, 0.15f, 1.0f, true);
		StateDuration = Length(VineSequence, 1.4f);
		VineReadyTime = GetWorld()->GetTimeSeconds() + RandomInRange(Data->VineInterval);
		break;

	case EVaelKelchreisserState::Return:
		Prey = nullptr;
		LostScentTime = 0.0f;
		break;

	default:
		break;
	}
}

void AVaelKelchreisser::UpdateGait()
{
	const bool bMovingState = State == EVaelKelchreisserState::Territory || State == EVaelKelchreisserState::Track
		|| State == EVaelKelchreisserState::Hunt || State == EVaelKelchreisserState::Return;
	if (!bMovingState || Animator == nullptr)
	{
		return;
	}
	const UVaelKelchreisserData* Data = GetData<UVaelKelchreisserData>();
	const float Speed = GetVelocity().Size2D();
	if (Speed < 40.0f)
	{
		PlayAnimation(IdleSequence, true, PoseBlend);
		return;
	}
	// Gallops on the scent, otherwise walks half upright (creepy, slow)
	const bool bGallop = Speed > (Data->WalkSpeed + Data->MoveSpeed) * 0.5f;
	UAnimSequence* Gait = bGallop ? GallopSequence : WalkSequence;
	const float Rate = FMath::Clamp(Speed / (bGallop ? Data->GallopAnimationSpeed : Data->WalkAnimationSpeed), 0.4f, 1.7f);
	PlayAnimation(Gait, true, GaitBlend, Rate);
	Animator->SetRate(Rate);
}

void AVaelKelchreisser::TurnTowards(const FVector& Location, float DeltaSeconds)
{
	const FVector To = (Location - GetActorLocation()).GetSafeNormal2D();
	if (!To.IsNearlyZero())
	{
		const float Step = GetData<UVaelKelchreisserData>()->TurnRate * DeltaSeconds;
		const float Delta = FMath::Clamp(FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, To.Rotation().Yaw), -Step, Step);
		SetActorRotation(FRotator(0.0f, GetActorRotation().Yaw + Delta, 0.0f));
	}
}

void AVaelKelchreisser::WatchPlayersCasting()
{
	WatchedCasters.RemoveAll([](const TWeakObjectPtr<UVaelElementComponent>& Caster) { return !Caster.IsValid(); });
	ForEachActivePlayer([this](AVaelCharacter* Player)
	{
		UVaelElementComponent* Elements = Player->GetElementComponent();
		if (Elements == nullptr || WatchedCasters.Contains(Elements))
		{
			return;
		}
		WatchedCasters.Add(Elements);
		TWeakObjectPtr<AActor> WeakPlayer = Player;
		Elements->OnCastFinished.AddWeakLambda(this, [this, WeakPlayer](EVaelCastResult Result, const UVaelFormula*)
		{
			if (Result == EVaelCastResult::Success && WeakPlayer.IsValid())
			{
				LastCastTime.Add(WeakPlayer, GetWorld()->GetTimeSeconds());
			}
		});
	});
}

bool AVaelKelchreisser::IsRainingHere() const
{
	const AVaelRegion* Region = AVaelRegion::GetRegionAt(GetWorld(), GetActorLocation());
	return AVaelLocalWeather::IsRainingAt(GetWorld(), GetActorLocation()) || (Region != nullptr && Region->GetRainStrength() > 0.2f);
}

AActor* AVaelKelchreisser::Listen(float& OutLoudness, FVector& OutWhere)
{
	const UVaelKelchreisserData* Data = GetData<UVaelKelchreisserData>();
	WatchPlayersCasting();
	const float Now = GetWorld()->GetTimeSeconds();
	// Rain on the leaves drowns everything; at night the forest is quiet and it hears further
	const float Awareness = GetTimeOfDayAwareness() * (IsRainingHere() ? Data->RainHearing : 1.0f);

	AActor* Best = nullptr;
	OutLoudness = 0.0f;
	ForEachActivePlayer([&](AVaelCharacter* Player)
	{
		const float Distance = GetDistanceTo2D(Player);
		// Steps: a run carries far, creeping (a slow walk) and standing still make no sound in the ground
		const float MaxSpeed = FMath::Max(Player->GetCharacterMovement()->MaxWalkSpeed, 1.0f);
		const float Pace = FMath::Clamp(Player->GetVelocity().Size2D() / MaxSpeed, 0.0f, 1.0f);
		float Range = Pace > Data->QuietPace ? Data->StepHearingRange * Pace : 0.0f;
		// Spells cast close by, and fights (the player hurt or hurting lately)
		const float* CastTime = LastCastTime.Find(Player);
		if (CastTime != nullptr && Now - *CastTime < Data->CastNoiseTime)
		{
			Range = FMath::Max(Range, Data->CastHearingRange);
		}
		Range *= Awareness;
		const float Loudness = Range > 0.0f ? 1.0f - Distance / Range : 0.0f;
		if (Loudness > OutLoudness)
		{
			OutLoudness = Loudness;
			Best = Player;
			OutWhere = Player->GetActorLocation();
		}
	});
	return Best;
}

void AVaelKelchreisser::TakeScent(AActor* Target)
{
	if (Target == nullptr)
	{
		return;
	}
	Prey = Target;
	LostScentTime = 0.0f;
	FaceTowards(Target->GetActorLocation());
	EnterState(bHasScreamed ? EVaelKelchreisserState::Hunt : EVaelKelchreisserState::Scream);
}

void AVaelKelchreisser::TakeScentNow()
{
	AActor* Nearest = nullptr;
	float NearestDistance = UE_BIG_NUMBER;
	ForEachActivePlayer([&](AVaelCharacter* Player)
	{
		const float Distance = GetDistanceTo2D(Player);
		if (Distance < NearestDistance)
		{
			NearestDistance = Distance;
			Nearest = Player;
		}
	});
	TakeScent(Nearest);
}

int32 AVaelKelchreisser::HitPlayersAround(float Range, float HalfAngle, float Damage, float Knockback, int32 MaxTargets, EVaelStatus Status, float StatusDuration)
{
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(HalfAngle));
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	TArray<TPair<float, AVaelCharacter*>> Targets;
	ForEachActivePlayer([&](AVaelCharacter* Player)
	{
		const FVector To = (Player->GetActorLocation() - GetActorLocation()) * FVector(1.0f, 1.0f, 0.0f);
		const float Distance = To.Size();
		if (Distance <= Range + GetCapsuleComponent()->GetScaledCapsuleRadius() && (HalfAngle >= 180.0f || FVector::DotProduct(To.GetSafeNormal(), Forward) >= MinDot))
		{
			Targets.Add({ Distance, Player });
		}
	});
	Targets.Sort([](const TPair<float, AVaelCharacter*>& A, const TPair<float, AVaelCharacter*>& B) { return A.Key < B.Key; });

	FVaelSpellHit Hit;
	Hit.Damage = Damage;
	Hit.Element = EVaelElement::Earth;
	Hit.Knockback = Knockback;
	Hit.Status = Status;
	Hit.StatusDuration = StatusDuration;
	int32 Hits = 0;
	for (const TPair<float, AVaelCharacter*>& Target : Targets)
	{
		if (Hits >= MaxTargets)
		{
			break;
		}
		if (UVaelCombatStatics::ApplySpellHit(this, Target.Value, Hit, GetActorLocation()))
		{
			++Hits;
			++HitCount;
		}
	}
	return Hits;
}

bool AVaelKelchreisser::TryAttack(AActor* Target, float Distance)
{
	const UVaelKelchreisserData* Data = GetData<UVaelKelchreisserData>();
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < RecoverUntil || IsBlinded())
	{
		return false;
	}
	const float Reach = Distance - GetCapsuleComponent()->GetScaledCapsuleRadius();
	const FVector To = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	const float Facing = FVector::DotProduct(To, GetActorForwardVector().GetSafeNormal2D());

	// Several players close round it: the wide slap; one right in front: the claws; further out: the vines
	int32 Close = 0;
	ForEachActivePlayer([&](AVaelCharacter* Player)
	{
		if (GetDistanceTo2D(Player) - GetCapsuleComponent()->GetScaledCapsuleRadius() <= Data->SlapRange)
		{
			++Close;
		}
	});
	if (Now >= SlapReadyTime && Reach <= Data->SlapRange && (Close >= 2 || Facing < 0.6f))
	{
		EnterState(EVaelKelchreisserState::Slap);
		return true;
	}
	if (Now >= ClawReadyTime && Reach <= Data->ClawRange)
	{
		FaceTowards(Target->GetActorLocation());
		EnterState(EVaelKelchreisserState::Claw);
		return true;
	}
	if (Now >= VineReadyTime && Reach <= Data->VineRange)
	{
		EnterState(EVaelKelchreisserState::VineBite);
		return true;
	}
	return false;
}

void AVaelKelchreisser::TickBehavior(float DeltaSeconds)
{
	const UVaelKelchreisserData* Data = GetData<UVaelKelchreisserData>();
	StateTime += DeltaSeconds;

	// Listens all the time, except while it already hunts
	float Loudness = 0.0f;
	FVector Where = FVector::ZeroVector;
	AActor* Heard = nullptr;
	ListenCountdown -= DeltaSeconds;
	const bool bListening = State == EVaelKelchreisserState::Territory || State == EVaelKelchreisserState::Listen
		|| State == EVaelKelchreisserState::Track || State == EVaelKelchreisserState::Return;
	if (bListening && ListenCountdown <= 0.0f)
	{
		ListenCountdown = ListenInterval;
		Heard = Listen(Loudness, Where);
		if (Heard != nullptr && Loudness >= Data->ScentLoudness)
		{
			TakeScent(Heard);
			return;
		}
		if (Heard != nullptr && Loudness > 0.0f)
		{
			SoundLocation = Where;
			if (State != EVaelKelchreisserState::Listen && State != EVaelKelchreisserState::Track)
			{
				EnterState(EVaelKelchreisserState::Listen);
			}
		}
	}

	switch (State)
	{
	case EVaelKelchreisserState::Territory:
	{
		// Roams between spots round its giant tree, standing a while at each
		const FVector ToGoal = (RoamGoal - GetActorLocation()) * FVector(1.0f, 1.0f, 0.0f);
		if (ToGoal.Size() > 150.0f)
		{
			MoveInDirection(ToGoal.GetSafeNormal(), Data->WalkSpeed);
		}
		else
		{
			RoamPause -= DeltaSeconds;
			if (RoamPause <= 0.0f)
			{
				RoamPause = RandomInRange(Data->RoamPause);
				const FVector2D Offset = FVector2D(FMath::VRand()).GetSafeNormal() * FMath::FRandRange(0.3f, 1.0f) * Data->TerritoryRadius;
				RoamGoal = HomeLocation + FVector(Offset.X, Offset.Y, 0.0f);
			}
		}
		break;
	}

	case EVaelKelchreisserState::Listen:
		// Dead still, the flower turned to the sound
		TurnTowards(SoundLocation, DeltaSeconds);
		if (StateTime >= StateDuration)
		{
			EnterState(EVaelKelchreisserState::Track);
		}
		break;

	case EVaelKelchreisserState::Track:
	{
		// Goes slowly to where it heard it; nothing more there: back home
		const FVector ToSound = (SoundLocation - GetActorLocation()) * FVector(1.0f, 1.0f, 0.0f);
		if (ToSound.Size() > 250.0f && StateTime < Data->TrackTime)
		{
			MoveInDirection(ToSound.GetSafeNormal(), Data->WalkSpeed * 1.3f);
		}
		else
		{
			EnterState(EVaelKelchreisserState::Return);
		}
		break;
	}

	case EVaelKelchreisserState::Scream:
		if (!bStruck && StateTime >= ScreamPeakTime)
		{
			bStruck = true;
			// Heard far through the forest; close by the ground shakes the players back a step and slows them
			HitPlayersAround(Data->ScreamRange, 180.0f, 0.0f, Data->ScreamKnockback, 4, EVaelStatus::Slowed, Data->ScreamSlowTime);
		}
		if (AActor* Target = Prey.Get())
		{
			TurnTowards(Target->GetActorLocation(), DeltaSeconds);
		}
		if (StateTime >= StateDuration)
		{
			EnterState(Prey.IsValid() ? EVaelKelchreisserState::Hunt : EVaelKelchreisserState::Return);
		}
		break;

	case EVaelKelchreisserState::Hunt:
	{
		AVaelCharacter* Target = Cast<AVaelCharacter>(Prey.Get());
		if (Target == nullptr || Target->IsDefeated())
		{
			// Its prey down: the next it hears close by, or home
			float NextLoudness = 0.0f;
			AActor* Next = Listen(NextLoudness, Where);
			if (Next != nullptr && NextLoudness > 0.0f)
			{
				TakeScent(Next);
			}
			else
			{
				EnterState(EVaelKelchreisserState::Return);
			}
			break;
		}
		const float Distance = GetDistanceTo2D(Target);
		// Never lets go of the scent, unless the prey gets far away for a while or it is lured far from its tree
		const bool bOutOfReach = Distance > Data->ScentRange || FVector::Dist2D(GetActorLocation(), HomeLocation) > Data->LeashRange;
		LostScentTime = bOutOfReach ? LostScentTime + DeltaSeconds : 0.0f;
		if (LostScentTime >= Data->ScentLoseTime)
		{
			EnterState(EVaelKelchreisserState::Return);
			break;
		}
		if (TryAttack(Target, Distance))
		{
			break;
		}
		if (Distance - GetCapsuleComponent()->GetScaledCapsuleRadius() > Data->ClawRange * 0.8f)
		{
			MoveInDirection((Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D(), Data->MoveSpeed);
		}
		else
		{
			TurnTowards(Target->GetActorLocation(), DeltaSeconds);
		}
		break;
	}

	case EVaelKelchreisserState::Claw:
	{
		AActor* Target = Prey.Get();
		if (Target != nullptr && StateTime < ClawHitTime * 0.7f)
		{
			TurnTowards(Target->GetActorLocation(), DeltaSeconds);
		}
		if (!bStruck && StateTime >= ClawHitTime)
		{
			bStruck = true;
			HitPlayersAround(Data->ClawRange, 45.0f, Data->ClawDamage, Data->ClawKnockback, 2);
		}
		if (StateTime >= StateDuration)
		{
			RecoverUntil = GetWorld()->GetTimeSeconds() + RandomInRange(Data->Recovery);
			EnterState(EVaelKelchreisserState::Hunt);
		}
		break;
	}

	case EVaelKelchreisserState::Slap:
		if (!bStruck && StateTime >= SlapHitTime)
		{
			bStruck = true;
			// Swept round in front and to the sides
			HitPlayersAround(Data->SlapRange, 110.0f, Data->SlapDamage, Data->SlapKnockback, 4);
		}
		if (StateTime >= StateDuration)
		{
			RecoverUntil = GetWorld()->GetTimeSeconds() + RandomInRange(Data->Recovery);
			EnterState(EVaelKelchreisserState::Hunt);
		}
		break;

	case EVaelKelchreisserState::VineBite:
		if (!bStruck && StateTime >= VineHitTime)
		{
			bStruck = true;
			// Each mouth picks its own victim, all round and over its head
			HitPlayersAround(Data->VineRange, 180.0f, Data->VineDamage, 0.0f, Data->VineBites);
		}
		if (StateTime >= StateDuration)
		{
			RecoverUntil = GetWorld()->GetTimeSeconds() + RandomInRange(Data->Recovery) * 0.5f;
			EnterState(EVaelKelchreisserState::Hunt);
		}
		break;

	case EVaelKelchreisserState::Return:
	{
		const FVector ToHome = (HomeLocation - GetActorLocation()) * FVector(1.0f, 1.0f, 0.0f);
		if (ToHome.Size() > Data->TerritoryRadius * 0.5f)
		{
			MoveInDirection(ToHome.GetSafeNormal(), Data->WalkSpeed * 1.3f);
		}
		else
		{
			RoamGoal = GetActorLocation();
			RoamPause = RandomInRange(Data->RoamPause);
			EnterState(EVaelKelchreisserState::Territory);
		}
		break;
	}
	}
}

void AVaelKelchreisser::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!IsDead())
	{
		UpdateGait();
	}
}

void AVaelKelchreisser::OnDamageTaken(float Damage, const FGameplayTagContainer& DamageTags)
{
	Super::OnDamageTaken(Damage, DamageTags);
	if (IsDead())
	{
		return;
	}

	// A wound tells it exactly where the attacker stands: it takes the scent of the nearest player
	if (!Prey.IsValid())
	{
		TakeScentNow();
		return;
	}

	// Badly hurt it screams once more, a warning to the whole forest
	const UVaelKelchreisserData* Data = GetData<UVaelKelchreisserData>();
	if (!bScreamedHurt && GetMaxHealth() > 0.0f && GetHealth() / GetMaxHealth() < Data->HurtScreamShare && State == EVaelKelchreisserState::Hunt)
	{
		bScreamedHurt = true;
		EnterState(EVaelKelchreisserState::Scream);
	}
}

void AVaelKelchreisser::Die()
{
	const bool bWasDead = IsDead();
	// Its death cry, slowed down, while it slumps (until it has a death animation of its own)
	PlayAnimation(ScreamSequence, false, 0.2f, 0.6f, true);
	Super::Die();
	if (!bWasDead && !IsOnPlayerSide())
	{
		// A Nid mutation less: its region heals a little
		if (AVaelRegion* Region = AVaelRegion::GetRegionAt(GetWorld(), GetActorLocation()))
		{
			Region->AddCorruption(-GetData<UVaelKelchreisserData>()->CleansingOnDeath);
		}
	}
}
