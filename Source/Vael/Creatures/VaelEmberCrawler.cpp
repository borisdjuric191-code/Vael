// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelEmberCrawler.h"
#include "AbilitySystemComponent.h"
#include "Combat/VaelCombatStatics.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Creatures/VaelCreatureData.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Magic/VaelGameplayTags.h"
#include "Magic/VaelGroundArea.h"
#include "Player/VaelCharacter.h"
#include "Vael.h"

#define LOCTEXT_NAMESPACE "VaelCreatures"

namespace
{
	/** The burrowed body is squashed to this share of its height */
	constexpr float BurrowedHeightScale = 0.2f;

	/** The crawler stops walking home closer than this, in cm */
	constexpr float CrawlerHomeTolerance = 140.0f;
}

AVaelEmberCrawler::AVaelEmberCrawler()
{
}

void AVaelEmberCrawler::BeginPlay()
{
	Super::BeginPlay();

	SurfacedBodyScale = GetBody()->GetRelativeScale3D();

	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent();
	AbilitySystem->RegisterGameplayTagEvent(VaelTags::Status_Wet, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &AVaelEmberCrawler::OnDoused);
	AbilitySystem->RegisterGameplayTagEvent(VaelTags::Status_Frozen, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &AVaelEmberCrawler::OnDoused);

	EnterState(EVaelCrawlerState::Burrowed);
}

float AVaelEmberCrawler::GetIncomingDamageMultiplier(const FGameplayTagContainer& DamageTags) const
{
	float Multiplier = Super::GetIncomingDamageMultiplier(DamageTags);

	if (State == EVaelCrawlerState::Burrowed)
	{
		const UVaelEmberCrawlerData* Data = GetData<UVaelEmberCrawlerData>();
		Multiplier *= DamageTags.HasTagExact(VaelTags::Element_Earth) ? Data->BurrowedEarthMultiplier : Data->BurrowedDamageMultiplier;
	}

	return Multiplier;
}

TSubclassOf<UVaelCreatureData> AVaelEmberCrawler::GetDefaultDataClass() const
{
	return UVaelEmberCrawlerData::StaticClass();
}

void AVaelEmberCrawler::TickBehavior(float DeltaSeconds)
{
	const UVaelEmberCrawlerData* Data = GetData<UVaelEmberCrawlerData>();

	StateTime += DeltaSeconds;
	ClawCooldown -= DeltaSeconds;

	float TargetDistance = 0.0f;
	AVaelCharacter* Target = FindNearestPlayer(Data->AggroRange, &TargetDistance);

	switch (State)
	{
	case EVaelCrawlerState::Burrowed:
		if (Target == nullptr)
		{
			if (FVector::Dist2D(GetActorLocation(), HomeLocation) > CrawlerHomeTolerance)
			{
				MoveTowards(HomeLocation, Data->MoveSpeed * Data->BurrowReturnSpeedScale);
			}
		}
		else if (TargetDistance < Data->SurfaceDistance)
		{
			EnterState(EVaelCrawlerState::Surfacing);
		}
		else
		{
			MoveTowards(Target->GetActorLocation(), Data->MoveSpeed * Data->BurrowChaseSpeedScale);
		}
		break;

	case EVaelCrawlerState::Surfacing:
		if (StateTime >= Data->SurfaceDuration)
		{
			EnterState(EVaelCrawlerState::Fuse);
		}
		break;

	case EVaelCrawlerState::Fuse:
		// Glows brighter the closer it gets to the explosion
		SetBodyColor(FMath::Lerp(Data->BodyColor, FLinearColor(1.0f, 0.85f, 0.3f), 0.5f + 0.5f * FMath::Sin(StateTime * (8.0f + 16.0f * StateTime / FMath::Max(Data->FuseDuration, 0.01f)))));

		if (Target != nullptr && TargetDistance > GetCapsuleComponent()->GetScaledCapsuleRadius() * 2.0f)
		{
			MoveTowards(Target->GetActorLocation(), Data->MoveSpeed * Data->FuseChaseSpeedScale);
		}

		if (StateTime >= Data->FuseDuration)
		{
			Explode();
		}
		break;

	case EVaelCrawlerState::Doused:
		if (StateTime >= Data->DousedDuration)
		{
			EnterState(EVaelCrawlerState::Crawling);
		}
		break;

	case EVaelCrawlerState::Crawling:
		if (Target == nullptr)
		{
			break;
		}

		if (TargetDistance > Data->ClawRange)
		{
			MoveTowards(Target->GetActorLocation(), Data->MoveSpeed * Data->CrawlSpeedScale);
		}
		else if (ClawCooldown <= 0.0f)
		{
			FaceTowards(Target->GetActorLocation());
			HitPlayer(Target, Data->ClawDamage, EVaelElement::Fire);
			ClawCooldown = Data->ClawInterval;
		}
		break;
	}
}

void AVaelEmberCrawler::EnterState(EVaelCrawlerState NewState)
{
	const UVaelEmberCrawlerData* Data = GetData<UVaelEmberCrawlerData>();

	State = NewState;
	StateTime = 0.0f;

	const bool bBurrowed = State == EVaelCrawlerState::Burrowed;

	// Burrowed crawlers slip under the feet of the players
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, bBurrowed ? ECR_Ignore : ECR_Block);

	// Placeholder look: a flat mound under the ash, a glowing body above it
	UStaticMeshComponent* BodyMesh = GetBody();
	const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	BodyMesh->SetRelativeScale3D(bBurrowed ? SurfacedBodyScale * FVector(1.0f, 1.0f, BurrowedHeightScale) : SurfacedBodyScale);
	BodyMesh->SetRelativeLocation(FVector(0.0f, 0.0f, bBurrowed ? -HalfHeight * (1.0f - BurrowedHeightScale) : 0.0f));

	switch (State)
	{
	case EVaelCrawlerState::Burrowed:
		SetBodyColor(FLinearColor(0.12f, 0.08f, 0.06f));
		break;
	case EVaelCrawlerState::Surfacing:
	case EVaelCrawlerState::Fuse:
		SetBodyColor(Data->BodyColor);
		break;
	case EVaelCrawlerState::Doused:
		SetBodyColor(FLinearColor(0.35f, 0.35f, 0.35f));
		break;
	case EVaelCrawlerState::Crawling:
		SetBodyColor(Data->BodyColor * 0.45f);
		break;
	}
}

void AVaelEmberCrawler::OnDoused(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount > 0 && State == EVaelCrawlerState::Fuse && !IsDead())
	{
		UE_LOG(LogVael, Verbose, TEXT("'%s' is doused"), *GetNameSafe(this));
		EnterState(EVaelCrawlerState::Doused);
	}
}

void AVaelEmberCrawler::Explode()
{
	const UVaelEmberCrawlerData* Data = GetData<UVaelEmberCrawlerData>();
	const FVector Center = GetActorLocation();

	// Players in the blast
	ForEachActivePlayer([this, Data, &Center](AVaelCharacter* Player)
	{
		if (FVector::Dist2D(Player->GetActorLocation(), Center) < Data->ExplosionRadius + Player->GetCapsuleComponent()->GetScaledCapsuleRadius())
		{
			HitPlayer(Player, Data->ExplosionDamage, EVaelElement::Fire);
		}
	});

	// Other creatures get caught too
	for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
	{
		AVaelCreature* Other = *It;
		if (Other != this && !Other->IsDead() && FVector::Dist2D(Other->GetActorLocation(), Center) < Data->ExplosionRadius)
		{
			UVaelCombatStatics::DealDamage(this, Other, Data->ExplosionDamageToCreatures, EVaelElement::Fire);
			Other->ApplyKnockback(Other->GetActorLocation() - Center, Data->ExplosionKnockback);
		}
	}

	// Whoever is still standing close by remembers the blast
	bool bWitnessed = false;
	ForEachActivePlayer([Data, &Center, &bWitnessed](AVaelCharacter* Player)
	{
		bWitnessed |= FVector::Dist2D(Player->GetActorLocation(), Center) < Data->WitnessDistance;
	});

	if (bWitnessed)
	{
		TeachFormula(Data->FormulaOnWitnessedExplosion, LOCTEXT("FormulaFromExplosion", "Die Glut der Explosion hat sich in dein Ged\u00E4chtnis gebrannt."));
	}

	FVector Ground;
	if (FindGround(GetWorld(), Center, Ground))
	{
		AVaelGroundArea::SpawnArea(GetWorld(), Ground + FVector(0.0f, 0.0f, 2.0f), EVaelElement::Fire, Data->FireRadius, Data->FireLifetime, Data->FireDamagePerSecond, this);
	}

#if ENABLE_DRAW_DEBUG
	DrawDebugSphere(GetWorld(), Center, Data->ExplosionRadius, 24, FColor(255, 140, 40), false, 0.3f);
#endif

	UE_LOG(LogVael, Verbose, TEXT("'%s' explodes"), *GetNameSafe(this));

	Die();

	// The fire it leaves only burns players while its instigator exists, so the crawler stays around unseen
	SetActorHiddenInGame(true);
	SetLifeSpan(Data->FireLifetime + 0.5f);
}

#undef LOCTEXT_NAMESPACE
