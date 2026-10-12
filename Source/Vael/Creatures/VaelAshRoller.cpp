// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelAshRoller.h"
#include "Combat/VaelCombatStatics.h"
#include "Combat/VaelHitFeedbackSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Creatures/VaelCreatureData.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Magic/VaelDebrisBurst.h"
#include "Magic/VaelSpellEffects.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/VaelCharacter.h"
#include "UI/VaelCombatTextSubsystem.h"
#include "Vael.h"

#define LOCTEXT_NAMESPACE "VaelCreatures"

namespace
{
	/** The roller stops walking home closer than this, in cm */
	constexpr float RollerHomeTolerance = 140.0f;

	/** Extra reach of a roll beyond the touching capsules, in cm */
	constexpr float RollerReach = 15.0f;

	/** Diameter of the engine sphere, edge of the engine cube and height of the engine cone in cm */
	constexpr float RollerShapeSize = 100.0f;

	/** The walking body is this much longer, and flattened to this share of its height */
	constexpr float RollerWalkLength = 1.3f;
	constexpr float RollerWalkHeight = 0.72f;

	/** How far it stays curled while it lies on its back */
	constexpr float RollerOnBackCurl = 0.3f;

	/** Curling and turning over per second */
	constexpr float RollerCurlRate = 4.0f;
	constexpr float RollerFlipRate = 5.0f;

	/** Seconds between two puffs of ash under a rolling roller */
	constexpr float RollerDustInterval = 0.07f;

	/** Surfaces steeper than this stop a roll: the up part of their normal is smaller */
	constexpr float RollerWallNormalZ = 0.5f;

	/** Bands of the shell, belly, ash and lane */
	const FLinearColor RollerBandColor(0.4f, 0.34f, 0.28f);
	const FLinearColor RollerBellyColor(0.62f, 0.5f, 0.4f);
	const FLinearColor RollerAshColor(0.2f, 0.17f, 0.15f);
	const FLinearColor RollerLaneColor(1.0f, 0.5f, 0.15f);
	const FLinearColor RollerFlashColor(1.0f, 0.9f, 0.75f);
}

AVaelAshRoller::AVaelAshRoller()
{
}

TSubclassOf<UVaelCreatureData> AVaelAshRoller::GetDefaultDataClass() const
{
	return UVaelAshRollerData::StaticClass();
}

void AVaelAshRoller::BeginPlay()
{
	Super::BeginPlay();

	RollCooldown = RandomInRange(GetData<UVaelAshRollerData>()->FirstRollDelay);

	BuildRig();
}

float AVaelAshRoller::GetIncomingDamageMultiplier(const FGameplayTagContainer& DamageTags) const
{
	const UVaelAshRollerData* Data = GetData<UVaelAshRollerData>();
	const float Multiplier = Super::GetIncomingDamageMultiplier(DamageTags);

	switch (State)
	{
	case EVaelRollerState::Curl:
	case EVaelRollerState::Roll:
		return Multiplier * Data->CurledDamageMultiplier;
	case EVaelRollerState::OnBack:
		return Multiplier * Data->OnBackDamageMultiplier;
	default:
		return Multiplier;
	}
}

void AVaelAshRoller::EnterState(EVaelRollerState NewState)
{
	State = NewState;
	StateTime = 0.0f;

	// A rolling roller runs through players and creatures instead of stopping at them
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, State == EVaelRollerState::Roll ? ECR_Ignore : ECR_Block);

	if (State == EVaelRollerState::Roll)
	{
		++RollCount;
	}
}

void AVaelAshRoller::StartRoll(const FVector& Direction)
{
	if (IsDead())
	{
		return;
	}

	RollDirection = Direction.GetSafeNormal2D();
	if (RollDirection.IsNearlyZero())
	{
		RollDirection = GetActorForwardVector().GetSafeNormal2D();
	}

	LaneStart = GetActorLocation();
	RollHits.Reset();

	SetActorRotation(FRotator(0.0f, RollDirection.Rotation().Yaw, 0.0f));
	EnterState(EVaelRollerState::Curl);
}

void AVaelAshRoller::TurnTowards(const FVector& Location, float DeltaSeconds)
{
	const FVector Direction = (Location - GetActorLocation()).GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		return;
	}

	const float Yaw = FMath::FixedTurn(GetActorRotation().Yaw, Direction.Rotation().Yaw, GetData<UVaelAshRollerData>()->TurnSpeed * DeltaSeconds);
	SetActorRotation(FRotator(0.0f, Yaw, 0.0f));
}

void AVaelAshRoller::TickBehavior(float DeltaSeconds)
{
	const UVaelAshRollerData* Data = GetData<UVaelAshRollerData>();

	StateTime += DeltaSeconds;

	switch (State)
	{
	case EVaelRollerState::Roam:
	{
		float TargetDistance = 0.0f;
		AActor* Target = FindTarget(Data->AggroRange, &TargetDistance);

		if (Target == nullptr)
		{
			if (FVector::Dist2D(GetActorLocation(), HomeLocation) > RollerHomeTolerance)
			{
				MoveTowards(HomeLocation, Data->MoveSpeed * Data->ReturnSpeedScale);
			}
			break;
		}

		if (TargetDistance > Data->PreferredDistance)
		{
			MoveTowards(Target->GetActorLocation(), Data->MoveSpeed);
		}
		else
		{
			TurnTowards(Target->GetActorLocation(), DeltaSeconds);
		}

		RollCooldown -= DeltaSeconds;
		if (RollCooldown <= 0.0f && TargetDistance < Data->RollRange && !IsBlinded())
		{
			StartRoll(Target->GetActorLocation() - GetActorLocation());
		}
		break;
	}

	case EVaelRollerState::Curl:
		if (StateTime >= Data->CurlTime)
		{
			EnterState(EVaelRollerState::Roll);
		}
		break;

	case EVaelRollerState::Roll:
		TickRoll(DeltaSeconds);
		break;

	case EVaelRollerState::Uncurl:
		if (StateTime >= Data->UncurlTime)
		{
			RollCooldown = RandomInRange(Data->RollInterval);
			EnterState(EVaelRollerState::Roam);
		}
		break;

	case EVaelRollerState::OnBack:
		if (StateTime >= Data->OnBackTime)
		{
			EnterState(EVaelRollerState::Uncurl);
		}
		break;
	}
}

void AVaelAshRoller::TickRoll(float DeltaSeconds)
{
	const UVaelAshRollerData* Data = GetData<UVaelAshRollerData>();

	if (IsBlockedAhead(Data->RollSpeed * DeltaSeconds))
	{
		Crash();
		return;
	}

	if (StateTime >= Data->RollLength / Data->RollSpeed)
	{
		EnterState(EVaelRollerState::Uncurl);
		return;
	}

	// Full speed from the first moment of the roll
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (Movement->Velocity.Size2D() < Data->RollSpeed * 0.5f)
	{
		Movement->Velocity = RollDirection * Data->RollSpeed;
	}

	MoveInDirection(RollDirection, Data->RollSpeed);

	const float RollerRadius = GetCapsuleComponent()->GetScaledCapsuleRadius();
	ForEachActivePlayer([this, Data, RollerRadius](AVaelCharacter* Player)
	{
		if (RollHits.Contains(Player) || GetDistanceTo2D(Player) > RollerRadius + Player->GetCapsuleComponent()->GetScaledCapsuleRadius() + RollerReach)
		{
			return;
		}

		RollHits.Add(Player);

		// Thrown out of the lane to the side they stand on
		const FVector Offset = (Player->GetActorLocation() - GetActorLocation()) * FVector(1.0f, 1.0f, 0.0f);
		const FVector Side = FVector::CrossProduct(FVector::UpVector, RollDirection);
		const FVector Aside = Side * (FVector::DotProduct(Offset, Side) < 0.0f ? -1.0f : 1.0f);

		FVaelSpellHit Hit;
		Hit.Damage = Data->RollDamage;
		Hit.Element = EVaelElement::Earth;
		Hit.Knockback = Data->RollKnockback;

		UVaelCombatStatics::ApplySpellHit(this, Player, Hit, (Aside + RollDirection * 0.5f).GetSafeNormal2D());
	});
}

bool AVaelAshRoller::IsBlockedAhead(float Distance) const
{
	const float CapsuleRadius = GetCapsuleComponent()->GetScaledCapsuleRadius();

	// A ball smaller than the capsule, so the ground it rolls on doesn't count
	const FVector Start = GetActorLocation();
	const FVector End = Start + RollDirection * (CapsuleRadius * 0.4f + Distance);

	FCollisionObjectQueryParams ObjectTypes;
	ObjectTypes.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectTypes.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VaelRollerCrash), false, this);

	TArray<FHitResult> Hits;
	GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, ObjectTypes, FCollisionShape::MakeSphere(CapsuleRadius * 0.7f), QueryParams);

	for (const FHitResult& Hit : Hits)
	{
		const UPrimitiveComponent* Component = Hit.GetComponent();

		// Only things that would stop a walking creature, met from the side
		if (Component != nullptr && !Hit.bStartPenetrating && Component->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block
			&& FMath::Abs(Hit.ImpactNormal.Z) < RollerWallNormalZ && Cast<APawn>(Hit.GetActor()) == nullptr)
		{
			return true;
		}
	}

	return false;
}

void AVaelAshRoller::Crash()
{
	UE_LOG(LogVael, Verbose, TEXT("'%s' rolls into something solid and lands on its back"), *GetNameSafe(this));

	EnterState(EVaelRollerState::OnBack);

	// Bounces back a little
	GetCharacterMovement()->Velocity = -RollDirection * 250.0f;

	const float CapsuleRadius = GetCapsuleComponent()->GetScaledCapsuleRadius();

	FVaelDebris Debris;
	Debris.Count = 14;
	Debris.Speed = 420.0f;
	Debris.Lift = 380.0f;
	Debris.Color = RollerAshColor;
	Debris.RockShare = 0.3f;
	AVaelDebrisBurst::Spawn(this, GetActorLocation() + RollDirection * CapsuleRadius, Debris);

	UVaelHitFeedbackSubsystem::Shake(this, 0.35f);
	UVaelCombatTextSubsystem::PostReaction(this, LOCTEXT("RollerOnBack", "Umgeworfen!"));
}

void AVaelAshRoller::Die()
{
	Super::Die();

	if (Lane != nullptr)
	{
		Lane->SetVisibility(false);
		LaneFill->SetVisibility(false);
	}
}

UStaticMeshComponent* AVaelAshRoller::AddRigPart(const FLinearColor& Color, const TCHAR* MeshPath)
{
	UMaterialInterface* CoreMaterial = nullptr;
	UMaterialInterface* GlowMaterial = nullptr;
	VaelEffects::LoadLookMaterials(CoreMaterial, GlowMaterial);

	UStaticMeshComponent* Part = VaelEffects::AddLookShape(this, CoreMaterial, Color, 0.0f);
	Part->AttachToComponent(Rig, FAttachmentTransformRules::KeepRelativeTransform);
	Part->SetCastShadow(true);
	Part->SetVisibility(true);

	if (MeshPath != nullptr)
	{
		Part->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, MeshPath));
	}

	return Part;
}

void AVaelAshRoller::BuildRig()
{
	const float Radius = GetCapsuleComponent()->GetUnscaledCapsuleRadius();
	const TCHAR* ConePath = TEXT("/Engine/BasicShapes/Cone.Cone");

	// The placeholder body gives way to the rig; its colors still arrive through OnBodyColorShown
	GetBody()->SetVisibility(false);

	Rig = NewObject<USceneComponent>(this, TEXT("Rig"));
	Rig->SetupAttachment(GetRootComponent());
	Rig->RegisterComponent();

	Shell = AddRigPart(ShellColor);
	Shell->SetRelativeScale3D(FVector(Radius * 2.0f / RollerShapeSize));

	// Bands of crust around the body, front to back; they show how the ball turns
	for (const float Along : { -0.66f, -0.34f, 0.0f, 0.34f, 0.66f })
	{
		const float BandDiameter = 2.0f * Radius * FMath::Sqrt(1.0f - Along * Along) + 8.0f;

		UStaticMeshComponent* Band = AddRigPart(RollerBandColor);
		Band->SetRelativeLocation(FVector(Along * Radius, 0.0f, 0.0f));
		Band->SetRelativeScale3D(FVector(Radius * 0.2f, BandDiameter, BandDiameter) / RollerShapeSize);
		Bands.Add(Band);
	}

	// The soft belly, out of sight until it lies on its back
	Belly = AddRigPart(RollerBellyColor);
	Belly->SetRelativeLocation(FVector(0.0f, 0.0f, -Radius * 0.45f));
	Belly->SetRelativeScale3D(FVector(Radius * 1.5f, Radius * 1.4f, Radius * 1.2f) / RollerShapeSize);

	Head = AddRigPart(ShellColor);
	Head->SetRelativeLocation(FVector(Radius * 0.95f, 0.0f, -Radius * 0.35f));

	Snout = AddRigPart(ShellColor, ConePath);
	Snout->SetRelativeLocation(FVector(Radius * 1.25f, 0.0f, -Radius * 0.42f));
	Snout->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));

	Tail = AddRigPart(ShellColor, ConePath);
	Tail->SetRelativeLocation(FVector(-Radius * 1.15f, 0.0f, -Radius * 0.5f));
	Tail->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));

	for (int32 FootIndex = 0; FootIndex < 4; ++FootIndex)
	{
		Feet.Add(AddRigPart(ShellColor));
	}

	// The lane lies in the world, not on the roller
	UMaterialInterface* CoreMaterial = nullptr;
	UMaterialInterface* GlowMaterial = nullptr;
	VaelEffects::LoadLookMaterials(CoreMaterial, GlowMaterial);

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	for (TObjectPtr<UStaticMeshComponent>* Strip : { &Lane, &LaneFill })
	{
		*Strip = VaelEffects::AddLookShape(this, GlowMaterial, RollerLaneColor, Strip == &Lane ? 1.2f : 4.0f);
		(*Strip)->SetStaticMesh(Cube);
		(*Strip)->SetUsingAbsoluteLocation(true);
		(*Strip)->SetUsingAbsoluteRotation(true);
		(*Strip)->SetUsingAbsoluteScale(true);
	}

	RefreshBodyColor();
}

void AVaelAshRoller::OnBodyColorShown(const FLinearColor& Color, bool bHitFlash)
{
	ShellColor = Color;
	bFlashing = bHitFlash;

	if (Rig == nullptr)
	{
		return;
	}

	const auto Paint = [bHitFlash](UStaticMeshComponent* Part, const FLinearColor& PartColor)
	{
		if (UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0)))
		{
			Material->SetVectorParameterValue(VaelEffects::LookColorParameter, bHitFlash ? RollerFlashColor : PartColor);
		}
	};

	// The bands and the belly keep their own color and take a little of the state, like the Mark
	const FLinearColor Tint = Color - GetData<UVaelAshRollerData>()->BodyColor;

	Paint(Shell, Color);
	Paint(Head, Color * 1.3f);
	Paint(Snout, Color * 1.3f);
	Paint(Tail, Color * 1.3f);
	Paint(Belly, RollerBellyColor + Tint);

	for (UStaticMeshComponent* Band : Bands)
	{
		Paint(Band, RollerBandColor + Tint);
	}

	for (UStaticMeshComponent* Foot : Feet)
	{
		Paint(Foot, Color * 1.3f);
	}
}

void AVaelAshRoller::UpdateLane()
{
	const UVaelAshRollerData* Data = GetData<UVaelAshRollerData>();
	const bool bShow = State == EVaelRollerState::Curl && !IsDead();

	Lane->SetVisibility(bShow);
	LaneFill->SetVisibility(bShow);

	if (!bShow)
	{
		return;
	}

	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const FVector LaneGround = LaneStart - FVector(0.0f, 0.0f, Capsule->GetScaledCapsuleHalfHeight() - 3.0f);
	const FRotator Rotation(0.0f, RollDirection.Rotation().Yaw, 0.0f);
	const float Width = Capsule->GetScaledCapsuleRadius() * 2.0f;
	const float FillLength = Data->RollLength * FMath::Clamp(StateTime / Data->CurlTime, 0.02f, 1.0f);

	Lane->SetWorldLocationAndRotation(LaneGround + RollDirection * Data->RollLength * 0.5f, Rotation);
	Lane->SetWorldScale3D(FVector(Data->RollLength, Width, 2.0f) / RollerShapeSize);

	LaneFill->SetWorldLocationAndRotation(LaneGround + RollDirection * FillLength * 0.5f + FVector(0.0f, 0.0f, 1.0f), Rotation);
	LaneFill->SetWorldScale3D(FVector(FillLength, Width * 0.8f, 2.0f) / RollerShapeSize);
}

void AVaelAshRoller::Tick(float DeltaSeconds)
{
	const FVector Before = GetActorLocation();

	Super::Tick(DeltaSeconds);

	if (Rig == nullptr)
	{
		return;
	}

	UpdateLane();

	const float Radius = GetCapsuleComponent()->GetUnscaledCapsuleRadius();

	if (IsDead())
	{
		// Placeholder for a death animation: the shell slumps
		Rig->SetRelativeScale3D(FVector(1.2f, 1.2f, 0.3f));
		Rig->SetRelativeLocation(FVector(0.0f, 0.0f, -Radius * 0.7f));
		return;
	}

	const bool bRolling = State == EVaelRollerState::Roll;
	const bool bOnBack = State == EVaelRollerState::OnBack;
	const float Moved = FVector::Dist2D(Before, GetActorLocation());

	// The ball turns with the way it rolls; the feet swing with the way it walks
	if (bRolling)
	{
		SpinDegrees = FMath::Fmod(SpinDegrees + FMath::RadiansToDegrees(Moved / Radius), 360.0f);

		if (FMath::Fmod(StateTime, RollerDustInterval) < DeltaSeconds)
		{
			FVaelDebris Dust;
			Dust.Count = 3;
			Dust.Speed = 120.0f;
			Dust.Lift = 140.0f;
			Dust.Gravity = 120.0f;
			Dust.Lifetime = 0.5f;
			Dust.MinSize = 14.0f;
			Dust.MaxSize = 30.0f;
			Dust.Color = RollerAshColor;
			Dust.bSoft = true;
			AVaelDebrisBurst::Spawn(this, GetActorLocation() - FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), Dust);
		}
	}
	else
	{
		SpinDegrees = FMath::FInterpTo(SpinDegrees, SpinDegrees > 180.0f ? 360.0f : 0.0f, DeltaSeconds, 10.0f);
		WalkedDistance += Moved;
	}

	const float CurlTarget = State == EVaelRollerState::Curl || bRolling ? 1.0f : bOnBack ? RollerOnBackCurl : 0.0f;
	CurlShown = FMath::FInterpConstantTo(CurlShown, CurlTarget, DeltaSeconds, RollerCurlRate);
	FlipShown = FMath::FInterpConstantTo(FlipShown, bOnBack ? 1.0f : 0.0f, DeltaSeconds, RollerFlipRate);

	// Long and flat while walking, a ball while curled; on its back it rocks
	const float Time = GetWorld()->GetTimeSeconds();
	const float HeightScale = FMath::Lerp(RollerWalkHeight, 1.0f, CurlShown);
	const float Rock = bOnBack ? FMath::Sin(Time * 5.0f) * 7.0f * FMath::Max(1.0f - StateTime * 0.4f, 0.25f) : 0.0f;
	const float Shiver = State == EVaelRollerState::Curl ? FMath::Sin(Time * 45.0f) * 2.0f : 0.0f;

	Rig->SetRelativeScale3D(FVector(FMath::Lerp(RollerWalkLength, 1.0f, CurlShown), 1.0f, HeightScale));
	Rig->SetRelativeLocation(FVector(Shiver, 0.0f, -Radius * (1.0f - HeightScale)));
	Rig->SetRelativeRotation(FRotator(-SpinDegrees, 0.0f, FlipShown * 180.0f + Rock));

	// Head, tail and feet vanish into the ball
	const float Out = 1.0f - CurlShown;
	const bool bPartsOut = Out > 0.02f;

	Head->SetVisibility(bPartsOut);
	Snout->SetVisibility(bPartsOut);
	Tail->SetVisibility(bPartsOut);

	Head->SetRelativeScale3D(FVector(Radius * 0.55f * Out / RollerShapeSize));
	Snout->SetRelativeScale3D(FVector(Radius * 0.25f, Radius * 0.25f, Radius * 0.5f) * Out / RollerShapeSize);
	Tail->SetRelativeScale3D(FVector(Radius * 0.2f, Radius * 0.2f, Radius * 0.7f) * Out / RollerShapeSize);

	for (int32 FootIndex = 0; FootIndex < Feet.Num(); ++FootIndex)
	{
		const float Front = FootIndex < 2 ? 1.0f : -1.0f;
		const float Side = FootIndex % 2 == 0 ? 1.0f : -1.0f;

		// Diagonal feet step together; on its back they paddle in the air
		const float Phase = Front * Side > 0.0f ? 0.0f : UE_PI;
		const float Swing = bOnBack ? FMath::Sin(Time * 9.0f + FootIndex * 1.7f) : FMath::Sin(WalkedDistance / 14.0f + Phase);

		UStaticMeshComponent* Foot = Feet[FootIndex];
		Foot->SetVisibility(bPartsOut);
		Foot->SetRelativeLocation(FVector(Front * Radius * 0.5f + Swing * Radius * 0.14f, Side * Radius * 0.55f, -Radius * (0.92f + (bOnBack ? 0.12f : 0.0f))));
		Foot->SetRelativeScale3D(FVector(Radius * 0.3f * Out / RollerShapeSize));
	}
}

#undef LOCTEXT_NAMESPACE
