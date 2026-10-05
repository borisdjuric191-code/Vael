// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelPreacher.h"
#include "UObject/ConstructorHelpers.h"
#include "Combat/VaelGroundStrike.h"
#include "Creatures/VaelCreatureData.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelSpellProjectile.h"
#include "Player/VaelCharacter.h"
#include "Vael.h"

#define LOCTEXT_NAMESPACE "VaelCreatures"

namespace
{
	/** The preacher stops walking home closer than this, in cm */
	constexpr float PreacherHomeTolerance = 140.0f;

	/** Radians per second of the sideways walk */
	constexpr float StrafeFrequency = 0.9f;
}

AVaelPreacher::AVaelPreacher()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (BodyMesh.Succeeded())
	{
		GetBody()->SetStaticMesh(BodyMesh.Object);
	}

	// Always faces its target, also while backing away
	GetCharacterMovement()->bOrientRotationToMovement = false;
}

void AVaelPreacher::BeginPlay()
{
	Super::BeginPlay();

	const UVaelPreacherData* Data = GetData<UVaelPreacherData>();
	BoltCooldown = RandomInRange(Data->BoltInterval);
	SpikeCooldown = RandomInRange(Data->FirstSpikeDelay);
	StrafePhase = FMath::FRandRange(0.0f, UE_TWO_PI);
}

TSubclassOf<UVaelCreatureData> AVaelPreacher::GetDefaultDataClass() const
{
	return UVaelPreacherData::StaticClass();
}

void AVaelPreacher::TickBehavior(float DeltaSeconds)
{
	const UVaelPreacherData* Data = GetData<UVaelPreacherData>();

	// Calling the spikes: stands still until the windup is over
	if (WindupRemaining > 0.0f)
	{
		WindupRemaining -= DeltaSeconds;
		if (WindupRemaining <= 0.0f)
		{
			// Whoever stood next to the line has seen how the preacher calls the bones of the earth
			bool bWitnessed = false;
			ForEachActivePlayer([this, Data, &bWitnessed](AVaelCharacter* Player)
			{
				const FVector Offset = (Player->GetActorLocation() - GetActorLocation()) * FVector(1.0f, 1.0f, 0.0f);
				const float Along = FVector::DotProduct(Offset, SpikeDirection);
				const float Across = FVector::CrossProduct(SpikeDirection, Offset).Size();

				bWitnessed |= Along > 0.0f && Along < SpikeLineLength + Data->SpikeSpacing && Across < Data->WitnessDistance;
			});

			if (bWitnessed)
			{
				TeachFormula(Data->FormulaOnWitnessedSpikes, LOCTEXT("FormulaFromSpikes", "Du hast gesehen, wie der Prediger die Knochen der Erde ruft."));
			}
		}
		return;
	}

	float TargetDistance = 0.0f;
	AVaelCharacter* Target = FindNearestPlayer(Data->AggroRange, &TargetDistance);

	if (Target == nullptr)
	{
		if (FVector::Dist2D(GetActorLocation(), HomeLocation) > PreacherHomeTolerance)
		{
			FaceTowards(HomeLocation);
			MoveTowards(HomeLocation, Data->MoveSpeed * Data->ReturnSpeedScale);
		}
		return;
	}

	FaceTowards(Target->GetActorLocation());

	// Keeps between the minimum and maximum distance and walks sideways
	const FVector ToTarget = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	const FVector Side(-ToTarget.Y, ToTarget.X, 0.0f);
	const float Approach = TargetDistance < Data->MinDistance ? -1.0f : TargetDistance > Data->MaxDistance ? 1.0f : 0.0f;
	const float Strafe = FMath::Sin(GetWorld()->GetTimeSeconds() * StrafeFrequency + StrafePhase) * Data->StrafeScale;

	const FVector Move = ToTarget * Approach + Side * Strafe;
	MoveInDirection(Move, Data->MoveSpeed * FMath::Min(1.0f, Move.Size()));

	BoltCooldown -= DeltaSeconds;
	if (BoltCooldown <= 0.0f)
	{
		BoltCooldown = RandomInRange(Data->BoltInterval);
		ShootBolt(Target);
	}

	SpikeCooldown -= DeltaSeconds;
	if (SpikeCooldown <= 0.0f && TargetDistance < Data->SpikeRange)
	{
		SpikeCooldown = RandomInRange(Data->SpikeInterval);
		StartSpikeLine(Target);
	}
}

void AVaelPreacher::ShootBolt(const AActor* Target)
{
	const UVaelPreacherData* Data = GetData<UVaelPreacherData>();
	const FVector Direction = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();

	FVaelSpellHit Hit;
	Hit.Damage = Data->BoltDamage;
	Hit.Element = EVaelElement::Mark;

	AVaelSpellProjectile::Launch(this, GetAttackLocation(Direction), Direction, Hit, Data->BoltSpeed, Data->BoltRadius, Data->BoltLifetime,
		UVaelMagicSettings::Get()->GetElementColor(EVaelElement::Mark));
}

void AVaelPreacher::StartSpikeLine(const AActor* Target)
{
	const UVaelPreacherData* Data = GetData<UVaelPreacherData>();
	const FVector Start = GetActorLocation();

	SpikeDirection = (Target->GetActorLocation() - Start).GetSafeNormal2D();
	SpikeLineLength = 0.0f;
	WindupRemaining = Data->SpikeWindup;

	FVaelSpellHit Hit;
	Hit.Damage = Data->SpikeDamage;
	Hit.Element = EVaelElement::Mark;

	const FLinearColor SpikeColor(0.79f, 0.73f, 0.63f);

	for (int32 SpikeIndex = 1; SpikeIndex <= Data->SpikeCount; ++SpikeIndex)
	{
		const FVector Point = Start + SpikeDirection * Data->SpikeSpacing * SpikeIndex;

		// The line ends at walls and rocks
		FHitResult WallHit;
		if (GetWorld()->LineTraceSingleByObjectType(WallHit, Start, Point, FCollisionObjectQueryParams(ECC_WorldStatic)))
		{
			break;
		}

		FVector Ground;
		if (!FindGround(GetWorld(), Point, Ground))
		{
			break;
		}

		AVaelGroundStrike::SpawnStrike(this, Ground, Hit, Data->SpikeRadius, Data->SpikeWindup + Data->SpikeStagger * SpikeIndex, SpikeColor);
		SpikeLineLength = Data->SpikeSpacing * SpikeIndex;
	}

	UE_LOG(LogVael, Verbose, TEXT("'%s' calls a spike line of %.0f cm"), *GetNameSafe(this), SpikeLineLength);
}

#undef LOCTEXT_NAMESPACE
