// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engineer/VaelDoubleSickle.h"
#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelCombatStatics.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engineer/VaelEngineerComponent.h"
#include "Engineer/VaelEngineerGear.h"
#include "EngineUtils.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelSpellEffects.h"
#include "NiagaraSystem.h"
#include "Player/VaelCharacter.h"
#include "VaelAssets.h"

namespace
{
	/** The Tripo model (Tools/Ingenieur/vael_tripo_werkstuecke.py): flat in XY round the hub, 55 cm across */
	const TCHAR* const SickleMeshPath = TEXT("/Game/Vael/Characters/Ingenieur/Werkstuecke/SM_Ing_Doppelsichel_Tripo.SM_Ing_Doppelsichel_Tripo");
	/** How far a throw goes at most, cm */
	constexpr float SickleReach = 1100.0f;
	/** Flight speed, cm/s, and how fast it comes back home */
	constexpr float SickleSpeed = 1500.0f;
	constexpr float SickleReturnSpeed = 1700.0f;
	/** Widest swing to the side of the held throw's loop, cm */
	constexpr float SickleLoopWidth = 380.0f;
	/** Turns per second of the blades about the hub */
	constexpr float SickleSpinRate = 1080.0f;
	/** How close an enemy must come to the blades to be cut: half the sickle plus a little */
	constexpr float SickleCutRadius = 45.0f;
	/** Within this of the hand it is caught */
	constexpr float SickleCatchRadius = 70.0f;
	/** Longest flight in all, s: past this it flies straight home whatever happens */
	constexpr float SickleMaxSeconds = 4.0f;
}

AVaelDoubleSickle::AVaelDoubleSickle()
{
	PrimaryActorTick.bCanEverTick = true;
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCastShadow(true);
	RootComponent = Body;
}

AVaelDoubleSickle* AVaelDoubleSickle::Throw(UVaelEngineerComponent* Engineer, const FVector& From, const FVector& Aim, bool bInCurve, float InDamage, float SpeedScale)
{
	UWorld* World = Engineer != nullptr ? Engineer->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVaelDoubleSickle* Sickle = World->SpawnActor<AVaelDoubleSickle>(AVaelDoubleSickle::StaticClass(), From, FRotator::ZeroRotator, Params);
	if (Sickle == nullptr)
	{
		return nullptr;
	}
	if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, SickleMeshPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		Sickle->Body->SetStaticMesh(Mesh);
	}
	Sickle->Engineer = Engineer;
	Sickle->bCurve = bInCurve;
	Sickle->Damage = InDamage;
	Sickle->Speed = SickleSpeed * SpeedScale;
	Sickle->ThrownFrom = From;
	Sickle->LastLocation = From;
	// the aim point at the height of the hand, within reach
	FVector Toward = Aim - From;
	Toward.Z = 0.0f;
	if (Toward.IsNearlyZero())
	{
		Toward = Engineer->GetOwner()->GetActorForwardVector();
	}
	const float Distance = FMath::Clamp(Toward.Size(), 300.0f, SickleReach);
	Sickle->Target = From + Toward.GetSafeNormal() * Distance;
	if (bInCurve)
	{
		// a boomerang loop: out past his right, through the aim point, home past his left (a right hand throws it so)
		Sickle->Phase = EPhase::Loop;
		Sickle->LoopFrom = From;
		Sickle->LoopAhead = Toward.GetSafeNormal() * Distance;
		Sickle->LoopSide = FVector::CrossProduct(FVector::UpVector, Toward.GetSafeNormal()) * FMath::Min(SickleLoopWidth, Distance * 0.45f);
		Sickle->LoopT = 0.0f;
	}
	return Sickle;
}

FVector AVaelDoubleSickle::GetHand() const
{
	const UVaelEngineerComponent* Eng = Engineer.Get();
	const AActor* Thrower = Eng != nullptr ? Eng->GetOwner() : nullptr;
	return Thrower != nullptr ? Thrower->GetActorLocation() + Thrower->GetActorForwardVector() * 30.0f + FVector(0.0f, 0.0f, 40.0f) : GetActorLocation();
}

float AVaelDoubleSickle::GetSecondsToHand() const
{
	return Phase == EPhase::Return ? FVector::Dist(GetActorLocation(), GetHand()) / SickleReturnSpeed : 1000.0f;
}

void AVaelDoubleSickle::CutAlong(const FVector& From, const FVector& To)
{
	UVaelEngineerComponent* Eng = Engineer.Get();
	const AActor* Thrower = Eng != nullptr ? Eng->GetOwner() : nullptr;
	if (Thrower == nullptr)
	{
		return;
	}
	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		AVaelCharacterBase* Enemy = *It;
		if (Enemy->IsA<AVaelCharacter>() || CutThisWay.Contains(Enemy) || Enemy->GetHealth() <= 0.0f || !UVaelCombatStatics::CanDamage(Thrower, Enemy))
		{
			continue;
		}
		const FVector Closest = FMath::ClosestPointOnSegment(Enemy->GetActorLocation(), From, To);
		if (FVector::Dist2D(Closest, Enemy->GetActorLocation()) > SickleCutRadius + Enemy->GetSimpleCollisionRadius()
			|| FMath::Abs(Closest.Z - Enemy->GetActorLocation().Z) > 160.0f)
		{
			continue;
		}
		CutThisWay.Add(Enemy);
		++Hits;
		Eng->ApplyHit(Enemy, Closest, Damage, VaelEngineerGear::Standard, ThrownFrom, NAME_None);
		const UVaelMagicSettings* Magic = UVaelMagicSettings::Get();
		VaelEffects::SpawnAt(this, VaelAssets::LoadOptional(Magic->DustPuffEffect), Closest, FRotator::ZeroRotator, FLinearColor(0.35f, 0.08f, 0.05f), 22.0f);
	}
}

void AVaelDoubleSickle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (!Engineer.IsValid())
	{
		Destroy();
		return;
	}
	FVector Location = GetActorLocation();
	switch (Phase)
	{
	case EPhase::Out:
	{
		const FVector Way = Target - Location;
		const float Step = Speed * DeltaSeconds;
		if (Way.Size() <= Step)
		{
			Location = Target;
			CutThisWay.Reset();
			Phase = EPhase::Return;
		}
		else
		{
			Location += Way.GetSafeNormal() * Step;
		}
		break;
	}
	case EPhase::Loop:
	{
		// P(t) = from + ahead (1 - cos 2 pi t) / 2 + side sin 2 pi t: through the aim point at t = 0.5; advanced by its speed along the curve
		const auto Rate = [this](float T)
		{
			const FVector Tangent = LoopAhead * PI * FMath::Sin(2.0f * PI * T) + LoopSide * 2.0f * PI * FMath::Cos(2.0f * PI * T);
			return FMath::Max(Tangent.Size(), 1.0f);
		};
		const float Before = LoopT;
		LoopT = FMath::Min(LoopT + Speed * DeltaSeconds / Rate(LoopT), 1.0f);
		Location = LoopFrom + LoopAhead * (1.0f - FMath::Cos(2.0f * PI * LoopT)) * 0.5f + LoopSide * FMath::Sin(2.0f * PI * LoopT);
		if (Before < 0.5f && LoopT >= 0.5f)
		{
			// past the aim point: the way back may cut again what it cut going out
			CutThisWay.Reset();
		}
		if (LoopT >= 0.97f)
		{
			// the last stretch home to the hand, wherever he went meanwhile
			Phase = EPhase::Return;
		}
		break;
	}
	case EPhase::Return:
	{
		const FVector Hand = GetHand();
		const FVector Way = Hand - Location;
		const float Step = SickleReturnSpeed * DeltaSeconds;
		if (Way.Size() <= FMath::Max(Step, SickleCatchRadius))
		{
			OnCaught.Broadcast(this);
			Destroy();
			return;
		}
		Location += Way.GetSafeNormal() * Step;
		break;
	}
	}
	if (Age > SickleMaxSeconds && Phase != EPhase::Return)
	{
		Phase = EPhase::Return;
		CutThisWay.Reset();
	}
	Furthest = FMath::Max(Furthest, FVector::Dist2D(Location, ThrownFrom));
	SetActorLocation(Location);
	// it spins flat about its hub, a little tilted so the blades read from the camera above
	SetActorRotation(FRotator(12.0f, FMath::Fmod(Age * SickleSpinRate, 360.0f), 0.0f));
	CutAlong(LastLocation, Location);
	LastLocation = Location;
}
