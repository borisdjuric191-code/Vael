// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelDraculaupe.h"
#include "AnimationRuntime.h"
#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Creatures/VaelBloodMoth.h"
#include "Creatures/VaelCreatureData.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Items/VaelMaterialBag.h"
#include "Magic/VaelSpellEffects.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/VaelCharacter.h"
#include "UI/VaelCombatTextSubsystem.h"
#include "UI/VaelNoticeSubsystem.h"
#include "Vael.h"

#define LOCTEXT_NAMESPACE "VaelCreatures"

namespace
{
	/** Number of threads whirling around a caterpillar that spins itself in */
	constexpr int32 CaterpillarThreads = 10;

	/** Diameter of the engine sphere the threads are made of, in cm */
	constexpr float CaterpillarShapeSize = 100.0f;

	/** The cocoon has its full size after this share of the cocoon time; the rest it swells and shakes */
	constexpr float CaterpillarSpinShare = 0.7f;

	/** Length of one crawl wave in cm of way, and how high the back arches in cm */
	constexpr float CaterpillarWaveLength = 55.0f;
	constexpr float CaterpillarArchHeight = 9.0f;

	/** Seconds between two new places a wandering caterpillar picks, and how far from home they lie, in cm */
	constexpr float CaterpillarRoamInterval = 5.0f;
	constexpr float CaterpillarRoamRadius = 350.0f;

	/** Color of the alarm notice */
	const FLinearColor CaterpillarAlarmColor(0.85f, 0.1f, 0.12f);
}

AVaelDraculaupe::AVaelDraculaupe()
{
}

TSubclassOf<UVaelCreatureData> AVaelDraculaupe::GetDefaultDataClass() const
{
	return UVaelDraculaupeData::StaticClass();
}

void AVaelDraculaupe::BeginPlay()
{
	Super::BeginPlay();

	const UVaelDraculaupeData* Data = GetData<UVaelDraculaupeData>();

	RoamPoint = GetActorLocation();
	LastLocation = GetActorLocation();
	CrawlPhase = FMath::FRand();

	// The cocoon stands on the ground where the caterpillar sits; hidden until it spins
	Cocoon = NewObject<UStaticMeshComponent>(this, TEXT("Cocoon"));
	Cocoon->SetupAttachment(GetRootComponent());
	Cocoon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Cocoon->SetRelativeLocation(FVector(0.0f, 0.0f, -GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
	Cocoon->RegisterComponent();
	Cocoon->SetStaticMesh(Data->CocoonMesh.IsNull() ? nullptr : Data->CocoonMesh.LoadSynchronous());
	Cocoon->SetVisibility(false);

	UMaterialInterface* CoreMaterial = nullptr;
	UMaterialInterface* GlowMaterial = nullptr;
	VaelEffects::LoadLookMaterials(CoreMaterial, GlowMaterial);

	for (int32 Index = 0; Index < CaterpillarThreads; ++Index)
	{
		UStaticMeshComponent* Thread = VaelEffects::AddLookShape(this, CoreMaterial, Data->ThreadColor, 0.0f);
		Thread->SetCastShadow(false);
		Thread->SetVisibility(false);
		Threads.Add(Thread);
	}

	if (GetFactoryMesh() != nullptr)
	{
		SetupSpine();
	}
}

float AVaelDraculaupe::GetIncomingDamageMultiplier(const FGameplayTagContainer& DamageTags) const
{
	const float Multiplier = Super::GetIncomingDamageMultiplier(DamageTags);
	return State == EVaelCaterpillarState::Cocoon ? Multiplier * GetData<UVaelDraculaupeData>()->CocoonDamageMultiplier : Multiplier;
}

void AVaelDraculaupe::Alarm(AVaelCharacter* Player)
{
	if (State == EVaelCaterpillarState::Crawl && !IsDead() && Player != nullptr)
	{
		Prey = Player;
		bAlarmed = true;
	}
}

void AVaelDraculaupe::Touch(AVaelCharacter* Player)
{
	if (State != EVaelCaterpillarState::Crawl || IsDead() || Player == nullptr)
	{
		return;
	}

	const UVaelDraculaupeData* Data = GetData<UVaelDraculaupeData>();

	Prey = Player;
	State = EVaelCaterpillarState::Cocoon;
	CocoonAge = 0.0f;
	GetCharacterMovement()->StopMovementImmediately();
	Cocoon->SetVisibility(true);

	UVaelCombatTextSubsystem::PostReaction(Player, LOCTEXT("CaterpillarTouch", "Berührt!"));

	// Its alarm calls every other caterpillar near to the same player
	if (Data->bAlarms)
	{
		int32 NumCalled = 0;
		for (TActorIterator<AVaelDraculaupe> It(GetWorld()); It; ++It)
		{
			if (*It != this && !It->IsDead() && It->GetCaterpillarState() == EVaelCaterpillarState::Crawl && GetDistanceTo(*It) < Data->AlarmRadius
				&& It->GetData<UVaelDraculaupeData>()->bAlarms)
			{
				It->Alarm(Player);
				++NumCalled;
			}
		}

		if (NumCalled > 0)
		{
			UVaelNoticeSubsystem::Post(this, FText::Format(LOCTEXT("CaterpillarAlarm", "{0} schlägt Alarm"), GetCreatureName()),
				LOCTEXT("CaterpillarAlarmDetail", "Die anderen Raupen kommen."), CaterpillarAlarmColor, 3.0f);
		}

		UE_LOG(LogVael, Verbose, TEXT("'%s' touches '%s' and calls %d others"), *GetNameSafe(this), *GetNameSafe(Player), NumCalled);
	}
}

void AVaelDraculaupe::TickBehavior(float DeltaSeconds)
{
	const UVaelDraculaupeData* Data = GetData<UVaelDraculaupeData>();

	if (State == EVaelCaterpillarState::Cocoon)
	{
		CocoonAge += DeltaSeconds;
		if (CocoonAge >= Data->CocoonTime)
		{
			Hatch();
		}
		return;
	}

	// Whoever it bumps into, it spins itself in; a player in a dodge roll slips past
	const float OwnRadius = GetCapsuleComponent()->GetScaledCapsuleRadius();
	AVaelCharacter* Touched = nullptr;
	AVaelCharacter* Nearest = nullptr;
	float NearestDistance = Data->AggroRange;

	ForEachActivePlayer([this, Data, OwnRadius, &Touched, &Nearest, &NearestDistance](AVaelCharacter* Player)
	{
		const float Distance = GetDistanceTo2D(Player);
		if (!Player->IsDodging() && Distance < OwnRadius + Player->GetCapsuleComponent()->GetScaledCapsuleRadius() + Data->TouchReach)
		{
			Touched = Player;
		}
		if (Distance < NearestDistance)
		{
			Nearest = Player;
			NearestDistance = Distance;
		}
	});

	if (Touched != nullptr)
	{
		Touch(Touched);
		return;
	}

	// Called by an alarm it crawls to that player, faster; otherwise to the closest one it notices
	AVaelCharacter* Target = Prey.IsValid() && !Prey->IsDefeated() ? Prey.Get() : nullptr;
	if (Target == nullptr && Data->bSeeksPlayers)
	{
		Target = Nearest;
	}

	if (Target != nullptr)
	{
		MoveTowards(Target->GetActorLocation(), Data->MoveSpeed * (bAlarmed ? Data->AlarmSpeedMultiplier : 1.0f));
		return;
	}

	// Nobody there: it wanders around its home
	RoamCountdown -= DeltaSeconds;
	if (RoamCountdown <= 0.0f)
	{
		RoamCountdown = CaterpillarRoamInterval * FMath::FRandRange(0.6f, 1.4f);
		RoamPoint = HomeLocation + FVector(FMath::FRandRange(-1.0f, 1.0f), FMath::FRandRange(-1.0f, 1.0f), 0.0f) * CaterpillarRoamRadius;
	}

	if (FVector::Dist2D(GetActorLocation(), RoamPoint) > 40.0f)
	{
		MoveTowards(RoamPoint, Data->MoveSpeed * 0.6f);
	}
}

void AVaelDraculaupe::Hatch()
{
	const UVaelDraculaupeData* Data = GetData<UVaelDraculaupeData>();

	FVector Ground = GetActorLocation();
	FindGround(GetWorld(), Ground, Ground);

	UVaelCreatureData* MothData = Data->MothData != nullptr ? Data->MothData->GetDefaultObject<UVaelCreatureData>() : nullptr;
	if (AVaelBloodMoth* Moth = MothData != nullptr ? Cast<AVaelBloodMoth>(SpawnCreature(GetWorld(), MothData, Ground, GetActorRotation())) : nullptr)
	{
		Moth->SetPrey(Prey.Get());
	}
	else
	{
		UE_LOG(LogVael, Warning, TEXT("'%s' has no moth to hatch"), *GetNameSafe(this));
	}

	// The caterpillar is the moth now: nothing is left of it, and it counts for nothing
	Destroy();
}

void AVaelDraculaupe::Die()
{
	const bool bWasCocoon = State == EVaelCaterpillarState::Cocoon;
	const bool bWasAlive = !IsDead();

	Super::Die();

	// A destroyed cocoon gives its silk
	if (bWasCocoon && bWasAlive)
	{
		UVaelMaterialBag::GiveToGroup(this, GetData<UVaelDraculaupeData>()->CocoonLoot);
	}

	for (UStaticMeshComponent* Thread : Threads)
	{
		Thread->SetVisibility(false);
	}
}

void AVaelDraculaupe::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const UVaelDraculaupeData* Data = GetData<UVaelDraculaupeData>();
	const float Time = GetWorld()->GetTimeSeconds();
	const bool bDeadNow = IsDead();
	const bool bSpinning = State == EVaelCaterpillarState::Cocoon && !bDeadNow;

	// How far the cocoon has grown, 0 to 1, and how close the moth is to hatching, 0 to 1
	const float Spun = State == EVaelCaterpillarState::Cocoon ? FMath::Clamp(CocoonAge / (Data->CocoonTime * CaterpillarSpinShare), 0.0f, 1.0f) : 0.0f;
	const float Ripe = State == EVaelCaterpillarState::Cocoon ? FMath::Clamp((CocoonAge / Data->CocoonTime - CaterpillarSpinShare) / (1.0f - CaterpillarSpinShare), 0.0f, 1.0f) : 0.0f;

	if (Cocoon != nullptr && State == EVaelCaterpillarState::Cocoon)
	{
		// Grows from the ground up around the caterpillar; ripe, it swells and shakes harder and harder
		const float Shake = bSpinning ? Ripe * FMath::Sin(Time * (18.0f + Ripe * 22.0f)) : 0.0f;
		const float Swell = bSpinning ? 1.0f + Ripe * 0.08f * (1.0f + FMath::Sin(Time * 9.0f)) : 1.0f;
		const float Width = FMath::InterpEaseOut(0.15f, 1.0f, Spun, 2.0f);

		Cocoon->SetRelativeScale3D(bDeadNow ? FVector(1.15f, 1.15f, 0.3f) * Width : FVector(Width * Swell, Width * Swell, Spun * Swell));
		Cocoon->SetRelativeRotation(FRotator(Shake * 4.0f, 0.0f, Shake * 3.0f));
	}

	// Threads whirl around it from all sides and settle onto the cocoon
	const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	for (int32 Index = 0; Index < Threads.Num(); ++Index)
	{
		UStaticMeshComponent* Thread = Threads[Index];
		const bool bShown = bSpinning && Spun < 1.0f;
		Thread->SetVisibility(bShown);
		if (!bShown)
		{
			continue;
		}

		const float Share = (Index + 0.5f) / Threads.Num();
		const float Angle = Time * (5.0f + Share * 3.0f) * (Index % 2 == 0 ? 1.0f : -1.0f) + Share * UE_TWO_PI * 3.0f;
		const float Radius = FMath::Lerp(95.0f, 48.0f, Spun) * (0.8f + 0.2f * FMath::Sin(Time * 3.0f + Index));
		const float Height = -HalfHeight + FMath::Fmod(Share * 7.0f + Time * 0.6f, 1.0f) * 130.0f * FMath::Max(Spun, 0.3f);

		const FVector Around(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, Height);
		Thread->SetRelativeLocation(Around);
		Thread->SetRelativeRotation(FRotator(FMath::Sin(Time * 2.0f + Index) * 25.0f, FMath::RadiansToDegrees(Angle) + 90.0f, 0.0f));
		Thread->SetRelativeScale3D(FVector(70.0f, 2.2f, 2.2f) / CaterpillarShapeSize);
	}

	if (GetFactoryMesh() != nullptr)
	{
		PoseFactoryMesh(DeltaSeconds);

		// Inside the growing cocoon the caterpillar draws itself together and is hidden once it is closed
		GetFactoryMesh()->SetVisibility(Spun < 0.85f || bDeadNow);
	}
}

void AVaelDraculaupe::SetupSpine()
{
	const USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(GetFactoryMesh()->GetSkinnedAsset());
	if (SkeletalMesh == nullptr)
	{
		return;
	}

	// From the hind end to the head; a mesh of the family Starr has only the first and the last
	const FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetRefSkeleton();
	for (const TCHAR* Bone : { TEXT("body"), TEXT("seg_2"), TEXT("seg_3"), TEXT("seg_4"), TEXT("seg_5"), TEXT("head") })
	{
		const int32 Index = RefSkeleton.FindBoneIndex(Bone);
		if (Index != INDEX_NONE)
		{
			SpineBones.Add(Bone);
			SpineRest.Add(FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, Index));
		}
	}
}

void AVaelDraculaupe::PoseFactoryMesh(float DeltaSeconds)
{
	UPoseableMeshComponent* WormMesh = GetFactoryMesh();
	const UVaelDraculaupeData* Data = GetData<UVaelDraculaupeData>();
	const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	const float Time = GetWorld()->GetTimeSeconds();
	const bool bDeadNow = IsDead();

	// The way it has really gone since the last frame drives the crawl
	const float Way = FVector::Dist2D(GetActorLocation(), LastLocation);
	LastLocation = GetActorLocation();
	CrawlPhase += Way / CaterpillarWaveLength;

	const bool bMoving = DeltaSeconds > 0.0f && Way / DeltaSeconds > 15.0f && State == EVaelCaterpillarState::Crawl && !bDeadNow;
	ArchShown = FMath::FInterpTo(ArchShown, bMoving ? 1.0f : 0.15f, DeltaSeconds, 6.0f);

	const float Spun = State == EVaelCaterpillarState::Cocoon ? FMath::Clamp(CocoonAge / (Data->CocoonTime * CaterpillarSpinShare), 0.0f, 1.0f) : 0.0f;
	const float Size = bDeadNow ? 1.0f : FMath::Lerp(1.0f, 0.6f, Spun);
	const float Wave = CrawlPhase * UE_TWO_PI + (bMoving ? 0.0f : Time * 1.2f);

	if (SpineBones.Num() >= 4)
	{
		// A wave runs from the hind end to the head: each part lifts and tips with the slope of the wave.
		// The mesh looks along +Y, so tipping is a turn around X.
		for (int32 Index = 0; Index < SpineBones.Num(); ++Index)
		{
			const float Along = Index * 0.95f;
			const float Lift = FMath::Max(FMath::Sin(Wave - Along), 0.0f) * CaterpillarArchHeight * ArchShown;
			const float Slope = FMath::Cos(Wave - Along) * 9.0f * ArchShown;

			// In the cocoon it curls up; dead it lies flat
			const float Curl = Spun * (Index - (SpineBones.Num() - 1) * 0.5f) * 14.0f;

			FTransform Posed = SpineRest[Index];
			Posed.SetLocation(SpineRest[Index].GetLocation() + FVector(0.0f, 0.0f, bDeadNow ? 0.0f : Lift));
			Posed.SetRotation(FQuat(FVector::ForwardVector, FMath::DegreesToRadians(bDeadNow ? 0.0f : Slope + Curl)) * SpineRest[Index].GetRotation());
			WormMesh->SetBoneTransformByName(SpineBones[Index], Posed, EBoneSpaces::ComponentSpace);
		}

		WormMesh->SetRelativeScale3D(bDeadNow ? FVector(1.1f, 1.1f, 0.45f) : FVector(Size));
		WormMesh->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -HalfHeight), FRotator(0.0f, Data->MeshYaw, 0.0f));
		return;
	}

	// No back bones (Tauraupe, sitting upright): it scoots on its hind end, stretching and gathering itself
	const float Scoot = FMath::Sin(Wave) * ArchShown;
	const FVector Squash = bDeadNow ? FVector(1.15f, 1.15f, 0.4f) : FVector(1.0f - Scoot * 0.05f, 1.0f + Scoot * 0.12f, 1.0f - Scoot * 0.07f) * Size;

	WormMesh->SetRelativeScale3D(Squash);
	WormMesh->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -HalfHeight),
		FRotator(Scoot * -6.0f, 0.0f, FMath::Sin(Wave * 0.5f) * 3.0f * ArchShown).Quaternion() * FRotator(0.0f, Data->MeshYaw, 0.0f).Quaternion());
}

#undef LOCTEXT_NAMESPACE
