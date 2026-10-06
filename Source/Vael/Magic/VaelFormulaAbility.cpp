// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelFormulaAbility.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayTag.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Combat/VaelAttributeSet.h"
#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelCombatStatics.h"
#include "Combat/VaelGameplayEffects.h"
#include "Combat/VaelHitFeedbackSubsystem.h"
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
#include "Magic/VaelRockWall.h"
#include "Magic/VaelSpellAura.h"
#include "Magic/VaelSpellBeam.h"
#include "Magic/VaelSpellProjectile.h"
#include "Magic/VaelSpellVortex.h"
#include "Player/VaelCharacter.h"
#include "Player/VaelPlayerController.h"
#include "Vael.h"
#include "World/VaelGround.h"

namespace
{
	/** Distance in front of the caster at which projectiles appear */
	constexpr float ProjectileSpawnDistance = 75.0f;

	/** Ground areas aimed with a gamepad: enemies up to this distance and angle off the aim direction are targeted (prototype: 9 tiles) */
	constexpr float AutoTargetRange = 1260.0f;
	constexpr float AutoTargetHalfAngle = 29.0f;

	/** A targeted enemy closer than this still gets the patch at this distance (2 tiles) */
	constexpr float AutoTargetMinDistance = 280.0f;

	/** Distance of the patch without a target (4.5 tiles) */
	constexpr float AutoTargetDefaultDistance = 630.0f;

	/** How far in front of a wall a patch stays */
	constexpr float WallBackOffDistance = 56.0f;

	/** How far below the aimed point the ground is searched */
	constexpr float AreaGroundSearchDepth = 500.0f;

	/** How far above it the search starts, so rising ground like ramps is found too */
	constexpr float AreaGroundSearchHeight = 80.0f;

	/** Surfaces that face up at least this much are ramps or slopes, not walls (about 45 degrees) */
	constexpr float WalkableSlopeNormalZ = 0.7f;

	/** Camera shake when a wall rises, as in the prototype */
	constexpr float WallShake = 0.25f;
}

UVaelFormulaAbility::UVaelFormulaAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// Casting the same formula again during its animation releases the first spell and starts anew
	bRetriggerInstancedAbility = true;
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

	// The element component forgets the cast right after activation, the animation takes longer
	CastFormula = Formula;
	CastPower = ElementComponent->GetPendingCast().Power;
	bSpellReleased = false;
	bCastAnimated = false;
	bWaitingForChannel = false;

	const ACharacter* Character = Cast<ACharacter>(Caster);
	UAnimMontage* Montage = Formula->FindCastMontage();

	if (Montage == nullptr || Character == nullptr || Character->GetMesh()->GetAnimInstance() == nullptr)
	{
		// No animation yet: the spell appears at once
		ReleaseSpell();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	bCastAnimated = true;
	ActorInfo->AbilitySystemComponent->AddLooseGameplayTag(VaelTags::State_Casting);

	UAbilityTask_WaitGameplayEvent* WaitCastPoint = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, VaelTags::Event_CastPoint, nullptr, true, true);
	WaitCastPoint->EventReceived.AddDynamic(this, &UVaelFormulaAbility::OnCastPoint);
	WaitCastPoint->ReadyForActivation();

	UAbilityTask_PlayMontageAndWait* PlayMontage = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage, Formula->CastMontagePlayRate);
	PlayMontage->OnBlendOut.AddDynamic(this, &UVaelFormulaAbility::OnCastMontageEnded);
	PlayMontage->OnCompleted.AddDynamic(this, &UVaelFormulaAbility::OnCastMontageEnded);
	PlayMontage->OnInterrupted.AddDynamic(this, &UVaelFormulaAbility::OnCastMontageEnded);
	PlayMontage->OnCancelled.AddDynamic(this, &UVaelFormulaAbility::OnCastMontageEnded);
	PlayMontage->ReadyForActivation();
}

void UVaelFormulaAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// A cast cut short, for example by casting again, still releases its spell; the mana is paid already
	if (!bWasCancelled && !bSpellReleased && CastFormula != nullptr)
	{
		ReleaseSpell();
	}

	if (bCastAnimated && ActorInfo != nullptr && ActorInfo->AbilitySystemComponent.IsValid())
	{
		ActorInfo->AbilitySystemComponent->RemoveLooseGameplayTag(VaelTags::State_Casting);
	}

	CastFormula = nullptr;
	bCastAnimated = false;
	bWaitingForChannel = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UVaelFormulaAbility::OnCastPoint(FGameplayEventData Payload)
{
	ReleaseSpell();

	// A channeled spell keeps its loop running until it ends; everything else lets the animation play out
	WaitForChannelEnd();
}

void UVaelFormulaAbility::OnCastMontageEnded()
{
	if (!IsActive() || bWaitingForChannel)
	{
		return;
	}

	// An animation without a cast point releases the spell at its end
	ReleaseSpell();

	if (!WaitForChannelEnd())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UVaelFormulaAbility::OnChannelEnded()
{
	if (IsActive())
	{
		MontageStop();
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

bool UVaelFormulaAbility::WaitForChannelEnd()
{
	if (bWaitingForChannel)
	{
		return true;
	}

	const UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	if (AbilitySystem == nullptr || !AbilitySystem->HasMatchingGameplayTag(VaelTags::State_Channeling))
	{
		return false;
	}

	bWaitingForChannel = true;

	UAbilityTask_WaitGameplayTagRemoved* WaitChannel = UAbilityTask_WaitGameplayTagRemoved::WaitGameplayTagRemove(this, VaelTags::State_Channeling, nullptr, true);
	WaitChannel->Removed.AddDynamic(this, &UVaelFormulaAbility::OnChannelEnded);
	WaitChannel->ReadyForActivation();

	return true;
}

void UVaelFormulaAbility::ReleaseSpell()
{
	if (bSpellReleased || CastFormula == nullptr)
	{
		return;
	}

	bSpellReleased = true;

	// A caster who went down meanwhile casts nothing
	AActor* Caster = GetAvatarActorFromActorInfo();
	const AVaelCharacterBase* CasterCharacter = Cast<AVaelCharacterBase>(Caster);
	if (Caster == nullptr || (CasterCharacter != nullptr && CasterCharacter->IsDefeated()))
	{
		return;
	}

	ExecuteFormula(*CastFormula, Caster, CastPower);
}

FVector UVaelFormulaAbility::GetProjectileStart(const AActor* Caster, const FVector& AimDirection) const
{
	const FVector InFront = Caster->GetActorLocation() + AimDirection * ProjectileSpawnDistance;

	const ACharacter* Character = Cast<ACharacter>(Caster);
	const FName SocketName = UVaelMagicSettings::Get()->CastSocketName;

	if (!bCastAnimated || Character == nullptr || SocketName.IsNone() || !Character->GetMesh()->DoesSocketExist(SocketName))
	{
		return InFront;
	}

	// The spell leaves the hand, but never behind the caster, so it can't fly out of their back
	const FVector Hand = Character->GetMesh()->GetSocketLocation(SocketName);
	const float AlongAim = FVector::DotProduct(Hand - Caster->GetActorLocation(), AimDirection);

	return Hand + AimDirection * FMath::Max(0.0f, ProjectileSpawnDistance * 0.5f - AlongAim);
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

	case EVaelSpellDelivery::Explosion:
		FireProjectile(Formula, Caster, Power);
		break;

	case EVaelSpellDelivery::GroundArea:
		PlaceGroundArea(Formula, Caster, Power);
		break;

	case EVaelSpellDelivery::Dash:
		StartDash(Formula, Caster, Power);
		break;

	case EVaelSpellDelivery::Wall:
		RaiseWall(Formula, Caster, Power);
		break;

	case EVaelSpellDelivery::Beam:
		StartBeam(Formula, Caster, Power);
		break;

	case EVaelSpellDelivery::Nova:
		HitNova(Formula, Caster, Power);
		break;

	case EVaelSpellDelivery::Vortex:
		LaunchVortex(Formula, Caster, Power);
		break;

	case EVaelSpellDelivery::Aura:
		StartAura(Formula, Caster, Power);
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
	const FTransform SpawnTransform(AimDirection.Rotation(), GetProjectileStart(Caster, AimDirection));

	AVaelSpellProjectile* Projectile = World->SpawnActorDeferred<AVaelSpellProjectile>(Formula.ProjectileClass, SpawnTransform, Caster, Cast<APawn>(Caster), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Projectile != nullptr)
	{
		Projectile->InitSpell(Formula.MakeSpellHit(Power), Formula.ProjectileSpeed, Formula.ProjectileRadius, Formula.ProjectileLifetime, Formula.ProjectilePierce,
			UVaelMagicSettings::Get()->GetElementColor(Formula.DamageElement));

		if (Formula.bProjectilePassesWalls)
		{
			Projectile->SetPassesWalls();
		}

		if (Formula.Delivery == EVaelSpellDelivery::Explosion)
		{
			Projectile->SetExplosion(Formula.MakeExplosionHit(Power), Formula.ExplosionRadius);

			if (Formula.AreaRadius > 0.0f)
			{
				Projectile->SetImpactArea(Formula.DamageElement, Formula.AreaRadius, Formula.AreaLifetime, Formula.AreaDamagePerSecond * Power, Formula.AreaEffect);
			}
		}

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

void UVaelFormulaAbility::PlaceGroundArea(const UVaelFormula& Formula, AActor* Caster, float Power)
{
	UWorld* World = Caster->GetWorld();
	if (World == nullptr || Formula.AreaRadius <= 0.0f)
	{
		return;
	}

	FVector Location = FindGroundTarget(Caster, Formula.AreaRange);

	// The patch lies on the ground below the aimed point
	FHitResult GroundHit;
	if (VaelGround::TraceGround(World, Location + FVector(0.0f, 0.0f, AreaGroundSearchHeight), Location - FVector(0.0f, 0.0f, AreaGroundSearchDepth), GroundHit, Caster))
	{
		Location = GroundHit.Location;
	}
	else
	{
		Location.Z -= Caster->GetSimpleCollisionHalfHeight();
	}

	AVaelGroundArea::SpawnArea(World, Location + FVector(0.0f, 0.0f, 2.0f), Formula.DamageElement, Formula.AreaRadius, Formula.AreaLifetime,
		Formula.AreaDamagePerSecond * Power, Cast<APawn>(Caster), true, Formula.AreaEffect);

	// Steam puts out the fires it covers
	if (Formula.DamageElement == EVaelElement::Water)
	{
		AVaelGroundArea::ExtinguishFires(World, Location, Formula.AreaRadius);
	}
}

void UVaelFormulaAbility::StartDash(const UVaelFormula& Formula, AActor* Caster, float Power)
{
	AVaelCharacter* Player = Cast<AVaelCharacter>(Caster);
	if (Player != nullptr && Player->StartSpellDash(GetAimDirection(Caster), Formula.DashSpeed, Formula.DashDuration, Formula.MakeSpellHit(Power), Formula.DashBurstRadius))
	{
		Player->SetInvulnerableFor(Formula.DashInvulnerability);
	}
}

void UVaelFormulaAbility::RaiseWall(const UVaelFormula& Formula, AActor* Caster, float Power)
{
	UWorld* World = Caster->GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const FVector Center = FindGroundTarget(Caster, Formula.WallRange);
	const FVector AimDirection = GetAimDirection(Caster);
	const FVector Across(-AimDirection.Y, AimDirection.X, 0.0f);
	const FQuat Rotation = AimDirection.ToOrientationQuat();
	const FVaelSpellHit Hit = Formula.MakeSpellHit(Power);

	const float HalfSize = Formula.WallBlockSize * 0.5f;
	const float HalfHeight = Formula.WallHeight * 0.5f;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VaelRaiseWall), false, Caster);
	int32 NumRaised = 0;

	for (int32 BlockIndex = 0; BlockIndex < Formula.WallBlocks; ++BlockIndex)
	{
		const FVector BlockLocation = Center + Across * ((BlockIndex - (Formula.WallBlocks - 1) * 0.5f) * Formula.WallBlockSpacing);

		// Each block stands on the ground below its spot
		FHitResult GroundHit;
		if (!VaelGround::TraceGround(World, BlockLocation + FVector(0.0f, 0.0f, AreaGroundSearchHeight), BlockLocation - FVector(0.0f, 0.0f, AreaGroundSearchDepth), GroundHit, Caster))
		{
			continue;
		}

		const FVector BlockCenter = GroundHit.Location + FVector(0.0f, 0.0f, HalfHeight);

		// No block inside rocks, walls or other blocks; a little margin lets it touch them.
		// The ground it stands on doesn't count, so blocks rise on ramps and slopes too.
		FCollisionQueryParams SolidQueryParams = QueryParams;
		SolidQueryParams.AddIgnoredComponent(GroundHit.GetComponent());

		const FCollisionShape SolidTestShape = FCollisionShape::MakeBox(FVector(HalfSize * 0.8f, HalfSize * 0.8f, HalfHeight * 0.8f));
		if (World->OverlapAnyTestByObjectType(BlockCenter + FVector(0.0f, 0.0f, HalfHeight * 0.1f), Rotation, FCollisionObjectQueryParams(ECC_WorldStatic), SolidTestShape, SolidQueryParams))
		{
			continue;
		}

		// Players are never walled in, enemies are hurt and thrown out of the way
		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByObjectType(Overlaps, BlockCenter, Rotation, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeBox(FVector(HalfSize, HalfSize, HalfHeight)), QueryParams);

		bool bPlayerInTheWay = false;
		TSet<AActor*> Enemies;
		for (const FOverlapResult& Overlap : Overlaps)
		{
			const APawn* Pawn = Cast<APawn>(Overlap.GetActor());
			if (Pawn != nullptr && Pawn->IsPlayerControlled())
			{
				bPlayerInTheWay = true;
			}
			else if (Overlap.GetActor() != nullptr)
			{
				Enemies.Add(Overlap.GetActor());
			}
		}

		if (bPlayerInTheWay)
		{
			continue;
		}

		for (AActor* Enemy : Enemies)
		{
			UVaelCombatStatics::ApplySpellHit(Caster, Enemy, Hit, Enemy->GetActorLocation() - Center);
		}

		if (AVaelRockWall::RaiseBlock(World, GroundHit.Location, AimDirection.Rotation().Yaw, Formula.WallBlockSize, Formula.WallHeight, Formula.WallLifetime, Cast<APawn>(Caster)) != nullptr)
		{
			++NumRaised;
		}
	}

	UE_LOG(LogVael, Verbose, TEXT("'%s' raises %d of %d rock blocks around %s"), *GetNameSafe(Caster), NumRaised, Formula.WallBlocks, *Center.ToCompactString());

	if (NumRaised > 0)
	{
		UVaelHitFeedbackSubsystem::Shake(Caster, WallShake);
	}
}

void UVaelFormulaAbility::StartBeam(const UVaelFormula& Formula, AActor* Caster, float Power)
{
	FVaelBeamSettings Beam;
	Beam.Hit = Formula.MakeSpellHit(Power);
	Beam.DamagePerSecond = Formula.Damage * Power;
	Beam.DamageStep = Formula.BeamDamageStep;
	Beam.Duration = Formula.BeamDuration;
	Beam.Length = Formula.BeamLength;
	Beam.HalfWidth = Formula.BeamHalfWidth;
	Beam.FireInterval = Formula.AreaRadius > 0.0f ? Formula.BeamFireInterval : 0.0f;
	Beam.FireRadius = Formula.AreaRadius;
	Beam.FireLifetime = Formula.AreaLifetime;
	Beam.FireDamagePerSecond = Formula.AreaDamagePerSecond * Power;
	Beam.Color = UVaelMagicSettings::Get()->GetElementColor(Formula.DamageElement);

	AVaelSpellBeam::StartBeam(Cast<APawn>(Caster), Beam);
}

void UVaelFormulaAbility::HitNova(const UVaelFormula& Formula, AActor* Caster, float Power)
{
	const FVector Center = Caster->GetActorLocation();

	UVaelCombatStatics::ApplySpellHitInRadius(Caster, Center, Formula.NovaRadius, Formula.MakeSpellHit(Power));
	UVaelHitFeedbackSubsystem::Shake(Caster, Formula.NovaShake);

#if ENABLE_DRAW_DEBUG
	// Placeholder look until the formulas get real effects
	DrawDebugCircle(Caster->GetWorld(), Center - FVector(0.0f, 0.0f, Caster->GetSimpleCollisionHalfHeight() - 5.0f), Formula.NovaRadius, 48,
		UVaelMagicSettings::Get()->GetElementColor(Formula.DamageElement).ToFColor(true), false, 0.4f, 0, 8.0f, FVector::ForwardVector, FVector::RightVector, false);
#endif
}

void UVaelFormulaAbility::LaunchVortex(const UVaelFormula& Formula, AActor* Caster, float Power)
{
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();

	FVaelVortexSettings Vortex;
	Vortex.Hit = Formula.MakeSpellHit(Power);
	Vortex.FireHit = Vortex.Hit;
	Vortex.FireHit.Element = EVaelElement::Fire;
	Vortex.FireHit.Status = EVaelStatus::Burning;
	Vortex.FireHit.StatusDuration = Formula.VortexBurnDuration;
	Vortex.FireHit.StatusDamagePerSecond = Formula.VortexBurnDamagePerSecond;
	Vortex.DamagePerSecond = Formula.Damage * Power;
	Vortex.FireDamageBonus = Formula.VortexFireDamageBonus;
	Vortex.DamageStep = Formula.BeamDamageStep;
	Vortex.Speed = Formula.VortexSpeed;
	Vortex.Radius = Formula.VortexRadius;
	Vortex.Lifetime = Formula.VortexLifetime;
	Vortex.PullSpeed = Formula.VortexPullSpeed;
	Vortex.FireInterval = Formula.AreaRadius > 0.0f ? Formula.VortexFireInterval : 0.0f;
	Vortex.FireRadius = Formula.AreaRadius;
	Vortex.FireLifetime = Formula.AreaLifetime;
	Vortex.FireDamagePerSecond = Formula.AreaDamagePerSecond * Power;
	Vortex.Color = MagicSettings->GetElementColor(Formula.DamageElement);
	Vortex.FireColor = MagicSettings->GetElementColor(EVaelElement::Fire);

	const FVector AimDirection = GetAimDirection(Caster);
	AVaelSpellVortex::Launch(Cast<APawn>(Caster), Caster->GetActorLocation() + AimDirection * ProjectileSpawnDistance, AimDirection, Vortex);
}

void UVaelFormulaAbility::StartAura(const UVaelFormula& Formula, AActor* Caster, float Power)
{
	FVaelAuraSettings Aura;
	Aura.Hit = Formula.MakeSpellHit(Power);
	Aura.DamagePerSecond = Formula.Damage * Power;
	Aura.DamageStep = Formula.BeamDamageStep;
	Aura.Radius = Formula.AuraRadius;
	Aura.Duration = Formula.AuraDuration;
	Aura.BlindDuration = Formula.AuraBlindDuration;
	Aura.Color = UVaelMagicSettings::Get()->GetElementColor(Formula.DamageElement);

	AVaelSpellAura::StartAura(Cast<APawn>(Caster), Aura);
}

FVector UVaelFormulaAbility::FindGroundTarget(AActor* Caster, float MaxRange)
{
	const FVector Origin = Caster->GetActorLocation();
	const FVector AimDirection = GetAimDirection(Caster);
	float Distance = AutoTargetDefaultDistance;

	const APawn* CasterPawn = Cast<APawn>(Caster);
	const AVaelPlayerController* PlayerController = CasterPawn != nullptr ? Cast<AVaelPlayerController>(CasterPawn->GetController()) : nullptr;

	FVector MouseLocation;
	if (PlayerController != nullptr && PlayerController->GetMouseAimLocation(MouseLocation))
	{
		Distance = FVector::Dist2D(Origin, MouseLocation);
	}
	else
	{
		// With a gamepad the patch lands on the nearest enemy roughly in the aim direction
		const float MinAimCosine = FMath::Cos(FMath::DegreesToRadians(AutoTargetHalfAngle));
		float BestDistance = AutoTargetRange;

		for (TActorIterator<AVaelCharacterBase> It(Caster->GetWorld()); It; ++It)
		{
			const FVector ToCandidate = It->GetActorLocation() - Origin;
			const float CandidateDistance = ToCandidate.Size2D();

			if (CandidateDistance < BestDistance && UVaelCombatStatics::CanDamage(Caster, *It) && FVector::DotProduct(ToCandidate.GetSafeNormal2D(), AimDirection) >= MinAimCosine)
			{
				BestDistance = CandidateDistance;
				Distance = FMath::Max(CandidateDistance, AutoTargetMinDistance);
			}
		}
	}

	FVector Target = Origin + AimDirection * FMath::Min(Distance, MaxRange);

	// Walls stop the patch in front of them
	FHitResult WallHit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VaelGroundTarget), false, Caster);
	if (Caster->GetWorld()->LineTraceSingleByObjectType(WallHit, Origin, Target, FCollisionObjectQueryParams(ECC_WorldStatic), QueryParams) && WallHit.ImpactNormal.Z < WalkableSlopeNormalZ)
	{
		Target = Origin + AimDirection * FMath::Max(0.0f, WallHit.Distance - WallBackOffDistance);
	}

	return Target;
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
