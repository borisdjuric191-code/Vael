// Copyright Epic Games, Inc. All Rights Reserved.

#include "Floromant/VaelBindweedLasso.h"
#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelCombatStatics.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Creatures/VaelCreature.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Floromant/VaelPlantArmData.h"
#include "Floromant/VaelSymbiosis.h"
#include "GameFramework/Pawn.h"
#include "Magic/VaelDebrisBurst.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Vael.h"
#include "VaelAssets.h"

namespace
{
	/** Pieces of the tendril, of each coil round the target, coils, and trumpets along it */
	constexpr int32 LassoPieces = 6;
	constexpr int32 LassoCoilPieces = 4;
	constexpr int32 LassoCoils = 2;
	constexpr int32 LassoTrumpets = 3;

	/** How fast the tip flies or crawls, in cm/s, and seconds it needs to draw back */
	constexpr float LassoFlySpeed = 2600.0f;
	constexpr float LassoCrawlSpeed = 900.0f;
	constexpr float LassoRetractTime = 0.3f;

	/** Seconds the yank or the flight to a heavy target may take at most */
	constexpr float LassoEndTime = 0.7f;
}

AVaelBindweedLasso::AVaelBindweedLasso()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

bool AVaelBindweedLasso::IsHeavy(const AActor* Actor)
{
	const AVaelCreature* Creature = Cast<AVaelCreature>(Actor);
	return Creature != nullptr && Creature->IsHeavy();
}

TArray<AActor*> AVaelBindweedLasso::FindTargets(APawn* InOwner, const FVector& InStart, const FVector& Direction, float Range, float HalfAngle, int32 Count)
{
	TArray<TPair<float, AActor*>> Found;
	UWorld* World = InOwner != nullptr ? InOwner->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return {};
	}
	const FVector Ahead = Direction.GetSafeNormal2D();
	const float MinDot = HalfAngle >= 180.0f ? -2.0f : FMath::Cos(FMath::DegreesToRadians(HalfAngle));
	for (TActorIterator<AVaelCharacterBase> It(World); It; ++It)
	{
		AVaelCharacterBase* Other = *It;
		if (Other->IsDefeated() || !UVaelCombatStatics::CanDamage(InOwner, Other))
		{
			continue;
		}
		const FVector Offset = (Other->GetActorLocation() - InStart) * FVector(1.0f, 1.0f, 0.0f);
		const float Distance = Offset.Size();
		if (Distance > Range + Other->GetSimpleCollisionRadius())
		{
			continue;
		}
		if (!Ahead.IsNearlyZero() && FVector::DotProduct(Offset.GetSafeNormal(), Ahead) < MinDot && Distance > 120.0f)
		{
			continue;
		}
		Found.Emplace(Distance, Other);
	}
	Found.Sort([](const TPair<float, AActor*>& A, const TPair<float, AActor*>& B) { return A.Key < B.Key; });
	TArray<AActor*> Targets;
	for (int32 Index = 0; Index < Found.Num() && Index < Count; ++Index)
	{
		Targets.Add(Found[Index].Value);
	}
	return Targets;
}

AVaelBindweedLasso* AVaelBindweedLasso::Throw(APawn* InOwner, const UVaelPlantArmData* InPlant, const FVector& InStart, AActor* InTarget, const FVaelLassoSpec& InSpec, bool bInShadow)
{
	UWorld* World = InOwner != nullptr ? InOwner->GetWorld() : nullptr;
	if (World == nullptr || InPlant == nullptr || InTarget == nullptr)
	{
		return nullptr;
	}
	AVaelBindweedLasso* Lasso = World->SpawnActorDeferred<AVaelBindweedLasso>(AVaelBindweedLasso::StaticClass(), FTransform(InStart), InOwner, InOwner, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Lasso == nullptr)
	{
		return nullptr;
	}
	Lasso->Plant = InPlant;
	Lasso->Target = InTarget;
	Lasso->Spec = InSpec;
	Lasso->Ending = InSpec.OnEnd;
	Lasso->bShadow = bInShadow;
	Lasso->Start = InStart;
	Lasso->Seed = FMath::FRandRange(0.0f, 10.0f);
	Lasso->FinishSpawning(FTransform(InStart));
	Lasso->Build();
	return Lasso;
}

USplineMeshComponent* AVaelBindweedLasso::MakeStrand(UStaticMesh* Mesh)
{
	USplineMeshComponent* Piece = NewObject<USplineMeshComponent>(this);
	Piece->SetMobility(EComponentMobility::Movable);
	Piece->SetForwardAxis(ESplineMeshAxis::Z, false);
	Piece->SetStaticMesh(Mesh);
	Piece->SetMaterial(0, Material);
	Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Piece->SetCanEverAffectNavigation(false);
	Piece->SetUsingAbsoluteLocation(true);
	Piece->SetUsingAbsoluteRotation(true);
	Piece->SetupAttachment(RootComponent);
	Piece->RegisterComponent();
	Piece->SetWorldTransform(FTransform::Identity);
	return Piece;
}

void AVaelBindweedLasso::Build()
{
	Material = UMaterialInstanceDynamic::Create(VaelAssets::LoadOptional(Plant->Material), this);
	if (Material != nullptr)
	{
		Material->SetScalarParameterValue(TEXT("Schatten"), bShadow ? 1.0f : 0.0f);
		Material->SetScalarParameterValue(TEXT("Wuchs"), 1.0f);
		Material->SetScalarParameterValue(TEXT("Wind"), 0.1f);
	}

	// The tendril with its small leaves, plain stems for the coils
	UStaticMesh* Leafy = nullptr;
	UStaticMesh* Plain = nullptr;
	for (const TSoftObjectPtr<UStaticMesh>& Mesh : Plant->FingerMeshes)
	{
		if (UStaticMesh* Loaded = VaelAssets::LoadOptional(Mesh))
		{
			Plain = Plain == nullptr ? Loaded : Plain;
			Leafy = Loaded;
		}
	}
	for (int32 Index = 0; Index < LassoPieces; ++Index)
	{
		Strand.Add(MakeStrand(Leafy));
	}
	for (int32 Index = 0; Index < LassoCoils * LassoCoilPieces; ++Index)
	{
		USplineMeshComponent* Coil = MakeStrand(Plain);
		Coil->SetVisibility(false);
		Coils.Add(Coil);
	}

	// Golden trumpets along it, the mouths towards the target
	if (UStaticMesh* TrumpetMesh = VaelAssets::LoadOptional(Plant->LeafMesh))
	{
		for (int32 Index = 0; Index < LassoTrumpets; ++Index)
		{
			UStaticMeshComponent* Trumpet = NewObject<UStaticMeshComponent>(this);
			Trumpet->SetMobility(EComponentMobility::Movable);
			Trumpet->SetStaticMesh(TrumpetMesh);
			Trumpet->SetMaterial(0, Material);
			Trumpet->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Trumpet->SetUsingAbsoluteLocation(true);
			Trumpet->SetUsingAbsoluteRotation(true);
			Trumpet->SetUsingAbsoluteScale(true);
			Trumpet->SetupAttachment(RootComponent);
			Trumpet->RegisterComponent();
			Trumpets.Add(Trumpet);
		}
	}
}

FVector AVaelBindweedLasso::GetTip() const
{
	const AActor* Goal = Target.Get();
	const FVector Aim = Goal != nullptr ? Goal->GetActorLocation() : LastTargetLocation;
	return FMath::Lerp(Start, Aim, FMath::Clamp(Reach, 0.0f, 1.0f));
}

void AVaelBindweedLasso::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	AActor* Goal = Target.Get();
	const AVaelCharacterBase* GoalCharacter = Cast<AVaelCharacterBase>(Goal);
	if (Goal != nullptr)
	{
		LastTargetLocation = Goal->GetActorLocation();
	}
	const bool bLost = Goal == nullptr || (GoalCharacter != nullptr && GoalCharacter->IsDefeated()) || GetInstigator() == nullptr;

	switch (Phase)
	{
	case EPhase::Flying:
	{
		if (bLost)
		{
			Phase = EPhase::Retracting;
			break;
		}
		const float Way = FMath::Max(FVector::Dist(Start, LastTargetLocation), 1.0f);
		Reach = FMath::Min(1.0f, Reach + DeltaSeconds * (Spec.bOverGround ? LassoCrawlSpeed : LassoFlySpeed) / Way);
		if (Reach >= 1.0f)
		{
			Catch();
		}
		break;
	}
	case EPhase::Holding:
		if (bLost)
		{
			Phase = EPhase::Retracting;
			break;
		}
		Hold(DeltaSeconds);
		break;
	case EPhase::Ending:
		End(DeltaSeconds);
		break;
	case EPhase::Retracting:
		Out -= DeltaSeconds / LassoRetractTime;
		if (Out <= 0.0f)
		{
			Destroy();
			return;
		}
		break;
	}

	UpdateLook(DeltaSeconds);
}

void AVaelBindweedLasso::Catch()
{
	Phase = EPhase::Holding;
	HeldTime = 0.0f;
	AActor* Goal = Target.Get();
	AActor* Holder = GetInstigator();
	const bool bEnemy = UVaelCombatStatics::CanDamage(Holder, Goal);

	// A puff of golden pollen where it wraps the enemy
	FVaelDebris Pollen;
	Pollen.Count = 14;
	Pollen.Spread = 30.0f;
	Pollen.Speed = 160.0f;
	Pollen.Lift = 120.0f;
	Pollen.Gravity = 150.0f;
	Pollen.Lifetime = 0.8f;
	Pollen.MinSize = 1.5f;
	Pollen.MaxSize = 3.5f;
	Pollen.Color = Spec.Color;
	Pollen.Glow = 4.0f;
	Pollen.bLandOnGround = false;
	AVaelDebrisBurst::Spawn(this, LastTargetLocation, Pollen);

	if (bEnemy && Spec.BlindSeconds > 0.0f)
	{
		if (AVaelCharacterBase* Character = Cast<AVaelCharacterBase>(Goal))
		{
			Character->ApplyBlind(Spec.BlindSeconds);
		}
	}
}

void AVaelBindweedLasso::Hold(float DeltaSeconds)
{
	AActor* Goal = Target.Get();
	AActor* Holder = GetInstigator();
	HeldTime += DeltaSeconds;

	// An enemy can't move or attack while it is wrapped; an ally is only held to be pulled
	if (UVaelCombatStatics::CanDamage(Holder, Goal))
	{
		if (AVaelCharacterBase* Character = Cast<AVaelCharacterBase>(Goal))
		{
			Character->ApplyStun(0.25f);
		}

		// Damage in small bites every half second, growing the longer it holds (the Würgwinde)
		const float Before = FMath::FloorToFloat((HeldTime - DeltaSeconds) * 2.0f);
		if (FMath::FloorToFloat(HeldTime * 2.0f) > Before)
		{
			const float PerSecond = (Spec.DamagePerSecond + Spec.DamageGrowth * HeldTime) * Spec.Power;
			if (PerSecond > 0.0f)
			{
				UVaelCombatStatics::DealDamage(Holder, Goal, PerSecond * 0.5f, EVaelElement::Earth);
			}
			if (Spec.Status != EVaelStatus::None)
			{
				UVaelCombatStatics::ApplyStatus(Holder, Goal, Spec.Status, 1.5f);
			}
		}
	}

	// Pulled far out of reach it tears
	if (FVector::Dist2D(Start, LastTargetLocation) > 1600.0f)
	{
		Finish(EVaelLassoEnd::Release);
		return;
	}
	if (Spec.HoldSeconds > 0.0f && HeldTime >= Spec.HoldSeconds)
	{
		Finish();
	}
}

void AVaelBindweedLasso::Finish()
{
	Finish(Ending);
}

void AVaelBindweedLasso::Finish(EVaelLassoEnd End)
{
	if (Phase == EPhase::Ending || Phase == EPhase::Retracting)
	{
		return;
	}
	// Let go before it caught anything: it just draws back
	if (Phase == EPhase::Flying)
	{
		Phase = EPhase::Retracting;
		Out = Reach;
		return;
	}
	Ending = End;
	Phase = EPhase::Ending;
	EndTime = 0.0f;
	UE_LOG(LogVael, Log, TEXT("Lasso of '%s' lets go of '%s' after %.1f s: %s%s"), *GetNameSafe(GetInstigator()), *GetNameSafe(Target.Get()), HeldTime,
		*UEnum::GetValueAsString(End), IsHeavy(Target.Get()) ? TEXT(" (heavy: she flies to it)") : TEXT(""));

	AActor* Goal = Target.Get();
	AActor* Holder = GetInstigator();
	AVaelCharacterBase* Character = Cast<AVaelCharacterBase>(Goal);
	switch (End)
	{
	case EVaelLassoEnd::Slam:
		// The Würgwinde throws it down hard
		if (Character != nullptr)
		{
			UVaelCombatStatics::DealDamage(Holder, Goal, Plant->SlamDamage * Spec.Power, EVaelElement::Earth);
			Character->ApplyStun(Plant->SlamStun);
		}
		{
			FVaelDebris Earth;
			Earth.Count = 14;
			Earth.Spread = 40.0f;
			Earth.Speed = 220.0f;
			Earth.Lift = 220.0f;
			Earth.Lifetime = 0.8f;
			Earth.MinSize = 3.0f;
			Earth.MaxSize = 7.0f;
			Earth.Color = FLinearColor(0.16f, 0.12f, 0.09f);
			AVaelDebrisBurst::Spawn(this, LastTargetLocation, Earth);
		}
		Phase = EPhase::Retracting;
		break;
	case EVaelLassoEnd::Fling:
		if (Character != nullptr)
		{
			Character->ApplyKnockback((LastTargetLocation - Start).GetSafeNormal2D(), 950.0f);
		}
		Phase = EPhase::Retracting;
		break;
	case EVaelLassoEnd::Bloom:
		// The capsule that bloomed on it bursts
		if (Spec.BloomPlant != nullptr)
		{
			VaelSymbiosis::ThrowCapsule(Cast<APawn>(Holder), Spec.BloomPlant, LastTargetLocation + FVector(0.0f, 0.0f, 60.0f), LastTargetLocation, 0.5f, Spec.Power, bShadow);
		}
		Phase = EPhase::Retracting;
		break;
	case EVaelLassoEnd::Release:
		Phase = EPhase::Retracting;
		break;
	case EVaelLassoEnd::Yank:
		// Played out in End: a small one flies to her in a low arc, to a heavy one she flies herself
		YankFrom = LastTargetLocation;
		break;
	}
}

void AVaelBindweedLasso::End(float DeltaSeconds)
{
	EndTime += DeltaSeconds;
	AActor* Goal = Target.Get();
	AActor* Holder = GetInstigator();
	AVaelCharacterBase* Character = Cast<AVaelCharacterBase>(Goal);
	if (Goal == nullptr || Holder == nullptr)
	{
		Phase = EPhase::Retracting;
		return;
	}

	// To a heavy one she flies herself along the vine
	if (IsHeavy(Goal) || Character == nullptr)
	{
		const float Gap = FVector::Dist2D(Holder->GetActorLocation(), Goal->GetActorLocation()) - Goal->GetSimpleCollisionRadius() - 60.0f;
		if (Gap > 40.0f && EndTime < LassoEndTime)
		{
			const FVector Way = (Goal->GetActorLocation() - Holder->GetActorLocation()).GetSafeNormal2D();
			Holder->AddActorWorldOffset(Way * FMath::Min(Plant->FlyToSpeed * DeltaSeconds, Gap), true);
			Start = Holder->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f);
			return;
		}
	}
	else
	{
		// A small one is thrown to her feet in a low arc over steps and edges, as fast as the yank allows
		const FVector Toward = (YankFrom - Holder->GetActorLocation()) * FVector(1.0f, 1.0f, 0.0f);
		const FVector Landing = Holder->GetActorLocation() + Toward.GetSafeNormal() * (Goal->GetSimpleCollisionRadius() + 80.0f);
		const float Feet = Holder->GetActorLocation().Z - Holder->GetSimpleCollisionHalfHeight();
		const FVector Spot(Landing.X, Landing.Y, Feet + Goal->GetSimpleCollisionHalfHeight() + 5.0f);
		const float Flight = FMath::Clamp(FVector::Dist2D(YankFrom, Spot) / Plant->YankSpeed, 0.2f, LassoEndTime);
		const float Share = FMath::Min(EndTime / Flight, 1.0f);
		const FVector Here = FMath::Lerp(YankFrom, Spot, FMath::InterpEaseOut(0.0f, 1.0f, Share, 1.6f)) + FVector(0.0f, 0.0f, 120.0f * FMath::Sin(Share * UE_PI));
		// Through the air the vine carries it over edges and steps; it lands swept, so it never ends up in the ground
		Goal->SetActorLocation(Here, Share >= 1.0f);
		if (Share < 1.0f)
		{
			return;
		}
	}

	if (Character != nullptr && UVaelCombatStatics::CanDamage(Holder, Goal))
	{
		Character->ApplyStun(Plant->YankStun);
	}
	Phase = EPhase::Retracting;
}

void AVaelBindweedLasso::UpdateLook(float DeltaSeconds)
{
	const float Time = GetWorld()->GetTimeSeconds();
	const AActor* Goal = Target.Get();
	const float Radius = Goal != nullptr ? Goal->GetSimpleCollisionRadius() : 40.0f;
	const FVector Tip = GetTip();
	const FVector End = FMath::Lerp(Start, Tip, FMath::Clamp(Out, 0.0f, 1.0f));
	const FVector Way = End - Start;
	const float Length = Way.Size();
	FVector Side = FVector::CrossProduct(Way.GetSafeNormal(), FVector::UpVector);
	Side = Side.IsNearlyZero() ? FVector::RightVector : Side.GetSafeNormal();

	// While it flies it snakes through the air (or over the ground); held it hangs taut with a little sag; drawing back it curls
	const float Snake = Phase == EPhase::Flying ? 18.0f * (1.0f - Reach) + 6.0f : (Phase == EPhase::Retracting ? 14.0f : 3.0f);
	const auto PointAt = [&](float Share)
	{
		FVector Point = Start + Way * Share;
		Point += Side * Snake * FMath::Sin(Share * 9.0f - Time * 14.0f + Seed) * FMath::Sin(Share * UE_PI);
		if (Spec.bOverGround)
		{
			// Low over the ground, rising up the enemy at the end
			Point.Z = FMath::Lerp(Start.Z, End.Z, FMath::Square(Share)) + 6.0f * FMath::Sin(Share * UE_PI);
		}
		else
		{
			Point.Z -= (Phase == EPhase::Holding ? 12.0f : 6.0f) * FMath::Sin(Share * UE_PI);
		}
		return Point;
	};

	const float Thickness = Plant->TendrilThickness * (Phase == EPhase::Retracting ? FMath::Max(Out, 0.3f) : 1.0f);
	for (int32 Index = 0; Index < Strand.Num(); ++Index)
	{
		const float A = Index / static_cast<float>(Strand.Num());
		const float B = (Index + 1) / static_cast<float>(Strand.Num());
		const FVector P0 = PointAt(A);
		const FVector P1 = PointAt(B);
		const FVector T0 = (PointAt(FMath::Min(A + 0.02f, 1.0f)) - PointAt(FMath::Max(A - 0.02f, 0.0f))).GetSafeNormal() * FVector::Dist(P0, P1);
		const FVector T1 = (PointAt(FMath::Min(B + 0.02f, 1.0f)) - PointAt(FMath::Max(B - 0.02f, 0.0f))).GetSafeNormal() * FVector::Dist(P0, P1);
		USplineMeshComponent* Piece = Strand[Index];
		Piece->SetVisibility(Length > 2.0f);
		Piece->SetStartAndEnd(P0, T0, P1, T1, false);
		Piece->SetStartScale(FVector2D(Thickness * FMath::Lerp(1.2f, 0.8f, A)), false);
		Piece->SetEndScale(FVector2D(Thickness * FMath::Lerp(1.2f, 0.8f, B)), false);
		Piece->SetStartRoll(Index * 1.3f, false);
		Piece->SetEndRoll(Index * 1.3f + 1.0f, false);
		Piece->UpdateMesh();
	}

	// The golden trumpets along it, turned towards the target
	for (int32 Index = 0; Index < Trumpets.Num(); ++Index)
	{
		const float Share = 0.3f + 0.25f * Index;
		const FVector Here = PointAt(Share);
		const FVector Along = (PointAt(Share + 0.03f) - PointAt(Share - 0.03f)).GetSafeNormal();
		const FVector Mouth = (Along + Side * (Index % 2 == 0 ? 0.6f : -0.6f) + FVector::UpVector * 0.3f).GetSafeNormal();
		Trumpets[Index]->SetVisibility(Length > 40.0f);
		Trumpets[Index]->SetWorldLocationAndRotation(Here, FRotationMatrix::MakeFromZ(Mouth).Rotator());
		Trumpets[Index]->SetWorldScale3D(FVector(1.1f * FMath::Clamp(Out, 0.2f, 1.0f)));
	}

	// The coils wrap round the target once it is caught, tighter the longer it holds
	const bool bCoiled = Goal != nullptr && (Phase == EPhase::Holding || Phase == EPhase::Ending);
	const float Wrap = bCoiled ? FMath::Clamp(HeldTime / 0.25f, 0.0f, 1.0f) : 0.0f;
	for (int32 Coil = 0; Coil < LassoCoils; ++Coil)
	{
		const float Height = Coil == 0 ? -15.0f : 20.0f;
		const float Ring = (Radius * 0.85f + 6.0f) * (1.15f - 0.15f * Wrap);
		for (int32 Piece = 0; Piece < LassoCoilPieces; ++Piece)
		{
			USplineMeshComponent* Segment = Coils[Coil * LassoCoilPieces + Piece];
			Segment->SetVisibility(Wrap > 0.01f);
			if (Wrap <= 0.01f)
			{
				continue;
			}
			// Each coil is a turn of a spiral, drawn as far round as it has wrapped
			const float A0 = (Piece / static_cast<float>(LassoCoilPieces)) * 2.0f * UE_PI * Wrap + Coil * 1.7f + Seed;
			const float A1 = ((Piece + 1) / static_cast<float>(LassoCoilPieces)) * 2.0f * UE_PI * Wrap + Coil * 1.7f + Seed;
			const FVector Center = LastTargetLocation + FVector(0.0f, 0.0f, Height);
			const FVector P0 = Center + FVector(FMath::Cos(A0), FMath::Sin(A0), 0.0f) * Ring + FVector(0.0f, 0.0f, 6.0f * Piece / LassoCoilPieces);
			const FVector P1 = Center + FVector(FMath::Cos(A1), FMath::Sin(A1), 0.0f) * Ring + FVector(0.0f, 0.0f, 6.0f * (Piece + 1) / LassoCoilPieces);
			const float Arc = Ring * (A1 - A0);
			Segment->SetStartAndEnd(P0, FVector(-FMath::Sin(A0), FMath::Cos(A0), 0.0f) * Arc, P1, FVector(-FMath::Sin(A1), FMath::Cos(A1), 0.0f) * Arc, false);
			Segment->SetStartScale(FVector2D(Plant->TendrilThickness * 0.9f), false);
			Segment->SetEndScale(FVector2D(Plant->TendrilThickness * 0.9f), false);
			Segment->UpdateMesh();
		}
	}

	// It glows with its colour while it holds, flaring when it catches
	if (Material != nullptr)
	{
		Material->SetScalarParameterValue(TEXT("Aufleuchten"), Phase == EPhase::Holding ? 0.25f + 0.75f * FMath::Max(0.0f, 1.0f - HeldTime * 3.0f) : 0.1f);
	}
}

void AVaelBindweedLasso::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Strand.Reset();
	Coils.Reset();
	Trumpets.Reset();
	Super::EndPlay(EndPlayReason);
}
