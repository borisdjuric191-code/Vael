// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/VaelAutoAimComponent.h"
#include "Combat/VaelCombatStatics.h"
#include "Creatures/VaelCreature.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"

UVaelAutoAimComponent::UVaelAutoAimComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UVaelAutoAimComponent::IsValidTarget(const AActor* Candidate, const APawn* Player, float MaxDistance)
{
	const AVaelCreature* Creature = Cast<AVaelCreature>(Candidate);
	return Creature != nullptr && !Creature->IsDead() && !Creature->IsOnPlayerSide() && UVaelCombatStatics::CanDamage(Player, Creature)
		&& FVector::Dist2D(Creature->GetActorLocation(), Player->GetActorLocation()) <= MaxDistance;
}

AActor* UVaelAutoAimComponent::FindInDirection(const APawn* Player, const FVector& Direction) const
{
	const float MinCos = FMath::Cos(FMath::DegreesToRadians(SwitchConeDegrees));
	const FVector Facing = Direction.GetSafeNormal2D();

	AActor* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();

	for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
	{
		if (!IsValidTarget(*It, Player, KeepRange))
		{
			continue;
		}

		const FVector ToCandidate = (It->GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D();
		const float Cos = FVector::DotProduct(Facing, ToCandidate);
		if (Cos < MinCos)
		{
			continue;
		}

		// Mostly the angle decides, distance breaks near ties
		const float Score = (1.0f - Cos) * 10.0f + FVector::Dist2D(It->GetActorLocation(), Player->GetActorLocation()) / KeepRange;
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = *It;
		}
	}

	return Best;
}

AActor* UVaelAutoAimComponent::FindClosest(const APawn* Player) const
{
	AActor* Best = nullptr;
	float BestDistance = AcquireRange;

	for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
	{
		const float Distance = FVector::Dist2D(It->GetActorLocation(), Player->GetActorLocation());
		if (Distance <= BestDistance && IsValidTarget(*It, Player, AcquireRange))
		{
			BestDistance = Distance;
			Best = *It;
		}
	}

	return Best;
}

void UVaelAutoAimComponent::UpdateTarget(float DeltaSeconds, const APawn* Player, const FVector& StickDirection, bool bEnabled)
{
	if (!bEnabled || Player == nullptr)
	{
		Target.Reset();
		SwitchTime = BreakTime = 0.0f;
		return;
	}

	// Dead, turned or too far: let go
	if (!IsValidTarget(Target.Get(), Player, KeepRange))
	{
		Target.Reset();
	}

	if (!StickDirection.IsNearlyZero())
	{
		// The stick points at another enemy: jump to it after a short flick
		AActor* Candidate = FindInDirection(Player, StickDirection);
		if (Candidate != nullptr && Candidate != Target.Get())
		{
			SwitchTime = Candidate == SwitchCandidate.Get() ? SwitchTime + DeltaSeconds : DeltaSeconds;
			SwitchCandidate = Candidate;

			if (SwitchTime >= SwitchHoldSeconds)
			{
				Target = Candidate;
				SwitchTime = BreakTime = 0.0f;
			}
		}
		else
		{
			SwitchTime = 0.0f;
			SwitchCandidate.Reset();
		}

		// The stick points away from the target, at no one: let go after a moment and aim freely
		if (AActor* Current = Target.Get())
		{
			const FVector ToTarget = (Current->GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D();
			const bool bAway = FVector::DotProduct(StickDirection.GetSafeNormal2D(), ToTarget) < FMath::Cos(FMath::DegreesToRadians(BreakAngleDegrees));

			BreakTime = bAway && Candidate == nullptr ? BreakTime + DeltaSeconds : 0.0f;
			if (BreakTime >= BreakHoldSeconds)
			{
				Target.Reset();
				BreakTime = 0.0f;
			}
		}

		// While the stick aims freely, nothing locks on by itself
		if (!Target.IsValid())
		{
			SuppressTime = ReacquireDelay;
		}

		return;
	}

	SwitchTime = BreakTime = 0.0f;
	SwitchCandidate.Reset();

	if (SuppressTime > 0.0f)
	{
		SuppressTime -= DeltaSeconds;
		return;
	}

	// An enemy came close: lock on
	if (!Target.IsValid())
	{
		Target = FindClosest(Player);
	}
}
