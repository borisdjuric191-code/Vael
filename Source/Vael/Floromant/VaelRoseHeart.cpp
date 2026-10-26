// Copyright Epic Games, Inc. All Rights Reserved.

#include "Floromant/VaelRoseHeart.h"
#include "Combat/VaelCombatStatics.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Floromant/VaelPlant.h"
#include "GameFramework/PlayerController.h"
#include "Magic/VaelDebrisBurst.h"
#include "Magic/VaelMagicSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/VaelCharacter.h"
#include "UI/VaelCombatTextSubsystem.h"
#include "UObject/ConstructorHelpers.h"
#include "Vael.h"
#include "VaelAssets.h"

namespace
{
	/** Every player character in the world */
	TArray<AVaelCharacter*> RosePlayers(const UWorld* World)
	{
		TArray<AVaelCharacter*> Players;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (AVaelCharacter* Player = It->Get() != nullptr ? It->Get()->GetPawn<AVaelCharacter>() : nullptr)
			{
				Players.Add(Player);
			}
		}
		return Players;
	}

	/** Radius of the engine sphere in cm */
	constexpr float HeartSphereRadius = 50.0f;
}

namespace VaelRoseCare
{
	AVaelCharacter* FindHurtAlly(const AVaelCharacter* Gardener, const FVector& From, float Range, bool bIncludeSelf, bool bPreferDowned, const FVector& Aim)
	{
		if (Gardener == nullptr)
		{
			return nullptr;
		}
		AVaelCharacter* Best = nullptr;
		float BestScore = -1.0f;
		const FVector Ahead = Aim.GetSafeNormal2D();
		for (AVaelCharacter* Player : RosePlayers(Gardener->GetWorld()))
		{
			if ((Player == Gardener && !bIncludeSelf) || FVector::Dist2D(Player->GetActorLocation(), From) > Range)
			{
				continue;
			}
			const bool bDown = Player->IsDowned();
			const float Missing = bDown ? 1.0f : 1.0f - Player->GetHealth() / FMath::Max(Player->GetMaxHealth(), 1.0f);
			if (Missing <= 0.01f)
			{
				continue;
			}
			float Score = Missing + (bDown && bPreferDowned ? 2.0f : 0.0f) - (bDown && !bPreferDowned ? 5.0f : 0.0f);
			if (!Ahead.IsNearlyZero() && Player != Gardener)
			{
				Score += 0.5f * FVector::DotProduct((Player->GetActorLocation() - From).GetSafeNormal2D(), Ahead);
			}
			if (Score > BestScore)
			{
				BestScore = Score;
				Best = Player;
			}
		}
		return BestScore > -1.0f ? Best : nullptr;
	}

	int32 HealAround(AActor* WorldContext, const FVector& Center, float Radius, float ShareOfMax)
	{
		int32 Count = 0;
		for (AVaelCharacter* Player : RosePlayers(WorldContext->GetWorld()))
		{
			if (!Player->IsDowned() && FVector::Dist2D(Player->GetActorLocation(), Center) < Radius && Player->GetHealth() < Player->GetMaxHealth())
			{
				UVaelCombatStatics::Heal(Player, Player->GetMaxHealth() * ShareOfMax);
				Petals(WorldContext, Center + FVector(0.0f, 0.0f, 60.0f), Player->GetActorLocation(), false, 4);
				++Count;
			}
		}
		return Count;
	}

	int32 DrainAround(AVaelCharacter* Gardener, const FVector& Center, float Radius, float Damage, float HealShare)
	{
		if (Gardener == nullptr)
		{
			return 0;
		}
		int32 Count = 0;
		FVaelSpellHit Drain;
		Drain.Damage = Damage;
		Drain.Element = EVaelElement::Earth;
		for (TActorIterator<AVaelCharacterBase> It(Gardener->GetWorld()); It; ++It)
		{
			if (FVector::Dist2D(It->GetActorLocation(), Center) < Radius && UVaelCombatStatics::CanDamage(Gardener, *It))
			{
				UVaelCombatStatics::ApplySpellHit(Gardener, *It, Drain, FVector::ZeroVector);
				Petals(Gardener, It->GetActorLocation(), Center + FVector(0.0f, 0.0f, 60.0f), true, 3);
				++Count;
			}
		}
		if (Count > 0)
		{
			UVaelCombatStatics::Heal(Gardener, Damage * Count * HealShare);
		}
		return Count;
	}

	int32 CleanseAround(AActor* WorldContext, const FVector& Center, float Radius, float PlantHeal)
	{
		int32 Count = 0;
		for (AVaelCharacter* Player : RosePlayers(WorldContext->GetWorld()))
		{
			if (FVector::Dist2D(Player->GetActorLocation(), Center) < Radius)
			{
				for (const EVaelStatus Status : { EVaelStatus::Burning, EVaelStatus::Wet, EVaelStatus::Frozen, EVaelStatus::Slowed })
				{
					UVaelCombatStatics::RemoveStatus(Player, Status);
				}
				++Count;
			}
		}
		for (TActorIterator<AVaelPlant> It(WorldContext->GetWorld()); It; ++It)
		{
			if (FVector::Dist2D(It->GetActorLocation(), Center) < Radius)
			{
				UVaelCombatStatics::Heal(*It, PlantHeal);
			}
		}
		return Count;
	}

	void Petals(AActor* WorldContext, const FVector& From, const FVector& To, bool bBlood, int32 Count)
	{
		// The petals scatter where they arrive (the burst flies outwards from one point)
		FVaelDebris Leaves;
		Leaves.Count = Count;
		Leaves.Spread = 25.0f;
		Leaves.Speed = 120.0f;
		Leaves.Lift = 60.0f;
		Leaves.Gravity = 60.0f;
		Leaves.Drag = 1.6f;
		Leaves.Lifetime = 0.7f;
		Leaves.MinSize = 3.0f;
		Leaves.MaxSize = 6.0f;
		Leaves.Color = bBlood ? FLinearColor(0.6f, 0.02f, 0.05f) : FLinearColor(0.2f, 0.9f, 0.45f);
		Leaves.Glow = 3.0f;
		Leaves.bSoft = true;
		Leaves.bLandOnGround = false;
		AVaelDebrisBurst::Spawn(WorldContext, To, Leaves);
	}
}

AVaelRoseHeart::AVaelRoseHeart()
{
	PrimaryActorTick.bCanEverTick = true;

	Heart = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Heart"));
	RootComponent = Heart;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		Heart->SetStaticMesh(Sphere.Object);
	}
	Heart->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Heart->SetCastShadow(false);
	Heart->SetWorldScale3D(FVector(18.0f / (HeartSphereRadius * 2.0f)));

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Heart);
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(40.0f);
	Light->SetAttenuationRadius(380.0f);
	Light->SetCastShadows(false);
}

AVaelRoseHeart* AVaelRoseHeart::Shoot(AVaelCharacter* InGardener, const FVector& From, AVaelCharacter* InAlly, const FVector& Aim, float InHealShare, float InReviveShare, float InSpeed,
	const FLinearColor& InColor, UStaticMesh* RoseMesh, float InRoseSize)
{
	UWorld* World = InGardener != nullptr ? InGardener->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}
	FActorSpawnParameters Parameters;
	Parameters.Owner = InGardener;
	Parameters.Instigator = InGardener;
	AVaelRoseHeart* Shot = World->SpawnActor<AVaelRoseHeart>(AVaelRoseHeart::StaticClass(), FTransform(From), Parameters);
	if (Shot == nullptr)
	{
		return nullptr;
	}
	Shot->Gardener = InGardener;
	Shot->Ally = InAlly != InGardener ? InAlly : nullptr;
	Shot->HealShare = InHealShare;
	Shot->ReviveShare = InReviveShare;
	Shot->Speed = InSpeed;
	Shot->Color = InColor;
	Shot->RoseSize = InRoseSize;
	Shot->Light->SetLightColor(InColor);

	// Alone the heart flies out along the aim, turns and comes back to her
	const FVector Ahead = Aim.GetSafeNormal2D().IsNearlyZero() ? InGardener->GetActorForwardVector() : Aim.GetSafeNormal2D();
	Shot->TurnPoint = From + Ahead * 420.0f;

	// The glowing heart
	if (UMaterialInterface* Core = VaelAssets::LoadOptional(UVaelMagicSettings::Get()->ElementOrbCoreMaterial))
	{
		UMaterialInstanceDynamic* Glow = UMaterialInstanceDynamic::Create(Core, Shot);
		Glow->SetVectorParameterValue(TEXT("Color"), InColor);
		Glow->SetScalarParameterValue(TEXT("Glow"), 12.0f);
		Shot->Heart->SetMaterial(0, Glow);
	}

	// The big rose the two shoulder roses grew into stays a moment where it shot, open, and fades
	if (RoseMesh != nullptr)
	{
		Shot->Rose = NewObject<UStaticMeshComponent>(Shot);
		Shot->Rose->SetStaticMesh(RoseMesh);
		Shot->Rose->SetUsingAbsoluteLocation(true);
		Shot->Rose->SetUsingAbsoluteRotation(true);
		Shot->Rose->SetUsingAbsoluteScale(true);
		Shot->Rose->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Shot->Rose->SetupAttachment(Shot->Heart);
		Shot->Rose->RegisterComponent();
		Shot->Rose->SetWorldLocationAndRotation(From - Ahead * 8.0f, FRotationMatrix::MakeFromZ(Ahead + FVector(0.0f, 0.0f, 0.3f)).Rotator());
		Shot->Rose->SetWorldScale3D(FVector(InRoseSize / 10.0f));
	}
	UE_LOG(LogVael, Log, TEXT("'%s' shoots the heart of the Rußrose at '%s'"), *GetNameSafe(InGardener), *GetNameSafe(Shot->Ally.Get()));
	return Shot;
}

void AVaelRoseHeart::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;

	AVaelCharacter* Shooter = Gardener.Get();
	if (Shooter == nullptr || Age > 8.0f)
	{
		Destroy();
		return;
	}

	// The rose it left fades, closing a little
	if (Rose != nullptr)
	{
		const float Fade = FMath::Clamp(1.0f - Age / 0.6f, 0.0f, 1.0f);
		Rose->SetWorldScale3D(FVector(RoseSize / 10.0f * FMath::Max(Fade, 0.01f)));
		Rose->SetVisibility(Fade > 0.0f);
	}

	// Where it flies: the ally, or out and back to her
	AVaelCharacter* Goal = Ally.Get();
	FVector Target;
	if (Goal != nullptr)
	{
		Target = Goal->GetActorLocation() + FVector(0.0f, 0.0f, 30.0f);
	}
	else if (!bReturning)
	{
		Target = TurnPoint;
	}
	else
	{
		Target = Shooter->GetActorLocation() + FVector(0.0f, 0.0f, 30.0f);
	}

	const FVector Here = GetActorLocation();
	const FVector Way = Target - Here;
	const float Step = Speed * DeltaSeconds;
	const float Beat = 1.0f + 0.25f * FMath::Square(FMath::Sin(Age * 9.0f));
	Heart->SetWorldScale3D(FVector(18.0f * Beat / (HeartSphereRadius * 2.0f)));
	Light->SetIntensity(40.0f * Beat);

	// Green motes trail behind it
	SinceMote += DeltaSeconds;
	if (SinceMote > 0.1f)
	{
		SinceMote = 0.0f;
		VaelRoseCare::Petals(this, Here, Here - Way.GetSafeNormal() * 60.0f, false, 2);
	}

	if (Way.Size() > Step)
	{
		SetActorLocation(Here + Way.GetSafeNormal() * Step);
		return;
	}
	SetActorLocation(Target);
	if (Goal == nullptr && !bReturning)
	{
		bReturning = true;
		return;
	}
	Arrive(Goal != nullptr ? Goal : Shooter);
	Destroy();
}

void AVaelRoseHeart::Arrive(AVaelCharacter* Reached)
{
	if (Reached == nullptr)
	{
		return;
	}

	// A fallen ally gets back on their feet; anyone else is healed, herself only by half when it came back to her
	if (Reached->IsDowned())
	{
		Reached->Revive(Reached->GetMaxHealth() * ReviveShare, 2.0f);
		UVaelCombatTextSubsystem::PostPickup(Reached, NSLOCTEXT("VaelRose", "Revived", "Herz der Rußrose"), Color);
	}
	else
	{
		const bool bSelf = Reached == Gardener.Get();
		UVaelCombatStatics::Heal(Reached, Reached->GetMaxHealth() * HealShare * (bSelf ? 0.5f : 1.0f));
	}

	FVaelDebris Bloom;
	Bloom.Count = 24;
	Bloom.Spread = 40.0f;
	Bloom.Speed = 240.0f;
	Bloom.Lift = 160.0f;
	Bloom.Gravity = 40.0f;
	Bloom.Drag = 2.0f;
	Bloom.Lifetime = 1.0f;
	Bloom.MinSize = 3.0f;
	Bloom.MaxSize = 7.0f;
	Bloom.Color = Color;
	Bloom.Glow = 5.0f;
	Bloom.bSoft = true;
	Bloom.bLandOnGround = false;
	AVaelDebrisBurst::Spawn(this, Reached->GetActorLocation(), Bloom);
}
