// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VaelCombatStatics.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelGameplayEffects.h"
#include "GameFramework/Pawn.h"
#include "Magic/VaelGameplayTags.h"
#include "Vael.h"

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
	UAbilitySystemComponent* TargetAbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (TargetAbilitySystem == nullptr || !CanDamage(Attacker, Target))
	{
		return false;
	}

	// The attacker may be gone by the time a projectile lands; the target then applies the effect to itself
	UAbilitySystemComponent* SourceAbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Attacker);
	if (SourceAbilitySystem == nullptr)
	{
		SourceAbilitySystem = TargetAbilitySystem;
	}

	if (Hit.Damage > 0.0f)
	{
		FGameplayEffectContextHandle Context = SourceAbilitySystem->MakeEffectContext();
		Context.AddInstigator(Attacker, Attacker);

		const FGameplayEffectSpecHandle DamageSpec = SourceAbilitySystem->MakeOutgoingSpec(UVaelGE_Damage::StaticClass(), 1.0f, Context);
		if (DamageSpec.IsValid())
		{
			DamageSpec.Data->SetSetByCallerMagnitude(VaelTags::SetByCaller_Damage, Hit.Damage);
			DamageSpec.Data->AddDynamicAssetTag(VaelTags::GetElementTag(Hit.Element));

			SourceAbilitySystem->ApplyGameplayEffectSpecToTarget(*DamageSpec.Data, TargetAbilitySystem);
		}
	}

	if (AVaelCharacterBase* TargetCharacter = Cast<AVaelCharacterBase>(Target))
	{
		TargetCharacter->ApplyKnockback(KnockbackDirection, Hit.Knockback);

		UE_LOG(LogVael, Verbose, TEXT("'%s' hits '%s': %.1f %s damage, health now %.1f"), *GetNameSafe(Attacker), *GetNameSafe(Target),
			Hit.Damage, *VaelTags::GetElementTag(Hit.Element).ToString(), TargetCharacter->GetHealth());
	}

	return true;
}
