// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelBloodMoth.h"
#include "AnimationRuntime.h"
#include "Combat/VaelCombatStatics.h"
#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Creatures/VaelCreatureData.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Magic/VaelSpellEffects.h"
#include "Nature/VaelRainBloom.h"
#include "Player/VaelCharacter.h"
#include "Vael.h"
#include "World/VaelSafeZone.h"

#define LOCTEXT_NAMESPACE "VaelCreatures"

bool AVaelBloodMoth::bFirstHarvestGrown = false;

namespace
{
	/** Drops in the air at once at most */
	constexpr int32 MothMaxDrops = 14;

	/** Diameter of the engine sphere the drops are made of, in cm */
	constexpr float MothShapeSize = 100.0f;

	/** Seconds it takes to climb out of the cocoon, and to fly off when its rain is spent */
	constexpr float MothRiseTime = 1.6f;
	constexpr float MothLeaveTime = 2.5f;

	/** Seconds the splash of a landed drop shows */
	constexpr float MothSplashTime = 0.3f;

	/** The body lies this far forward in the air, in degrees */
	constexpr float MothLean = 72.0f;
}

AVaelBloodMoth::AVaelBloodMoth()
{
	// Flies at the height it was spawned at and passes over the heads of the players
	GetCharacterMovement()->DefaultLandMovementMode = MOVE_Flying;
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
}

TSubclassOf<UVaelCreatureData> AVaelBloodMoth::GetDefaultDataClass() const
{
	return UVaelBloodMothData::StaticClass();
}

float AVaelBloodMoth::GetStatusTextHeight() const
{
	return Super::GetStatusTextHeight() + GetData<UVaelBloodMothData>()->FlightHeight;
}

void AVaelBloodMoth::BeginPlay()
{
	Super::BeginPlay();

	GetCharacterMovement()->SetMovementMode(MOVE_Flying);

	const UVaelBloodMothData* Data = GetData<UVaelBloodMothData>();
	RainLength = RandomInRange(Data->RainTime);
	CircleAngle = FMath::FRandRange(0.0f, UE_TWO_PI);
	DropCountdown = RandomInRange(Data->DropInterval);

	UMaterialInterface* CoreMaterial = nullptr;
	UMaterialInterface* GlowMaterial = nullptr;
	VaelEffects::LoadLookMaterials(CoreMaterial, GlowMaterial);

	Drops.SetNum(MothMaxDrops);
	for (int32 Index = 0; Index < MothMaxDrops; ++Index)
	{
		UStaticMeshComponent* Shape = VaelEffects::AddLookShape(this, GlowMaterial, Data->DropColor, Data->bHealingRain ? 3.0f : 1.2f);
		Shape->SetCastShadow(false);
		Shape->SetVisibility(false);
		Shape->SetUsingAbsoluteLocation(true);
		Shape->SetUsingAbsoluteRotation(true);
		Shape->SetUsingAbsoluteScale(true);
		DropShapes.Add(Shape);
	}

	if (GetFactoryMesh() != nullptr)
	{
		SetupWings();
	}
}

void AVaelBloodMoth::SetPrey(AVaelCharacter* Player)
{
	Prey = Player;
}

AVaelCharacter* AVaelBloodMoth::FindPrey() const
{
	if (AVaelCharacter* Own = Prey.Get(); Own != nullptr && !Own->IsDefeated())
	{
		return Own;
	}

	AVaelCharacter* Nearest = nullptr;
	float NearestDistance = UE_BIG_NUMBER;
	ForEachActivePlayer([this, &Nearest, &NearestDistance](AVaelCharacter* Player)
	{
		const float Distance = GetDistanceTo2D(Player);
		if (Distance < NearestDistance)
		{
			Nearest = Player;
			NearestDistance = Distance;
		}
	});

	return Nearest;
}

void AVaelBloodMoth::TickBehavior(float DeltaSeconds)
{
	const UVaelBloodMothData* Data = GetData<UVaelBloodMothData>();

	StateTime += DeltaSeconds;

	switch (State)
	{
	case EVaelMothState::Rising:
		if (StateTime >= MothRiseTime)
		{
			State = EVaelMothState::Raining;
			StateTime = 0.0f;
		}
		break;

	case EVaelMothState::Raining:
	{
		// Follows its player and circles above them; the rain follows the moth
		if (const AVaelCharacter* Target = FindPrey())
		{
			CircleAngle += Data->CircleSpeed * DeltaSeconds;
			const FVector CirclePoint = Target->GetActorLocation() + FVector(FMath::Cos(CircleAngle), FMath::Sin(CircleAngle), 0.0f) * Data->CircleRadius;
			MoveTowards(CirclePoint, FMath::Min(Data->MoveSpeed, FVector::Dist2D(CirclePoint, GetActorLocation()) * 2.5f + 80.0f));
		}

		DropCountdown -= DeltaSeconds;
		if (DropCountdown <= 0.0f)
		{
			DropCountdown = RandomInRange(Data->DropInterval);
			ReleaseDrop();
		}

		if (StateTime >= RainLength)
		{
			State = EVaelMothState::Leaving;
			StateTime = 0.0f;
		}
		break;
	}

	case EVaelMothState::Leaving:
		MoveInDirection(GetActorForwardVector(), Data->MoveSpeed);
		if (StateTime >= MothLeaveTime)
		{
			// Spent, it leaves the land; nothing drops, and it counts for nothing
			Destroy();
		}
		break;
	}
}

void AVaelBloodMoth::ReleaseDrop()
{
	const UVaelBloodMothData* Data = GetData<UVaelBloodMothData>();

	for (FVaelMothDrop& Drop : Drops)
	{
		if (Drop.Age >= 0.0f)
		{
			continue;
		}

		// Somewhere within its rain, evenly over the area
		const float Angle = FMath::FRandRange(0.0f, UE_TWO_PI);
		const float Distance = FMath::Sqrt(FMath::FRand()) * Data->RainRadius;
		FVector Ground = GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Distance;
		if (!FindGround(GetWorld(), Ground, Ground))
		{
			return;
		}

		// It falls off the body, which hangs above the capsule
		Drop.End = Ground;
		Drop.Start = FVector(FMath::Lerp(GetActorLocation().X, Ground.X, 0.55f), FMath::Lerp(GetActorLocation().Y, Ground.Y, 0.55f), GetActorLocation().Z + BodyHeight);
		Drop.Age = 0.0f;
		return;
	}
}

void AVaelBloodMoth::LandDrop(const FVector& Location)
{
	const UVaelBloodMothData* Data = GetData<UVaelBloodMothData>();

	++NumDrops;

	ForEachActivePlayer([this, Data, &Location](AVaelCharacter* Player)
	{
		if (FVector::Dist2D(Player->GetActorLocation(), Location) > Data->DropRadius)
		{
			return;
		}

		if (Data->bHealingRain)
		{
			UVaelCombatStatics::Heal(Player, Data->DropAmount);
		}
		else if (!Player->IsDodging())
		{
			HitPlayer(Player, Data->DropAmount, EVaelElement::Mark);
		}
	});

	if (!IsDead() && FMath::FRand() < Data->BloomChance)
	{
		TryGrowBloom(Location);
	}
}

void AVaelBloodMoth::TryGrowBloom(const FVector& Location)
{
	const UVaelBloodMothData* Data = GetData<UVaelBloodMothData>();
	if (NumBlooms >= Data->MaxBlooms || Data->Blooms.IsEmpty() || AVaelSafeZone::IsSafe(GetWorld(), Location, 0.0f))
	{
		return;
	}

	// Here one, there one: never a thicket
	for (const TWeakObjectPtr<AVaelRainBloom>& Other : Blooms)
	{
		if (Other.IsValid() && FVector::Dist2D(Other->GetActorLocation(), Location) < Data->BloomSpacing)
		{
			return;
		}
	}

	// Weighed by their chances; the first rain of a session grows a harvest flower for certain, so players learn there is something to find
	const FVaelRainBloomKind* Picked = nullptr;
	if (!bFirstHarvestGrown)
	{
		Picked = Data->Blooms.FindByPredicate([](const FVaelRainBloomKind& Bloom) { return Bloom.Kind == EVaelBloomKind::Harvest; });
	}

	if (Picked == nullptr)
	{
		float Total = 0.0f;
		for (const FVaelRainBloomKind& Bloom : Data->Blooms)
		{
			Total += Bloom.Chance;
		}

		float Roll = FMath::FRand() * Total;
		for (const FVaelRainBloomKind& Bloom : Data->Blooms)
		{
			Picked = &Bloom;
			Roll -= Bloom.Chance;
			if (Roll <= 0.0f)
			{
				break;
			}
		}
	}

	if (AVaelRainBloom* Bloom = Picked != nullptr ? AVaelRainBloom::Grow(GetWorld(), Location, *Picked, RandomInRange(Data->BloomLifetime)) : nullptr)
	{
		bFirstHarvestGrown |= Picked->Kind == EVaelBloomKind::Harvest;
		Blooms.Add(Bloom);
		++NumBlooms;

		UE_LOG(LogVael, Verbose, TEXT("'%s': a %s grows under its rain"), *GetNameSafe(this), *Picked->DisplayName.ToString());
	}
}

void AVaelBloodMoth::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const UVaelBloodMothData* Data = GetData<UVaelBloodMothData>();
	const float Time = GetWorld()->GetTimeSeconds();
	const bool bDeadNow = IsDead();

	// The body climbs out of the cocoon, hangs at its height, rises away when spent and drops when shot down
	float TargetHeight = Data->FlightHeight + FMath::Sin(Time * 2.4f + CircleAngle) * 18.0f;
	float HeightSpeed = 2.2f;
	float TargetSize = 1.0f;

	if (State == EVaelMothState::Leaving)
	{
		TargetHeight = Data->FlightHeight + 900.0f;
		HeightSpeed = 0.8f;
		TargetSize = 0.3f;
	}
	if (bDeadNow)
	{
		DeadTime += DeltaSeconds;
		TargetHeight = 20.0f - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		HeightSpeed = 3.0f + DeadTime * 6.0f;
	}

	BodyHeight = FMath::FInterpTo(BodyHeight, TargetHeight, DeltaSeconds, HeightSpeed);
	SizeShown = FMath::FInterpTo(SizeShown, TargetSize, DeltaSeconds, 1.8f);
	GetBody()->SetRelativeLocation(FVector(0.0f, 0.0f, BodyHeight));

	if (GetFactoryMesh() != nullptr)
	{
		PoseFactoryMesh(DeltaSeconds);
	}

	// Drops fall, splash and are gone
	for (int32 Index = 0; Index < Drops.Num(); ++Index)
	{
		FVaelMothDrop& Drop = Drops[Index];
		UStaticMeshComponent* Shape = DropShapes[Index];
		if (Drop.Age < 0.0f)
		{
			continue;
		}

		const float Before = Drop.Age;
		Drop.Age += DeltaSeconds;

		if (Before < Data->DropFallTime && Drop.Age >= Data->DropFallTime)
		{
			LandDrop(Drop.End);
		}

		if (Drop.Age >= Data->DropFallTime + MothSplashTime)
		{
			Drop.Age = -1.0f;
			Shape->SetVisibility(false);
			continue;
		}

		Shape->SetVisibility(true);
		if (Drop.Age < Data->DropFallTime)
		{
			// Faster and faster, drawn out long by its speed
			const float Fallen = FMath::Square(Drop.Age / Data->DropFallTime);
			Shape->SetWorldLocation(FMath::Lerp(Drop.Start, Drop.End, Fallen));
			Shape->SetWorldScale3D(FVector(7.0f, 7.0f, 12.0f + Fallen * 26.0f) / MothShapeSize);
		}
		else
		{
			const float Splash = (Drop.Age - Data->DropFallTime) / MothSplashTime;
			Shape->SetWorldLocation(Drop.End + FVector(0.0f, 0.0f, 2.0f));
			Shape->SetWorldScale3D(FVector(18.0f + Splash * 60.0f, 18.0f + Splash * 60.0f, 4.0f * (1.0f - Splash) + 0.5f) / MothShapeSize);
		}
	}
}

void AVaelBloodMoth::SetupWings()
{
	const USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(GetFactoryMesh()->GetSkinnedAsset());
	if (SkeletalMesh == nullptr)
	{
		return;
	}

	const FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetRefSkeleton();
	const TCHAR* Names[2][2] = { { TEXT("wing_L_upper"), TEXT("wing_L_lower") }, { TEXT("wing_R_upper"), TEXT("wing_R_lower") } };

	bHasWings = true;
	for (int32 Side = 0; Side < 2; ++Side)
	{
		for (int32 Part = 0; Part < 2; ++Part)
		{
			WingBones[Side][Part] = Names[Side][Part];
			const int32 Index = RefSkeleton.FindBoneIndex(WingBones[Side][Part]);
			bHasWings &= Index != INDEX_NONE;
			if (Index != INDEX_NONE)
			{
				WingRest[Side][Part] = FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, Index);
			}
		}
	}

	// The mesh stands upright on its origin: it leans around its middle
	MeshMiddle = SkeletalMesh->GetBounds().Origin.Z;

	if (!bHasWings)
	{
		UE_LOG(LogVael, Warning, TEXT("'%s': the mesh has no wings to pose, it keeps its rest pose"), *GetNameSafe(this));
	}
}

void AVaelBloodMoth::PoseFactoryMesh(float DeltaSeconds)
{
	UPoseableMeshComponent* MothMesh = GetFactoryMesh();
	const UVaelBloodMothData* Data = GetData<UVaelBloodMothData>();
	const bool bDeadNow = IsDead();

	// It hangs upright as it climbs out, lies forward in the air while it circles, and tumbles down dead
	const float Lean = bDeadNow ? 95.0f : State == EVaelMothState::Rising ? 15.0f : MothLean;
	LeanShown = FMath::FInterpTo(LeanShown, Lean, DeltaSeconds, 3.0f);

	const float Roll = bDeadNow ? FMath::Min(DeadTime * 240.0f, 160.0f) : FMath::Sin(FlapPhase * 0.7f) * 6.0f;
	const FQuat Tilt = FRotator(-LeanShown, 0.0f, Roll).Quaternion();

	MothMesh->SetRelativeScale3D(FVector(SizeShown));
	MothMesh->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, BodyHeight) - Tilt.RotateVector(FVector(0.0f, 0.0f, MeshMiddle * SizeShown)),
		Tilt * FRotator(0.0f, Data->MeshYaw, 0.0f).Quaternion());

	if (!bHasWings)
	{
		return;
	}

	// The mesh looks along +Y and its wings lie in the plane across it: a beat swings them towards its back and its front.
	// The outer half of a wing follows a little later.
	FlapPhase += Data->FlapsPerSecond * (State == EVaelMothState::Rising ? 1.8f : 1.0f) * DeltaSeconds;
	const float Beat = bDeadNow ? 0.6f : FMath::Sin(FlapPhase * UE_TWO_PI);
	const float OuterBeat = bDeadNow ? 0.3f : FMath::Sin(FlapPhase * UE_TWO_PI - 0.8f);
	const FVector Back(0.0f, -1.0f, 0.0f);

	for (int32 Side = 0; Side < 2; ++Side)
	{
		const FTransform& RestInner = WingRest[Side][0];
		const FTransform& RestOuter = WingRest[Side][1];
		const FVector Along = (RestOuter.GetLocation() - RestInner.GetLocation()).GetSafeNormal();
		const FVector Axis = FVector::CrossProduct(Along, Back).GetSafeNormal();

		// Held a little up, like a moth at rest in the air
		const FQuat Turn(Axis, FMath::DegreesToRadians(12.0f + Beat * Data->FlapAngle));
		const FQuat Bend(Axis, FMath::DegreesToRadians(OuterBeat * Data->FlapAngle * 0.45f));

		FTransform Inner = RestInner;
		Inner.SetRotation(Turn * RestInner.GetRotation());
		MothMesh->SetBoneTransformByName(WingBones[Side][0], Inner, EBoneSpaces::ComponentSpace);

		FTransform Outer = RestOuter;
		Outer.SetLocation(RestInner.GetLocation() + Turn.RotateVector(RestOuter.GetLocation() - RestInner.GetLocation()));
		Outer.SetRotation(Turn * Bend * RestOuter.GetRotation());
		MothMesh->SetBoneTransformByName(WingBones[Side][1], Outer, EBoneSpaces::ComponentSpace);
	}
}

#undef LOCTEXT_NAMESPACE
