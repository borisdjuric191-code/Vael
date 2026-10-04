// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelFormulaAbility.h"
#include "AbilitySystemComponent.h"
#include "Combat/VaelAttributeSet.h"
#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelCombatStatics.h"
#include "Combat/VaelGameplayEffects.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Magic/VaelElementComponent.h"
#include "Magic/VaelFormula.h"
#include "Magic/VaelGameplayTags.h"
#include "Magic/VaelGroundArea.h"
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

	case EVaelSpellDelivery::Chain:
		HitChain(Formula, Caster, Power);
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
		Projectile->InitSpell(Formula.MakeSpellHit(Power), Formula.ProjectileSpeed, Formula.ProjectileRadius, Formula.ProjectileLifetime, Formula.ProjectilePierce,
			UVaelMagicSettings::Get()->GetElementColor(Formula.DamageElement));
		Projectile->FinishSpawning(SpawnTransform);
	}
}

void UVaelFormulaAbility::HitChain(const UVaelFormula& Formula, AActor* Caster, float Power)
{
	UWorld* World = Caster->GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const FVaelSpellHit Hit = Formula.MakeSpellHit(Power);
	const FVector AimDirection = GetAimDirection(Caster);
	const float MinAimCosine = FMath::Cos(FMath::DegreesToRadians(Formula.ChainAimHalfAngle));

	// Everything the caster may hurt is a possible link of the chain
	TArray<AVaelCharacterBase*> Candidates;
	for (TActorIterator<AVaelCharacterBase> It(World); It; ++It)
	{
		if (UVaelCombatStatics::CanDamage(Caster, *It))
		{
			Candidates.Add(*It);
		}
	}

	// The first link is the nearest enemy roughly in the aim direction
	FVector LinkStart = Caster->GetActorLocation();
	AVaelCharacterBase* Target = nullptr;
	float BestDistance = Formula.ChainRange;

	for (AVaelCharacterBase* Candidate : Candidates)
	{
		const FVector ToCandidate = Candidate->GetActorLocation() - LinkStart;
		const float Distance = ToCandidate.Size2D();

		if (Distance <= BestDistance && FVector::DotProduct(ToCandidate.GetSafeNormal2D(), AimDirection) >= MinAimCosine)
		{
			BestDistance = Distance;
			Target = Candidate;
		}
	}

	const FColor Color = UVaelMagicSettings::Get()->GetElementColor(Formula.DamageElement).ToFColor(true);

	if (Target == nullptr)
	{
#if ENABLE_DRAW_DEBUG
		// Placeholder look: the bolt fizzles out in the aim direction
		DrawDebugLine(World, LinkStart, LinkStart + AimDirection * Formula.ChainJumpRange, Color, false, 0.2f, 0, 3.0f);
#endif
		return;
	}

	for (int32 NumHit = 0; NumHit < Formula.ChainMaxTargets && Target != nullptr; ++NumHit)
	{
		const FVector TargetLocation = Target->GetActorLocation();

#if ENABLE_DRAW_DEBUG
		DrawDebugLine(World, LinkStart, TargetLocation, Color, false, 0.25f, 0, 4.0f);
#endif

		UVaelCombatStatics::ApplySpellHit(Caster, Target, Hit, TargetLocation - LinkStart);
		Candidates.RemoveSingleSwap(Target);

		// Jump on to the nearest enemy that hasn't been hit yet
		LinkStart = TargetLocation;
		Target = nullptr;
		BestDistance = Formula.ChainJumpRange;

		for (AVaelCharacterBase* Candidate : Candidates)
		{
			const float Distance = FVector::Dist2D(Candidate->GetActorLocation(), LinkStart);
			if (Distance <= BestDistance)
			{
				BestDistance = Distance;
				Target = Candidate;
			}
		}
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

	const FVaelSpellHit Hit = Formula.MakeSpellHit(Power);

	// Wind carries fires on, water puts them out
	if (Formula.DamageElement == EVaelElement::Air)
	{
		AVaelGroundArea::SpreadFires(Cast<APawn>(Caster), Origin, AimDirection, Formula.ConeRange, Formula.ConeHalfAngle);
	}
	else if (Formula.DamageElement == EVaelElement::Water)
	{
		AVaelGroundArea::ExtinguishFires(World, Origin + AimDirection * Formula.ConeRange * 0.5f, Formula.ConeRange * 0.5f);
	}

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
