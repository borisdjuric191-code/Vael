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
#include "AnimationRuntime.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Magic/VaelElementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/VaelCharacter.h"
#include "World/VaelLocalWeather.h"
#include "World/VaelGround.h"
#include "World/VaelRegion.h"

namespace
{
	/** Seconds into the attack animations (Tools/Kelchreisser/06_animationen.py) where the blows land */
	constexpr float ClawHitTime = 0.5f;
	constexpr float SwipeHitStart = 0.46f;
	constexpr float SwipeHitEnd = 0.64f;
	constexpr float BiteLungeStart = 0.32f;
	constexpr float BiteHitTime = 0.46f;
	constexpr float LeapStart = 0.35f;
	constexpr float LeapEnd = 0.8f;
	constexpr float ScreamPeakTime = 0.5f;

	/** Seconds for blends between animations */
	constexpr float GaitBlend = 0.3f;
	constexpr float PoseBlend = 0.35f;
	constexpr float AttackBlend = 0.15f;

	/** How often it listens, in seconds */
	constexpr float ListenInterval = 0.25f;

	/** Hands and feet set down onto the ground: the bones that bend, the piece at the end and the one that touches */
	struct FGroundLimbBones
	{
		const TCHAR* Upper;
		const TCHAR* Lower;
		const TCHAR* End;
		const TCHAR* Touch;
	};
	const FGroundLimbBones GroundLimbBones[] =
	{
		{ TEXT("oberarm_l"), TEXT("unterarm_l"), TEXT("hand_l"), TEXT("finger_l") },
		{ TEXT("oberarm_r"), TEXT("unterarm_r"), TEXT("hand_r"), TEXT("finger_r") },
		{ TEXT("oberschenkel_l"), TEXT("unterschenkel_l"), TEXT("mittelfuss_l"), TEXT("zehen_l") },
		{ TEXT("oberschenkel_r"), TEXT("unterschenkel_r"), TEXT("mittelfuss_r"), TEXT("zehen_r") },
	};
	/** Most it bends a limb or lowers the body for the ground, in cm */
	constexpr float MaxGroundLift = 110.0f;

	/** The lower inner jaws, and where their lips' tips are from the jaws' hinges in the reference pose (cm) */
	const TCHAR* LipJaws[] = { TEXT("kiefer_ul"), TEXT("kiefer_ur") };
	constexpr int32 DroolDrops = 8;
	const FVector LipFromHinge(90.0f, 0.0f, -45.0f);

	/** A vine's strike: drawing back, shooting out, holding on and pulling in (seconds) */
	constexpr float VineCoil = 0.32f;
	constexpr float VineLunge = 0.2f;
	constexpr float VineHold = 0.18f;
	constexpr float VineRetract = 0.55f;

	float Smooth(float T)
	{
		T = FMath::Clamp(T, 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}

	float EaseOut(float T)
	{
		T = FMath::Clamp(T, 0.0f, 1.0f);
		return 1.0f - FMath::Cube(1.0f - T);
	}
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

	RoamGoal = HomeLocation;
	RoamPause = FMath::FRandRange(0.0f, 4.0f);
	ListenCountdown = FMath::FRandRange(0.0f, ListenInterval);

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
	SwipeSequence = Data->SwipeAnimation.LoadSynchronous();
	ClawSequence = Data->ClawAnimation.LoadSynchronous();
	BiteSequence = Data->BiteAnimation.LoadSynchronous();
	LeapSequence = Data->LeapAnimation.LoadSynchronous();
	ScreamSequence = Data->ScreamAnimation.LoadSynchronous();
	LurkSequence = Data->LurkAnimation.LoadSynchronous();

	for (int32 Slot = 0; Slot < GetMesh()->GetNumMaterials(); ++Slot)
	{
		Materials.Add(GetMesh()->CreateDynamicMaterialInstance(Slot));
	}
	RefreshNidGlow();
	SetupVines();
	SetupDrool();
	GroundLifts.Init(0.0f, UE_ARRAY_COUNT(GroundLimbBones));
	EnterState(EVaelKelchreisserState::Territory);
}

void AVaelKelchreisser::SetupVines()
{
	// Bones of step 5 (Tools/Kelchreisser/05_skelett.py): ranke_<i>_01.. > ranke_<i>_maul > ranke_<i>_kiefer_oben/unten
	for (int32 Vine = 0; Vine < 16; ++Vine)
	{
		FVaelVineStrike Strike;
		for (int32 Link = 1; Link < 40; ++Link)
		{
			const FName Bone(*FString::Printf(TEXT("ranke_%d_%02d"), Vine, Link));
			if (GetMesh()->GetBoneIndex(Bone) == INDEX_NONE)
			{
				break;
			}
			Strike.Bones.Add(Bone);
		}
		const FName Bud(*FString::Printf(TEXT("ranke_%d_maul"), Vine));
		if (Strike.Bones.Num() < 2 || GetMesh()->GetBoneIndex(Bud) == INDEX_NONE)
		{
			continue;
		}
		Strike.Bones.Add(Bud);
		Strike.UpperJaw = FName(*FString::Printf(TEXT("ranke_%d_kiefer_oben"), Vine));
		Strike.LowerJaw = FName(*FString::Printf(TEXT("ranke_%d_kiefer_unten"), Vine));
		Vines.Add(Strike);
	}
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
	const float Now = GetWorld()->GetTimeSeconds();
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
		// Ducks low like a dog that has heard something
		PlayAnimation(LurkSequence != nullptr ? LurkSequence : IdleSequence, true, 0.5f);
		StateDuration = Data->ListenTime;
		break;

	case EVaelKelchreisserState::Scream:
		PlayAnimation(ScreamSequence, false, 0.15f, 1.0f, true);
		StateDuration = Length(ScreamSequence, 2.0f);
		bHasScreamed = true;
		break;

	case EVaelKelchreisserState::Claw:
		++AttackCount;
		PlayAnimation(ClawSequence, false, AttackBlend, 1.0f, true);
		StateDuration = Length(ClawSequence, 1.0f);
		ClawReadyTime = Now + RandomInRange(Data->ClawInterval);
		break;

	case EVaelKelchreisserState::Swipe:
		++AttackCount;
		PlayAnimation(SwipeSequence, false, AttackBlend, 1.0f, true);
		StateDuration = Length(SwipeSequence, 1.2f);
		SwipeReadyTime = Now + RandomInRange(Data->SwipeInterval);
		break;

	case EVaelKelchreisserState::Bite:
		++AttackCount;
		PlayAnimation(BiteSequence, false, AttackBlend, 1.0f, true);
		StateDuration = Length(BiteSequence, 0.9f);
		BiteReadyTime = Now + RandomInRange(Data->BiteInterval);
		break;

	case EVaelKelchreisserState::Leap:
		++AttackCount;
		PlayAnimation(LeapSequence, false, AttackBlend, 1.0f, true);
		StateDuration = Length(LeapSequence, 1.2f);
		LeapReadyTime = Now + RandomInRange(Data->LeapInterval);
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
		// Standing in a fight it crouches, ready to spring; otherwise it stands
		PlayAnimation(State == EVaelKelchreisserState::Hunt && LurkSequence != nullptr ? LurkSequence : IdleSequence, true, PoseBlend);
		return;
	}
	// Gallops on the scent, otherwise walks half upright (creepy, slow)
	const bool bGallop = Speed > Data->WalkSpeed * 1.8f;
	UAnimSequence* Gait = bGallop ? GallopSequence : WalkSequence;
	const float Rate = FMath::Clamp(Speed / (bGallop ? Data->GallopAnimationSpeed : Data->WalkAnimationSpeed), 0.55f, 1.6f);
	PlayAnimation(Gait, true, GaitBlend, Rate);
	Animator->SetRate(Rate);
}

void AVaelKelchreisser::TurnTowards(const FVector& Location, float DeltaSeconds, float Speed)
{
	const FVector To = (Location - GetActorLocation()).GetSafeNormal2D();
	if (!To.IsNearlyZero())
	{
		const float Step = GetData<UVaelKelchreisserData>()->TurnRate * Speed * DeltaSeconds;
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
		// Spells cast close by
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

int32 AVaelKelchreisser::HitPlayersAround(const FVector& Centre, float Range, float HalfAngle, float Damage, float Knockback, int32 MaxTargets, EVaelStatus Status, float StatusDuration)
{
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(HalfAngle));
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	TArray<TPair<float, AVaelCharacter*>> Targets;
	ForEachActivePlayer([&](AVaelCharacter* Player)
	{
		const FVector To = (Player->GetActorLocation() - Centre) * FVector(1.0f, 1.0f, 0.0f);
		const float Distance = To.Size();
		if (Distance <= Range && (HalfAngle >= 180.0f || FVector::DotProduct(To.GetSafeNormal(), Forward) >= MinDot))
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
		if (UVaelCombatStatics::ApplySpellHit(this, Target.Value, Hit, Centre))
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

	// Still some way off: it throws itself at the prey
	if (Now >= LeapReadyTime && Reach >= Data->LeapRange.X && Reach <= Data->LeapRange.Y && Facing > 0.5f)
	{
		// Aims where the prey will be, lands just short of it
		const FVector Ahead = Target->GetActorLocation() + Target->GetVelocity() * FVector(1.0f, 1.0f, 0.0f) * 0.3f;
		const FVector Way = (Ahead - GetActorLocation()) * FVector(1.0f, 1.0f, 0.0f);
		LeapDirection = Way.GetSafeNormal();
		LeapSpeed = FMath::Min((Way.Size() - GetCapsuleComponent()->GetScaledCapsuleRadius() - 150.0f) / (LeapEnd - LeapStart), Data->LeapMaxSpeed);
		FaceTowards(Ahead);
		EnterState(EVaelKelchreisserState::Leap);
		return true;
	}

	// Several players close round it, or one at its side: the wide swipe
	int32 Close = 0;
	ForEachActivePlayer([&](AVaelCharacter* Player)
	{
		if (GetDistanceTo2D(Player) - GetCapsuleComponent()->GetScaledCapsuleRadius() <= Data->SwipeRange)
		{
			++Close;
		}
	});
	if (Now >= SwipeReadyTime && Reach <= Data->SwipeRange && (Close >= 2 || Facing < 0.5f || FMath::FRand() < 0.3f))
	{
		EnterState(EVaelKelchreisserState::Swipe);
		return true;
	}
	// Right in front: the flower bites, or the claws rake
	if (Now >= BiteReadyTime && Reach <= Data->BiteRange && Facing > 0.6f)
	{
		EnterState(EVaelKelchreisserState::Bite);
		return true;
	}
	if (Now >= ClawReadyTime && Reach <= Data->ClawRange)
	{
		EnterState(EVaelKelchreisserState::Claw);
		return true;
	}
	return false;
}

void AVaelKelchreisser::UpdateVines(float DeltaSeconds)
{
	if (Animator == nullptr || Vines.Num() == 0)
	{
		return;
	}
	const UVaelKelchreisserData* Data = GetData<UVaelKelchreisserData>();
	const float Now = GetWorld()->GetTimeSeconds();
	const bool bFighting = !IsDead() && Prey.IsValid() && State != EVaelKelchreisserState::Scream && State != EVaelKelchreisserState::Leap
		&& State != EVaelKelchreisserState::Territory && State != EVaelKelchreisserState::Return;

	int32 Striking = 0;
	for (const FVaelVineStrike& Vine : Vines)
	{
		Striking += Vine.Time >= 0.0f ? 1 : 0;
	}

	const FTransform MeshTransform = GetMesh()->GetComponentTransform();
	TArray<FVaelReachChain> Chains;
	for (FVaelVineStrike& Vine : Vines)
	{
		const FVector Root = GetMesh()->GetSocketLocation(Vine.Bones[0]);

		// A resting vine picks a victim of its own: the player in reach that the fewest vines are after
		if (Vine.Time < 0.0f)
		{
			if (!bFighting || Now < Vine.ReadyTime || Striking >= Data->MaxVineStrikes || IsBlinded())
			{
				continue;
			}
			AVaelCharacter* Best = nullptr;
			int32 BestCount = TNumericLimits<int32>::Max();
			ForEachActivePlayer([&](AVaelCharacter* Player)
			{
				if (Player->IsDefeated() || FVector::Dist2D(Root, Player->GetActorLocation()) > Data->VineReach)
				{
					return;
				}
				int32 After = 0;
				for (const FVaelVineStrike& Other : Vines)
				{
					After += Other.Time >= 0.0f && Other.Victim == Player ? 1 : 0;
				}
				if (After < BestCount)
				{
					BestCount = After;
					Best = Player;
				}
			});
			if (Best == nullptr)
			{
				continue;
			}
			Vine.Victim = Best;
			Vine.Time = 0.0f;
			Vine.bBitten = false;
			// Draws back up and away from its victim before it strikes
			const FVector Away = (Root - Best->GetActorLocation()).GetSafeNormal2D();
			Vine.CoilPoint = Root + FVector(0.0f, 0.0f, 320.0f) + Away * 220.0f;
			++Striking;
			++AttackCount;
			++VineStrikeCount;
		}

		Vine.Time += DeltaSeconds;
		const float T = Vine.Time;
		AActor* Victim = Vine.Victim.Get();
		FVaelReachChain Chain;
		Chain.Bones = Vine.Bones;
		Chain.UpperJaw = Vine.UpperJaw;
		Chain.LowerJaw = Vine.LowerJaw;
		Chain.WavePhase = Now * 11.0f + Vine.Bones.Num();
		FVector Target = Vine.CoilPoint;

		if (T < VineCoil)
		{
			// Coils back, jaws opening
			const float U = Smooth(T / VineCoil);
			Chain.Weight = 0.55f * U;
			Chain.Arc = 120.0f;
			Chain.Wave = 40.0f * U;
			Chain.JawOpen = 35.0f * U;
			// Fixes where it strikes: where the victim will be (it can still dodge out of the bite)
			if (Victim != nullptr)
			{
				const FVector Ahead = Victim->GetActorLocation() + Victim->GetVelocity() * FVector(1.0f, 1.0f, 0.0f) * 0.2f;
				const FVector Way = Ahead - Root;
				Vine.BitePoint = Root + Way.GetClampedToMaxSize(Data->VineReach);
			}
		}
		else if (T < VineCoil + VineLunge)
		{
			// Shoots out over its head and down onto the victim
			const float U = EaseOut((T - VineCoil) / VineLunge);
			Chain.Weight = FMath::Lerp(0.55f, 1.0f, U);
			Target = FMath::Lerp(Vine.CoilPoint, Vine.BitePoint, U);
			Chain.Arc = 150.0f + 0.18f * FVector::Dist(Root, Vine.BitePoint) * (1.0f - U * 0.5f);
			Chain.Wave = 40.0f * (1.0f - U);
			Chain.JawOpen = 50.0f;
		}
		else if (T < VineCoil + VineLunge + VineHold)
		{
			// Snaps shut, and holds on a moment
			if (!Vine.bBitten)
			{
				Vine.bBitten = true;
				if (Victim != nullptr && !IsDead() && FVector::Dist(Victim->GetActorLocation(), Vine.BitePoint) <= Data->VineBiteRadius)
				{
					FVaelSpellHit Hit;
					Hit.Damage = Data->VineDamage;
					Hit.Element = EVaelElement::Earth;
					if (UVaelCombatStatics::ApplySpellHit(this, Victim, Hit, Vine.BitePoint))
					{
						++HitCount;
					}
				}
			}
			Chain.Weight = 1.0f;
			Target = Vine.BitePoint;
			Chain.Arc = 90.0f;
			Chain.JawOpen = -12.0f;
		}
		else if (T < VineCoil + VineLunge + VineHold + VineRetract)
		{
			// Pulls back in, whipping
			const float U = Smooth((T - VineCoil - VineLunge - VineHold) / VineRetract);
			Chain.Weight = 1.0f - U;
			Target = FMath::Lerp(Vine.BitePoint, Vine.CoilPoint, U * 0.6f);
			Chain.Arc = 90.0f + 150.0f * U;
			Chain.Wave = 70.0f * FMath::Sin(U * UE_PI);
			Chain.JawOpen = 10.0f * U;
		}
		else
		{
			Vine.Time = -1.0f;
			Vine.Victim = nullptr;
			Vine.ReadyTime = Now + RandomInRange(Data->VineInterval);
			continue;
		}
		Chain.Target = MeshTransform.InverseTransformPosition(Target);
		Chains.Add(Chain);
	}
	Animator->SetReachChains(Chains);
}

void AVaelKelchreisser::UpdateGrounding(float DeltaSeconds)
{
	if (Animator == nullptr || GroundLifts.Num() != UE_ARRAY_COUNT(GroundLimbBones))
	{
		return;
	}
	// In the air and dead it doesn't reach for the ground
	const bool bAirborne = State == EVaelKelchreisserState::Leap && StateTime > LeapStart && StateTime < LeapEnd;
	const float Floor = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	TArray<FVaelGroundLimb> Limbs;
	float Lowest = 0.0f;
	for (int32 Index = 0; Index < GroundLifts.Num(); ++Index)
	{
		const FGroundLimbBones& Bones = GroundLimbBones[Index];
		float Wanted = 0.0f;
		if (!bAirborne && !IsDead())
		{
			// The ground under where the hand or foot is, against the ground the body stands on
			const FVector Touch = GetMesh()->GetSocketLocation(Bones.Touch);
			FHitResult Hit;
			if (VaelGround::TraceGround(GetWorld(), FVector(Touch.X, Touch.Y, Floor + MaxGroundLift + 50.0f), FVector(Touch.X, Touch.Y, Floor - MaxGroundLift - 50.0f), Hit, this))
			{
				Wanted = FMath::Clamp(Hit.Location.Z - Floor, -MaxGroundLift, MaxGroundLift);
			}
		}
		GroundLifts[Index] = FMath::FInterpTo(GroundLifts[Index], Wanted, DeltaSeconds, 10.0f);
		Lowest = FMath::Min(Lowest, GroundLifts[Index]);
		FVaelGroundLimb& Limb = Limbs.AddDefaulted_GetRef();
		Limb.Upper = Bones.Upper;
		Limb.Lower = Bones.Lower;
		Limb.End = Bones.End;
		Limb.Lift = GroundLifts[Index];
	}
	// The body sinks so its lowest limb can still reach down
	PelvisShift = FMath::FInterpTo(PelvisShift, Lowest * 0.85f, DeltaSeconds, 6.0f);
	Animator->SetGrounding(Limbs, TEXT("becken"), PelvisShift);
}

void AVaelKelchreisser::SetupDrool()
{
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UMaterialInterface* Slime = GetData<UVaelKelchreisserData>()->DroolMaterial.LoadSynchronous();
	if (Sphere == nullptr)
	{
		return;
	}
	for (int32 Index = 0; Index < DroolDrops * 2; ++Index)
	{
		UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(this);
		Piece->SetStaticMesh(Sphere);
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Piece->SetCastShadow(false);
		Piece->SetUsingAbsoluteLocation(true);
		Piece->SetUsingAbsoluteRotation(true);
		Piece->SetUsingAbsoluteScale(true);
		if (Slime != nullptr)
		{
			Piece->SetMaterial(0, Slime);
		}
		Piece->SetupAttachment(GetRootComponent());
		Piece->RegisterComponent();
		Piece->SetVisibility(false);
		(Index % 2 == 0 ? DroolThreads : DroolBlobs).Add(Piece);
	}
	Drool.SetNum(DroolDrops);

	// Where the lips sit on the lower inner jaws, as a turn of each jaw (the bones carry a scale, the turn does not)
	const FReferenceSkeleton& Skeleton = GetMesh()->GetSkinnedAsset()->GetRefSkeleton();
	for (int32 Lip = 0; Lip < 2; ++Lip)
	{
		const int32 Bone = Skeleton.FindBoneIndex(LipJaws[Lip]);
		LipOffset[Lip] = FVector::ZeroVector;
		if (Bone != INDEX_NONE)
		{
			const FTransform Rest = FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, Bone);
			LipOffset[Lip] = Rest.GetRotation().UnrotateVector(LipFromHinge);
		}
	}
}

FVector AVaelKelchreisser::GetLip(int32 Lip) const
{
	const FTransform Jaw = GetMesh()->GetSocketTransform(LipJaws[Lip]);
	return Jaw.GetLocation() + Jaw.GetRotation().RotateVector(LipOffset[Lip]);
}

void AVaelKelchreisser::FlingDrool(int32 Count)
{
	for (FVaelDrool& Drop : Drool)
	{
		if (Count <= 0)
		{
			break;
		}
		if (Drop.Age >= 0.0f)
		{
			continue;
		}
		--Count;
		Drop.Age = 0.0f;
		Drop.HangTime = 0.0f;
		Drop.Size = FMath::FRandRange(4.0f, 8.0f);
		Drop.Lip = FMath::RandRange(0, 1);
		Drop.Location = GetLip(Drop.Lip);
		Drop.Velocity = GetActorForwardVector() * FMath::FRandRange(250.0f, 550.0f) + FMath::VRand() * 220.0f + FVector(0.0f, 0.0f, 200.0f);
		Drop.Landed = -1.0f;
		Drop.bFree = false;
	}
}

void AVaelKelchreisser::UpdateDrool(float DeltaSeconds)
{
	if (Drool.Num() == 0 || Drool.Num() != DroolThreads.Num() || Drool.Num() != DroolBlobs.Num())
	{
		return;
	}
	const UVaelKelchreisserData* Data = GetData<UVaelKelchreisserData>();
	// It slobbers all the time, more when it has a scent
	DroolCountdown -= DeltaSeconds;
	if (!IsDead() && DroolCountdown <= 0.0f)
	{
		DroolCountdown = RandomInRange(Data->DroolInterval) * (Prey.IsValid() ? 0.5f : 1.0f);
		for (FVaelDrool& Drop : Drool)
		{
			if (Drop.Age < 0.0f)
			{
				Drop.Age = 0.0f;
				Drop.HangTime = FMath::FRandRange(0.6f, 1.6f);
				Drop.Size = FMath::FRandRange(3.5f, 7.0f);
				Drop.Lip = FMath::RandRange(0, 1);
				Drop.Location = GetLip(Drop.Lip);
				Drop.Velocity = FVector::ZeroVector;
				Drop.Landed = -1.0f;
				Drop.bFree = false;
				break;
			}
		}
	}

	const FVector Motion = GetVelocity();
	for (int32 Index = 0; Index < Drool.Num(); ++Index)
	{
		FVaelDrool& Drop = Drool[Index];
		UStaticMeshComponent* Thread = DroolThreads[Index];
		UStaticMeshComponent* Blob = DroolBlobs[Index];
		if (Drop.Age < 0.0f)
		{
			continue;
		}
		Drop.Age += DeltaSeconds;
		const FVector Lip = GetLip(Drop.Lip);
		float ThreadLength = 0.0f;

		if (!Drop.bFree && Drop.Age < Drop.HangTime)
		{
			// Runs out and stretches down into a thread, trailing behind when the head moves
			const float Hang = 8.0f + 70.0f * FMath::Square(Drop.Age / Drop.HangTime);
			const FVector Wanted = Lip + FVector(0.0f, 0.0f, -Hang) - Motion.GetClampedToMaxSize(900.0f) * 0.06f;
			Drop.Location = FMath::VInterpTo(Drop.Location, Wanted, DeltaSeconds, 8.0f);
			Drop.Velocity = Motion;
			ThreadLength = FVector::Dist(Lip, Drop.Location);
		}
		else if (Drop.Landed < 0.0f)
		{
			if (!Drop.bFree)
			{
				// Tears off and falls onto the ground below
				Drop.bFree = true;
				FHitResult Hit;
				Drop.GroundZ = VaelGround::TraceGround(GetWorld(), Drop.Location, Drop.Location - FVector(0.0f, 0.0f, 1500.0f), Hit, this)
					? Hit.Location.Z : Drop.Location.Z - 600.0f;
			}
			Drop.Velocity.Z -= 980.0f * DeltaSeconds;
			Drop.Location += Drop.Velocity * DeltaSeconds;
			// The thread snaps back a moment after
			ThreadLength = FMath::Max(0.0f, FVector::Dist(Lip, Drop.Location) * (1.0f - (Drop.Age - Drop.HangTime) / 0.12f));
			if (Drop.Location.Z <= Drop.GroundZ)
			{
				Drop.Location.Z = Drop.GroundZ + 1.0f;
				Drop.Landed = 0.0f;
			}
		}
		else
		{
			Drop.Landed += DeltaSeconds;
		}

		// A blob, flattened to a splash where it landed, drying away
		const float Dry = Drop.Landed >= 0.0f ? FMath::Clamp(1.0f - Drop.Landed / 1.6f, 0.0f, 1.0f) : 1.0f;
		if (Dry <= 0.0f)
		{
			Drop.Age = -1.0f;
			Thread->SetVisibility(false);
			Blob->SetVisibility(false);
			continue;
		}
		const float Diameter = Drop.Size / 50.0f;
		const FVector BlobScale = Drop.Landed >= 0.0f ? FVector(Diameter * 2.6f, Diameter * 2.6f, Diameter * 0.25f) * Dry
			: FVector(Diameter, Diameter, Diameter * (Drop.bFree ? 1.5f : 1.2f));
		const FQuat Falling = Drop.bFree && Drop.Landed < 0.0f && !Drop.Velocity.IsNearlyZero()
			? FQuat::FindBetweenNormals(FVector::UpVector, -Drop.Velocity.GetSafeNormal()) : FQuat::Identity;
		Blob->SetWorldLocationAndRotation(Drop.Location, Falling);
		Blob->SetWorldScale3D(BlobScale);
		Blob->SetVisibility(true);

		Thread->SetVisibility(ThreadLength > 2.0f);
		if (ThreadLength > 2.0f)
		{
			const FVector Middle = (Lip + Drop.Location) * 0.5f;
			const FVector Along = (Drop.Location - Lip).GetSafeNormal();
			Thread->SetWorldLocationAndRotation(Middle, FQuat::FindBetweenNormals(FVector::UpVector, Along));
			Thread->SetWorldScale3D(FVector(Diameter * 0.3f, Diameter * 0.3f, ThreadLength / 100.0f));
		}
	}
}

void AVaelKelchreisser::TickBehavior(float DeltaSeconds)
{
	const UVaelKelchreisserData* Data = GetData<UVaelKelchreisserData>();
	StateTime += DeltaSeconds;

	// Listens all the time, except while it already hunts
	float Loudness = 0.0f;
	FVector Where = FVector::ZeroVector;
	ListenCountdown -= DeltaSeconds;
	const bool bListening = State == EVaelKelchreisserState::Territory || State == EVaelKelchreisserState::Listen
		|| State == EVaelKelchreisserState::Track || State == EVaelKelchreisserState::Return;
	if (bListening && ListenCountdown <= 0.0f)
	{
		ListenCountdown = ListenInterval;
		AActor* Heard = Listen(Loudness, Where);
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

	AActor* Target = Prey.Get();
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
		TurnTowards(SoundLocation, DeltaSeconds, 1.5f);
		if (StateTime >= StateDuration)
		{
			EnterState(EVaelKelchreisserState::Track);
		}
		break;

	case EVaelKelchreisserState::Track:
	{
		// Goes to where it heard it; nothing more there: back home
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
			// Close by the ground shakes the players back a step and slows them
			HitPlayersAround(GetActorLocation(), Data->ScreamRange, 180.0f, 0.0f, Data->ScreamKnockback, 4, EVaelStatus::Slowed, Data->ScreamSlowTime);
			FlingDrool(6);
		}
		if (Target != nullptr)
		{
			TurnTowards(Target->GetActorLocation(), DeltaSeconds, 2.0f);
		}
		if (StateTime >= StateDuration)
		{
			EnterState(Prey.IsValid() ? EVaelKelchreisserState::Hunt : EVaelKelchreisserState::Return);
		}
		break;

	case EVaelKelchreisserState::Hunt:
	{
		AVaelCharacter* Player = Cast<AVaelCharacter>(Target);
		if (Player == nullptr || Player->IsDefeated())
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
		const float Distance = GetDistanceTo2D(Player);
		// Never lets go of the scent, unless the prey gets far away for a while or it is lured far from its tree
		const bool bOutOfReach = Distance > Data->ScentRange || FVector::Dist2D(GetActorLocation(), HomeLocation) > Data->LeashRange;
		LostScentTime = bOutOfReach ? LostScentTime + DeltaSeconds : 0.0f;
		if (LostScentTime >= Data->ScentLoseTime)
		{
			EnterState(EVaelKelchreisserState::Return);
			break;
		}
		if (TryAttack(Player, Distance))
		{
			break;
		}
		// Runs it down, or circles close in front of it, turning on the spot
		if (Distance - GetCapsuleComponent()->GetScaledCapsuleRadius() > Data->BiteRange * 0.7f)
		{
			MoveInDirection((Player->GetActorLocation() - GetActorLocation()).GetSafeNormal2D(), Data->MoveSpeed);
		}
		else
		{
			TurnTowards(Player->GetActorLocation(), DeltaSeconds, 2.0f);
		}
		break;
	}

	case EVaelKelchreisserState::Claw:
		if (Target != nullptr && StateTime < ClawHitTime * 0.8f)
		{
			TurnTowards(Target->GetActorLocation(), DeltaSeconds, 2.0f);
		}
		if (!bStruck && StateTime >= ClawHitTime)
		{
			bStruck = true;
			HitPlayersAround(GetActorLocation(), Data->ClawRange + GetCapsuleComponent()->GetScaledCapsuleRadius(), 45.0f, Data->ClawDamage, Data->ClawKnockback, 2);
		}
		if (StateTime >= StateDuration)
		{
			RecoverUntil = GetWorld()->GetTimeSeconds() + RandomInRange(Data->Recovery);
			EnterState(EVaelKelchreisserState::Hunt);
		}
		break;

	case EVaelKelchreisserState::Swipe:
		if (Target != nullptr && StateTime < SwipeHitStart)
		{
			TurnTowards(Target->GetActorLocation(), DeltaSeconds, 1.5f);
		}
		// The hand sweeps round from its right through the front to its left: whoever is in the circle is hit once
		if (!bStruck && StateTime >= SwipeHitStart)
		{
			bStruck = true;
			HitPlayersAround(GetActorLocation(), Data->SwipeRange + GetCapsuleComponent()->GetScaledCapsuleRadius(), 125.0f, Data->SwipeDamage, Data->SwipeKnockback, 4);
		}
		if (StateTime >= SwipeHitStart && StateTime <= SwipeHitEnd)
		{
			// The body throws itself into the swing
			MoveInDirection(GetActorForwardVector(), Data->MoveSpeed * 0.4f);
		}
		if (StateTime >= StateDuration)
		{
			RecoverUntil = GetWorld()->GetTimeSeconds() + RandomInRange(Data->Recovery);
			EnterState(EVaelKelchreisserState::Hunt);
		}
		break;

	case EVaelKelchreisserState::Bite:
		if (Target != nullptr && StateTime < BiteLungeStart)
		{
			TurnTowards(Target->GetActorLocation(), DeltaSeconds, 3.0f);
		}
		if (StateTime >= BiteLungeStart && StateTime <= BiteHitTime)
		{
			// Lunges forward with the flower torn open
			GetCharacterMovement()->Velocity = GetActorForwardVector() * Data->BiteLungeSpeed;
			MoveInDirection(GetActorForwardVector(), Data->BiteLungeSpeed);
		}
		if (!bStruck && StateTime >= BiteHitTime)
		{
			bStruck = true;
			const FVector Mouth = GetActorLocation() + GetActorForwardVector() * (GetCapsuleComponent()->GetScaledCapsuleRadius() + Data->BiteRange * 0.5f);
			HitPlayersAround(Mouth, Data->BiteRange * 0.6f, 180.0f, Data->BiteDamage, Data->BiteKnockback, 1);
			FlingDrool(4);
		}
		if (StateTime >= StateDuration)
		{
			RecoverUntil = GetWorld()->GetTimeSeconds() + RandomInRange(Data->Recovery);
			EnterState(EVaelKelchreisserState::Hunt);
		}
		break;

	case EVaelKelchreisserState::Leap:
		if (StateTime < LeapStart && Target != nullptr)
		{
			TurnTowards(Target->GetActorLocation(), DeltaSeconds, 3.0f);
			LeapDirection = GetActorForwardVector().GetSafeNormal2D();
		}
		else if (StateTime <= LeapEnd)
		{
			GetCharacterMovement()->Velocity = LeapDirection * LeapSpeed;
			MoveInDirection(LeapDirection, LeapSpeed);
		}
		if (!bStruck && StateTime > LeapEnd)
		{
			// Lands among them: everyone close is knocked away
			bStruck = true;
			HitPlayersAround(GetActorLocation() + LeapDirection * GetCapsuleComponent()->GetScaledCapsuleRadius(), Data->LeapLandRange, 180.0f,
				Data->LeapDamage, Data->LeapKnockback, 4);
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
	UpdateVines(DeltaSeconds);
	UpdateGrounding(DeltaSeconds);
	UpdateDrool(DeltaSeconds);
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
