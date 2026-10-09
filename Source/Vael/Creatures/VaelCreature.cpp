// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelCreature.h"
#include "UObject/ConstructorHelpers.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Combat/VaelAttributeSet.h"
#include "Combat/VaelCombatStatics.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Creatures/VaelCreatureData.h"
#include "DrawDebugHelpers.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Items/VaelItemDrop.h"
#include "Items/VaelItemSettings.h"
#include "Items/VaelMaterial.h"
#include "Items/VaelMaterialBag.h"
#include "Items/VaelPickupOrb.h"
#include "Magic/VaelClayGolem.h"
#include "Magic/VaelFormula.h"
#include "Magic/VaelGameplayTags.h"
#include "Magic/VaelGrimoireSubsystem.h"
#include "Magic/VaelGroundArea.h"
#include "Magic/VaelMagicSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Player/VaelCharacter.h"
#include "UI/VaelCombatTextSubsystem.h"
#include "UI/VaelUISettings.h"
#include "Vael.h"
#include "VaelAssets.h"
#include "World/VaelRegion.h"
#include "World/VaelWorldSettings.h"
#include "World/VaelGround.h"

#define LOCTEXT_NAMESPACE "VaelCreatures"

namespace
{
	/** Size of the engine basic shapes used as placeholders */
	constexpr float CreatureShapeSize = 100.0f;

	/** Height above the ground at which creatures shoot, about the chest of a player */
	constexpr float ProjectileHeight = 90.0f;
}

AVaelCreature::AVaelCreature()
{
	// Simple AI controller: the behavior lives in the creature, the controller only feeds its movement input
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	StartingMana = 0.0f;

	bUseControllerRotationYaw = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
	Movement->MaxAcceleration = 4000.0f;
	Movement->BrakingDecelerationWalking = 2000.0f;
	Movement->BrakingDecelerationFlying = 2000.0f;

	// Placeholder look from engine assets, subclasses pick the shape
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(RootComponent);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (BodyMesh.Succeeded())
	{
		Body->SetStaticMesh(BodyMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BodyMaterialAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BodyMaterialAsset.Succeeded())
	{
		Body->SetMaterial(0, BodyMaterialAsset.Object);
	}

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AVaelCreature::PostInitializeComponents()
{
	// Without a fitting data asset the creature falls back to the defaults of its data class
	// Kept apart from CreatureData so the defaults never end up saved in a level
	const TSubclassOf<UVaelCreatureData> DefaultDataClass = GetDefaultDataClass();
	ActiveData = CreatureData;

	if (ActiveData != nullptr && DefaultDataClass != nullptr && !ActiveData->IsA(DefaultDataClass))
	{
		UE_LOG(LogVael, Warning, TEXT("'%s' has creature data '%s' of the wrong kind, using the defaults instead"), *GetNameSafe(this), *GetNameSafe(ActiveData));
		ActiveData = nullptr;
	}

	if (ActiveData == nullptr)
	{
		const TSubclassOf<UVaelCreatureData> FallbackClass = DefaultDataClass != nullptr ? DefaultDataClass : TSubclassOf<UVaelCreatureData>(UVaelEmberCrawlerData::StaticClass());
		ActiveData = FallbackClass->GetDefaultObject<UVaelCreatureData>();
	}

	StartingHealth = ActiveData->MaxHealth;

	GetCapsuleComponent()->SetCapsuleSize(ActiveData->CollisionRadius, ActiveData->CollisionHalfHeight);

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = ActiveData->MoveSpeed;
	Movement->MaxFlySpeed = ActiveData->MoveSpeed;

	Body->SetRelativeScale3D(FVector(ActiveData->CollisionRadius * 2.0f, ActiveData->CollisionRadius * 2.0f, ActiveData->CollisionHalfHeight * 2.0f) / CreatureShapeSize);

	Super::PostInitializeComponents();
}

void AVaelCreature::BeginPlay()
{
	Super::BeginPlay();

	HomeLocation = GetActorLocation();

	BodyMaterial = Body->CreateAndSetMaterialInstanceDynamic(0);
	SetBodyColor(ActiveData->BodyColor);

	// A corrupted region sends stronger creatures
	const UVaelWorldSettings* WorldSettings = UVaelWorldSettings::Get();
	const AVaelRegion* Region = AVaelRegion::GetRegionAt(GetWorld(), GetActorLocation());
	const float Corruption = Region != nullptr ? Region->GetCorruption() : 0.0f;

	if (ActiveData->bCanBeMarked && Corruption > WorldSettings->MarkedCreatureThreshold && FMath::FRand() < Corruption / WorldSettings->MarkedCreatureChanceDivisor)
	{
		SetMarked();
	}
}

void AVaelCreature::SetMarked()
{
	if (bMarked || bDead)
	{
		return;
	}

	bMarked = true;

	const float MarkedMaxHealth = GetMaxHealth() * UVaelWorldSettings::Get()->MarkedHealthMultiplier;
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent();
	AbilitySystem->SetNumericAttributeBase(UVaelAttributeSet::GetMaxHealthAttribute(), MarkedMaxHealth);
	AbilitySystem->SetNumericAttributeBase(UVaelAttributeSet::GetHealthAttribute(), MarkedMaxHealth);

	RefreshBodyColor();

	UE_LOG(LogVael, Verbose, TEXT("'%s' is marked by the corruption"), *GetNameSafe(this));
}

float AVaelCreature::GetOutgoingDamageMultiplier() const
{
	return bMarked ? UVaelWorldSettings::Get()->MarkedDamageMultiplier : 1.0f;
}

void AVaelCreature::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bShowingHitFlash && GetWorld()->GetTimeSeconds() >= HitFlashEndTime)
	{
		bShowingHitFlash = false;
		RefreshBodyColor();
	}

	// A staggering body tips over and rights itself again
	if (StaggerLength > 0.0f)
	{
		const float Remaining = StaggerEndTime - GetWorld()->GetTimeSeconds();
		const float Tip = bDead ? 0.0f : FMath::Clamp(Remaining / StaggerLength, 0.0f, 1.0f);

		Body->SetRelativeRotation(FRotator(StaggerTipDirection.X * Tip, 0.0f, StaggerTipDirection.Y * Tip));

		if (Tip <= 0.0f)
		{
			StaggerLength = 0.0f;
		}
	}

#if ENABLE_DRAW_DEBUG
	if (bShowStatusText && !bDead && (AlwaysShowStatusText() || GetHealth() < GetMaxHealth()))
	{
		const FString Status = FString::Printf(TEXT("%s  %.0f / %.0f  %s"), *GetCreatureName().ToString(), GetHealth(), GetMaxHealth(), *GetStatusText().ToString());
		DrawDebugString(GetWorld(), FVector(0.f, 0.f, GetStatusTextHeight()), Status, this, FColor(255, 214, 160), 0.f, true);
	}
#endif

	if (!bDead)
	{
		UpdateGroundEffects();
	}

	if (bDead || GetWorld()->GetTimeSeconds() < StunEndTime || UVaelCombatStatics::HasStatus(this, EVaelStatus::Frozen))
	{
		return;
	}

	// Fear drives it away from the nearest player instead of fighting
	if (UVaelCombatStatics::HasStatus(this, EVaelStatus::Feared))
	{
		const AVaelCharacter* Nearest = nullptr;
		float NearestDistance = UE_BIG_NUMBER;

		ForEachActivePlayer([this, &Nearest, &NearestDistance](AVaelCharacter* Player)
		{
			const float Distance = GetDistanceTo2D(Player);
			if (Distance < NearestDistance)
			{
				Nearest = Player;
				NearestDistance = Distance;
			}
		});

		if (Nearest != nullptr)
		{
			MoveInDirection((GetActorLocation() - Nearest->GetActorLocation()).GetSafeNormal2D(), ActiveData->MoveSpeed);
		}

		return;
	}

	TickBehavior(DeltaSeconds);
}

AVaelCreature* AVaelCreature::SpawnCreature(UWorld* World, UVaelCreatureData* Data, const FVector& GroundLocation, const FRotator& Rotation)
{
	if (World == nullptr || Data == nullptr || Data->CreatureClass == nullptr)
	{
		return nullptr;
	}

	const FTransform SpawnTransform(FRotator(0.0f, Rotation.Yaw, 0.0f), GroundLocation + FVector(0.0f, 0.0f, Data->CollisionHalfHeight + 2.0f));

	AVaelCreature* Creature = World->SpawnActorDeferred<AVaelCreature>(Data->CreatureClass, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (Creature != nullptr)
	{
		Creature->CreatureData = Data;
		Creature->FinishSpawning(SpawnTransform);
	}

	return Creature;
}

bool AVaelCreature::FindGround(const UWorld* World, const FVector& Location, FVector& OutGroundLocation)
{
	FHitResult GroundHit;
	if (World != nullptr && VaelGround::TraceGround(World, Location + FVector(0.0f, 0.0f, 500.0f), Location - FVector(0.0f, 0.0f, 2000.0f), GroundHit))
	{
		OutGroundLocation = GroundHit.Location;
		return true;
	}

	return false;
}

FText AVaelCreature::GetCreatureName() const
{
	return ActiveData != nullptr ? ActiveData->DisplayName : FText::GetEmpty();
}

float AVaelCreature::GetIncomingDamageMultiplier(const FGameplayTagContainer& DamageTags) const
{
	float Multiplier = 1.0f;

	for (const TPair<EVaelElement, float>& ElementMultiplier : ActiveData->ElementMultipliers)
	{
		if (DamageTags.HasTagExact(VaelTags::GetElementTag(ElementMultiplier.Key)))
		{
			Multiplier *= ElementMultiplier.Value;
		}
	}

	return Multiplier;
}

bool AVaelCreature::CanReceiveStatus(EVaelStatus Status) const
{
	return !bDead && (Status != EVaelStatus::Frozen || ActiveData->bCanBeFrozen);
}

void AVaelCreature::ApplyKnockback(const FVector& Direction, float Speed)
{
	if (!bDead)
	{
		Super::ApplyKnockback(Direction, Speed * ActiveData->KnockbackMultiplier);
	}
}

void AVaelCreature::ApplyStun(float Duration)
{
	if (!bDead)
	{
		Stun(Duration * ActiveData->StunMultiplier);
	}
}

void AVaelCreature::ApplyPull(const FVector& Location, float Speed, float DeltaSeconds)
{
	if (!bDead && ActiveData->PullMultiplier > 0.0f)
	{
		Super::ApplyPull(Location, Speed * ActiveData->PullMultiplier, DeltaSeconds);
	}
}

void AVaelCreature::ApplyBlind(float Duration)
{
	if (!bDead)
	{
		BlindEndTime = FMath::Max(BlindEndTime, GetWorld()->GetTimeSeconds() + Duration);
	}
}

TSubclassOf<UVaelCreatureData> AVaelCreature::GetDefaultDataClass() const
{
	return nullptr;
}

float AVaelCreature::GetStatusTextHeight() const
{
	return GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 40.0f;
}

void AVaelCreature::OnHealthChanged(float OldValue, float NewValue)
{
	Super::OnHealthChanged(OldValue, NewValue);

	if (bDead || NewValue >= OldValue)
	{
		return;
	}

	HitFlashEndTime = GetWorld()->GetTimeSeconds() + HitFlashDuration;
	bShowingHitFlash = true;
	RefreshBodyColor();

	if (NewValue <= 0.0f)
	{
		Die();
	}
}

void AVaelCreature::Die()
{
	if (bDead)
	{
		return;
	}

	bDead = true;

	UE_LOG(LogVael, Log, TEXT("'%s' (%s) dies"), *GetNameSafe(this), *GetCreatureName().ToString());

	TeachFormula(ActiveData->FormulaOnDeath, FText::Format(LOCTEXT("FormulaFromCorpse", "Im Leib von {0} liegt ein Fragment."), GetCreatureName()));

	OnDied.Broadcast(this);

	// Every marked creature that falls cleanses its region a little
	if (bMarked)
	{
		if (AVaelRegion* Region = AVaelRegion::GetRegionAt(GetWorld(), GetActorLocation()))
		{
			Region->AddCorruption(-UVaelWorldSettings::Get()->CleansingPerMarkedKill);
		}
	}

	DropLoot();

	// Nothing may bump into or target the corpse
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	DetachFromControllerPendingDestroy();

	// Placeholder for a death animation: the body slumps and darkens
	Body->SetRelativeScale3D(Body->GetRelativeScale3D() * FVector(1.2f, 1.2f, 0.25f));
	bShowingHitFlash = false;
	SetBodyColor(ActiveData->BodyColor * 0.25f);

	SetLifeSpan(FMath::Max(CorpseDuration, 0.01f));
}

void AVaelCreature::DropLoot()
{
	// Materials: rolled once, every player gets their own copy straight into the bag
	TArray<FVaelLootEntry> Entries = ActiveData->Loot;
	if (bMarked)
	{
		Entries.Add(UVaelWorldSettings::Get()->MarkedLoot);
	}

	for (const FVaelLootEntry& Entry : Entries)
	{
		const int32 Count = Entry.Roll();
		const UVaelMaterial* Material = Count > 0 ? VaelAssets::LoadOptional(Entry.Material) : nullptr;
		if (Material == nullptr)
		{
			continue;
		}

		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* PlayerController = It->Get();
			const AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
			if (Player != nullptr)
			{
				Player->GetMaterialBag()->AddMaterial(Material, Count);
			}
		}

		const FText Label = Count > 1 ? FText::Format(LOCTEXT("LootCount", "{0} ×{1}"), Material->DisplayName, Count) : Material->DisplayName;
		UVaelCombatTextSubsystem::PostPickup(this, Label, Material->Color);

		UE_LOG(LogVael, Verbose, TEXT("'%s' drops %d x %s for every player"), *GetNameSafe(this), Count, *Material->DisplayName.ToString());
	}

	// Gear is personal: rolled for every player, lying in their color
	int32 PlayerIndex = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It, ++PlayerIndex)
	{
		AVaelCharacter* Player = It->Get() != nullptr ? It->Get()->GetPawn<AVaelCharacter>() : nullptr;
		if (Player == nullptr)
		{
			continue;
		}

		const int32 NumItems = ActiveData->GuaranteedItems + (FMath::FRand() < ActiveData->ItemDropChance ? 1 : 0);
		for (int32 ItemIndex = 0; ItemIndex < NumItems; ++ItemIndex)
		{
			const float Angle = 2.0f * PI * (PlayerIndex * 3 + ItemIndex) / 12.0f;
			const FVector Offset(FMath::Cos(Angle) * 90.0f, FMath::Sin(Angle) * 90.0f, 0.0f);
			AVaelItemDrop::DropItem(GetWorld(), GetActorLocation() + Offset, UVaelItemSettings::Get()->RollItem(ActiveData->MinItemRarity), Player);
		}
	}

	// Orbs lie on the ground for whoever needs them first
	const auto DropOrbNearby = [this](EVaelOrbKind Kind)
	{
		const FVector Scatter(FMath::FRandRange(-56.0f, 56.0f), FMath::FRandRange(-56.0f, 56.0f), 0.0f);
		AVaelPickupOrb::DropOrb(GetWorld(), GetActorLocation() + Scatter, Kind);
	};

	if (FMath::FRand() < ActiveData->HealthOrbChance)
	{
		DropOrbNearby(EVaelOrbKind::Health);
	}

	if (FMath::FRand() < ActiveData->ManaOrbChance)
	{
		DropOrbNearby(EVaelOrbKind::Mana);
	}

	for (int32 OrbIndex = 0; OrbIndex < ActiveData->GuaranteedHealthOrbs; ++OrbIndex)
	{
		DropOrbNearby(EVaelOrbKind::Health);
	}
}

AActor* AVaelCreature::FindTarget(float MaxDistance, float* OutDistance) const
{
	AActor* Nearest = nullptr;
	float NearestDistance = MaxDistance;

	// In fever it goes for its own kind instead
	if (UVaelCombatStatics::HasStatus(this, EVaelStatus::Fevered))
	{
		for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
		{
			const float Distance = GetDistanceTo2D(*It);
			if (*It != this && !It->IsDead() && Distance < NearestDistance)
			{
				Nearest = *It;
				NearestDistance = Distance;
			}
		}

		if (OutDistance != nullptr)
		{
			*OutDistance = NearestDistance;
		}

		return Nearest;
	}

	// A golem of a mage draws every enemy near it onto itself
	for (TActorIterator<AVaelClayGolem> It(GetWorld()); It; ++It)
	{
		const float Distance = GetDistanceTo2D(*It);
		if (!It->IsDefeated() && Distance < FMath::Min(NearestDistance, It->GetLureRadius()))
		{
			Nearest = *It;
			NearestDistance = Distance;
		}
	}

	ForEachActivePlayer([this, &Nearest, &NearestDistance](AVaelCharacter* Player)
	{
		if (Cast<AVaelClayGolem>(Nearest) != nullptr)
		{
			return;
		}

		// Players in the mist of a mage can't be made out from afar
		if (AVaelGroundArea::IsHiddenInMist(Player, this))
		{
			return;
		}

		const float Distance = GetDistanceTo2D(Player);
		if (Distance < NearestDistance)
		{
			Nearest = Player;
			NearestDistance = Distance;
		}
	});

	if (OutDistance != nullptr)
	{
		*OutDistance = NearestDistance;
	}

	return Nearest;
}

void AVaelCreature::ForEachActivePlayer(TFunctionRef<void(AVaelCharacter*)> Function) const
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;

		if (Player != nullptr && !Player->IsDefeated())
		{
			Function(Player);
		}
	}
}

float AVaelCreature::GetDistanceTo2D(const AActor* Other) const
{
	return Other != nullptr ? FVector::Dist2D(GetActorLocation(), Other->GetActorLocation()) : UE_BIG_NUMBER;
}

void AVaelCreature::MoveInDirection(const FVector& Direction, float Speed)
{
	const FVector GroundDirection = Direction.GetSafeNormal2D();
	if (Speed <= 0.0f || GroundDirection.IsNearlyZero())
	{
		return;
	}

	// Slowing spells hold back walking and flying creatures alike, mud only walking ones
	const float SlowMultiplier = UVaelCombatStatics::HasStatus(this, EVaelStatus::Slowed) ? UVaelMagicSettings::Get()->SlowedSpeedMultiplier : 1.0f;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = Speed * GroundSpeedMultiplier * SlowMultiplier;
	Movement->MaxFlySpeed = Speed * SlowMultiplier;

	AddMovementInput(GroundDirection, 1.0f);
}

void AVaelCreature::MoveTowards(const FVector& Location, float Speed)
{
	MoveInDirection(Location - GetActorLocation(), Speed);
}

void AVaelCreature::FaceTowards(const FVector& Location)
{
	const FVector Direction = (Location - GetActorLocation()).GetSafeNormal2D();
	if (!Direction.IsNearlyZero())
	{
		SetActorRotation(FRotator(0.0f, Direction.Rotation().Yaw, 0.0f));
	}
}

FVector AVaelCreature::GetAttackLocation(const FVector& Direction, float DistanceBeyondCapsule) const
{
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const FVector Feet = GetActorLocation() - FVector(0.0f, 0.0f, Capsule->GetScaledCapsuleHalfHeight());

	return Feet + FVector(0.0f, 0.0f, ProjectileHeight) + Direction.GetSafeNormal2D() * (Capsule->GetScaledCapsuleRadius() + DistanceBeyondCapsule);
}

bool AVaelCreature::HitPlayer(AActor* Target, float Damage, EVaelElement Element)
{
	FVaelSpellHit Hit;
	Hit.Damage = Damage;
	Hit.Element = Element;

	return UVaelCombatStatics::ApplySpellHit(this, Target, Hit, FVector::ZeroVector);
}

bool AVaelCreature::TeachFormula(TConstArrayView<EVaelElement> Elements, const FText& Reason) const
{
	if (Elements.IsEmpty())
	{
		return false;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	UVaelGrimoireSubsystem* Grimoire = GameInstance != nullptr ? GameInstance->GetSubsystem<UVaelGrimoireSubsystem>() : nullptr;
	UVaelFormula* Formula = Grimoire != nullptr ? Grimoire->FindFormula(Elements) : nullptr;

	if (Formula == nullptr)
	{
		UE_LOG(LogVael, Verbose, TEXT("'%s' would teach a formula of %d elements, but it isn't in the game yet"), *GetNameSafe(this), Elements.Num());
		return false;
	}

	return Grimoire->LearnFormula(Formula, Reason);
}

void AVaelCreature::Stun(float Duration)
{
	StunEndTime = FMath::Max(StunEndTime, GetWorld()->GetTimeSeconds() + Duration);
}

void AVaelCreature::Stagger(float Duration)
{
	const float Multiplier = ActiveData->StaggerMultiplier;
	const float StaggerTime = Duration * Multiplier;
	if (bDead || StaggerTime <= 0.0f)
	{
		return;
	}

	Stun(StaggerTime);

	// Tips the body in a random direction, bosses less
	const float Tilt = UVaelUISettings::Get()->StaggerTilt * FMath::Min(Multiplier, 1.0f);
	const FVector2D Direction = FVector2D(FMath::FRandRange(-1.0f, 1.0f), FMath::FRandRange(-1.0f, 1.0f)).GetSafeNormal();

	StaggerTipDirection = Direction * Tilt;
	StaggerLength = StaggerTime;
	StaggerEndTime = GetWorld()->GetTimeSeconds() + StaggerTime;
}

void AVaelCreature::SetBodyColor(const FLinearColor& Color)
{
	BodyColor = Color;
	RefreshBodyColor();
}

void AVaelCreature::RefreshBodyColor()
{
	if (BodyMaterial != nullptr)
	{
		// Marked creatures carry the violet of the Mark
		const FLinearColor ShownColor = bMarked ? FMath::Lerp(BodyColor, FLinearColor(0.38f, 0.06f, 1.0f), 0.45f) : BodyColor;
		BodyMaterial->SetVectorParameterValue(BodyColorParameter, bShowingHitFlash ? FLinearColor(1.0f, 0.9f, 0.75f) : ShownColor);
	}
}

#undef LOCTEXT_NAMESPACE

bool AVaelCreature::IsBlinded() const
{
	return GetWorld()->GetTimeSeconds() < BlindEndTime;
}

void AVaelCreature::UpdateGroundEffects()
{
	bool bInSteam = false;
	AVaelGroundArea::GetEffectsOn(this, bInSteam, GroundSpeedMultiplier);

	// Mud only holds back creatures that walk
	if (GetCharacterMovement()->IsFlying())
	{
		GroundSpeedMultiplier = 1.0f;
	}

	if (bInSteam)
	{
		ApplyBlind(UVaelMagicSettings::Get()->SteamBlindLinger);
	}
}
