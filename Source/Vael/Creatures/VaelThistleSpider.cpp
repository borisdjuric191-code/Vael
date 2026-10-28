// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelThistleSpider.h"
#include "Combat/VaelCombatStatics.h"
#include "Creatures/VaelCreatureData.h"
#include "Creatures/VaelLegIKComponent.h"
#include "Creatures/VaelPollenBurst.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Magic/VaelGameplayTags.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelSpellEffects.h"
#include "NiagaraSystem.h"
#include "VaelAssets.h"
#include "Player/VaelCharacter.h"
#include "Vael.h"

namespace
{
	/** How much the thistle swells before it bursts */
	constexpr float BurstSwell = 0.18f;

	/** Seconds between two looks for seedlings to set on fire */
	constexpr float FireSpreadInterval = 0.4f;

	/** A leaping seedling touches a player this far beyond its own capsule, sideways, and from this far below the
	 *  middle of the player (its feet) to this far above it, in cm */
	constexpr float LeapTouchReach = 55.0f;
	constexpr float LeapTouchBelow = 130.0f;
	constexpr float LeapTouchAbove = 80.0f;
}

// ---------------------------------------------------------------- Distelspinne

AVaelThistleSpider::AVaelThistleSpider()
{
	// The burst leaves no corpse to look at, the swarm is the show
	CorpseDuration = 0.2f;
}

TSubclassOf<UVaelCreatureData> AVaelThistleSpider::GetDefaultDataClass() const
{
	return UVaelThistleSpiderData::StaticClass();
}

void AVaelThistleSpider::BeginPlay()
{
	Super::BeginPlay();

	// Starts as a thistle in the undergrowth
	ApplyLurkPose(1.0f);
}

void AVaelThistleSpider::ApplyLurkPose(float Share)
{
	// Share 1: a thistle bud, legs pulled in and sunk, head raised, swaying; 0: standing as a spider
	const UVaelThistleSpiderData* Data = GetData<UVaelThistleSpiderData>();
	const float Sway = Data->LurkSway * FMath::Sin(GetWorld()->GetTimeSeconds() * UE_TWO_PI / Data->LurkSwayTime + GetActorLocation().X * 0.01f);
	SetFactoryMeshOffset(FVector(0.0f, 0.0f, -Data->LurkSink * Share));
	if (UVaelLegIKComponent* Legs = GetFactoryLegs())
	{
		Legs->SetCurl(Data->LurkCurl * Share);
		Legs->SetExtraBodyTilt(FRotator(Data->LurkUpright * Share, 0.0f, Sway * Share));
	}
}

int32 AVaelThistleSpider::GetLegCount() const
{
	return GetFactoryLegs() != nullptr ? GetFactoryLegs()->GetNumLegs() : 0;
}

int32 AVaelThistleSpider::GetStepCount() const
{
	return GetFactoryLegs() != nullptr ? GetFactoryLegs()->GetStepCount() : 0;
}

bool AVaelThistleSpider::IsHiddenFromAim() const
{
	return State == EVaelThistleState::Lurking;
}

void AVaelThistleSpider::ProwlTowards(const FVector& GroupCentre, float GroupDistance)
{
	const UVaelThistleSpiderData* Data = GetData<UVaelThistleSpiderData>();
	if (State == EVaelThistleState::Lurking && GroupDistance < Data->AggroRange * GetTimeOfDayAwareness())
	{
		State = EVaelThistleState::Rising;
		StateTime = 0.0f;
	}
}

void AVaelThistleSpider::OnDamageTaken(float Damage, const FGameplayTagContainer& DamageTags)
{
	Super::OnDamageTaken(Damage, DamageTags);

	if (IsDead())
	{
		return;
	}
	if (State == EVaelThistleState::Lurking)
	{
		State = EVaelThistleState::Rising;
		StateTime = 0.0f;
	}
	// Fire sets the dry thistle off at once
	if (DamageTags.HasTagExact(VaelTags::GetElementTag(EVaelElement::Fire)) && State != EVaelThistleState::Bursting)
	{
		StartBurst();
	}
}

void AVaelThistleSpider::StartBurst()
{
	// Soaked down sticks together: a wet thistle can't burst
	if (UVaelCombatStatics::HasStatus(this, EVaelStatus::Wet))
	{
		return;
	}
	State = EVaelThistleState::Bursting;
	StateTime = 0.0f;
}

void AVaelThistleSpider::TickBehavior(float DeltaSeconds)
{
	const UVaelThistleSpiderData* Data = GetData<UVaelThistleSpiderData>();
	StateTime += DeltaSeconds;
	BiteCooldown -= DeltaSeconds;

	switch (State)
	{
	case EVaelThistleState::Lurking:
	{
		ApplyLurkPose(1.0f);
		float Distance = 0.0f;
		if (FindTarget(Data->WakeRange * GetTimeOfDayAwareness(), &Distance) != nullptr)
		{
			State = EVaelThistleState::Rising;
			StateTime = 0.0f;
		}
		break;
	}
	case EVaelThistleState::Rising:
	{
		const float Share = FMath::Clamp(StateTime / Data->RiseTime, 0.0f, 1.0f);
		ApplyLurkPose(1.0f - FMath::SmoothStep(0.0f, 1.0f, Share));
		if (Share >= 1.0f)
		{
			State = EVaelThistleState::Hunting;
			StateTime = 0.0f;
			HuntTime = 0.0f;
		}
		break;
	}
	case EVaelThistleState::Hunting:
	{
		float Distance = 0.0f;
		AActor* Target = FindTarget(Data->AggroRange * GetTimeOfDayAwareness(), &Distance);
		if (Target == nullptr)
		{
			break;
		}
		if (Distance < Data->BurstRange)
		{
			HuntTime += DeltaSeconds;
		}
		const bool bHurt = GetMaxHealth() > 0.0f && GetHealth() / GetMaxHealth() < Data->BurstHealthShare;
		if ((HuntTime >= Data->BurstDelay || bHurt) && !UVaelCombatStatics::HasStatus(this, EVaelStatus::Wet))
		{
			StartBurst();
			break;
		}
		if (UpdateBite(DeltaSeconds, Target))
		{
			break;
		}
		if (Distance > Data->BiteRange * 0.8f)
		{
			MoveTowards(Target->GetActorLocation(), Data->MoveSpeed);
		}
		else
		{
			FaceTowards(Target->GetActorLocation());
		}
		if (Distance <= Data->BiteRange && BiteCooldown <= 0.0f && !IsBlinded())
		{
			BiteTime = 0.0f;
			bBiteStruck = false;
		}
		break;
	}
	case EVaelThistleState::Bursting:
	{
		if (UVaelLegIKComponent* Legs = GetFactoryLegs())
		{
			Legs->SetExtraBodyTilt(FRotator::ZeroRotator);
		}
		// Swells and trembles, then bursts
		const float Share = FMath::Clamp(StateTime / Data->BurstWindup, 0.0f, 1.0f);
		const float Tremble = FMath::Sin(StateTime * 60.0f) * 2.0f * Share;
		SetFactoryMeshOffset(FVector(Tremble, 0.0f, 0.0f), 1.0f + BurstSwell * Share);
		if (Share >= 1.0f)
		{
			Burst();
		}
		break;
	}
	}
}

bool AVaelThistleSpider::UpdateBite(float DeltaSeconds, AActor* Target)
{
	if (BiteTime < 0.0f)
	{
		return false;
	}
	const UVaelThistleSpiderData* Data = GetData<UVaelThistleSpiderData>();
	UVaelLegIKComponent* Legs = GetFactoryLegs();
	FaceTowards(Target->GetActorLocation());
	BiteTime += DeltaSeconds;
	const float Share = FMath::Clamp(BiteTime / Data->BiteDuration, 0.0f, 1.0f);

	// First half: rears up, fangs raised; then it throws itself forward and down, and back to rest
	float Pitch = 0.0f;
	if (Share < 0.5f)
	{
		Pitch = Data->BiteRear * FMath::SmoothStep(0.0f, 1.0f, Share / 0.5f);
	}
	else if (Share < 0.7f)
	{
		Pitch = FMath::Lerp(Data->BiteRear, -Data->BiteRear * 0.5f, (Share - 0.5f) / 0.2f);
	}
	else
	{
		Pitch = FMath::Lerp(-Data->BiteRear * 0.5f, 0.0f, (Share - 0.7f) / 0.3f);
	}
	if (Legs != nullptr)
	{
		Legs->SetExtraBodyTilt(FRotator(Pitch, 0.0f, 0.0f));
	}

	if (!bBiteStruck && Share >= 0.5f)
	{
		bBiteStruck = true;
		if (Legs != nullptr)
		{
			Legs->AddBodyJolt(GetActorForwardVector() * Data->BiteLunge);
		}
		if (GetDistanceTo2D(Target) <= Data->BiteRange + Data->BiteLunge && !IsBlinded())
		{
			HitPlayer(Target, Data->BiteDamage, EVaelElement::Earth);
		}
	}
	if (Share >= 1.0f)
	{
		BiteTime = -1.0f;
		BiteCooldown = RandomInRange(Data->BiteInterval);
		if (Legs != nullptr)
		{
			Legs->SetExtraBodyTilt(FRotator::ZeroRotator);
		}
	}
	return true;
}

void AVaelThistleSpider::Burst()
{
	const UVaelThistleSpiderData* Data = GetData<UVaelThistleSpiderData>();
	UWorld* World = GetWorld();
	const bool bBurning = UVaelCombatStatics::HasStatus(this, EVaelStatus::Burning);

	// Seed down: everyone close is slowed
	int32 Players = 0;
	ForEachActivePlayer([&](AVaelCharacter* Player)
	{
		++Players;
		if (GetDistanceTo2D(Player) < Data->DownRange)
		{
			UVaelCombatStatics::ApplyStatus(this, Player, EVaelStatus::Slowed, Data->DownSlowTime);
		}
	});

	// The swarm runs out in a ring, more with more players
	UVaelCreatureData* SeedlingData = Data->SeedlingData != nullptr ? Data->SeedlingData.Get() : GetMutableDefault<UVaelThistleSeedlingData>();
	const int32 Count = FMath::RoundToInt(FMath::RandRange(Data->SeedlingCount.X, Data->SeedlingCount.Y) * (1.0f + Data->ExtraPlayerShare * FMath::Max(Players - 1, 0)));
	int32 Spawned = 0;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Angle = UE_TWO_PI * Index / Count + FMath::FRandRange(-0.2f, 0.2f);
		const FVector Spot = GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * FMath::FRandRange(40.0f, 130.0f);
		FVector Ground;
		if (!FindGround(World, Spot, Ground))
		{
			continue;
		}
		if (AVaelCreature* Seedling = SpawnCreature(World, SeedlingData, Ground, FRotator(0.0f, FMath::RadiansToDegrees(Angle), 0.0f)))
		{
			++Spawned;
			if (bBurning)
			{
				UVaelCombatStatics::ApplyStatus(this, Seedling, EVaelStatus::Burning, 3.0f, 4.0f);
			}
		}
	}
	// A cloud of seed down and pollen swells out of it, with pink puffs, and a little dirt is thrown up
	AVaelPollenBurst::Spawn(World, GetActorLocation(), Data->PollenCloudSize, Data->PollenColor, Data->PuffColor);
	VaelEffects::SpawnAt(this, VaelAssets::LoadOptional(UVaelMagicSettings::Get()->DustPuffEffect), GetActorLocation(), FRotator::ZeroRotator,
	                     Data->PollenColor, Data->PollenCloudSize * 0.3f);

	UE_LOG(LogVael, Log, TEXT("'%s' bursts into %d seed spiders%s at %s"), *GetName(), Spawned, bBurning ? TEXT(", burning") : TEXT(""), *GetActorLocation().ToCompactString());

	// What is left is an empty husk
	Die();
}

// ---------------------------------------------------------------- Distelsämling

AVaelThistleSeedling::AVaelThistleSeedling()
{
	CorpseDuration = 0.3f;
}

TSubclassOf<UVaelCreatureData> AVaelThistleSeedling::GetDefaultDataClass() const
{
	return UVaelThistleSeedlingData::StaticClass();
}

void AVaelThistleSeedling::BeginPlay()
{
	Super::BeginPlay();

	PlaceAngle = FMath::FRandRange(0.0f, UE_TWO_PI);
	LeapCooldown = FMath::FRandRange(0.2f, 0.8f);
	FireSpreadCountdown = FMath::FRandRange(0.0f, FireSpreadInterval);
}

void AVaelThistleSeedling::TickBehavior(float DeltaSeconds)
{
	const UVaelThistleSeedlingData* Data = GetData<UVaelThistleSeedlingData>();
	LeapCooldown -= DeltaSeconds;

	// A burning seedling sets its neighbours on fire: fire runs through the swarm
	FireSpreadCountdown -= DeltaSeconds;
	if (FireSpreadCountdown <= 0.0f)
	{
		FireSpreadCountdown = FireSpreadInterval;
		if (UVaelCombatStatics::HasStatus(this, EVaelStatus::Burning))
		{
			for (TActorIterator<AVaelThistleSeedling> It(GetWorld()); It; ++It)
			{
				if (*It != this && !It->IsDead() && GetDistanceTo2D(*It) < Data->FireSpreadRange
					&& !UVaelCombatStatics::HasStatus(*It, EVaelStatus::Burning))
				{
					UVaelCombatStatics::ApplyStatus(this, *It, EVaelStatus::Burning, Data->FireSpreadTime, Data->FireSpreadDamage);
				}
			}
		}
	}

	UVaelLegIKComponent* Legs = GetFactoryLegs();

	// In the air: legs tucked, nose down; touching a player hurts once per leap
	if (bLeaping)
	{
		LeapClock += DeltaSeconds;
		AActor* Target = LeapTarget.Get();
		// Touching: side by side with the player and within the height of its body (its middle is at the hip)
		const float Height = Target != nullptr ? GetActorLocation().Z - Target->GetActorLocation().Z : 0.0f;
		if (!bLeapHit && Target != nullptr && GetDistanceTo2D(Target) < GetCapsuleComponent()->GetScaledCapsuleRadius() + LeapTouchReach
			&& Height > -LeapTouchBelow && Height < LeapTouchAbove)
		{
			bLeapHit = true;
			LeapHitCount += HitPlayer(Target, Data->LeapDamage, EVaelElement::Earth) ? 1 : 0;
		}
		if (LeapClock > 0.15f && GetCharacterMovement()->IsMovingOnGround())
		{
			bLeaping = false;
			LandPauseLeft = Data->LandPause;
			PlaceTime = 0.0f;
			// Its new place is where it landed, a little shifted, so the next leap comes back through from this side
			if (Target != nullptr)
			{
				const FVector Away = GetActorLocation() - Target->GetActorLocation();
				PlaceAngle = FMath::Atan2(Away.Y, Away.X) + FMath::FRandRange(-0.6f, 0.6f);
			}
			if (Legs != nullptr)
			{
				Legs->SetCurl(0.0f);
				Legs->SetExtraBodyTilt(FRotator::ZeroRotator);
				Legs->PlantAllFeet();
			}
		}
		return;
	}
	if (LandPauseLeft > 0.0f)
	{
		LandPauseLeft -= DeltaSeconds;
		return;
	}

	float Distance = 0.0f;
	AActor* Target = FindTarget(Data->AggroRange * GetTimeOfDayAwareness(), &Distance);
	if (Target == nullptr)
	{
		return;
	}
	// Its place on the ring around the target; far away it just runs at the target from the side of its place
	const FVector Place = Target->GetActorLocation() + FVector(FMath::Cos(PlaceAngle), FMath::Sin(PlaceAngle), 0.0f) * Data->SurroundRadius;
	const float ToPlace = FVector::Dist2D(GetActorLocation(), Place);
	const bool bAtPlace = ToPlace < 90.0f;
	if (Distance < Data->SurroundRadius * 1.8f)
	{
		PlaceTime += DeltaSeconds;
	}
	const bool bReady = LeapCooldown <= 0.0f && !IsBlinded() && GetCharacterMovement()->IsMovingOnGround();
	if (bReady && Distance <= Data->LeapRange && (bAtPlace || PlaceTime > Data->PlaceTimeout))
	{
		StartLeap(Target);
		return;
	}
	if (!bAtPlace)
	{
		MoveTowards(Place, Data->MoveSpeed);
	}
	else
	{
		FaceTowards(Target->GetActorLocation());
	}
}

void AVaelThistleSeedling::StartLeap(const AActor* Target)
{
	const UVaelThistleSeedlingData* Data = GetData<UVaelThistleSeedlingData>();
	FVector Direction = (Target->GetActorLocation() - GetActorLocation()) * FVector(1.0f, 1.0f, 0.0f);
	const float Distance = Direction.Size();
	Direction = Direction.GetSafeNormal();

	// Up to chest height and through the target, landing on its other side
	const float Gravity = -GetCharacterMovement()->GetGravityZ();
	const float Up = FMath::Sqrt(2.0f * Gravity * Data->LeapApex);
	const float AirTime = 2.0f * Up / Gravity;
	const float Across = Distance + Data->SurroundRadius * Data->LeapThrough;
	FaceTowards(Target->GetActorLocation());
	LaunchCharacter(Direction * (Across / AirTime) + FVector(0.0f, 0.0f, Up), true, true);

	bLeaping = true;
	++LeapCount;
	bLeapHit = false;
	LeapClock = 0.0f;
	LeapTarget = const_cast<AActor*>(Target);
	LeapCooldown = AirTime + RandomInRange(Data->LeapInterval);
	if (UVaelLegIKComponent* Legs = GetFactoryLegs())
	{
		Legs->SetCurl(0.8f);
		Legs->SetExtraBodyTilt(FRotator(-25.0f, 0.0f, 0.0f));
	}
}
