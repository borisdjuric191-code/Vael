// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelSourceGuardian.h"
#include "AbilitySystemComponent.h"
#include "Combat/VaelAttributeSet.h"
#include "Combat/VaelCombatStatics.h"
#include "Combat/VaelGroundStrike.h"
#include "Combat/VaelHitFeedbackSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Creatures/VaelCreatureData.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Magic/VaelMagicSettings.h"
#include "Player/VaelCharacter.h"
#include "UI/VaelCombatTextSubsystem.h"
#include "UI/VaelNoticeSubsystem.h"
#include "Vael.h"
#include "World/VaelMarkSource.h"

#define LOCTEXT_NAMESPACE "VaelCreatures"

namespace
{
	/** It follows its target across the whole area of the source */
	constexpr float GuardianPursuitRange = 100000.0f;

	/** Extra reach of a dive beyond the touching capsules, in cm */
	constexpr float GuardianDiveReach = 20.0f;

	/** Swarm creatures appear this far from the middle of the source, in cm */
	constexpr float SwarmSpawnDistance = 200.0f;

	/** Color of the warning of the ember burst */
	const FLinearColor BurstColor(1.0f, 0.42f, 0.1f);
}

AVaelSourceGuardian* AVaelSourceGuardian::RiseFromSource(AVaelMarkSource* InSource, UVaelCreatureData* Data)
{
	UWorld* World = InSource != nullptr ? InSource->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}

	if (Data == nullptr)
	{
		Data = GetMutableDefault<UVaelSourceGuardianData>();
	}

	FVector Ground = InSource->GetActorLocation();
	FindGround(World, Ground, Ground);

	AVaelSourceGuardian* Guardian = Cast<AVaelSourceGuardian>(SpawnCreature(World, Data, Ground));
	if (Guardian != nullptr)
	{
		Guardian->Source = InSource;
		Guardian->Rise();
	}

	return Guardian;
}

TSubclassOf<UVaelCreatureData> AVaelSourceGuardian::GetDefaultDataClass() const
{
	return UVaelSourceGuardianData::StaticClass();
}

void AVaelSourceGuardian::Rise()
{
	const UVaelSourceGuardianData* Data = GetData<UVaelSourceGuardianData>();

	bRising = true;
	RiseTime = 0.0f;

	// Tougher with every player in the game, like the other bosses
	const int32 NumPlayers = FMath::Max(1, GetWorld()->GetNumPlayerControllers());
	const float ScaledMaxHealth = Data->MaxHealth * (Data->HealthScaleBase + Data->HealthScalePerPlayer * NumPlayers);

	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent();
	AbilitySystem->SetNumericAttributeBase(UVaelAttributeSet::GetMaxHealthAttribute(), ScaledMaxHealth);
	AbilitySystem->SetNumericAttributeBase(UVaelAttributeSet::GetHealthAttribute(), ScaledMaxHealth);

	UVaelHitFeedbackSubsystem::Shake(this, 0.8f);

	UE_LOG(LogVael, Log, TEXT("'%s' rises with %.0f health for %d players"), *GetNameSafe(this), ScaledMaxHealth, NumPlayers);

	UVaelNoticeSubsystem::Post(this, FText::Format(LOCTEXT("GuardianRises", "{0} erhebt sich"), GetCreatureName()),
		LOCTEXT("GuardianRisesDetail", "Die Wesen der Quelle, vom Mark zu einem verschmolzen. Fällt es, schließt sich die Wunde."), UVaelMagicSettings::Get()->GetElementColor(EVaelElement::Mark), 6.0f);
}

void AVaelSourceGuardian::OnHealthChanged(float OldValue, float NewValue)
{
	Super::OnHealthChanged(OldValue, NewValue);

	// Each share of health it loses lets it scream
	const UVaelSourceGuardianData* Data = GetData<UVaelSourceGuardianData>();
	if (IsDead() || NewValue >= OldValue || !Data->ScreamHealthShares.IsValidIndex(NumScreams))
	{
		return;
	}

	if (NewValue < GetMaxHealth() * Data->ScreamHealthShares[NumScreams])
	{
		Scream();
	}
}

void AVaelSourceGuardian::Die()
{
	Super::Die();

	UVaelHitFeedbackSubsystem::Shake(this, 1.0f);

	// Its fall closes the wound
	if (AVaelMarkSource* SealedSource = Source.Get())
	{
		SealedSource->Seal();
	}
}

void AVaelSourceGuardian::TickBehavior(float DeltaSeconds)
{
	const UVaelSourceGuardianData* Data = GetData<UVaelSourceGuardianData>();

	if (bRising)
	{
		RiseTime += DeltaSeconds;
		if (RiseTime >= Data->RiseDuration)
		{
			bRising = false;
			BurstCooldown = Data->BurstInterval * 0.5f;
			DiveCooldown = RandomInRange(Data->DiveInterval);
			SwarmCooldown = 2.0f;
		}
		return;
	}

	if (bDiving)
	{
		TickDive(DeltaSeconds);
		return;
	}

	float TargetDistance = 0.0f;
	AActor* Target = FindTarget(GuardianPursuitRange, &TargetDistance);
	if (Target == nullptr)
	{
		return;
	}

	// Walks up and strikes
	StrikeCooldown -= DeltaSeconds;
	if (TargetDistance > Data->StrikeRange)
	{
		MoveTowards(Target->GetActorLocation(), Data->MoveSpeed);
	}
	else
	{
		FaceTowards(Target->GetActorLocation());

		if (StrikeCooldown <= 0.0f && !IsBlinded())
		{
			StrikeCooldown = Data->StrikeInterval;
			HitPlayer(Target, Data->StrikeDamage, EVaelElement::Mark);
		}
	}

	BurstCooldown -= DeltaSeconds;
	if (BurstCooldown <= 0.0f && !IsBlinded())
	{
		BurstCooldown = Data->BurstInterval;
		StartBurst();
	}

	SwarmCooldown -= DeltaSeconds;
	if (SwarmCooldown <= 0.0f)
	{
		SwarmCooldown = FMath::Max(Data->SwarmInterval - NumScreams * Data->SwarmIntervalReductionPerScream, 2.0f);
		CallSwarm();
	}

	DiveCooldown -= DeltaSeconds;
	if (DiveCooldown <= 0.0f && TargetDistance < Data->DiveRange && !IsBlinded())
	{
		DiveCooldown = RandomInRange(Data->DiveInterval);

		bDiving = true;
		DiveTime = 0.0f;
		DiveDirection = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		DiveHits.Reset();
		FaceTowards(Target->GetActorLocation());
	}
}

void AVaelSourceGuardian::TickDive(float DeltaSeconds)
{
	const UVaelSourceGuardianData* Data = GetData<UVaelSourceGuardianData>();

	DiveTime += DeltaSeconds;

	// Warning: stands still and shows where it will rush
	if (DiveTime < Data->DiveWarning)
	{
#if ENABLE_DRAW_DEBUG
		const FVector Start = GetActorLocation();
		DrawDebugLine(GetWorld(), Start, Start + DiveDirection * Data->DiveSpeed * Data->DiveDuration, FColor(214, 200, 175), false, -1.0f, 0, 8.0f);
#endif
		return;
	}

	if (DiveTime >= Data->DiveWarning + Data->DiveDuration)
	{
		bDiving = false;
		return;
	}

	// Full speed from the first moment of the rush
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (Movement->Velocity.Size2D() < Data->DiveSpeed * 0.5f)
	{
		Movement->Velocity = DiveDirection * Data->DiveSpeed;
	}

	MoveInDirection(DiveDirection, Data->DiveSpeed);

	const float GuardianRadius = GetCapsuleComponent()->GetScaledCapsuleRadius();
	ForEachActivePlayer([this, Data, GuardianRadius](AVaelCharacter* Player)
	{
		if (!DiveHits.Contains(Player) && GetDistanceTo2D(Player) < GuardianRadius + Player->GetCapsuleComponent()->GetScaledCapsuleRadius() + GuardianDiveReach)
		{
			DiveHits.Add(Player);
			HitPlayer(Player, Data->DiveDamage, EVaelElement::Air);
		}
	});
}

void AVaelSourceGuardian::StartBurst()
{
	const UVaelSourceGuardianData* Data = GetData<UVaelSourceGuardianData>();

	FVaelSpellHit Hit;
	Hit.Damage = Data->BurstDamage * GetOutgoingDamageMultiplier();
	Hit.Element = EVaelElement::Fire;

	const FVector Feet = GetActorLocation() - FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	AVaelGroundStrike::SpawnStrike(this, Feet, Hit, Data->BurstRadius, Data->BurstWarning, BurstColor);
}

void AVaelSourceGuardian::CallSwarm()
{
	const UVaelSourceGuardianData* Data = GetData<UVaelSourceGuardianData>();

	// The swarm only grows back once players have thinned it out
	Swarm.RemoveAll([](const TWeakObjectPtr<AVaelCreature>& Member) { return !Member.IsValid() || Member->IsDead(); });
	const int32 NumToCall = FMath::Min(Data->SwarmCount + NumScreams * Data->SwarmCountPerScream, Data->SwarmMaxAlive - Swarm.Num());
	if (NumToCall <= 0)
	{
		return;
	}

	// Ember crawlers and ash harpies, the creatures it is made of, unless the data names others
	TArray<UVaelCreatureData*> Kinds;
	for (UVaelCreatureData* Kind : Data->SwarmData)
	{
		if (Kind != nullptr)
		{
			Kinds.Add(Kind);
		}
	}
	if (Kinds.IsEmpty())
	{
		Kinds = { GetMutableDefault<UVaelEmberCrawlerData>(), GetMutableDefault<UVaelAshHarpyData>() };
	}

	const FVector Center = Source.IsValid() ? Source->GetActorLocation() : GetActorLocation();

	for (int32 Index = 0; Index < NumToCall; ++Index)
	{
		const float Angle = FMath::FRandRange(0.0f, UE_TWO_PI);
		const FVector Location = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * SwarmSpawnDistance;

		FVector Ground;
		if (!FindGround(GetWorld(), Location, Ground))
		{
			continue;
		}

		UVaelCreatureData* Kind = Kinds[NextSwarmKind++ % Kinds.Num()];
		if (AVaelCreature* Member = SpawnCreature(GetWorld(), Kind, Ground, FRotator(0.0f, FMath::RadiansToDegrees(Angle), 0.0f)))
		{
			Swarm.Add(Member);
		}
	}

	UE_LOG(LogVael, Verbose, TEXT("'%s' calls %d creatures of its swarm"), *GetNameSafe(this), NumToCall);
}

void AVaelSourceGuardian::Scream()
{
	const UVaelSourceGuardianData* Data = GetData<UVaelSourceGuardianData>();

	++NumScreams;

	// The scream throws everyone close back
	ForEachActivePlayer([this, Data](AVaelCharacter* Player)
	{
		if (GetDistanceTo2D(Player) < Data->ScreamRadius)
		{
			Player->ApplyKnockback(Player->GetActorLocation() - GetActorLocation(), Data->ScreamKnockback);
		}
	});

	UVaelHitFeedbackSubsystem::Shake(this, 0.9f);
	UVaelCombatTextSubsystem::PostReaction(this, LOCTEXT("GuardianScreamShort", "Schrei!"));

	UVaelNoticeSubsystem::Post(this, FText::Format(LOCTEXT("GuardianScreams", "{0} schreit"), GetCreatureName()),
		LOCTEXT("GuardianScreamsDetail", "Schmerz, ein Ruf nach Hilfe und Wut aus der Tiefe. Der Schwarm wird dichter."), UVaelMagicSettings::Get()->GetElementColor(EVaelElement::Mark), 4.0f);

	// The swarm answers at once
	SwarmCooldown = 0.0f;
}

#undef LOCTEXT_NAMESPACE
