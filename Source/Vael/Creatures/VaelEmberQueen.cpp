// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelEmberQueen.h"
#include "AbilitySystemComponent.h"
#include "Combat/VaelAttributeSet.h"
#include "Combat/VaelCombatStatics.h"
#include "Combat/VaelFlameRing.h"
#include "Components/CapsuleComponent.h"
#include "Creatures/VaelCreatureData.h"
#include "Creatures/VaelEmberCrawler.h"
#include "Creatures/VaelLegIKComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelSpellProjectile.h"
#include "Player/VaelCharacter.h"
#include "UI/VaelNoticeSubsystem.h"
#include "Combat/VaelHitFeedbackSubsystem.h"
#include "Vael.h"

#define LOCTEXT_NAMESPACE "VaelCreatures"

namespace
{
	/** She follows her target across the whole arena */
	constexpr float PursuitRange = 100000.0f;

	/** Extra reach of a charge beyond the touching capsules, in cm */
	constexpr float ChargeReach = 20.0f;
}

AVaelEmberQueen::AVaelEmberQueen()
{
	// The queen glows hotter than any of her brood; with her fires she heats the whole fight
	AirHeatPerSecond = 3.0f;
}

void AVaelEmberQueen::BeginPlay()
{
	Super::BeginPlay();

	SetBodyColor(GetData<UVaelEmberQueenData>()->BodyColor * 0.3f);
}

bool AVaelEmberQueen::IsInSecondPhase() const
{
	return GetHealth() < GetMaxHealth() * GetData<UVaelEmberQueenData>()->SecondPhaseHealthShare;
}

TSubclassOf<UVaelCreatureData> AVaelEmberQueen::GetDefaultDataClass() const
{
	return UVaelEmberQueenData::StaticClass();
}

void AVaelEmberQueen::OnHealthChanged(float OldValue, float NewValue)
{
	Super::OnHealthChanged(OldValue, NewValue);

	// A hit wakes her up
	if (State == EVaelQueenState::Sleeping && NewValue < OldValue && !IsDead())
	{
		WakeUp();
	}
}

void AVaelEmberQueen::Die()
{
	Super::Die();
	UVaelHitFeedbackSubsystem::Shake(this, 1.2f);

	UVaelNoticeSubsystem::Post(this, FText::Format(LOCTEXT("QueenFallen", "{0} ist gefallen"), GetCreatureName()), LOCTEXT("QueenFallenDetail", "Akt I des Prototyps ist abgeschlossen."), FLinearColor(FColor(255, 154, 74)), 10.0f);
}

void AVaelEmberQueen::TickBehavior(float DeltaSeconds)
{
	const UVaelEmberQueenData* Data = GetData<UVaelEmberQueenData>();

	StateTime += DeltaSeconds;

	if (State == EVaelQueenState::Sleeping)
	{
		if (FindTarget(Data->AggroRange) != nullptr)
		{
			WakeUp();
		}
		return;
	}

	if (State == EVaelQueenState::Waking)
	{
		if (StateTime >= Data->WakeDuration)
		{
			State = EVaelQueenState::Fighting;
			StateTime = 0.0f;
			SpitCooldown = Data->FirstSpitDelay;
			BroodCooldown = Data->FirstBroodDelay;
			RingCooldown = Data->FirstRingDelay;
			ChargeCooldown = Data->FirstChargeDelay;
		}
		return;
	}

	if (bCharging)
	{
		TickCharge(DeltaSeconds);
		return;
	}

	float TargetDistance = 0.0f;
	AActor* Target = FindTarget(PursuitRange, &TargetDistance);
	if (Target == nullptr)
	{
		return;
	}

	const bool bSecondPhase = IsInSecondPhase();
	const bool bWet = UVaelCombatStatics::HasStatus(this, EVaelStatus::Wet);

	// Walks up to her target and bites
	BiteCooldown -= DeltaSeconds;
	if (TargetDistance > Data->BiteRange)
	{
		MoveTowards(Target->GetActorLocation(), Data->MoveSpeed * (bWet ? Data->WetSpeedScale : 1.0f));
	}
	else
	{
		FaceTowards(Target->GetActorLocation());

		if (BiteCooldown <= 0.0f)
		{
			BiteCooldown = Data->BiteInterval;
			SinceBite = 0.0f;
			HitPlayer(Target, Data->BiteDamage, EVaelElement::Fire);
		}
	}

	SpitCooldown -= DeltaSeconds;
	if (SpitCooldown <= 0.0f && !IsBlinded())
	{
		SpitCooldown = (bWet ? Data->SpitIntervalWet : Data->SpitInterval) - (bSecondPhase ? Data->SpitIntervalReductionSecondPhase : 0.0f);
		Spit(Target, bSecondPhase);
	}

	BroodCooldown -= DeltaSeconds;
	if (BroodCooldown <= 0.0f)
	{
		BroodCooldown = bSecondPhase ? Data->BroodIntervalSecondPhase : Data->BroodInterval;
		CallBrood();
	}

	if (!bSecondPhase)
	{
		return;
	}

	RingCooldown -= DeltaSeconds;
	if (RingCooldown <= 0.0f)
	{
		RingCooldown = Data->RingInterval;

		FVaelSpellHit Hit;
		Hit.Damage = Data->RingDamage;
		Hit.Element = EVaelElement::Fire;

		const FVector Feet = GetActorLocation() - FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		AVaelFlameRing::SpawnRing(this, Feet, Hit, Data->RingSpeed, Data->RingMaxRadius, Data->RingHalfWidth, FLinearColor(1.0f, 0.45f, 0.05f));

		UVaelNoticeSubsystem::Post(this, LOCTEXT("FlameRing", "Flammenring"), LOCTEXT("FlameRingDetail", "Weiche aus (Leertaste / LT), wenn die Welle dich erreicht."), FLinearColor(FColor(255, 179, 107)), 3.0f);
	}

	ChargeCooldown -= DeltaSeconds;
	if (ChargeCooldown <= 0.0f && TargetDistance < Data->ChargeRange)
	{
		ChargeCooldown = RandomInRange(Data->ChargeInterval);

		bCharging = true;
		ChargeTime = 0.0f;
		ChargeDirection = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		ChargeHits.Reset();
		FaceTowards(Target->GetActorLocation());
	}
}

void AVaelEmberQueen::TickCharge(float DeltaSeconds)
{
	const UVaelEmberQueenData* Data = GetData<UVaelEmberQueenData>();

	ChargeTime += DeltaSeconds;

	// Windup: stands still and shows where she will run
	if (ChargeTime < Data->ChargeWindup)
	{
#if ENABLE_DRAW_DEBUG
		const FVector Start = GetActorLocation();
		DrawDebugLine(GetWorld(), Start, Start + ChargeDirection * Data->ChargeSpeed * Data->ChargeDuration, FColor(255, 80, 40), false, -1.0f, 0, 8.0f);
#endif
		return;
	}

	if (ChargeTime >= Data->ChargeWindup + Data->ChargeDuration)
	{
		bCharging = false;
		return;
	}

	// Full speed from the first moment of the run
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (Movement->Velocity.Size2D() < Data->ChargeSpeed * 0.5f)
	{
		Movement->Velocity = ChargeDirection * Data->ChargeSpeed;
	}

	MoveInDirection(ChargeDirection, Data->ChargeSpeed);

	const float QueenRadius = GetCapsuleComponent()->GetScaledCapsuleRadius();
	ForEachActivePlayer([this, Data, QueenRadius](AVaelCharacter* Player)
	{
		if (!ChargeHits.Contains(Player) && GetDistanceTo2D(Player) < QueenRadius + Player->GetCapsuleComponent()->GetScaledCapsuleRadius() + ChargeReach)
		{
			ChargeHits.Add(Player);
			HitPlayer(Player, Data->ChargeDamage, EVaelElement::Fire);
		}
	});
}

void AVaelEmberQueen::WakeUp()
{
	if (State != EVaelQueenState::Sleeping)
	{
		return;
	}

	const UVaelEmberQueenData* Data = GetData<UVaelEmberQueenData>();

	State = EVaelQueenState::Waking;
	StateTime = 0.0f;

	// Tougher with every player in the game
	const int32 NumPlayers = FMath::Max(1, GetWorld()->GetNumPlayerControllers());
	const float ScaledMaxHealth = Data->MaxHealth * (Data->HealthScaleBase + Data->HealthScalePerPlayer * NumPlayers);

	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent();
	AbilitySystem->SetNumericAttributeBase(UVaelAttributeSet::GetMaxHealthAttribute(), ScaledMaxHealth);
	AbilitySystem->SetNumericAttributeBase(UVaelAttributeSet::GetHealthAttribute(), FMath::Min(GetHealth(), ScaledMaxHealth));

	UVaelHitFeedbackSubsystem::Shake(this, 0.8f);
	SetBodyColor(Data->BodyColor);

	UE_LOG(LogVael, Log, TEXT("'%s' wakes up with %.0f health for %d players"), *GetNameSafe(this), ScaledMaxHealth, NumPlayers);

	UVaelNoticeSubsystem::Post(this, FText::Format(LOCTEXT("QueenWakes", "{0} erwacht"), GetCreatureName()), LOCTEXT("QueenWakesDetail", "Mutter aller Glutkriecher. Wasser schw\u00E4cht sie, Feuer kaum."), FLinearColor(FColor(255, 138, 61)), 6.0f);
}

void AVaelEmberQueen::Spit(const AActor* Target, bool bSecondPhase)
{
	SinceSpit = 0.0f;

	const UVaelEmberQueenData* Data = GetData<UVaelEmberQueenData>();

	FVaelSpellHit Hit;
	Hit.Damage = Data->SpitDamage;
	Hit.Element = EVaelElement::Fire;

	const FVector AimDirection = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	const int32 NumGlobs = bSecondPhase ? Data->SpitCountSecondPhase : Data->SpitCount;
	const FLinearColor GlobColor = UVaelMagicSettings::Get()->GetElementColor(EVaelElement::Fire);

	for (int32 GlobIndex = 0; GlobIndex < NumGlobs; ++GlobIndex)
	{
		const float AngleOffset = (GlobIndex - (NumGlobs - 1) * 0.5f) * Data->SpitSpreadDegrees;
		const FVector Direction = AimDirection.RotateAngleAxis(AngleOffset, FVector::UpVector);

		AVaelSpellProjectile::Launch(this, GetAttackLocation(Direction, -Data->SpitRadius), Direction, Hit, Data->SpitSpeed, Data->SpitRadius, Data->SpitLifetime, GlobColor,
			Data->SpitFireRadius, Data->SpitFireLifetime, Data->SpitFireDamagePerSecond);
	}
}

void AVaelEmberQueen::CallBrood()
{
	const UVaelEmberQueenData* Data = GetData<UVaelEmberQueenData>();

	int32 NumNearby = 0;
	for (TActorIterator<AVaelEmberCrawler> It(GetWorld()); It; ++It)
	{
		if (!It->IsDead() && GetDistanceTo2D(*It) < Data->BroodNearbyRange)
		{
			++NumNearby;
		}
	}

	if (NumNearby >= Data->BroodMaxNearby)
	{
		return;
	}

	SinceBrood = 0.0f;

	UVaelCreatureData* BroodData = Data->BroodData != nullptr ? Data->BroodData.Get() : GetMutableDefault<UVaelEmberCrawlerData>();

	for (int32 BroodIndex = 0; BroodIndex < Data->BroodCount; ++BroodIndex)
	{
		const float Angle = FMath::FRandRange(0.0f, UE_TWO_PI);
		const FVector Location = GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Data->BroodSpawnDistance;

		FVector Ground;
		if (FindGround(GetWorld(), Location, Ground))
		{
			SpawnCreature(GetWorld(), BroodData, Ground, FRotator(0.0f, FMath::RadiansToDegrees(Angle), 0.0f));
		}
	}

	UE_LOG(LogVael, Verbose, TEXT("'%s' calls her brood"), *GetNameSafe(this));
}

#undef LOCTEXT_NAMESPACE

void AVaelEmberQueen::OnBodyColorShown(const FLinearColor& Color, bool bHitFlash)
{
	// The mesh of the creature factory glows dim while she sleeps and fully once she is awake, like the placeholder body
	const UVaelEmberQueenData* Data = GetData<UVaelEmberQueenData>();
	AwakeGlow = FMath::Max(Data->BodyColor.R > KINDA_SMALL_NUMBER ? FMath::Clamp(Color.R / Data->BodyColor.R, 0.0f, 2.0f) : 1.0f, 0.25f);
}

void AVaelEmberQueen::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UVaelLegIKComponent* Legs = GetFactoryLegs();
	if (Legs == nullptr || IsDead())
	{
		return;
	}

	const UVaelEmberQueenData* Data = GetData<UVaelEmberQueenData>();
	const float Time = GetWorld()->GetTimeSeconds();
	const float Radius = Data->CollisionRadius;

	SinceBite += DeltaSeconds;
	SinceSpit += DeltaSeconds;
	SinceBrood += DeltaSeconds;

	// Asleep she lies on her belly with her legs drawn in; waking she heaves herself up
	const float RiseTarget = State == EVaelQueenState::Sleeping ? 0.0f : State == EVaelQueenState::Waking ? FMath::Clamp(StateTime / FMath::Max(Data->WakeDuration, 0.1f), 0.0f, 1.0f) : 1.0f;
	Risen = FMath::FInterpTo(Risen, RiseTarget, DeltaSeconds, 5.0f);

	float Glow = Data->MeshGlow * AwakeGlow;
	float Pitch = 0.0f;
	float Sink = (1.0f - Risen) * Radius * 0.32f;

	if (State == EVaelQueenState::Sleeping)
	{
		// Slow breath
		Glow *= 0.8f + 0.35f * FMath::Sin(Time * 1.4f);
		Sink += FMath::Sin(Time * 1.4f) * 3.0f;
	}
	else if (State == EVaelQueenState::Waking)
	{
		Glow *= 1.0f + 1.5f * Risen;
		Sink += FMath::Sin(Time * 38.0f) * 2.5f * (1.0f - Risen);
	}
	else if (bCharging)
	{
		// Scrapes the ground and crouches, then throws herself forward
		const float Windup = FMath::Clamp(ChargeTime / FMath::Max(Data->ChargeWindup, 0.05f), 0.0f, 1.0f);
		if (ChargeTime < Data->ChargeWindup)
		{
			Pitch = 9.0f * Windup;
			Sink += Radius * 0.1f * Windup;
			if (FMath::Fmod(ChargeTime, 0.16f) < DeltaSeconds)
			{
				Legs->AddBodyJolt(GetActorForwardVector() * (FMath::Fmod(ChargeTime, 0.32f) < 0.16f ? -9.0f : 7.0f));
			}
			Glow *= 1.0f + Windup;
		}
		else
		{
			Pitch = -8.0f;
			Glow *= 2.2f;
		}
	}
	else
	{
		const bool bSecondPhase = IsInSecondPhase();

		// Her throat glows and she rears before a volley, and snaps forward with it
		const float SpitWarning = 0.45f;
		if (SpitCooldown < SpitWarning)
		{
			const float Rear = 1.0f - FMath::Max(SpitCooldown, 0.0f) / SpitWarning;
			Pitch += 11.0f * Rear;
			Glow *= 1.0f + 1.2f * Rear;
		}
		else if (SinceSpit < 0.3f)
		{
			Pitch -= 7.0f * (1.0f - SinceSpit / 0.3f);
		}

		// A bite throws her head down and forward
		if (SinceBite < 0.28f)
		{
			const float Snap = FMath::Sin(UE_PI * SinceBite / 0.28f);
			Pitch -= 10.0f * Snap;
			if (SinceBite <= DeltaSeconds)
			{
				Legs->AddBodyJolt(GetActorForwardVector() * 16.0f);
			}
		}

		// Calling the brood she squats and her abdomen flares
		if (SinceBrood < 0.7f)
		{
			const float Squat = FMath::Sin(UE_PI * SinceBrood / 0.7f);
			Sink += Radius * 0.09f * Squat;
			Pitch += 5.0f * Squat;
			Glow *= 1.0f + 1.8f * Squat;
		}

		// Before a ring of flame her abdomen pulses, quicker and brighter
		const float RingWarning = 1.0f;
		if (bSecondPhase && RingCooldown < RingWarning)
		{
			const float Near = 1.0f - FMath::Max(RingCooldown, 0.0f) / RingWarning;
			Glow *= 1.0f + (0.5f + 0.5f * FMath::Sin(Time * (14.0f + 22.0f * Near))) * 2.5f * Near;
		}

		// Breath of the fight, faster in the second phase
		Glow *= 1.0f + 0.12f * FMath::Sin(Time * (bSecondPhase ? 5.0f : 2.6f));
	}

	Legs->SetCurl((1.0f - Risen) * 0.55f);
	Legs->SetExtraBodyTilt(FRotator(Pitch, 0.0f, 0.0f));
	SetFactoryMeshOffset(FVector(0.0f, 0.0f, -Sink));
	SetFactoryGlow(Glow);
}
