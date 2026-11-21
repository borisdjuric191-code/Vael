// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelKobold.h"
#include "AbilitySystemComponent.h"
#include "Combat/VaelAttributeSet.h"
#include "Combat/VaelCombatStatics.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Creatures/VaelPollenBurst.h"
#include "Creatures/VaelSequenceAnimInstance.h"
#include "Creatures/VaelTropfseuche.h"
#include "Engine/StaticMesh.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"
#include "Items/VaelMaterial.h"
#include "Items/VaelMaterialBag.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/VaelCharacter.h"
#include "UI/VaelCombatTextSubsystem.h"
#include "UI/VaelNoticeSubsystem.h"
#include "Vael.h"

#define LOCTEXT_NAMESPACE "VaelKobold"

namespace
{
	const FLinearColor KoboldKristall(1.0f, 0.2f, 0.6f);

	FVaelNightAttack MakeAttack(FName Clip, FVector2D Range, float HitTime, float Reach, float HitRadius, float Damage, float Cooldown,
		float Lunge = 0.0f, EVaelElement Element = EVaelElement::Earth)
	{
		FVaelNightAttack Attack;
		Attack.Clip = Clip;
		Attack.Range = Range;
		Attack.HitTime = HitTime;
		Attack.Reach = Reach;
		Attack.HitRadius = HitRadius;
		Attack.Damage = Damage;
		Attack.Cooldown = Cooldown;
		Attack.Lunge = Lunge;
		Attack.Element = Element;
		return Attack;
	}
}

// ------------------------------------------------------------------ Data

UVaelKoboldData::UVaelKoboldData()
{
	CreatureClass = AVaelKobold::StaticClass();
	AssetRegion = TEXT("Eisenschlund");
	// Under the ground day and night are the same to them
	DayAwareness = 1.0f;
	NightAwareness = 1.0f;
	AggroRange = 1500.0f;
	TurnRate = 220.0f;
	AttackPause = 0.8f;
	SizeRange = FVector2D(0.85f, 1.0f);
	IdleClip = TEXT("Wach_Idle");
	MoveClip = TEXT("Schleichen");
	HitClip = TEXT("Treffer");
	DeathClip = TEXT("Tod");
	LookBone = TEXT("head");
	LookMaxYaw = 70.0f;
	TwitchBones = { TEXT("head") };
	TwitchAngle = 8.0f;
	// Wind blows their tricks away (weakness table, Eisenschlund round 4)
	ElementMultipliers.Add(EVaelElement::Air, 1.5f);
}

UVaelKoboldDiebData::UVaelKoboldDiebData()
{
	DisplayName = LOCTEXT("Dieb", "Kobold-Dieb");
	KindId = TEXT("Kobold_Dieb");
	CompendiumId = KindId;
	AssetName = TEXT("Schleimkratzer");
	Rolle = EVaelKoboldRolle::Dieb;
	MaxHealth = 45.0f;
	MoveSpeed = 62.0f;
	MoveClipSpeed = 40.0f;
	CollisionRadius = 22.0f;
	CollisionHalfHeight = 52.0f;
	KeepDistance = 60.0f;
	FleeSpeed = 220.0f;
	// A quick grab: the claws go for the bag
	Attacks.Add(MakeAttack(TEXT("Kratzen"), FVector2D(0.0f, 120.0f), 0.4f, 55.0f, 80.0f, 4.0f, 3.0f, 130.0f));
}

UVaelKoboldWaechterData::UVaelKoboldWaechterData()
{
	DisplayName = LOCTEXT("Waechter", "Kobold-Hortwächter");
	KindId = TEXT("Kobold_Waechter");
	CompendiumId = KindId;
	AssetName = TEXT("Hortwaechter");
	Rolle = EVaelKoboldRolle::Waechter;
	MaxHealth = 170.0f;
	MoveSpeed = 72.0f;
	MoveClipSpeed = 51.0f;
	CollisionRadius = 30.0f;
	CollisionHalfHeight = 67.0f;
	KeepDistance = 80.0f;
	KnockbackMultiplier = 0.5f;
	Attacks.Add(MakeAttack(TEXT("Schlag"), FVector2D(0.0f, 140.0f), 0.6f, 75.0f, 95.0f, 18.0f, 2.6f, 60.0f));
	Attacks.Add(MakeAttack(TEXT("Kratzen"), FVector2D(0.0f, 110.0f), 0.4f, 60.0f, 80.0f, 9.0f, 1.6f));
}

UVaelKoboldZaubererData::UVaelKoboldZaubererData()
{
	DisplayName = LOCTEXT("Zauberer", "Kobold-Kontorältester");
	KindId = TEXT("Kobold_Zauberer");
	CompendiumId = KindId;
	AssetName = TEXT("Kontoraeltester");
	Rolle = EVaelKoboldRolle::Zauberer;
	MaxHealth = 70.0f;
	MoveSpeed = 60.0f;
	MoveClipSpeed = 43.6f;
	CollisionRadius = 24.0f;
	CollisionHalfHeight = 57.0f;
	// Keeps away and lets the others fight
	KeepDistance = 520.0f;
	// Air = false copies, Earth = a misleading sign at the feet of its prey; both do no damage themselves
	Attacks.Add(MakeAttack(TEXT("Zaubern"), FVector2D(300.0f, 1000.0f), 0.8f, 0.0f, 0.0f, 0.0f, 14.0f, 0.0f, EVaelElement::Air));
	Attacks.Add(MakeAttack(TEXT("Zaubern"), FVector2D(150.0f, 900.0f), 0.8f, 0.0f, 0.0f, 0.0f, 7.0f, 0.0f, EVaelElement::Earth));
	Attacks.Add(MakeAttack(TEXT("Kratzen"), FVector2D(0.0f, 100.0f), 0.4f, 50.0f, 75.0f, 7.0f, 2.0f));
}

// ------------------------------------------------------------------ Misleading sign

AVaelIrrzeichen::AVaelIrrzeichen()
{
	PrimaryActorTick.bCanEverTick = true;
	Disc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Disc"));
	Disc->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	Disc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Disc->SetCastShadow(false);
	RootComponent = Disc;
	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Disc);
	Glow->SetCastShadows(false);
	Glow->SetLightColor(KoboldKristall);
	Glow->SetIntensity(0.0f);
}

AVaelIrrzeichen* AVaelIrrzeichen::Place(AActor* InCaster, const FVector& GroundLocation, float InRadius, float Seconds, float InConfusion)
{
	UWorld* World = InCaster != nullptr ? InCaster->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVaelIrrzeichen* Sign = World->SpawnActor<AVaelIrrzeichen>(GroundLocation + FVector(0.0f, 0.0f, 2.0f), FRotator::ZeroRotator, Parameters);
	if (Sign == nullptr)
	{
		return nullptr;
	}
	Sign->Caster = InCaster;
	Sign->Radius = InRadius;
	Sign->Lifetime = Seconds;
	Sign->Confusion = InConfusion;
	// A thin glowing disc in the colour of the Kobold crystals (the cylinder is 100 cm across and high)
	Sign->Disc->SetWorldScale3D(FVector(InRadius / 50.0f, InRadius / 50.0f, 0.015f));
	Sign->Look = UVaelTropfseuche::MakeLumpLook(Sign, EVaelSeuchenStamm::Verwesung);
	if (Sign->Look != nullptr)
	{
		Sign->Look->SetVectorParameterValue(TEXT("Farbe"), KoboldKristall);
		Sign->Look->SetScalarParameterValue(TEXT("Wabern"), 0.0f);
		Sign->Disc->SetMaterial(0, Sign->Look);
	}
	Sign->Glow->SetAttenuationRadius(InRadius * 2.0f);
	Sign->Glow->SetRelativeLocation(FVector(0.0f, 0.0f, 3000.0f));
	UE_LOG(LogVael, Log, TEXT("'%s' lays a misleading sign at %s"), *GetNameSafe(InCaster), *GroundLocation.ToCompactString());
	return Sign;
}

void AVaelIrrzeichen::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const float Shown = FMath::Clamp(Age / 0.6f, 0.0f, 1.0f) * FMath::Clamp((Lifetime - Age) / 0.8f, 0.0f, 1.0f);
	const float Beat = 0.6f + 0.4f * FMath::Sin(Age * 6.0f);
	if (Look != nullptr)
	{
		Look->SetScalarParameterValue(TEXT("Staerke"), 2.0f * Shown * Beat);
		Look->SetScalarParameterValue(TEXT("Sichtbar"), Shown);
	}
	Glow->SetIntensity(5.0f * Shown * Beat);
	if (Age >= Lifetime)
	{
		Destroy();
		return;
	}
	// Armed once it glows fully: whoever stands on it loses the direction
	if (Age > 0.6f && Age >= NextCheck)
	{
		NextCheck = Age + 0.25f;
		for (TActorIterator<AVaelCharacter> It(GetWorld()); It; ++It)
		{
			if (!It->IsDowned() && FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) <= Radius + It->GetSimpleCollisionRadius()
				&& !UVaelCombatStatics::HasStatus(*It, EVaelStatus::Confused))
			{
				UVaelCombatStatics::ApplyStatus(Caster.Get(), *It, EVaelStatus::Confused, Confusion);
				UE_LOG(LogVael, Log, TEXT("'%s' steps on a misleading sign and is confused"), *It->GetName());
			}
		}
	}
}

// ------------------------------------------------------------------ Kobold

AVaelKobold::AVaelKobold()
{
}

TSubclassOf<UVaelCreatureData> AVaelKobold::GetDefaultDataClass() const
{
	return UVaelKoboldData::StaticClass();
}

void AVaelKobold::BeginPlay()
{
	Super::BeginPlay();
	if (bIllusion)
	{
		// A false copy: one hit and it is gone
		if (UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent())
		{
			AbilitySystem->SetNumericAttributeBase(UVaelAttributeSet::GetMaxHealthAttribute(), 1.0f);
			AbilitySystem->SetNumericAttributeBase(UVaelAttributeSet::GetHealthAttribute(), 1.0f);
		}
	}
	UE_LOG(LogVael, Log, TEXT("'%s' is a living Kobold (%s)%s"), *GetName(), *GetCreatureName().ToString(), bIllusion ? TEXT(", a false copy") : TEXT(""));
}

float AVaelKobold::GetOutgoingDamageMultiplier() const
{
	return bIllusion ? 0.0f : Super::GetOutgoingDamageMultiplier();
}

void AVaelKobold::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bIllusion && !IsDead())
	{
		IllusionLeft -= DeltaSeconds;
		if (IllusionLeft <= 0.0f)
		{
			Vanish();
		}
	}
}

void AVaelKobold::TickBehavior(float DeltaSeconds)
{
	if (!bFleeing)
	{
		Super::TickBehavior(DeltaSeconds);
		return;
	}
	// Running off with what it took, away from the nearest player
	const UVaelKoboldData* Data = GetData<UVaelKoboldData>();
	float Distance = 0.0f;
	const AActor* Chaser = FindTarget(6000.0f, &Distance);
	const FVector Away = Chaser != nullptr ? (GetActorLocation() - Chaser->GetActorLocation()).GetSafeNormal2D() : GetActorForwardVector();
	TurnTowards(GetActorLocation() + Away * 400.0f, DeltaSeconds * 1.5f);
	MoveInDirection(GetActorForwardVector(), Data->FleeSpeed);
	Loop(Data->RunClip, 0.2f);
	if (Animator != nullptr)
	{
		Animator->SetRate(FMath::Clamp(GetVelocity().Size2D() / FMath::Max(91.0f * Size, 1.0f), 0.6f, 1.8f));
	}
	FleeLeft -= DeltaSeconds;
	if (FleeLeft <= 0.0f)
	{
		UVaelNoticeSubsystem::Post(this, LOCTEXT("ThiefGone", "Der Dieb ist weg"),
			LOCTEXT("ThiefGoneDetail", "Der Kobold ist mit deiner Beute in den Stollen verschwunden."), KoboldKristall, 3.5f);
		UE_LOG(LogVael, Log, TEXT("'%s' got away with %d stolen stacks"), *GetName(), Stolen.Num());
		Vanish();
	}
}

void AVaelKobold::OnPreyHit(AVaelCharacter* Player, const FVaelNightAttack& Attack)
{
	if (!bIllusion && GetData<UVaelKoboldData>()->Rolle == EVaelKoboldRolle::Dieb && Attack.Clip == TEXT("Kratzen"))
	{
		Steal(Player);
	}
}

void AVaelKobold::OnAttackLanded(const FVaelNightAttack& Attack)
{
	if (bIllusion || Attack.Clip != TEXT("Zaubern"))
	{
		return;
	}
	if (Attack.Element == EVaelElement::Air)
	{
		CastIllusions();
	}
	else
	{
		CastSign();
	}
}

void AVaelKobold::Steal(AVaelCharacter* Player)
{
	UVaelMaterialBag* Bag = Player != nullptr ? Player->GetMaterialBag() : nullptr;
	if (Bag == nullptr)
	{
		return;
	}
	// Anything but potions
	TArray<FVaelMaterialStack> Stacks = Bag->GetStacks();
	Stacks.RemoveAll([](const FVaelMaterialStack& Stack) { return Stack.Material == nullptr || Stack.Count <= 0 || Stack.Material->IsPotion(); });
	if (Stacks.IsEmpty())
	{
		return;
	}
	const FVaelMaterialStack& Stack = Stacks[FMath::RandHelper(Stacks.Num())];
	const int32 Count = FMath::Min(GetData<UVaelKoboldData>()->StealCount, Stack.Count);
	if (!Bag->RemoveMaterial(Stack.Material, Count))
	{
		return;
	}
	Stolen.Add({ Player, Stack.Material.Get(), Count });
	UVaelCombatTextSubsystem::PostReaction(Player, FText::Format(LOCTEXT("KoboldSteals", "−{0} {1}"), FText::AsNumber(Count), Stack.Material->DisplayName));
	UE_LOG(LogVael, Log, TEXT("'%s' steals %d x %s and runs"), *GetName(), Count, *GetNameSafe(Stack.Material));
	bFleeing = true;
	FleeLeft = GetData<UVaelKoboldData>()->FleeSeconds;
}

void AVaelKobold::ReturnLoot()
{
	int32 Returned = 0;
	for (const FVaelStolenGoods& Goods : Stolen)
	{
		AVaelCharacter* Victim = Goods.Owner.Get();
		if (Victim != nullptr && Goods.Material.IsValid() && Victim->GetMaterialBag() != nullptr)
		{
			Victim->GetMaterialBag()->AddMaterial(Goods.Material.Get(), Goods.Count);
			Returned += Goods.Count;
		}
	}
	Stolen.Reset();
	if (Returned > 0)
	{
		UVaelNoticeSubsystem::Post(this, LOCTEXT("LootBack", "Beute zurück"),
			FText::Format(LOCTEXT("LootBackDetail", "Der Kobold lässt fallen, was er gestohlen hat: {0} Teile."), FText::AsNumber(Returned)), KoboldKristall, 3.5f);
	}
}

void AVaelKobold::CastIllusions()
{
	const UVaelKoboldData* Data = GetData<UVaelKoboldData>();
	UWorld* World = GetWorld();
	const FVector Side = FVector::CrossProduct(GetActorForwardVector(), FVector::UpVector).GetSafeNormal();
	for (int32 Index = 0; Index < Data->IllusionCount; ++Index)
	{
		const float Sign = Index % 2 == 0 ? 1.0f : -1.0f;
		const FVector Spot = GetActorLocation() + Side * Sign * FMath::FRandRange(160.0f, 260.0f) + GetActorForwardVector() * FMath::FRandRange(-80.0f, 80.0f)
			+ FVector(0.0f, 0.0f, 200.0f);
		FVector Ground;
		if (!AVaelCreature::FindGround(World, Spot, Ground))
		{
			continue;
		}
		const float Seconds = Data->IllusionSeconds;
		AVaelCreature::SpawnCreature(World, const_cast<UVaelKoboldData*>(Data), Ground, GetActorRotation(), nullptr,
			[Seconds](AVaelCreature* Creature)
			{
				if (AVaelKobold* Copy = Cast<AVaelKobold>(Creature))
				{
					Copy->MakeIllusion(Seconds);
				}
			});
		AVaelPollenBurst::Spawn(World, Ground + FVector(0.0f, 0.0f, 50.0f), 90.0f, FLinearColor(0.75f, 0.7f, 0.9f), KoboldKristall, 1.2f);
	}
	UE_LOG(LogVael, Log, TEXT("'%s' makes %d false copies of itself"), *GetName(), Data->IllusionCount);
}

void AVaelKobold::CastSign()
{
	const UVaelKoboldData* Data = GetData<UVaelKoboldData>();
	if (!Prey.IsValid())
	{
		return;
	}
	FVector Ground;
	if (AVaelCreature::FindGround(GetWorld(), Prey->GetActorLocation() + FVector(0.0f, 0.0f, 100.0f), Ground))
	{
		AVaelIrrzeichen::Place(this, Ground, Data->SignRadius, Data->SignSeconds, Data->SignConfusion);
	}
}

void AVaelKobold::Vanish()
{
	AVaelPollenBurst::Spawn(GetWorld(), GetActorLocation(), 80.0f, FLinearColor(0.6f, 0.55f, 0.7f), KoboldKristall, 1.0f);
	Destroy();
}

void AVaelKobold::Die()
{
	if (bIllusion)
	{
		// Only a false image: it bursts into dust
		Vanish();
		return;
	}
	ReturnLoot();
	bFleeing = false;
	Super::Die();
}

// ------------------------------------------------------------------ In the caves

UVaelKoboldData* UVaelKobolde::GetData(EVaelKoboldRolle Rolle)
{
	switch (Rolle)
	{
	case EVaelKoboldRolle::Dieb:	return GetMutableDefault<UVaelKoboldDiebData>();
	case EVaelKoboldRolle::Zauberer:	return GetMutableDefault<UVaelKoboldZaubererData>();
	default:						return GetMutableDefault<UVaelKoboldWaechterData>();
	}
}

void UVaelKobolde::SpawnInCaves(UWorld* World)
{
	if (World == nullptr || !World->IsGameWorld())
	{
		return;
	}
	const FString Level = UWorld::RemovePIEPrefix(World->GetMapName());
	if (!Level.Contains(TEXT("L_KoboldStollen")) && !Level.Contains(TEXT("L_Ameisenbau")))
	{
		return;
	}
	int32 Count = 0;
	for (TActorIterator<ATargetPoint> It(World); It; ++It)
	{
		if (!It->ActorHasTag(TEXT("VaelHoehleGegner")) || FMath::FRand() > 0.75f)
		{
			continue;
		}
		// Mostly thieves, some guards, few casters (proposal)
		for (int32 Index = FMath::RandRange(1, 2); Index > 0; --Index)
		{
			const float Roll = FMath::FRand();
			const EVaelKoboldRolle Rolle = Roll < 0.5f ? EVaelKoboldRolle::Dieb : Roll < 0.8f ? EVaelKoboldRolle::Waechter : EVaelKoboldRolle::Zauberer;
			FVector Ground;
			if (AVaelCreature::FindGround(World, It->GetActorLocation() + FVector(FMath::FRandRange(-150.0f, 150.0f), FMath::FRandRange(-150.0f, 150.0f), 200.0f), Ground)
				&& AVaelCreature::SpawnCreature(World, GetData(Rolle), Ground, FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f)) != nullptr)
			{
				++Count;
			}
		}
	}
	UE_LOG(LogVael, Log, TEXT("Kobolde: %d living Kobolde in %s"), Count, *Level);
}

#undef LOCTEXT_NAMESPACE
