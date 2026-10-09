// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelAshHarpy.h"
#include "UObject/ConstructorHelpers.h"
#include "Combat/VaelCombatStatics.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Creatures/VaelCreatureData.h"
#include "DrawDebugHelpers.h"
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
}

void AVaelAshHarpy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (IsDead())
	{
		return;
	}

	// The body glides between flight and dive height and bobs while circling
	const UVaelAshHarpyData* Data = GetData<UVaelAshHarpyData>();
	const bool bLow = State == EVaelHarpyState::Diving;
	const float Bob = bLow ? 0.0f : FMath::Sin(GetWorld()->GetTimeSeconds() * 3.0f + CircleAngle) * BobAmplitude;
	const float TargetHeight = (bLow ? DiveBodyHeight : Data->FlightHeight) + Bob;

	BodyHeight = FMath::FInterpTo(BodyHeight, TargetHeight, DeltaSeconds, BodyHeightFollowSpeed);
	GetBody()->SetRelativeLocation(FVector(0.0f, 0.0f, BodyHeight));
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
