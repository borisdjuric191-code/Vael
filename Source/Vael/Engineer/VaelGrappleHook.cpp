// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engineer/VaelGrappleHook.h"

#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelCombatStatics.h"
#include "Components/StaticMeshComponent.h"
#include "Creatures/VaelCreature.h"
#include "Creatures/VaelCreatureData.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engineer/VaelEngineerComponent.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/VaelCharacter.h"
#include "UI/VaelCombatTextSubsystem.h"
#include "Vael.h"

#define LOCTEXT_NAMESPACE "VaelGrappleHook"

namespace
{
	constexpr float HookSpeed = 3200.0f;
	constexpr float HookRange = 1100.0f;
	constexpr float HookCatch = 80.0f;
	constexpr float ReelSpeed = 4200.0f;
	constexpr float YankSpeed = 2200.0f;
	constexpr float SelfPullSpeed = 2400.0f;
	/** The jerk is a hop with this much lift; its speed is reckoned so it lands in front of the other */
	constexpr float HopLift = 420.0f;
	constexpr float MaxPullSeconds = 1.2f;
	constexpr float SnagDamage = 12.0f;
	constexpr float YankedConfused = 1.0f;

	FVector HandOf(const AActor* Owner)
	{
		return Owner != nullptr ? Owner->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f) : FVector::ZeroVector;
	}
}

AVaelGrappleHook::AVaelGrappleHook()
{
	PrimaryActorTick.bCanEverTick = true;

	Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Head->SetCastShadow(false);
	RootComponent = Head;

	// The rope is not attached: it is laid between the hand and the hook each frame in world space
	Rope = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Rope"));
	Rope->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Rope->SetCastShadow(false);
	Rope->SetupAttachment(Head);
	Rope->SetUsingAbsoluteLocation(true);
	Rope->SetUsingAbsoluteRotation(true);
	Rope->SetUsingAbsoluteScale(true);
}

AVaelGrappleHook* AVaelGrappleHook::Throw(UVaelEngineerComponent* InEngineer, const FVector& From, const FVector& InDirection)
{
	UWorld* World = InEngineer != nullptr ? InEngineer->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}
	FVector Flat = InDirection;
	Flat.Z = 0.0f;
	Flat = Flat.GetSafeNormal();
	if (Flat.IsNearlyZero())
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVaelGrappleHook* Hook = World->SpawnActor<AVaelGrappleHook>(From, Flat.Rotation(), Params);
	if (Hook == nullptr)
	{
		return nullptr;
	}
	Hook->Engineer = InEngineer;
	Hook->Direction = Flat;
	Hook->PhaseStart = World->GetTimeSeconds();

	// Placeholder: a claw (cone pointing ahead) and a rope of fibre
	UStaticMesh* Cone = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	Hook->Head->SetStaticMesh(Cone);
	Hook->Head->SetRelativeScale3D(FVector(0.22f, 0.22f, 0.3f));
	Hook->Head->SetWorldRotation(FRotationMatrix::MakeFromZ(Flat).Rotator());
	Hook->Rope->SetStaticMesh(Cylinder);
	const TPair<UStaticMeshComponent*, FLinearColor> Looks[] = {
		{ Hook->Head.Get(), FLinearColor(FColor(120, 100, 80)) },
		{ Hook->Rope.Get(), FLinearColor(FColor(150, 125, 85)) } };
	for (const TPair<UStaticMeshComponent*, FLinearColor>& Look : Looks)
	{
		Look.Key->SetMaterial(0, Material);
		if (UMaterialInstanceDynamic* Dynamic = Look.Key->CreateAndSetMaterialInstanceDynamic(0))
		{
			Dynamic->SetVectorParameterValue(TEXT("Color"), Look.Value);
		}
	}
	Hook->UpdateRope();
	UE_LOG(LogVael, Log, TEXT("Engineer: Wurfhaken thrown towards %s"), *Flat.ToString());
	return Hook;
}

void AVaelGrappleHook::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UVaelEngineerComponent* Eng = Engineer.Get();
	AVaelCharacter* Player = Eng != nullptr ? Cast<AVaelCharacter>(Eng->GetOwner()) : nullptr;
	if (Player == nullptr || Player->IsDowned())
	{
		Destroy();
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();

	switch (Phase)
	{
	case EPhase::Flying:
	{
		const FVector Start = GetActorLocation();
		const float Step = HookSpeed * DeltaSeconds;
		const FVector End = Start + Direction * Step;

		// The first enemy along this step (nearest to where the hook was)
		AVaelCharacterBase* Best = nullptr;
		float BestAlong = Step + 1.0f;
		for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
		{
			if (It->IsA<AVaelCharacter>() || It->GetHealth() <= 0.0f || !UVaelCombatStatics::CanDamage(Player, *It))
			{
				continue;
			}
			const FVector To = It->GetActorLocation() - Start;
			const float Along = FVector::DotProduct(FVector(To.X, To.Y, 0.0f), Direction);
			const float Side = FVector::Dist2D(Start + Direction * FMath::Clamp(Along, 0.0f, Step), It->GetActorLocation());
			if (Along > -HookCatch && Along < BestAlong && Side < HookCatch + It->GetSimpleCollisionRadius() * 0.6f)
			{
				Best = *It;
				BestAlong = Along;
			}
		}
		if (Best != nullptr)
		{
			Snag(Best);
			return;
		}

		// It skims along the ground (the Aschenmark is all slopes); only a steep rock in the way stops it, it clatters off
		// and comes back
		FHitResult Hit;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(VaelGrappleHook), false, Player);
		Query.AddIgnoredActor(this);
		FVector Next = End;
		if (GetWorld()->LineTraceSingleByChannel(Hit, End + FVector(0.0f, 0.0f, 150.0f), End - FVector(0.0f, 0.0f, 400.0f), ECC_WorldStatic, Query))
		{
			Next.Z = Hit.ImpactPoint.Z + 60.0f;
		}
		if (GetWorld()->LineTraceSingleByChannel(Hit, Start, Next, ECC_WorldStatic, Query) && Cast<APawn>(Hit.GetActor()) == nullptr && Hit.ImpactNormal.Z < 0.5f)
		{
			UE_LOG(LogVael, Log, TEXT("Engineer: Wurfhaken stopped by %s after %.0f cm"), *GetNameSafe(Hit.GetActor()), Travelled);
			SetActorLocation(Hit.ImpactPoint - Direction * 10.0f);
			Phase = EPhase::Reeling;
			UpdateRope();
			return;
		}
		SetActorLocation(Next);
		Travelled += Step;
		if (Travelled >= HookRange && Phase == EPhase::Flying)
		{
			UE_LOG(LogVael, Log, TEXT("Engineer: Wurfhaken caught nothing in %.0f cm"), Travelled);
			Phase = EPhase::Reeling;
		}
		break;
	}
	case EPhase::Pulling:
	{
		AVaelCharacterBase* Target = Snagged.Get();
		const bool bTimeUp = Now - PhaseStart > MaxPullSeconds;
		if (Target == nullptr || Target->GetHealth() <= 0.0f || bTimeUp)
		{
			Phase = EPhase::Reeling;
			break;
		}
		const float Gap = FVector::Dist2D(Target->GetActorLocation(), Player->GetActorLocation());
		const float Close = Target->GetSimpleCollisionRadius() + Player->GetSimpleCollisionRadius() + 60.0f;
		if (Gap <= Close)
		{
			if (!bPullEngineer)
			{
				// yanked off its feet: it staggers a moment in front of him
				UVaelCombatStatics::ApplyStatus(Player, Target, EVaelStatus::Confused, YankedConfused);
			}
			Phase = EPhase::Reeling;
			break;
		}
		SetActorLocation(Target->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f));
		break;
	}
	case EPhase::Reeling:
	{
		const FVector Hand = HandOf(Player);
		const FVector To = Hand - GetActorLocation();
		const float Step = ReelSpeed * DeltaSeconds;
		if (To.Size() <= Step)
		{
			Destroy();
			return;
		}
		SetActorLocation(GetActorLocation() + To.GetSafeNormal() * Step);
		break;
	}
	}
	UpdateRope();
}

void AVaelGrappleHook::Snag(AVaelCharacterBase* Target)
{
	UVaelEngineerComponent* Eng = Engineer.Get();
	AActor* Thrower = Eng != nullptr ? Eng->GetOwner() : nullptr;
	Snagged = Target;
	const AVaelCreature* Creature = Cast<AVaelCreature>(Target);
	bPullEngineer = Creature == nullptr || Creature->IsHeavy();
	Phase = EPhase::Pulling;
	PhaseStart = GetWorld()->GetTimeSeconds();
	SetActorLocation(Target->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f));

	// One hard jerk on the rope: a short hop through the air that lands right in front of the other (a hop gets over the
	// bumps of the ground where a slide would stick). Small ones fly to him, as far as they let themselves be pulled; he
	// flies to the heavy ones.
	if (ACharacter* Pulled = bPullEngineer ? Cast<ACharacter>(Thrower) : Target)
	{
		const AActor* Anchor = bPullEngineer ? static_cast<AActor*>(Target) : Thrower;
		FVector To = Anchor != nullptr ? Anchor->GetActorLocation() - Pulled->GetActorLocation() : FVector::ZeroVector;
		// the time in the air of the hop, so it lands where the other stands even up or down a slope
		const float Gravity = FMath::Max(FMath::Abs(Pulled->GetCharacterMovement()->GetGravityZ()), 100.0f);
		const float Rise = FMath::Clamp(To.Z, -400.0f, 0.9f * HopLift * HopLift / (2.0f * Gravity));
		const float AirTime = (HopLift + FMath::Sqrt(FMath::Max(HopLift * HopLift - 2.0f * Gravity * Rise, 0.0f))) / Gravity;
		To.Z = 0.0f;
		const float Close = Target->GetSimpleCollisionRadius() + Pulled->GetSimpleCollisionRadius() + 60.0f;
		const float Willing = !bPullEngineer && Creature != nullptr && Creature->GetCreatureData() != nullptr ? Creature->GetCreatureData()->PullMultiplier : 1.0f;
		const float Speed = FMath::Min(FMath::Max(To.Size() - Close, 0.0f) / AirTime, bPullEngineer ? SelfPullSpeed : YankSpeed) * FMath::Clamp(Willing, 0.0f, 1.5f);
		Pulled->LaunchCharacter(To.GetSafeNormal() * Speed + FVector(0.0f, 0.0f, HopLift), true, true);
	}
	UVaelCombatStatics::DealDamage(Thrower, Target, SnagDamage, EVaelElement::Earth);
	UVaelCombatTextSubsystem::PostReaction(Target, bPullEngineer ? LOCTEXT("HookHeavy", "Festgehakt") : LOCTEXT("HookYank", "Herangerissen"));
	UE_LOG(LogVael, Log, TEXT("Engineer: Wurfhaken snags %s (%s)"), *Target->GetName(), bPullEngineer ? TEXT("pulls him") : TEXT("yanks it"));
	UpdateRope();
}

void AVaelGrappleHook::UpdateRope()
{
	const UVaelEngineerComponent* Eng = Engineer.Get();
	const FVector Hand = HandOf(Eng != nullptr ? Eng->GetOwner() : nullptr);
	const FVector Tip = GetActorLocation();
	const FVector Span = Tip - Hand;
	const float Length = Span.Size();
	Rope->SetVisibility(Length > 5.0f);
	if (Length <= 5.0f)
	{
		return;
	}
	// The engine cylinder is 100 cm high round its middle
	Rope->SetWorldLocationAndRotation((Hand + Tip) * 0.5f, FRotationMatrix::MakeFromZ(Span / Length).Rotator());
	Rope->SetWorldScale3D(FVector(0.025f, 0.025f, Length / 100.0f));
}

#undef LOCTEXT_NAMESPACE
