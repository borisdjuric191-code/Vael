// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelClayGolem.h"
#include "VaelAssets.h"
#include "AIController.h"
#include "Combat/VaelHitFeedbackSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Magic/VaelDebrisBurst.h"
#include "Magic/VaelGroundArea.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelSpellEffects.h"

namespace
{
	/** Size of the capsule in cm */
	constexpr float GolemCapsuleRadius = 45.0f;
	constexpr float GolemCapsuleHalfHeight = 90.0f;

	/** Health of the golem */
	constexpr float GolemHealth = 160.0f;

	/** Seconds the golem needs to rise out of the ground */
	constexpr float GolemRiseTime = 0.6f;

	/** How far the arms lift before a stamp, in cm, and the share of the interval they start lifting at */
	constexpr float GolemArmLift = 55.0f;
	constexpr float GolemWindupStart = 0.5f;

	/** Camera shake of a stamp */
	constexpr float GolemStampShake = 0.08f;

	/** Diameter of the lump mesh in cm */
	constexpr float GolemSphereSize = 100.0f;

	/** Lumbering gait: cm of one step, how high a foot lifts, how far feet and arms swing, how far the trunk sways, in cm */
	constexpr float GolemStepLength = 65.0f;
	constexpr float GolemFootLift = 16.0f;
	constexpr float GolemLegSwing = 24.0f;
	constexpr float GolemArmSwing = 20.0f;
	constexpr float GolemSway = 7.0f;

	/** Turning in degrees per second */
	constexpr float GolemTurnRate = 120.0f;

	/** Drops of mud: how many slide down and how many creep up the legs, cm/s of sliding and creeping, seconds of a splash */
	constexpr int32 GolemSlidingDrops = 14;
	constexpr int32 GolemRisingDrops = 6;
	constexpr float GolemSlideSpeedMin = 22.0f;
	constexpr float GolemSlideSpeedMax = 55.0f;
	constexpr float GolemRiseSpeedMin = 28.0f;
	constexpr float GolemRiseSpeedMax = 48.0f;
	constexpr float GolemSplashTime = 1.2f;

	/** Height in cm (from the middle of the capsule) where drops leave the trunk or a hand, and where rising mud melts into the legs */
	constexpr float GolemDropOffTrunk = -30.0f;
	constexpr float GolemDropOffHand = -78.0f;
	constexpr float GolemRiseTop = -25.0f;

	const FLinearColor Clay(0.45f, 0.33f, 0.22f);
	const FLinearColor WetClay(0.36f, 0.26f, 0.17f);
}

AVaelClayGolem::AVaelClayGolem()
{
	GetCapsuleComponent()->InitCapsuleSize(GolemCapsuleRadius, GolemCapsuleHalfHeight);

	// The AI controller keeps the movement running; the golem steers itself towards the enemies
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, GolemTurnRate, 0.0f);

	StartingHealth = GolemHealth;
	StartingMana = 0.0f;

	PrimaryActorTick.bCanEverTick = true;
}

AVaelClayGolem* AVaelClayGolem::Summon(TSubclassOf<AVaelClayGolem> GolemClass, APawn* Caster, const FVector& GroundLocation, const FVaelSpellHit& StampHit, float Lifetime)
{
	UWorld* World = Caster != nullptr ? Caster->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}

	if (GolemClass == nullptr)
	{
		GolemClass = AVaelClayGolem::StaticClass();
	}

	const FTransform SpawnTransform(FRotator(0.0f, Caster->GetActorRotation().Yaw, 0.0f), GroundLocation + FVector(0.0f, 0.0f, GolemCapsuleHalfHeight + 2.0f));

	AVaelClayGolem* Golem = World->SpawnActorDeferred<AVaelClayGolem>(GolemClass, SpawnTransform, Caster, Caster, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (Golem != nullptr)
	{
		Golem->StampHit = StampHit;
		Golem->InitialLifeSpan = Lifetime;
		Golem->FinishSpawning(SpawnTransform);
	}

	return Golem;
}

void AVaelClayGolem::BeginPlay()
{
	Super::BeginPlay();

	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	Home = GetActorLocation();
	LastTrail = Home;

	UMaterialInterface* CoreMaterial = nullptr;
	UMaterialInterface* GlowMaterial = nullptr;
	VaelEffects::LoadLookMaterials(CoreMaterial, GlowMaterial);
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	UMaterialInterface* Mud = VaelAssets::LoadOptional(MagicSettings->ClayMaterial);
	UStaticMesh* Lump = VaelAssets::LoadOptional(MagicSettings->ClayLumpMesh);

	auto AddLump = [&](const FVector& Size, const FLinearColor& Color)
	{
		UStaticMeshComponent* Shape = VaelEffects::AddLookShape(this, CoreMaterial, Color, 0.0f);
		if (Lump != nullptr)
		{
			Shape->SetStaticMesh(Lump);
		}
		if (Mud != nullptr)
		{
			Shape->SetMaterial(0, Mud);
		}
		Shape->SetRelativeScale3D(Size / GolemSphereSize);
		Shape->SetCastShadow(true);
		Shape->SetVisibility(true);
		return Shape;
	};

	// A thing out of the swamp: legs, a hunched trunk with a hump, the head sunk forward between the shoulders,
	// long arms hanging to the knees. Place and size in cm, its limb, how low on it (1 = foot or hand)
	struct FLump { FVector Place; FVector Size; EPartRole Role; float Low; };
	TArray<FLump> Lumps;
	for (const float Side : { -1.0f, 1.0f })
	{
		const EPartRole Leg = Side < 0.0f ? EPartRole::LegLeft : EPartRole::LegRight;
		const EPartRole Arm = Side < 0.0f ? EPartRole::ArmLeft : EPartRole::ArmRight;
		Lumps.Add({ FVector(0.0f, 24.0f * Side, -38.0f), FVector(44.0f, 42.0f, 56.0f), Leg, 0.3f });
		Lumps.Add({ FVector(2.0f, 27.0f * Side, -66.0f), FVector(38.0f, 36.0f, 46.0f), Leg, 0.7f });
		Lumps.Add({ FVector(10.0f, 28.0f * Side, -84.0f), FVector(50.0f, 38.0f, 20.0f), Leg, 1.0f });
		Lumps.Add({ FVector(10.0f, 50.0f * Side, 46.0f), FVector(56.0f, 54.0f, 50.0f), EPartRole::Trunk, 0.0f });
		Lumps.Add({ FVector(2.0f, 36.0f * Side, 64.0f), FVector(36.0f, 34.0f, 26.0f), EPartRole::Trunk, 0.0f });
		Lumps.Add({ FVector(14.0f, 66.0f * Side, 22.0f), FVector(36.0f, 34.0f, 60.0f), Arm, 0.3f });
		Lumps.Add({ FVector(20.0f, 70.0f * Side, -16.0f), FVector(34.0f, 32.0f, 62.0f), Arm, 0.65f });
		Lumps.Add({ FVector(26.0f, 70.0f * Side, -52.0f), FVector(44.0f, 40.0f, 40.0f), Arm, 1.0f });
		Lumps.Add({ FVector(34.0f, 72.0f * Side, -72.0f), FVector(26.0f, 22.0f, 26.0f), Arm, 1.0f });
	}
	Lumps.Add({ FVector(0.0f, 0.0f, -18.0f), FVector(84.0f, 78.0f, 52.0f), EPartRole::Trunk, 0.0f });
	Lumps.Add({ FVector(6.0f, 0.0f, 8.0f), FVector(88.0f, 80.0f, 66.0f), EPartRole::Trunk, 0.0f });
	Lumps.Add({ FVector(14.0f, 0.0f, 36.0f), FVector(104.0f, 94.0f, 70.0f), EPartRole::Trunk, 0.0f });
	Lumps.Add({ FVector(-14.0f, 0.0f, 52.0f), FVector(74.0f, 86.0f, 58.0f), EPartRole::Trunk, 0.0f });
	Lumps.Add({ FVector(34.0f, 0.0f, 50.0f), FVector(44.0f, 42.0f, 40.0f), EPartRole::Head, 0.0f });
	Lumps.Add({ FVector(44.0f, 0.0f, 58.0f), FVector(30.0f, 38.0f, 16.0f), EPartRole::Head, 0.0f });
	Lumps.Add({ FVector(40.0f, 0.0f, 38.0f), FVector(34.0f, 32.0f, 22.0f), EPartRole::Head, 0.0f });

	for (const FLump& Each : Lumps)
	{
		BodyParts.Add(AddLump(Each.Size, Each.Low > 0.5f ? WetClay : Clay));
		BodyParts.Last()->SetRelativeRotation(FRotator(FMath::FRandRange(0.0f, 360.0f), FMath::FRandRange(0.0f, 360.0f), 0.0f));
		FBodyPart& Part = Parts.AddDefaulted_GetRef();
		Part.Place = Each.Place;
		Part.Size = Each.Size;
		Part.Role = Each.Role;
		Part.Low = Each.Low;
		// Every lump churns slowly around itself, so the mud never stands still
		Part.Spin = FRotator(FMath::FRandRange(-12.0f, 12.0f), FMath::FRandRange(-18.0f, 18.0f), FMath::FRandRange(-12.0f, 12.0f));
	}

	// Drops that run down it and mud that creeps up its legs
	for (int32 Index = 0; Index < GolemSlidingDrops + GolemRisingDrops; ++Index)
	{
		DropMeshes.Add(AddLump(FVector(10.0f), WetClay));
		Drops.AddDefaulted();
		StartDrop(Index, Index >= GolemSlidingDrops);
	}

	StampCountdown = StampInterval;

	// Clay and pebbles burst where it breaks out of the ground
	FVaelDebris Dust;
	Dust.Count = 10;
	Dust.Spread = GolemCapsuleRadius;
	Dust.Speed = 240.0f;
	Dust.Lift = 360.0f;
	Dust.Color = WetClay;
	Dust.RockShare = 0.2f;
	Dust.bDust = true;
	AVaelDebrisBurst::Spawn(this, GetActorLocation() - FVector(0.0f, 0.0f, GolemCapsuleHalfHeight), Dust);

	Tick(0.0f);
}

void AVaelClayGolem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bCrumbled)
	{
		return;
	}

	Age += DeltaSeconds;

	if (Age > GolemRiseTime)
	{
		Walk(DeltaSeconds);
	}

	// It only winds up while an enemy is close enough; otherwise the arms rest
	bool bEnemyClose = false;
	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It && !bEnemyClose; ++It)
	{
		bEnemyClose = UVaelCombatStatics::CanDamage(this, *It) && GetHorizontalDistanceTo(*It) <= StampRadius;
	}

	StampCountdown -= DeltaSeconds;
	if (!bEnemyClose)
	{
		StampCountdown = FMath::Max(StampCountdown, StampInterval * GolemWindupStart);
	}
	else if (StampCountdown <= 0.0f)
	{
		Stamp();
		StampCountdown = StampInterval;
	}

	// The gait follows how fast it really goes
	const float Speed = GetVelocity().Size2D();
	Stride = FMath::FInterpTo(Stride, FMath::Clamp(Speed / FMath::Max(WalkSpeed, 1.0f), 0.0f, 1.0f), DeltaSeconds, 5.0f);
	Gait = FMath::Fmod(Gait + Speed * DeltaSeconds / GolemStepLength * PI, 2.0f * PI);

	Pose(FMath::SmoothStep(GolemWindupStart, 0.95f, 1.0f - StampCountdown / StampInterval), DeltaSeconds);
	Flow(DeltaSeconds);
}

void AVaelClayGolem::Walk(float DeltaSeconds)
{
	// The nearest enemy it may reach without going farther than its leash from where it was called
	const AVaelCharacterBase* Target = nullptr;
	float Nearest = LureRadius;
	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		const float Distance = GetHorizontalDistanceTo(*It);
		if (Distance < Nearest && UVaelCombatStatics::CanDamage(this, *It) && FVector::Dist2D(It->GetActorLocation(), Home) <= Leash + StampRadius)
		{
			Target = *It;
			Nearest = Distance;
		}
	}

	const FVector Goal = Target != nullptr ? Target->GetActorLocation() : Home;
	const float StopAt = Target != nullptr ? StampRadius * 0.6f : 120.0f;
	FVector Way = Goal - GetActorLocation();
	Way.Z = 0.0f;
	if (Way.Size() > StopAt)
	{
		AddMovementInput(Way.GetSafeNormal());
	}
	else if (Target != nullptr)
	{
		// Standing at the enemy it still turns to face it
		const float Yaw = FMath::FixedTurn(GetActorRotation().Yaw, Way.Rotation().Yaw, GolemTurnRate * DeltaSeconds);
		SetActorRotation(FRotator(0.0f, Yaw, 0.0f));
	}

	// Mud stays behind where it walked and slows the enemies of the caster
	if (FVector::Dist2D(GetActorLocation(), LastTrail) >= TrailSpacing)
	{
		LastTrail = GetActorLocation();
		AVaelGroundArea::SpawnArea(GetWorld(), LastTrail - FVector(0.0f, 0.0f, GolemCapsuleHalfHeight - 2.0f), EVaelElement::Earth, TrailRadius, TrailLifetime, 0.0f, GetInstigator(), true, EVaelGroundEffect::Slow);
	}
}

void AVaelClayGolem::Pose(float Windup, float DeltaSeconds)
{
	// Rises out of the ground
	const float Rise = FMath::Clamp(Age / GolemRiseTime, 0.0f, 1.0f);
	const float Sunk = GolemCapsuleHalfHeight * 2.0f * FMath::Square(1.0f - Rise);

	// Heavy steps: the trunk sinks with each one and rolls from side to side, it breathes slowly when it stands
	const float Step = FMath::Sin(Gait);
	const float Bob = -FMath::Abs(Step) * 5.0f * Stride + FMath::Sin(Age * 1.7f) * 2.5f;
	const float Sway = Step * GolemSway * Stride + FMath::Sin(Age * 0.6f) * 3.0f;
	const float Lean = 6.0f * Stride;

	for (int32 Index = 0; Index < Parts.Num(); ++Index)
	{
		const FBodyPart& Part = Parts[Index];
		FVector Offset(0.0f, 0.0f, -Sunk);

		switch (Part.Role)
		{
		case EPartRole::Trunk:
		case EPartRole::Head:
		{
			// The higher, the more it sways; the head lags behind with its own nod
			const float Height = FMath::Clamp(0.5f + Part.Place.Z / 100.0f, 0.2f, 1.2f);
			Offset += FVector(Lean * Height, Sway * Height, Bob);
			if (Part.Role == EPartRole::Head)
			{
				Offset += FVector(FMath::Sin(Age * 1.1f) * 2.0f, 0.0f, FMath::Sin(Age * 1.1f + 0.8f) * 3.0f - 4.0f * Stride);
			}
			break;
		}
		case EPartRole::LegLeft:
		case EPartRole::LegRight:
		{
			const float Phase = Gait + (Part.Role == EPartRole::LegRight ? PI : 0.0f);
			Offset += FVector(FMath::Cos(Phase) * GolemLegSwing * Stride * Part.Low, Sway * 0.3f,
				FMath::Max(0.0f, FMath::Sin(Phase)) * GolemFootLift * Stride * Part.Low + Bob * (1.0f - Part.Low));
			break;
		}
		case EPartRole::ArmLeft:
		case EPartRole::ArmRight:
		{
			// Each arm swings with the other leg and dangles heavily; before a stamp both rise high
			const bool bLeft = Part.Role == EPartRole::ArmLeft;
			const float Phase = Gait + (bLeft ? PI : 0.0f);
			const float Dangle = FMath::Sin(Age * 1.3f + (bLeft ? 0.0f : 1.7f)) * 4.0f;
			Offset += FVector(FMath::Cos(Phase) * GolemArmSwing * Stride * Part.Low + Lean + Windup * 15.0f * Part.Low,
				Sway * 0.8f + Dangle * Part.Low,
				Bob + GolemArmLift * Windup * (0.4f + Part.Low));
			break;
		}
		}

		BodyParts[Index]->SetRelativeLocation(Part.Place + Offset);
		BodyParts[Index]->AddRelativeRotation(Part.Spin * DeltaSeconds);
	}
}

void AVaelClayGolem::StartDrop(int32 Index, bool bRise)
{
	FDrop& Drop = Drops[Index];
	UStaticMeshComponent* DropMesh = DropMeshes[Index];
	Drop.Time = 0.0f;
	Drop.Velocity = FVector::ZeroVector;
	Drop.Size = FMath::FRandRange(7.0f, 13.0f);

	// Back onto the body: the drop moves with the golem again
	DropMesh->SetUsingAbsoluteLocation(false);

	if (bRise)
	{
		// Mud gathers on the ground at a foot and creeps up the leg
		const float Side = Index % 2 == 0 ? -1.0f : 1.0f;
		Drop.State = FDrop::EState::Rise;
		Drop.Speed = FMath::FRandRange(GolemRiseSpeedMin, GolemRiseSpeedMax);
		Drop.Place = FVector(FMath::FRandRange(-12.0f, 22.0f), Side * FMath::FRandRange(14.0f, 40.0f), FMath::FRandRange(-GolemCapsuleHalfHeight, GolemRiseTop));
		return;
	}

	// A drop leaves the upper body, the head or an arm somewhere on its surface
	TArray<int32, TInlineAllocator<32>> Upper;
	for (int32 PartIndex = 0; PartIndex < Parts.Num(); ++PartIndex)
	{
		const FBodyPart& Part = Parts[PartIndex];
		if (Part.Role != EPartRole::LegLeft && Part.Role != EPartRole::LegRight && (Part.Role != EPartRole::Trunk || Part.Place.Z > -5.0f))
		{
			Upper.Add(PartIndex);
		}
	}
	const FBodyPart& From = Parts[Upper[FMath::RandRange(0, Upper.Num() - 1)]];
	const bool bArm = From.Role == EPartRole::ArmLeft || From.Role == EPartRole::ArmRight;
	const float Around = FMath::FRandRange(0.0f, 2.0f * PI);
	Drop.State = FDrop::EState::Slide;
	Drop.Speed = FMath::FRandRange(GolemSlideSpeedMin, GolemSlideSpeedMax);
	Drop.DropOff = bArm ? GolemDropOffHand : GolemDropOffTrunk;
	Drop.Place = From.Place + FVector(FMath::Cos(Around) * From.Size.X * 0.45f, FMath::Sin(Around) * From.Size.Y * 0.45f, From.Size.Z * FMath::FRandRange(0.0f, 0.4f));
}

void AVaelClayGolem::Flow(float DeltaSeconds)
{
	const float Ground = GetActorLocation().Z - GolemCapsuleHalfHeight + 1.0f;

	for (int32 Index = 0; Index < Drops.Num(); ++Index)
	{
		FDrop& Drop = Drops[Index];
		UStaticMeshComponent* DropMesh = DropMeshes[Index];
		Drop.Time += DeltaSeconds;

		switch (Drop.State)
		{
		case FDrop::EState::Slide:
			// Runs down the surface, stretched long, and bulges a little outwards as it goes
			Drop.Place.Z -= Drop.Speed * DeltaSeconds;
			Drop.Place += FVector(Drop.Place.X, Drop.Place.Y, 0.0f).GetSafeNormal() * 3.0f * DeltaSeconds;
			DropMesh->SetRelativeLocation(Drop.Place);
			DropMesh->SetRelativeScale3D(FVector(Drop.Size, Drop.Size, Drop.Size * 1.7f) / GolemSphereSize);
			if (Drop.Place.Z <= Drop.DropOff)
			{
				// Lets go: from now on it falls in the world and stays where it lands
				const FVector World = GetActorTransform().TransformPosition(Drop.Place);
				DropMesh->SetUsingAbsoluteLocation(true);
				DropMesh->SetWorldLocation(World);
				Drop.State = FDrop::EState::Fall;
				Drop.Place = World;
				Drop.Velocity = GetVelocity() * 0.5f + FVector(0.0f, 0.0f, -40.0f);
			}
			break;
		case FDrop::EState::Fall:
			Drop.Velocity.Z -= 980.0f * DeltaSeconds;
			Drop.Place += Drop.Velocity * DeltaSeconds;
			if (Drop.Place.Z <= Ground)
			{
				Drop.Place.Z = Ground;
				Drop.State = FDrop::EState::Splash;
				Drop.Time = 0.0f;
			}
			DropMesh->SetWorldLocation(Drop.Place);
			DropMesh->SetRelativeScale3D(FVector(Drop.Size * 0.9f, Drop.Size * 0.9f, Drop.Size * 1.2f) / GolemSphereSize);
			break;
		case FDrop::EState::Splash:
		{
			// Splashes flat and sinks into the ground, then the mud comes back on the body
			const float Spread = FMath::Clamp(Drop.Time / 0.15f, 0.0f, 1.0f);
			const float Sink = FMath::Clamp((Drop.Time - 0.15f) / (GolemSplashTime - 0.15f), 0.0f, 1.0f);
			DropMesh->SetWorldLocation(Drop.Place - FVector(0.0f, 0.0f, Sink * Drop.Size * 0.3f));
			DropMesh->SetRelativeScale3D(FVector(Drop.Size * (1.0f + 1.4f * Spread), Drop.Size * (1.0f + 1.4f * Spread), Drop.Size * FMath::Lerp(0.35f, 0.02f, Sink)) / GolemSphereSize);
			if (Drop.Time >= GolemSplashTime)
			{
				StartDrop(Index, false);
			}
			break;
		}
		case FDrop::EState::Rise:
		{
			// Swells out of the ground, creeps up and melts into the leg
			Drop.Place.Z += Drop.Speed * DeltaSeconds;
			const float Way = FMath::Clamp((Drop.Place.Z + GolemCapsuleHalfHeight) / (GolemRiseTop + GolemCapsuleHalfHeight), 0.0f, 1.0f);
			const float Swell = FMath::Sin(Way * PI) * 1.4f + 0.2f;
			DropMesh->SetRelativeLocation(Drop.Place);
			DropMesh->SetRelativeScale3D(FVector(Drop.Size * Swell, Drop.Size * Swell, Drop.Size * Swell * 1.4f) / GolemSphereSize);
			if (Way >= 1.0f)
			{
				StartDrop(Index, true);
				Drops[Index].Place.Z = -GolemCapsuleHalfHeight;
			}
			break;
		}
		}
	}
}

void AVaelClayGolem::Stamp()
{
	// The hit counts as the caster's, so it hurts their enemies
	AActor* Attacker = GetInstigator() != nullptr ? static_cast<AActor*>(GetInstigator()) : this;
	const FVector Feet = GetActorLocation() - FVector(0.0f, 0.0f, GolemCapsuleHalfHeight);

	UVaelCombatStatics::ApplySpellHitInRadius(Attacker, Feet, StampRadius, StampHit);
	UVaelHitFeedbackSubsystem::Shake(this, GolemStampShake);

	// Wet mud splashes from its fists
	FVaelDebris Dust;
	Dust.Count = 10;
	Dust.Spread = StampRadius * 0.5f;
	Dust.Speed = 300.0f;
	Dust.Lift = 260.0f;
	Dust.Color = WetClay;
	Dust.bDust = true;
	AVaelDebrisBurst::Spawn(this, Feet + GetActorForwardVector() * 40.0f, Dust);
}

void AVaelClayGolem::OnHealthChanged(float OldValue, float NewValue)
{
	Super::OnHealthChanged(OldValue, NewValue);

	if (NewValue <= 0.0f)
	{
		Crumble();
	}
}

void AVaelClayGolem::LifeSpanExpired()
{
	Crumble();
}

void AVaelClayGolem::Crumble()
{
	if (bCrumbled)
	{
		return;
	}

	bCrumbled = true;

	// Falls apart into lumps of clay that leave a patch of mud slowing the enemies of the caster
	const FVector Feet = GetActorLocation() - FVector(0.0f, 0.0f, GolemCapsuleHalfHeight);

	AVaelGroundArea::SpawnArea(GetWorld(), Feet + FVector(0.0f, 0.0f, 2.0f), EVaelElement::Earth, MudRadius, MudLifetime, 0.0f, GetInstigator(), true, EVaelGroundEffect::Slow);

	FVaelDebris Lumps;
	Lumps.Count = 16;
	Lumps.Spread = GolemCapsuleRadius;
	Lumps.Speed = 260.0f;
	Lumps.Lift = 220.0f;
	Lumps.Lifetime = 0.8f;
	Lumps.MinSize = 12.0f;
	Lumps.MaxSize = 24.0f;
	Lumps.Color = Clay;
	AVaelDebrisBurst::Spawn(this, GetActorLocation(), Lumps);

	Destroy();
}
