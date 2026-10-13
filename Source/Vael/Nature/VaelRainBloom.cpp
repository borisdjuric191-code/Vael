// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nature/VaelRainBloom.h"
#include "Combat/VaelCombatStatics.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Items/VaelMaterialBag.h"
#include "Player/VaelCharacter.h"
#include "UI/VaelCombatTextSubsystem.h"
#include "Vael.h"

#define LOCTEXT_NAMESPACE "VaelNature"

namespace
{
	/** Seconds a flower takes to grow out of the ground, and to wilt away */
	constexpr float BloomGrowTime = 1.4f;
	constexpr float BloomWiltTime = 1.6f;

	/** Seconds the lunge of a thorn flower shows */
	constexpr float BloomStrikeTime = 0.3f;

	/** Seconds a slumber flower dazes whoever is near; renewed while they stay */
	constexpr float BloomDazeTime = 0.6f;

	/** Colors of the texts over the players */
	const FLinearColor BloomHealColor(0.75f, 1.0f, 0.8f);
	const FLinearColor BloomHarvestColor(0.55f, 0.85f, 1.0f);
}

AVaelRainBloom::AVaelRainBloom()
{
	PrimaryActorTick.bCanEverTick = true;

	Model = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Model"));
	Model->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Model->SetCastShadow(true);
	RootComponent = Model;

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Model);
	Glow->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));
	Glow->SetIntensity(0.0f);
	Glow->SetAttenuationRadius(420.0f);
	Glow->SetCastShadows(false);
}

AVaelRainBloom* AVaelRainBloom::Grow(UWorld* World, const FVector& GroundLocation, const FVaelRainBloomKind& InKind, float InLifetime)
{
	if (World == nullptr)
	{
		return nullptr;
	}

	const FTransform SpawnTransform(FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), GroundLocation);
	AVaelRainBloom* Bloom = World->SpawnActorDeferred<AVaelRainBloom>(AVaelRainBloom::StaticClass(), SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Bloom != nullptr)
	{
		Bloom->Kind = InKind;
		Bloom->Lifetime = InLifetime;
		Bloom->SizeScale = FMath::FRandRange(0.85f, 1.15f);
		Bloom->FinishSpawning(SpawnTransform);
	}

	return Bloom;
}

void AVaelRainBloom::BeginPlay()
{
	Super::BeginPlay();

	if (UStaticMesh* Mesh = Kind.Mesh.IsNull() ? nullptr : Kind.Mesh.LoadSynchronous())
	{
		Model->SetStaticMesh(Mesh);
	}
	else
	{
		UE_LOG(LogVael, Warning, TEXT("'%s': the flower '%s' has no model"), *GetNameSafe(this), *Kind.DisplayName.ToString());
	}

	Model->SetRelativeScale3D(FVector(0.01f));
	Model->SetReceivesDecals(false);
	BaseYaw = GetActorRotation().Yaw;
	Glow->SetLightColor(Kind.GlowColor);

	// Only the good kinds can be used; the others act by themselves
	if (Kind.Kind == EVaelBloomKind::Harvest || Kind.Kind == EVaelBloomKind::Heal)
	{
		UVaelInteractionSubsystem::Register(this);
	}
}

void AVaelRainBloom::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UVaelInteractionSubsystem::Unregister(this);

	Super::EndPlay(EndPlayReason);
}

bool AVaelRainBloom::CanInteract(const AVaelCharacter* Player) const
{
	return WiltAge < 0.0f && Age >= BloomGrowTime && (Kind.Kind == EVaelBloomKind::Harvest || Kind.Kind == EVaelBloomKind::Heal);
}

FText AVaelRainBloom::GetInteractPrompt() const
{
	return Kind.Kind == EVaelBloomKind::Heal
		? FText::Format(LOCTEXT("BloomTouch", "{0} berühren"), Kind.DisplayName)
		: FText::Format(LOCTEXT("BloomPick", "{0} pflücken"), Kind.DisplayName);
}

void AVaelRainBloom::Interact(AVaelCharacter* Player)
{
	if (Player == nullptr || !CanInteract(Player))
	{
		return;
	}

	if (Kind.Kind == EVaelBloomKind::Heal)
	{
		UVaelCombatStatics::Heal(Player, Kind.Amount);
		UVaelCombatTextSubsystem::PostPickup(Player, FText::Format(LOCTEXT("BloomHealed", "{0}: +{1} Leben"), Kind.DisplayName, FText::AsNumber(FMath::RoundToInt(Kind.Amount))), BloomHealColor);
	}
	else
	{
		UVaelMaterialBag::GiveToGroup(this, Kind.Loot);
		UVaelCombatTextSubsystem::PostPickup(Player, FText::Format(LOCTEXT("BloomPicked", "{0} gepflückt"), Kind.DisplayName), BloomHarvestColor);
	}

	Wilt();
}

void AVaelRainBloom::Wilt()
{
	if (WiltAge < 0.0f)
	{
		WiltAge = 0.0f;
		UVaelInteractionSubsystem::Unregister(this);
	}
}

void AVaelRainBloom::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;
	if (Age >= Lifetime)
	{
		Wilt();
	}

	// Grows out of the wet earth, stands, sinks and shrivels
	float Size = FMath::InterpEaseOut(0.0f, 1.0f, FMath::Clamp(Age / BloomGrowTime, 0.0f, 1.0f), 2.0f);
	float Droop = 0.0f;
	if (WiltAge >= 0.0f)
	{
		WiltAge += DeltaSeconds;
		const float Wilted = FMath::Clamp(WiltAge / BloomWiltTime, 0.0f, 1.0f);
		Size *= 1.0f - Wilted;
		Droop = Wilted * 55.0f;

		if (Wilted >= 1.0f)
		{
			Destroy();
			return;
		}
	}

	const bool bActs = WiltAge < 0.0f && Age >= BloomGrowTime;

	// The bad kinds act on everyone who comes near, without being touched
	if (bActs && (Kind.Kind == EVaelBloomKind::Thorn || Kind.Kind == EVaelBloomKind::Slumber))
	{
		StrikeCountdown -= DeltaSeconds;
		const bool bStrikes = Kind.Kind == EVaelBloomKind::Thorn && StrikeCountdown <= 0.0f;
		bool bHitAnyone = false;

		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			AVaelCharacter* Player = It->IsValid() ? Cast<AVaelCharacter>((*It)->GetPawn()) : nullptr;
			if (Player == nullptr || Player->IsDefeated() || FVector::Dist2D(Player->GetActorLocation(), GetActorLocation()) > Kind.Radius)
			{
				continue;
			}

			if (Kind.Kind == EVaelBloomKind::Slumber)
			{
				Player->SetBurden(Kind.Amount, BloomDazeTime);
			}
			else if (bStrikes)
			{
				UVaelCombatStatics::DealDamage(this, Player, Kind.Amount, EVaelElement::Earth);
				bHitAnyone = true;
			}
		}

		if (bStrikes)
		{
			StrikeCountdown = Kind.Interval;
			StrikeShown = bHitAnyone ? BloomStrikeTime : 0.0f;
		}
	}

	// A thorn flower lunges as it strikes; a slumber flower nods; all sway a little
	StrikeShown = FMath::Max(StrikeShown - DeltaSeconds, 0.0f);
	const float Lunge = StrikeShown / BloomStrikeTime;
	const float Sway = FMath::Sin(Age * 1.7f + GetActorLocation().X * 0.01f) * (Kind.Kind == EVaelBloomKind::Slumber ? 5.0f : 2.0f);

	Model->SetRelativeScale3D(FVector(1.0f + Lunge * 0.25f, 1.0f + Lunge * 0.25f, 1.0f - Lunge * 0.15f) * Size * SizeScale);
	Model->SetRelativeRotation(FRotator(Sway + Droop, BaseYaw, Sway * 0.6f));

	// Only the healing flower shines, so it can be told from the others
	const float Shine = Kind.Kind == EVaelBloomKind::Heal && bActs ? 2600.0f + FMath::Sin(Age * 2.2f) * 500.0f : 0.0f;
	Glow->SetIntensity(Shine * Size);
}

#undef LOCTEXT_NAMESPACE
