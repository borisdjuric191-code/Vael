// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelFormulaAbility.h"
#include "AbilitySystemComponent.h"
#include "Combat/VaelAttributeSet.h"
#include "Combat/VaelCombatStatics.h"
#include "Combat/VaelGameplayEffects.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Magic/VaelElementComponent.h"
#include "Magic/VaelFormula.h"
#include "Magic/VaelGameplayTags.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelSpellProjectile.h"

namespace
{
	/** Distance in front of the caster at which projectiles appear */
	constexpr float ProjectileSpawnDistance = 75.0f;
}

UVaelFormulaAbility::UVaelFormulaAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

bool UVaelFormulaAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	const UVaelElementComponent* ElementComponent = GetElementComponent(ActorInfo);
	const UAbilitySystemComponent* AbilitySystem = ActorInfo != nullptr ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

	if (ElementComponent == nullptr || AbilitySystem == nullptr)
	{
		return false;
	}

	return AbilitySystem->GetNumericAttribute(UVaelAttributeSet::GetManaAttribute()) >= ElementComponent->GetPendingCast().ManaCost;
}

void UVaelFormulaAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	const UVaelElementComponent* ElementComponent = GetElementComponent(ActorInfo);
	if (ElementComponent == nullptr || ElementComponent->GetPendingCast().ManaCost <= 0.0f)
	{
		return;
	}

	const FGameplayEffectSpecHandle CostSpec = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, UVaelGE_ManaCost::StaticClass());
	if (CostSpec.IsValid())
	{
		CostSpec.Data->SetSetByCallerMagnitude(VaelTags::SetByCaller_Mana, -ElementComponent->GetPendingCast().ManaCost);
		ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, CostSpec);
	}
}

void UVaelFormulaAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const UVaelFormula* Formula = Cast<UVaelFormula>(GetCurrentSourceObject());
	const UVaelElementComponent* ElementComponent = GetElementComponent(ActorInfo);
	AActor* Caster = ActorInfo != nullptr ? ActorInfo->AvatarActor.Get() : nullptr;

	if (Formula == nullptr || ElementComponent == nullptr || Caster == nullptr || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ExecuteFormula(*Formula, Caster, ElementComponent->GetPendingCast().Power);

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UVaelFormulaAbility::ExecuteFormula(const UVaelFormula& Formula, AActor* Caster, float Power)
{
	switch (Formula.Delivery)
	{
	case EVaelSpellDelivery::Projectile:
		FireProjectile(Formula, Caster, Power);
		break;

	case EVaelSpellDelivery::Cone:
		HitCone(Formula, Caster, Power);
		break;
	}
}

void UVaelFormulaAbility::FireProjectile(const UVaelFormula& Formula, AActor* Caster, float Power)
{
	UWorld* World = Caster->GetWorld();
	if (World == nullptr || Formula.ProjectileClass == nullptr)
	{
		return;
	}

	const FVector AimDirection = GetAimDirection(Caster);
	const FTransform SpawnTransform(AimDirection.Rotation(), Caster->GetActorLocation() + AimDirection * ProjectileSpawnDistance);

	AVaelSpellProjectile* Projectile = World->SpawnActorDeferred<AVaelSpellProjectile>(Formula.ProjectileClass, SpawnTransform, Caster, Cast<APawn>(Caster), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Projectile != nullptr)
	{
		FVaelSpellHit Hit;
		Hit.Damage = Formula.Damage * Power;
		Hit.Element = Formula.DamageElement;
		Hit.Knockback = Formula.Knockback;

		Projectile->InitSpell(Hit, Formula.ProjectileSpeed, Formula.ProjectileRadius, Formula.ProjectileLifetime, UVaelMagicSettings::Get()->GetElementColor(Formula.DamageElement));
		Projectile->FinishSpawning(SpawnTransform);
	}
}

void UVaelFormulaAbility::HitCone(const UVaelFormula& Formula, AActor* Caster, float Power)
{
	UWorld* World = Caster->GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const FVector Origin = Caster->GetActorLocation();
	const FVector AimDirection = GetAimDirection(Caster);
	const float MinCosine = FMath::Cos(FMath::DegreesToRadians(Formula.ConeHalfAngle));

	FVaelSpellHit Hit;
	Hit.Damage = Formula.Damage * Power;
	Hit.Element = Formula.DamageElement;
	Hit.Knockback = Formula.Knockback;

	// Collect every pawn in reach, then keep those inside the cone
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VaelSpellCone), false, Caster);
	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(Formula.ConeRange), QueryParams);

	TSet<AActor*> HitActors;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		if (Target == nullptr || HitActors.Contains(Target))
		{
			continue;
		}

		const FVector ToTarget = (Target->GetActorLocation() - Origin).GetSafeNormal2D();
		if (FVector::DotProduct(ToTarget, AimDirection) < MinCosine)
		{
			continue;
		}

		HitActors.Add(Target);
		UVaelCombatStatics::ApplySpellHit(Caster, Target, Hit, AimDirection);
	}

#if ENABLE_DRAW_DEBUG
	// Placeholder look until the formulas get real effects
	const float HalfAngle = FMath::DegreesToRadians(Formula.ConeHalfAngle);
	DrawDebugCone(World, Origin, AimDirection, Formula.ConeRange, HalfAngle, FMath::DegreesToRadians(8.0f), 16,
		UVaelMagicSettings::Get()->GetElementColor(Formula.DamageElement).ToFColor(true), false, 0.25f, 0, 3.0f);
#endif
}

FVector UVaelFormulaAbility::GetAimDirection(const AActor* Caster)
{
	// The controller holds the aim direction, the body only turns towards it over time
	const APawn* CasterPawn = Cast<APawn>(Caster);
	const float AimYaw = CasterPawn != nullptr && CasterPawn->GetController() != nullptr ? CasterPawn->GetControlRotation().Yaw : Caster->GetActorRotation().Yaw;

	return FRotator(0.0f, AimYaw, 0.0f).Vector();
}

UVaelElementComponent* UVaelFormulaAbility::GetElementComponent(const FGameplayAbilityActorInfo* ActorInfo)
{
	const AActor* Avatar = ActorInfo != nullptr ? ActorInfo->AvatarActor.Get() : nullptr;
	return Avatar != nullptr ? Avatar->FindComponentByClass<UVaelElementComponent>() : nullptr;
}
