// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VaelCombatStatics.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelGameplayEffects.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Magic/VaelGameplayTags.h"
#include "Magic/VaelMagicSettings.h"
#include "UI/VaelCombatTextSubsystem.h"
#include "Vael.h"

namespace
{
	UAbilitySystemComponent* GetAbilitySystem(const AActor* Actor)
	{
		return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	}

	/** The attacker may be gone by the time an effect lands; the target then applies the effect to itself */
	UAbilitySystemComponent* GetSourceAbilitySystem(const AActor* Attacker, UAbilitySystemComponent* TargetAbilitySystem)
	{
		UAbilitySystemComponent* SourceAbilitySystem = GetAbilitySystem(Attacker);
		return SourceAbilitySystem != nullptr ? SourceAbilitySystem : TargetAbilitySystem;
	}
}

bool UVaelCombatStatics::CanDamage(const AActor* Attacker, const AActor* Target)
{
	if (Attacker == nullptr || Target == nullptr || Attacker == Target)
	{
		return false;
	}

	// Nothing hurts a dodging player or a creature that is already dead
	const AVaelCharacterBase* TargetCharacter = Cast<AVaelCharacterBase>(Target);
	if (TargetCharacter != nullptr && (TargetCharacter->IsInvulnerable() || TargetCharacter->IsDefeated()))
	{
		return false;
	}

	const APawn* AttackerPawn = Cast<APawn>(Attacker);
	const APawn* TargetPawn = Cast<APawn>(Target);

	const bool bAttackerIsPlayer = AttackerPawn != nullptr && AttackerPawn->IsPlayerControlled();
	const bool bTargetIsPlayer = TargetPawn != nullptr && TargetPawn->IsPlayerControlled();

	return bAttackerIsPlayer != bTargetIsPlayer;
}

bool UVaelCombatStatics::ApplySpellHit(AActor* Attacker, AActor* Target, const FVaelSpellHit& Hit, const FVector& KnockbackDirection)
{
	if (GetAbilitySystem(Target) == nullptr || !CanDamage(Attacker, Target))
	{
		return false;
	}

	return ApplyHit(Attacker, Target, Hit, KnockbackDirection);
}

bool UVaelCombatStatics::ApplyNatureHit(AActor* Target, const FVaelSpellHit& Hit, const FVector& KnockbackDirection)
{
	const AVaelCharacterBase* TargetCharacter = Cast<AVaelCharacterBase>(Target);
	if (GetAbilitySystem(Target) == nullptr || (TargetCharacter != nullptr && (TargetCharacter->IsInvulnerable() || TargetCharacter->IsDefeated())))
	{
		return false;
	}

	return ApplyHit(nullptr, Target, Hit, KnockbackDirection);
}

int32 UVaelCombatStatics::ApplySpellHitInRadius(AActor* Attacker, const FVector& Center, float Radius, const FVaelSpellHit& Hit)
{
	UWorld* World = Attacker != nullptr ? Attacker->GetWorld() : nullptr;
	if (World == nullptr || Radius <= 0.0f)
	{
		return 0;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VaelSpellRadius), false, Attacker);
	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(Radius), QueryParams);

	// A pawn can overlap with several components, it is hit only once
	TSet<AActor*> HitActors;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		if (Target != nullptr && !HitActors.Contains(Target) && ApplySpellHit(Attacker, Target, Hit, Target->GetActorLocation() - Center))
		{
			HitActors.Add(Target);
		}
	}

	return HitActors.Num();
}

bool UVaelCombatStatics::ApplyHit(AActor* Attacker, AActor* Target, const FVaelSpellHit& Hit, const FVector& KnockbackDirection)
{
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();

	// Reactions between the hit and the conditions of the target
	float DamageMultiplier = 1.0f;
	const TCHAR* Reaction = TEXT("");

	if (Hit.bLightning && HasStatus(Target, EVaelStatus::Wet))
	{
		DamageMultiplier *= MagicSettings->LightningOnWetMultiplier;
		Reaction = TEXT(", overloaded");
		UVaelCombatTextSubsystem::PostReaction(Target, NSLOCTEXT("VaelHUD", "Overloaded", "\u00DCberladen!"));
	}

	if (Hit.Element == EVaelElement::Earth && RemoveStatus(Target, EVaelStatus::Frozen))
	{
		DamageMultiplier *= MagicSettings->EarthOnFrozenMultiplier;
		Reaction = TEXT(", shattered");
		UVaelCombatTextSubsystem::PostReaction(Target, NSLOCTEXT("VaelHUD", "Shattered", "Zerschmettert!"));
	}

	if (Hit.Element == EVaelElement::Fire && RemoveStatus(Target, EVaelStatus::Wet))
	{
		DamageMultiplier *= MagicSettings->FireOnWetMultiplier;
		Reaction = TEXT(", dried");
	}

	if (Hit.Element == EVaelElement::Water)
	{
		if (RemoveStatus(Target, EVaelStatus::Burning))
		{
			Reaction = TEXT(", extinguished");
			UVaelCombatTextSubsystem::PostReaction(Target, NSLOCTEXT("VaelHUD", "Extinguished", "Gel\u00F6scht!"));
		}

		// Water leaves its target wet even if the formula names no condition, unless it comes as ice
		if (Hit.Status == EVaelStatus::None)
		{
			ApplyStatus(Attacker, Target, EVaelStatus::Wet, MagicSettings->DefaultWetDuration);
		}
	}

	DealDamage(Attacker, Target, Hit.Damage * DamageMultiplier, Hit.Element, DamageMultiplier);

	ApplyStatus(Attacker, Target, Hit.Status, Hit.StatusDuration, Hit.StatusDamagePerSecond);

	if (AVaelCharacterBase* TargetCharacter = Cast<AVaelCharacterBase>(Target))
	{
		TargetCharacter->ApplyKnockback(KnockbackDirection, Hit.Knockback);

		UE_LOG(LogVael, Verbose, TEXT("'%s' hits '%s': %.1f %s damage%s, health now %.1f, conditions: %s"), *GetNameSafe(Attacker), *GetNameSafe(Target),
			Hit.Damage * DamageMultiplier, *VaelTags::GetElementTag(Hit.Element).ToString(), Reaction, TargetCharacter->GetHealth(), *TargetCharacter->GetStatusText().ToString());
	}

	return true;
}

void UVaelCombatStatics::DealDamage(AActor* Attacker, AActor* Target, float Damage, EVaelElement Element, float ReactionMultiplier)
{
	UAbilitySystemComponent* TargetAbilitySystem = GetAbilitySystem(Target);
	if (TargetAbilitySystem == nullptr || Damage <= 0.0f)
	{
		return;
	}

	UAbilitySystemComponent* SourceAbilitySystem = GetSourceAbilitySystem(Attacker, TargetAbilitySystem);

	FGameplayEffectContextHandle Context = SourceAbilitySystem->MakeEffectContext();
	Context.AddInstigator(Attacker, Attacker);

	const FGameplayEffectSpecHandle DamageSpec = SourceAbilitySystem->MakeOutgoingSpec(UVaelGE_Damage::StaticClass(), 1.0f, Context);
	if (DamageSpec.IsValid())
	{
		DamageSpec.Data->SetSetByCallerMagnitude(VaelTags::SetByCaller_Damage, Damage);
		DamageSpec.Data->SetSetByCallerMagnitude(VaelTags::SetByCaller_Multiplier, ReactionMultiplier);
		DamageSpec.Data->AddDynamicAssetTag(VaelTags::GetElementTag(Element));

		SourceAbilitySystem->ApplyGameplayEffectSpecToTarget(*DamageSpec.Data, TargetAbilitySystem);
	}
}

bool UVaelCombatStatics::HasStatus(const AActor* Target, EVaelStatus Status)
{
	const UAbilitySystemComponent* TargetAbilitySystem = GetAbilitySystem(Target);
	const FGameplayTag StatusTag = VaelTags::GetStatusTag(Status);

	return TargetAbilitySystem != nullptr && StatusTag.IsValid() && TargetAbilitySystem->HasMatchingGameplayTag(StatusTag);
}

void UVaelCombatStatics::ApplyStatus(AActor* Attacker, AActor* Target, EVaelStatus Status, float Duration, float DamagePerSecond)
{
	UAbilitySystemComponent* TargetAbilitySystem = GetAbilitySystem(Target);
	const FGameplayTag StatusTag = VaelTags::GetStatusTag(Status);

	if (TargetAbilitySystem == nullptr || !StatusTag.IsValid() || Duration <= 0.0f)
	{
		return;
	}

	const AVaelCharacterBase* TargetCharacter = Cast<AVaelCharacterBase>(Target);
	if (TargetCharacter != nullptr && !TargetCharacter->CanReceiveStatus(Status))
	{
		return;
	}

	// One effect per condition: renew it and keep the longer of the two durations
	const FGameplayEffectQuery StatusQuery = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(StatusTag));
	for (const float TimeRemaining : TargetAbilitySystem->GetActiveEffectsTimeRemaining(StatusQuery))
	{
		Duration = FMath::Max(Duration, TimeRemaining);
	}
	TargetAbilitySystem->RemoveActiveEffects(StatusQuery);

	UAbilitySystemComponent* SourceAbilitySystem = GetSourceAbilitySystem(Attacker, TargetAbilitySystem);

	FGameplayEffectContextHandle Context = SourceAbilitySystem->MakeEffectContext();
	Context.AddInstigator(Attacker, Attacker);

	const bool bBurning = Status == EVaelStatus::Burning;
	const TSubclassOf<UGameplayEffect> EffectClass = bBurning ? TSubclassOf<UGameplayEffect>(UVaelGE_Burning::StaticClass()) : TSubclassOf<UGameplayEffect>(UVaelGE_Status::StaticClass());

	const FGameplayEffectSpecHandle StatusSpec = SourceAbilitySystem->MakeOutgoingSpec(EffectClass, 1.0f, Context);
	if (StatusSpec.IsValid())
	{
		StatusSpec.Data->SetSetByCallerMagnitude(VaelTags::SetByCaller_Duration, Duration);
		StatusSpec.Data->DynamicGrantedTags.AddTag(StatusTag);

		if (bBurning)
		{
			StatusSpec.Data->SetSetByCallerMagnitude(VaelTags::SetByCaller_Damage, DamagePerSecond * UVaelGE_Burning::StepInterval);
			StatusSpec.Data->AddDynamicAssetTag(VaelTags::Element_Fire);
		}

		SourceAbilitySystem->ApplyGameplayEffectSpecToTarget(*StatusSpec.Data, TargetAbilitySystem);
	}
}

bool UVaelCombatStatics::RemoveStatus(AActor* Target, EVaelStatus Status)
{
	UAbilitySystemComponent* TargetAbilitySystem = GetAbilitySystem(Target);
	const FGameplayTag StatusTag = VaelTags::GetStatusTag(Status);

	if (TargetAbilitySystem == nullptr || !StatusTag.IsValid())
	{
		return false;
	}

	return TargetAbilitySystem->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(StatusTag)) > 0;
}

