// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VaelCravingComponent.h"

#include "Combat/VaelCombatStatics.h"
#include "Creatures/VaelCreature.h"
#include "EngineUtils.h"
#include "Player/VaelCharacter.h"
#include "UI/VaelCombatTextSubsystem.h"
#include "Vael.h"
#include "World/VaelBalance.h"

#define LOCTEXT_NAMESPACE "VaelCraving"

namespace
{
	/** Enemies this close make it a fight */
	constexpr float FightReach = 1500.0f;
	constexpr float LookEvery = 0.25f;
}

FVaelCravingSpec VaelCravings::Hunger()
{
	FVaelCravingSpec Spec;
	Spec.Name = LOCTEXT("Hunger", "Hunger");
	Spec.Sin = EVaelSin::Gluttony;
	Spec.Color = FLinearColor(0.5f, 0.25f, 0.7f);
	return Spec;
}

FVaelCravingSpec VaelCravings::Thirst()
{
	// drinking: grows faster, a drink feeds less but heals more
	FVaelCravingSpec Spec;
	Spec.Name = LOCTEXT("Thirst", "Durst");
	Spec.Sin = EVaelSin::Gluttony;
	Spec.GrowthPerSecond = 0.045f;
	Spec.FeedPerKill = 0.3f;
	Spec.HealPerFeed = 10.0f;
	Spec.Color = FLinearColor(0.75f, 0.08f, 0.12f);
	return Spec;
}

UVaelCravingComponent::UVaelCravingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = LookEvery;
}

AVaelCharacter* UVaelCravingComponent::GetPlayer() const
{
	return Cast<AVaelCharacter>(GetOwner());
}

void UVaelCravingComponent::SetCraving(const FVaelCravingSpec& InSpec)
{
	Spec = InSpec;
	bActive = true;
	Meter = 0.0f;
	bPossessed = false;
	NearAlive.Reset();
	UE_LOG(LogVael, Log, TEXT("Craving: %s wakes on %s"), *Spec.Name.ToString(), *GetNameSafe(GetOwner()));
}

void UVaelCravingComponent::ClearCraving()
{
	bActive = false;
	bPossessed = false;
	Meter = 0.0f;
	NearAlive.Reset();
}

void UVaelCravingComponent::Feed(float Amount)
{
	if (!bActive || Amount <= 0.0f)
	{
		return;
	}
	Meter = FMath::Max(Meter - Amount, 0.0f);
	++Feedings;
	if (Spec.HealPerFeed > 0.0f)
	{
		UVaelCombatStatics::Heal(GetOwner(), Spec.HealPerFeed);
	}
	if (bPossessed && Meter < Spec.StarvingAbove)
	{
		bPossessed = false;
		UVaelCombatTextSubsystem::PostReaction(GetOwner(), LOCTEXT("Released", "Es lässt los"));
	}
}

bool UVaelCravingComponent::IsInFight() const
{
	const AActor* Owner = GetOwner();
	for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
	{
		if (!It->IsDead() && !It->IsOnPlayerSide() && FVector::Dist2D(It->GetActorLocation(), Owner->GetActorLocation()) < FightReach)
		{
			return true;
		}
	}
	return false;
}

void UVaelCravingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AVaelCharacter* Player = GetPlayer();
	if (!bActive || Player == nullptr)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	const float Corruption = UVaelBalanceSubsystem::GetPlayer(Player);

	// Deaths close by feed it: whoever was alive near him at the last look and lies dead now
	TSet<TWeakObjectPtr<AVaelCreature>> Alive;
	for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
	{
		if (It->IsOnPlayerSide() || FVector::Dist2D(It->GetActorLocation(), Player->GetActorLocation()) > Spec.FeedReach)
		{
			continue;
		}
		if (!It->IsDead())
		{
			Alive.Add(*It);
		}
		else if (NearAlive.Contains(*It))
		{
			Feed(Spec.FeedPerKill);
			UE_LOG(LogVael, Log, TEXT("Craving: %s feeds on %s (meter %.2f)"), *Spec.Name.ToString(), *It->GetName(), Meter);
		}
	}
	NearAlive = MoveTemp(Alive);

	// It grows in a fight, the faster the more Nid is in him, and settles outside one
	if (IsInFight())
	{
		Meter = FMath::Min(Meter + Spec.GrowthPerSecond * (1.0f + Corruption / Spec.CorruptionScale) * DeltaTime, 1.0f);
	}
	else
	{
		Meter = FMath::Max(Meter - Spec.CalmPerSecond * DeltaTime, 0.0f);
	}

	// Starving it costs his corruption (never his life); starving with much Nid in him it takes him over
	if (IsStarving())
	{
		UVaelBalanceSubsystem::ChangePlayer(Player, Spec.CorruptionPerSecond * DeltaTime, Spec.Name);
	}
	const bool bWasPossessed = bPossessed;
	bPossessed = Meter >= 0.999f && UVaelBalanceSubsystem::GetPlayer(Player) >= Spec.PossessAtCorruption;
	if (bPossessed && !bWasPossessed)
	{
		UVaelCombatTextSubsystem::PostReaction(Player, FText::Format(LOCTEXT("Possessed", "{0} übernimmt"), Spec.Name));
		UE_LOG(LogVael, Log, TEXT("Craving: %s takes %s over (corruption %.0f)"), *Spec.Name.ToString(), *Player->GetName(), UVaelBalanceSubsystem::GetPlayer(Player));
	}

	// Taken over, the set lashes out at the nearest enemy by itself
	if (bPossessed && Now >= NextLash)
	{
		AVaelCreature* Nearest = nullptr;
		float Best = Spec.LashReach;
		for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
		{
			const float Distance = FVector::Dist2D(It->GetActorLocation(), Player->GetActorLocation()) - It->GetSimpleCollisionRadius();
			if (!It->IsDead() && !It->IsOnPlayerSide() && Distance < Best)
			{
				Best = Distance;
				Nearest = *It;
			}
		}
		if (Nearest != nullptr)
		{
			NextLash = Now + Spec.LashEvery;
			FVaelSpellHit Lash;
			Lash.Damage = Spec.LashDamage;
			Lash.Element = EVaelElement::Earth;
			Lash.Knockback = 200.0f;
			UVaelCombatStatics::ApplySpellHit(Player, Nearest, Lash, (Nearest->GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D());
			++Lashes;
		}
	}
}

#undef LOCTEXT_NAMESPACE
