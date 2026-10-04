// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VaelCombatStatics.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelGameplayEffects.h"
#include "GameFramework/Pawn.h"
#include "Magic/VaelGameplayTags.h"
#include "Magic/VaelMagicSettings.h"
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

	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();

	// Reactions between the hit and the conditions of the target
	float DamageMultiplier = 1.0f;
	const TCHAR* Reaction = TEXT("");

	if (Hit.bLightning && HasStatus(Target, EVaelStatus::Wet))
	{
		DamageMultiplier *= MagicSettings->LightningOnWetMultiplier;
		Reaction = TEXT(", overloaded");
	}

	if (Hit.Element == EVaelElement::Earth && RemoveStatus(Target, EVaelStatus::Frozen))
	{
		DamageMultiplier *= MagicSettings->EarthOnFrozenMultiplier;
		Reaction = TEXT(", shattered");
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
		}

		// Water leaves its target wet even if the formula names no condition, unless it comes as ice
		if (Hit.Status == EVaelStatus::None)
		{
			ApplyStatus(Attacker, Target, EVaelStatus::Wet, MagicSettings->DefaultWetDuration);
		}
	}

	DealDamage(Attacker, Target, Hit.Damage * DamageMultiplier, Hit.Element);

	ApplyStatus(Attacker, Target, Hit.Status, Hit.StatusDuration, Hit.StatusDamagePerSecond);

	if (AVaelCharacterBase* TargetCharacter = Cast<AVaelCharacterBase>(Target))
	{
		TargetCharacter->ApplyKnockback(KnockbackDirection, Hit.Knockback);

		UE_LOG(LogVael, Verbose, TEXT("'%s' hits '%s': %.1f %s damage%s, health now %.1f, conditions: %s"), *GetNameSafe(Attacker), *GetNameSafe(Target),
			Hit.Damage * DamageMultiplier, *VaelTags::GetElementTag(Hit.Element).ToString(), Reaction, TargetCharacter->GetHealth(), *TargetCharacter->GetStatusText().ToString());
	}

	return true;
}

void UVaelCombatStatics::DealDamage(AActor* Attacker, AActor* Target, float Damage, EVaelElement Element)
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
