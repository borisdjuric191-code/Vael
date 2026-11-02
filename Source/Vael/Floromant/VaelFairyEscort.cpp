// Copyright Epic Games, Inc. All Rights Reserved.

#include "Floromant/VaelFairyEscort.h"
#include "Combat/VaelCombatStatics.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Creatures/VaelCreature.h"
#include "EngineUtils.h"
#include "Floromant/VaelPlantArmData.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInterface.h"
#include "Vael.h"
#include "VaelAssets.h"

namespace
{
	/** Her ring around the Floromant: its radius, height above her feet and turns per second */
	constexpr float RingRadius = 150.0f;

	constexpr float RingSpeed = 0.06f;

	/** Closer than this she has arrived */
	constexpr float ArriveDistance = 70.0f;

	/** She circles an enemy at this radius, this height above its feet and this many radians per second */
	constexpr float CircleRadius = 90.0f;
	constexpr float CircleHeight = 60.0f;
	constexpr float CircleSpeed = 2.6f;

	/** A status lasts a little longer than the step that renews it */
	constexpr float LureStep = 0.6f;
	constexpr float StingEvery = 0.6f;
	constexpr float HopReach = 700.0f;
}

AVaelFairyEscort::AVaelFairyEscort()
{
	PrimaryActorTick.bCanEverTick = true;
	Fairies = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Fairies"));
	RootComponent = Fairies;
	Fairies->SetUsingAbsoluteLocation(true);
	Fairies->SetUsingAbsoluteRotation(true);
	Fairies->SetUsingAbsoluteScale(true);
	Fairies->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Fairies->SetGenerateOverlapEvents(false);
	Fairies->SetCanEverAffectNavigation(false);
	Fairies->NumCustomDataFloats = 4;
}

void AVaelFairyEscort::Setup(APawn* InMistress, const UVaelPlantArmData* InPlant, float Side, AActor* InAnchor, float InRingHeight)
{
	Mistress = InMistress;
	Anchor = InAnchor != nullptr ? InAnchor : InMistress;
	RingHeight = InRingHeight;
	Plant = InPlant;
	RingTurn = Side;
	Flight.FlySpeed = Plant->FairySpeed;
	Flight.Scale = Plant->FairyScale;
	Fairies->SetStaticMesh(VaelAssets::LoadOptional(Plant->FairyMesh));
	if (UMaterialInterface* Material = VaelAssets::LoadOptional(Plant->FairyMaterial))
	{
		Fairies->SetMaterial(0, Material);
	}
	Fairies->SetHasPerInstancePrevTransforms(true);
	for (int32 Index = 0; Index < Plant->FairyCount; ++Index)
	{
		FEscortFairy& Fairy = Flock.AddDefaulted_GetRef();
		Fairy.Seed = FMath::FRandRange(0.0f, UE_TWO_PI);
		Fairy.BeatPhase = Fairy.Seed;
		Fairy.Location = RingPlace(Index);
		Fairies->AddInstance(FTransform(Fairy.Location));
		VaelFairyFlight::Show(Fairies, Index, Fairy, 0.0f, Flight, Index == Plant->FairyCount - 1);
	}
}

FVector AVaelFairyEscort::RingPlace(int32 Index) const
{
	const float Angle = UE_TWO_PI * (Clock * RingSpeed + RingTurn + float(Index) / FMath::Max(Flock.Num(), 1));
	const FVector Middle = Anchor != nullptr ? Anchor->GetActorLocation() : GetActorLocation();
	return Middle + FVector(FMath::Cos(Angle) * RingRadius, FMath::Sin(Angle) * RingRadius, RingHeight);
}

int32 AVaelFairyEscort::CountReady() const
{
	int32 Ready = 0;
	for (const FEscortFairy& Fairy : Flock)
	{
		Ready += Fairy.Duty == EDuty::Follow && Fairy.RestSeconds <= 0.0f ? 1 : 0;
	}
	return Ready;
}

bool AVaelFairyEscort::Send(AActor* Target, bool bShadow)
{
	for (FEscortFairy& Fairy : Flock)
	{
		if (Target != nullptr && Fairy.Duty == EDuty::Follow && Fairy.RestSeconds <= 0.0f)
		{
			Fairy.Duty = EDuty::ToEnemy;
			Fairy.Target = Target;
			Fairy.bShadow = bShadow;
			return true;
		}
	}
	return false;
}

bool AVaelFairyEscort::SendAll(const FVector& Place, bool bShadow)
{
	bool bSent = false;
	for (FEscortFairy& Fairy : Flock)
	{
		if (Fairy.Duty == EDuty::Follow && Fairy.RestSeconds <= 0.0f)
		{
			Fairy.Duty = EDuty::ToPlace;
			Fairy.Place = Place;
			Fairy.bShadow = bShadow;
			bSent = true;
		}
	}
	return bSent;
}

void AVaelFairyEscort::Dismiss(FEscortFairy& Fairy)
{
	Fairy.Duty = EDuty::Follow;
	Fairy.Target = nullptr;
	Fairy.RestSeconds = Plant->FairyRest;
}

AActor* AVaelFairyEscort::FindNextEnemy(const FEscortFairy& Fairy) const
{
	AActor* Best = nullptr;
	float BestDistance = FMath::Square(HopReach);
	for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
	{
		const float Distance = FVector::DistSquared2D(It->GetActorLocation(), Fairy.Location);
		if (*It != Fairy.Target.Get() && !It->IsDead() && UVaelCombatStatics::CanDamage(Mistress, *It) && Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = *It;
		}
	}
	return Best;
}

void AVaelFairyEscort::Serve(FEscortFairy& Fairy, float DeltaSeconds)
{
	AVaelCreature* Enemy = Cast<AVaelCreature>(Fairy.Target.Get());
	if ((Fairy.Duty == EDuty::ToEnemy || Fairy.Duty == EDuty::Distract || Fairy.Duty == EDuty::Sting) && (Enemy == nullptr || Enemy->IsDead()))
	{
		Dismiss(Fairy);
		return;
	}

	switch (Fairy.Duty)
	{
	case EDuty::ToEnemy:
		if (FVector::Dist(Fairy.Location, Enemy->GetActorLocation()) < ArriveDistance + CircleRadius)
		{
			Fairy.Duty = Fairy.bShadow ? EDuty::Sting : EDuty::Distract;
			// Bosses see through it sooner
			Fairy.DutySeconds = Fairy.bShadow ? Plant->FairyStingSeconds : Plant->FairyLureSeconds * (Enemy->IsHeavy() ? 0.5f : 1.0f);
			Fairy.StingSeconds = 0.0f;
			UE_LOG(LogVael, Verbose, TEXT("A fairy reaches '%s'"), *Enemy->GetName());
		}
		break;

	case EDuty::Distract:
		Fairy.DutySeconds -= DeltaSeconds;
		Enemy->LureTo(Mistress, Fairy.Location, LureStep);
		if (Fairy.DutySeconds <= 0.0f)
		{
			Dismiss(Fairy);
		}
		break;

	case EDuty::Sting:
		Fairy.DutySeconds -= DeltaSeconds;
		Fairy.StingSeconds -= DeltaSeconds;
		if (Fairy.StingSeconds <= 0.0f)
		{
			Fairy.StingSeconds = StingEvery;
			UVaelCombatStatics::DealDamage(Mistress, Enemy, Plant->FairyStingDamage, EVaelElement::Air);
		}
		if (Fairy.DutySeconds <= 0.0f)
		{
			// On to the next; with no enemy left the forced fairy turns on the one who forced her
			if (AActor* Next = FindNextEnemy(Fairy))
			{
				Fairy.Target = Next;
				Fairy.Duty = EDuty::ToEnemy;
			}
			else
			{
				UVaelCombatStatics::DealDamage(nullptr, Mistress, Plant->FairyStingDamage, EVaelElement::Air);
				Dismiss(Fairy);
			}
		}
		break;

	case EDuty::ToPlace:
		if (FVector::Dist2D(Fairy.Location, Fairy.Place) < ArriveDistance)
		{
			Fairy.Duty = EDuty::Lure;
			Fairy.DutySeconds = Plant->FairyLureSeconds;
		}
		break;

	case EDuty::Lure:
		Fairy.DutySeconds -= DeltaSeconds;
		for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
		{
			if (!It->IsDead() && !It->IsHeavy() && UVaelCombatStatics::CanDamage(Mistress, *It)
				&& FVector::Dist2D(It->GetActorLocation(), Fairy.Place) < Plant->FairyLureRadius)
			{
				It->LureTo(Mistress, Fairy.Place, LureStep);
			}
		}
		if (Fairy.DutySeconds <= 0.0f)
		{
			Dismiss(Fairy);
		}
		break;

	default:
		break;
	}
}

void AVaelFairyEscort::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Mistress == nullptr || Flock.IsEmpty())
	{
		return;
	}

	Clock += DeltaSeconds;
	const FVector Ahead = Mistress->GetActorForwardVector();
	for (int32 Index = 0; Index < Flock.Num(); ++Index)
	{
		FEscortFairy& Fairy = Flock[Index];
		Fairy.RestSeconds = FMath::Max(0.0f, Fairy.RestSeconds - DeltaSeconds);
		Serve(Fairy, DeltaSeconds);

		FVector Goal = RingPlace(Index);
		FVector Look = Ahead;
		const AActor* Target = Fairy.Target.Get();
		if (Target != nullptr)
		{
			const float Angle = Clock * CircleSpeed + Fairy.Seed;
			Goal = Target->GetActorLocation() + FVector(FMath::Cos(Angle) * CircleRadius, FMath::Sin(Angle) * CircleRadius, CircleHeight);
			Look = Target->GetActorLocation() - Fairy.Location;
		}
		else if (Fairy.Duty == EDuty::ToPlace || Fairy.Duty == EDuty::Lure)
		{
			const float Angle = Clock * CircleSpeed * 0.5f + Fairy.Seed;
			Goal = Fairy.Place + FVector(FMath::Cos(Angle) * 60.0f, FMath::Sin(Angle) * 60.0f, CircleHeight);
		}
		VaelFairyFlight::Steer(Fairy, Goal + VaelFairyFlight::Drift(Fairy, Clock), Look, DeltaSeconds, Flight);
		VaelFairyFlight::Show(Fairies, Index, Fairy, Clock, Flight, Index == Flock.Num() - 1);
	}
}
