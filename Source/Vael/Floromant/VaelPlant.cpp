// Copyright Epic Games, Inc. All Rights Reserved.

#include "Floromant/VaelPlant.h"
#include "AbilitySystemComponent.h"
#include "Combat/VaelCombatStatics.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Creatures/VaelCreature.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Magic/VaelDebrisBurst.h"
#include "Magic/VaelGameplayTags.h"
#include "Magic/VaelSpellEffects.h"
#include "Magic/VaelSpellProjectile.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Player/VaelCharacter.h"
#include "ProceduralMeshComponent.h"
#include "Vael.h"
#include "VaelAssets.h"

namespace
{
	/** Half height of the capsule every plant stands in */
	constexpr float PlantCapsuleHalfHeight = 50.0f;

	/** Seconds a plant needs to wither */
	constexpr float WitherDuration = 0.7f;

	/** The plant grows in a little higher than its model, so the soft edge of the mask clears the top */
	constexpr float GrowMaskOvershoot = 1.15f;

	/** Size of the model at the very start of the growth, before it pops up to its full size */
	constexpr float GrowStartScale = 0.3f;

	/** Grow height the material gets once the plant has grown: far above any plant */
	constexpr float FullyGrownHeight = 100000.0f;

	/** How fast squash and lean ease back, per second */
	constexpr float SquashRecovery = 7.0f;

	/** Degrees per second a thistle turns towards its enemy */
	constexpr float ThistleTurnSpeed = 240.0f;

	/** Rings along each root of the wall and corners around each ring */
	constexpr int32 RootRings = 14;
	constexpr int32 RootSides = 6;

	/** Material parameters of M_Floromant_Pflanze */
	const FName GroundZParameter(TEXT("GroundZ"));
	const FName PlantHeightParameter(TEXT("PlantHeight"));
	const FName GrowHeightParameter(TEXT("GrowHeight"));
	const FName GrowColorParameter(TEXT("GrowColor"));
	const FName FlashParameter(TEXT("Flash"));
	const FName FlashColorParameter(TEXT("FlashColor"));

	/** Ease that swings a little beyond 1 before it settles: the pop of a plant shooting up */
	float EaseOutBack(float X)
	{
		constexpr float C1 = 1.70158f;
		constexpr float C3 = C1 + 1.0f;
		const float T = X - 1.0f;
		return 1.0f + C3 * T * T * T + C1 * T * T;
	}
}

AVaelPlant::AVaelPlant()
{
	GetCapsuleComponent()->InitCapsuleSize(30.0f, PlantCapsuleHalfHeight);

	// Plants are found by spells like any pawn, but nobody bumps into them: walking through the leaves is fine
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCapsuleComponent()->CanCharacterStepUpOn = ECB_No;

	// Rooted: no controller, no movement
	AutoPossessAI = EAutoPossessAI::Disabled;
	GetCharacterMovement()->GravityScale = 0.0f;
	GetCharacterMovement()->bRunPhysicsWithNoController = false;

	GetMesh()->SetVisibility(false);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(RootComponent);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->bReceivesDecals = false;

	StartingMana = 0.0f;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

AVaelPlant* AVaelPlant::GrowPlant(EVaelSeedKind Kind, AVaelCharacter* Gardener, const FVector& GroundLocation, float Yaw)
{
	UWorld* World = Gardener != nullptr ? Gardener->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}

	TSubclassOf<AVaelPlant> PlantClass;
	switch (Kind)
	{
	case EVaelSeedKind::RootWall:	PlantClass = AVaelRootWallPlant::StaticClass(); break;
	case EVaelSeedKind::Chalice:	PlantClass = AVaelChalicePlant::StaticClass(); break;
	default:						PlantClass = AVaelThistlePlant::StaticClass(); break;
	}

	const FTransform SpawnTransform(FRotator(0.0f, Yaw, 0.0f), GroundLocation + FVector(0.0f, 0.0f, PlantCapsuleHalfHeight));
	AVaelPlant* Plant = World->SpawnActorDeferred<AVaelPlant>(PlantClass, SpawnTransform, Gardener, Gardener, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Plant == nullptr)
	{
		return nullptr;
	}

	Plant->Kind = Kind;
	Plant->Gardener = Gardener;
	Plant->GroundZ = GroundLocation.Z;
	Plant->StartingHealth = UVaelFloromantSettings::Get()->GetPlant(Kind).Health;
	Plant->FinishSpawning(SpawnTransform);

	UE_LOG(LogVael, Log, TEXT("'%s' grows %s"), *GetNameSafe(Gardener), *Plant->GetLook().DisplayName.ToString());
	return Plant;
}

const FVaelPlantLook& AVaelPlant::GetLook() const
{
	return UVaelFloromantSettings::Get()->GetPlant(Kind);
}

void AVaelPlant::BeginPlay()
{
	Super::BeginPlay();

	GetCharacterMovement()->DisableMovement();
	GetCharacterMovement()->SetComponentTickEnabled(false);

	BuildLook();
	UpdateLook(0.0f);

	// Earth sprays up where it breaks out of the ground, with a few sparks in its colour
	const FVector Ground(GetActorLocation().X, GetActorLocation().Y, GroundZ);

	FVaelDebris Earth;
	Earth.Count = 16;
	Earth.Spread = 25.0f;
	Earth.Speed = 240.0f;
	Earth.Lift = 380.0f;
	Earth.Lifetime = 0.7f;
	Earth.MinSize = 4.0f;
	Earth.MaxSize = 10.0f;
	Earth.Color = FLinearColor(0.11f, 0.075f, 0.05f);
	Earth.RockShare = 0.15f;
	AVaelDebrisBurst::Spawn(this, Ground, Earth);

	FVaelDebris Sparks;
	Sparks.Count = 10;
	Sparks.Spread = 30.0f;
	Sparks.Speed = 60.0f;
	Sparks.Lift = 160.0f;
	Sparks.Gravity = -120.0f;
	Sparks.Drag = 1.2f;
	Sparks.Lifetime = 1.0f;
	Sparks.MinSize = 3.0f;
	Sparks.MaxSize = 6.0f;
	Sparks.Color = GetLook().Color;
	Sparks.Glow = 3.0f;
	Sparks.bSoft = true;
	Sparks.bLandOnGround = false;
	AVaelDebrisBurst::Spawn(this, Ground + FVector(0.0f, 0.0f, 20.0f), Sparks);
}

void AVaelPlant::BuildLook()
{
	const FVaelPlantLook& Look = GetLook();

	UStaticMesh* PlantMesh = VaelAssets::LoadOptional(Look.Mesh);
	if (PlantMesh == nullptr)
	{
		// Placeholder while the model is missing: a stretched sphere
		PlantMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	}

	Body->SetStaticMesh(PlantMesh);
	GrowingPart = Body;

	// Scaled to its height with a little variation, standing with its lowest point on the ground, turned at random
	const FBox Bounds = PlantMesh != nullptr ? PlantMesh->GetBoundingBox() : FBox(FVector(-50.0f), FVector(50.0f));
	const float Scale = Look.Height / FMath::Max(Bounds.GetSize().Z, 1.0f) * FMath::FRandRange(0.92f, 1.08f);
	GrownScale = FVector(Scale);

	const FVector Center = Bounds.GetCenter();
	Body->SetRelativeLocation(FVector(-Center.X * Scale, -Center.Y * Scale, GroundZ - GetActorLocation().Z - Bounds.Min.Z * Scale));
	Body->SetRelativeRotation(FRotator(FMath::FRandRange(-4.0f, 4.0f), FMath::FRandRange(0.0f, 360.0f), FMath::FRandRange(-4.0f, 4.0f)));

	UseMaterial(Body, VaelAssets::LoadOptional(Look.Material));
}

void AVaelPlant::UseMaterial(UPrimitiveComponent* Part, UMaterialInterface* Material)
{
	if (Part == nullptr || Material == nullptr)
	{
		return;
	}

	const FVaelPlantLook& Look = GetLook();
	UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Material, this);
	Instance->SetScalarParameterValue(GroundZParameter, GroundZ);
	Instance->SetScalarParameterValue(PlantHeightParameter, Look.Height);
	Instance->SetVectorParameterValue(GrowColorParameter, Look.Color);
	Instance->SetVectorParameterValue(FlashColorParameter, Look.Color);

	for (int32 MaterialIndex = 0; MaterialIndex < FMath::Max(Part->GetNumMaterials(), 1); ++MaterialIndex)
	{
		Part->SetMaterial(MaterialIndex, Instance);
	}
	Materials.Add(Instance);
}

void AVaelPlant::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;
	UpdateLook(DeltaSeconds);

	if (bWithering)
	{
		WitherTime += DeltaSeconds;
		if (WitherTime >= WitherDuration)
		{
			Destroy();
		}
		return;
	}

	if (Age >= UVaelFloromantSettings::Get()->GrowTime)
	{
		TickGrown(DeltaSeconds);
	}

	if (Age >= GetLook().Lifetime)
	{
		Wither();
	}
}

void AVaelPlant::UpdateLook(float DeltaSeconds)
{
	const FVaelPlantLook& Look = GetLook();
	const float Growth = FMath::Clamp(Age / UVaelFloromantSettings::Get()->GrowTime, 0.0f, 1.0f);
	const float Wilt = bWithering ? FMath::Clamp(WitherTime / WitherDuration, 0.0f, 1.0f) : 0.0f;

	// The mask fades the plant in from the bottom up while it grows and takes it back from the top when it withers
	float GrowHeight = Growth < 1.0f ? FMath::SmoothStep(0.0f, 1.0f, Growth) * Look.Height * GrowMaskOvershoot : FullyGrownHeight;
	if (bWithering)
	{
		GrowHeight = (1.0f - Wilt) * Look.Height * GrowMaskOvershoot;
	}

	FlashStrength = FMath::Max(0.0f, FlashStrength - DeltaSeconds * FlashFade);
	for (UMaterialInstanceDynamic* Material : Materials)
	{
		Material->SetScalarParameterValue(GrowHeightParameter, GrowHeight);
		Material->SetScalarParameterValue(FlashParameter, FlashStrength);
	}

	// Size: pops up while it grows, swinging a little beyond its size; squashes and leans for its actions; sinks when it withers
	SquashAmount = FMath::FInterpTo(SquashAmount, 1.0f, DeltaSeconds, SquashRecovery);
	LeanAmount = FMath::FInterpTo(LeanAmount, 0.0f, DeltaSeconds, SquashRecovery);

	if (GrowingPart != nullptr)
	{
		const float Pop = Growth < 1.0f ? FMath::Lerp(GrowStartScale, 1.0f, EaseOutBack(Growth)) : 1.0f;
		const float Sink = 1.0f - 0.35f * Wilt;
		const float Wide = 1.0f / FMath::Sqrt(FMath::Max(SquashAmount, 0.1f));
		GrowingPart->SetRelativeScale3D(GrownScale * Pop * Sink * FVector(Wide, Wide, SquashAmount));

		FRotator Rotation = GrowingPart->GetRelativeRotation();
		Rotation.Pitch = -LeanAmount;
		GrowingPart->SetRelativeRotation(Rotation);
	}
}

void AVaelPlant::Flash(float Seconds, float Strength)
{
	FlashStrength = FMath::Max(FlashStrength, Strength);
	FlashFade = Strength / FMath::Max(Seconds, 0.05f);
}

void AVaelPlant::Squash(float Amount, float LeanDegrees)
{
	SquashAmount = Amount;
	LeanAmount = LeanDegrees;
}

void AVaelPlant::Wither()
{
	if (bWithering)
	{
		return;
	}

	bWithering = true;
	WitherTime = 0.0f;
	OnWither();

	// Dry leaves fall off
	FVaelDebris Leaves;
	Leaves.Count = 12;
	Leaves.Spread = 30.0f;
	Leaves.Speed = 90.0f;
	Leaves.Lift = 120.0f;
	Leaves.Gravity = 300.0f;
	Leaves.Drag = 2.5f;
	Leaves.Lifetime = 1.1f;
	Leaves.MinSize = 3.0f;
	Leaves.MaxSize = 7.0f;
	Leaves.Color = FLinearColor(0.16f, 0.1f, 0.05f);
	AVaelDebrisBurst::Spawn(this, FVector(GetActorLocation().X, GetActorLocation().Y, GroundZ + GetLook().Height * 0.6f), Leaves);
}

void AVaelPlant::OnHealthChanged(float OldValue, float NewValue)
{
	Super::OnHealthChanged(OldValue, NewValue);

	if (NewValue >= OldValue || bWithering)
	{
		return;
	}

	if (NewValue <= 0.0f)
	{
		UE_LOG(LogVael, Log, TEXT("%s of '%s' burns down"), *GetLook().DisplayName.ToString(), *GetNameSafe(Gardener.Get()));
		Wither();
		return;
	}

	Squash(0.9f);
}

float AVaelPlant::GetIncomingDamageMultiplier(const FGameplayTagContainer& DamageTags) const
{
	// Only the fire of enemies harms a plant; other players can't hurt it anyway
	return DamageTags.HasTag(VaelTags::GetElementTag(EVaelElement::Fire)) ? 1.0f : 0.0f;
}

AVaelCreature* AVaelPlant::FindEnemy(float Range) const
{
	AVaelCreature* Nearest = nullptr;
	float NearestDistance = Range;

	for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
	{
		const float Distance = FVector::Dist2D(It->GetActorLocation(), GetActorLocation());
		if (!It->IsDead() && !It->IsOnPlayerSide() && Distance < NearestDistance)
		{
			Nearest = *It;
			NearestDistance = Distance;
		}
	}

	return Nearest;
}

// ---------------------------------------------------------------- Glutdistel

void AVaelThistlePlant::TickGrown(float DeltaSeconds)
{
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	ShotCountdown -= DeltaSeconds;

	AActor* Target = bWindingUp ? WindupTarget.Get() : FindEnemy(Settings->ThistleRange);

	// Turns towards its enemy
	if (Target != nullptr)
	{
		const float TargetYaw = (Target->GetActorLocation() - GetActorLocation()).Rotation().Yaw;
		SetActorRotation(FRotator(0.0f, FMath::FixedTurn(GetActorRotation().Yaw, TargetYaw, ThistleTurnSpeed * DeltaSeconds), 0.0f));
	}

	if (!bWindingUp)
	{
		if (ShotCountdown > Settings->ThistleWindup)
		{
			return;
		}

		if (Target == nullptr)
		{
			// Waits ready to shoot until an enemy comes close
			ShotCountdown = Settings->ThistleWindup;
			return;
		}

		// Before it shoots it glows up and ducks, so everyone sees the shot coming
		bWindingUp = true;
		WindupTarget = Target;
		Flash(Settings->ThistleWindup + 0.15f, 1.3f);
		Squash(0.8f);
		return;
	}

	if (ShotCountdown > 0.0f)
	{
		return;
	}

	bWindingUp = false;
	ShotCountdown = Settings->ThistleShotInterval;

	if (Target != nullptr)
	{
		Shoot(Target);
	}
}

void AVaelThistlePlant::Shoot(AActor* Target)
{
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	const FVaelPlantLook& Look = GetLook();

	// Snaps forwards and spits the seed from its head, flying flat at the height of the head
	Squash(1.18f, 14.0f);

	const FVector Muzzle = FVector(GetActorLocation().X, GetActorLocation().Y, GetGroundZ() + Look.Height * 0.8f) + GetActorForwardVector() * 25.0f;
	FVector Direction = Target->GetActorLocation() - Muzzle;
	Direction.Z = 0.0f;
	if (Direction.IsNearlyZero())
	{
		return;
	}

	FVaelSpellHit Hit;
	Hit.Damage = Settings->ThistleDamage;
	Hit.Element = EVaelElement::Fire;

	FVaelSpellHit Burst;
	Burst.Damage = Settings->ThistleBurstDamage;
	Burst.Element = EVaelElement::Fire;

	const FTransform SpawnTransform(Direction.Rotation(), Muzzle);
	AVaelSpellProjectile* Seed = GetWorld()->SpawnActorDeferred<AVaelSpellProjectile>(AVaelSpellProjectile::StaticClass(), SpawnTransform, this, this, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Seed == nullptr)
	{
		return;
	}

	Seed->InitSpell(Hit, Settings->ThistleShotSpeed, 14.0f, Settings->ThistleRange * 1.2f / Settings->ThistleShotSpeed, 0, Look.Color);
	Seed->SetExplosion(Burst, Settings->ThistleBurstRadius);
	Seed->SetEffects(VaelEffects::Load(EVaelElement::Fire));
	Seed->FinishSpawning(SpawnTransform);
	++ShotCount;

	// A puff of embers at its head
	FVaelDebris Embers;
	Embers.Count = 8;
	Embers.Spread = 10.0f;
	Embers.Speed = 160.0f;
	Embers.Lift = 120.0f;
	Embers.Gravity = -60.0f;
	Embers.Lifetime = 0.45f;
	Embers.MinSize = 2.0f;
	Embers.MaxSize = 5.0f;
	Embers.Color = Look.Color;
	Embers.Glow = 4.0f;
	Embers.bSoft = true;
	Embers.bLandOnGround = false;
	AVaelDebrisBurst::Spawn(this, Muzzle, Embers);
}

// ---------------------------------------------------------------- Wurzelwand

AVaelRootWallPlant::AVaelRootWallPlant()
{
	// Holds back pawns only; players are let through one by one, shots and the camera pass anyway
	Barrier = CreateDefaultSubobject<UBoxComponent>(TEXT("Barrier"));
	Barrier->SetupAttachment(RootComponent);
	Barrier->SetCollisionObjectType(ECC_WorldDynamic);
	Barrier->SetCollisionResponseToAllChannels(ECR_Ignore);
	Barrier->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Barrier->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Barrier->CanCharacterStepUpOn = ECB_No;

	Roots = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Roots"));
	Roots->SetupAttachment(RootComponent);
	Roots->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Body->SetVisibility(false);
}

void AVaelRootWallPlant::BuildLook()
{
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	const float Length = Settings->WallLength;
	const float Height = Settings->WallHeight;
	const float Thickness = Settings->WallThickness;
	const float GroundOffset = GetGroundZ() - GetActorLocation().Z;

	// The wall stands across the throw: along the right of the plant
	Barrier->SetBoxExtent(FVector(Thickness * 0.5f, Length * 0.5f, Height * 0.5f));
	Barrier->SetRelativeLocation(FVector(0.0f, 0.0f, GroundOffset + Height * 0.5f));

	// Roots break out of the ground: arches that cross each other and spikes that rise and curl, all of them gnarled
	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<int32> Triangles;

	const int32 NumRoots = Settings->WallRoots;
	for (int32 RootIndex = 0; RootIndex < NumRoots; ++RootIndex)
	{
		const bool bSpike = RootIndex % 3 == 1;
		const float Place = NumRoots > 1 ? RootIndex / static_cast<float>(NumRoots - 1) : 0.5f;
		const float CenterY = (Place - 0.5f) * Length * 0.9f;
		const float Span = Length * FMath::FRandRange(0.14f, 0.3f);
		const float Top = Height * FMath::FRandRange(bSpike ? 0.7f : 0.3f, bSpike ? 1.05f : 0.8f);
		const float Side = FMath::FRandRange(-0.35f, 0.35f) * Thickness;
		const float Thick = FMath::FRandRange(bSpike ? 9.0f : 8.0f, bSpike ? 15.0f : 13.0f);
		const float Seed = FMath::FRandRange(0.0f, 100.0f);
		const float Direction = FMath::RandBool() ? 1.0f : -1.0f;

		auto PointAt = [&](float Along)
		{
			if (bSpike)
			{
				// Rises out of the ground and curls over at the top
				const float Curl = FMath::Square(Along) * 0.35f * Top * Direction;
				return FVector(Side + FMath::Sin(Along * 5.0f + Seed) * 6.0f, CenterY + Curl, Along * Top);
			}

			// An arch out of the ground and back into it, leaning a little
			const float Y = CenterY + (Along - 0.5f) * Span * Direction;
			const float Z = FMath::Sin(Along * UE_PI) * Top;
			return FVector(Side + FMath::Sin(Along * 7.0f + Seed) * 8.0f + (Along - 0.5f) * 14.0f, Y, Z - 8.0f);
		};

		const int32 First = Vertices.Num();
		for (int32 RingIndex = 0; RingIndex <= RootRings; ++RingIndex)
		{
			const float Along = RingIndex / static_cast<float>(RootRings);
			const FVector Center = PointAt(Along);
			const FVector Forward = (PointAt(FMath::Min(Along + 0.02f, 1.0f)) - PointAt(FMath::Max(Along - 0.02f, 0.0f))).GetSafeNormal();
			FVector Right = FVector::CrossProduct(Forward, FVector::UpVector);
			Right = Right.IsNearlyZero() ? FVector::RightVector : Right.GetSafeNormal();
			const FVector Up = FVector::CrossProduct(Right, Forward);

			// Thick where it leaves the ground, thin at the tip of a spike; knots along the way
			const float Taper = bSpike ? FMath::Lerp(1.0f, 0.15f, Along) : 1.0f - 0.45f * FMath::Sin(Along * UE_PI);
			const float Knot = 1.0f + 0.25f * FMath::Max(0.0f, FMath::Sin(Along * 13.0f + Seed));

			for (int32 SideIndex = 0; SideIndex < RootSides; ++SideIndex)
			{
				const float Angle = SideIndex * 2.0f * UE_PI / RootSides;
				const FVector Out = Right * FMath::Cos(Angle) + Up * FMath::Sin(Angle);
				Vertices.Add(Center + Out * Thick * Taper * Knot);
				Normals.Add(Out);
				UVs.Add(FVector2D(SideIndex / static_cast<float>(RootSides), Along * Top / 60.0f));
			}
		}

		for (int32 RingIndex = 0; RingIndex < RootRings; ++RingIndex)
		{
			for (int32 SideIndex = 0; SideIndex < RootSides; ++SideIndex)
			{
				const int32 A = First + RingIndex * RootSides + SideIndex;
				const int32 B = First + RingIndex * RootSides + (SideIndex + 1) % RootSides;
				Triangles.Append({ A, B, A + RootSides, B, B + RootSides, A + RootSides });
			}
		}
	}

	Roots->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
	Roots->SetRelativeLocation(FVector(0.0f, 0.0f, GroundOffset));
	GrowingPart = Roots;
	GrownScale = FVector::OneVector;

	UMaterialInterface* Material = VaelAssets::LoadOptional(GetLook().Material);
	if (Material == nullptr)
	{
		Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}
	UseMaterial(Roots, Material);

	// The whole wall is a target for the fire of enemies, not only its middle
	GetCapsuleComponent()->SetCapsuleSize(FMath::Min(Length * 0.5f, Thickness), PlantCapsuleHalfHeight);
}

void AVaelRootWallPlant::TickGrown(float DeltaSeconds)
{
	if (Barrier->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
	{
		LetPlayersThrough();
		Barrier->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	PassCountdown -= DeltaSeconds;
	if (PassCountdown <= 0.0f)
	{
		PassCountdown = 1.0f;
		LetPlayersThrough();
	}
}

void AVaelRootWallPlant::LetPlayersThrough()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		if (const AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr)
		{
			Player->GetCapsuleComponent()->IgnoreActorWhenMoving(this, true);
		}
	}
}

bool AVaelRootWallPlant::IsHolding() const
{
	return Barrier->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
}

void AVaelRootWallPlant::OnWither()
{
	Barrier->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

// ---------------------------------------------------------------- Tränenkelch

void AVaelChalicePlant::TickGrown(float DeltaSeconds)
{
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();

	PulseCountdown -= DeltaSeconds;
	if (PulseCountdown > 0.0f)
	{
		return;
	}
	PulseCountdown = Settings->ChalicePulseInterval;

	// Heals every player on their feet within the radius
	int32 NumHealed = 0;
	int32 NumReached = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
		if (Player != nullptr && !Player->IsDowned() && FVector::Dist2D(Player->GetActorLocation(), GetActorLocation()) < Settings->ChaliceRadius)
		{
			++NumReached;
			if (Player->GetHealth() < Player->GetMaxHealth())
			{
				UVaelCombatStatics::Heal(Player, Settings->ChaliceHealPerSecond * Settings->ChalicePulseInterval);
				++NumHealed;
			}
		}
	}

	PulsesReachingAllies += NumReached > 0 ? 1 : 0;

	// Each pulse it breathes out: a soft glow, a small stretch and drops of light that rise over the healing ground
	Flash(0.6f, NumHealed > 0 ? 0.9f : 0.4f);
	Squash(1.07f);

	FVaelDebris Drops;
	Drops.Count = 14;
	Drops.Spread = Settings->ChaliceRadius * 0.85f;
	Drops.Speed = 20.0f;
	Drops.Lift = 110.0f;
	Drops.Gravity = -140.0f;
	Drops.Drag = 1.0f;
	Drops.Lifetime = 1.1f;
	Drops.MinSize = 4.0f;
	Drops.MaxSize = 8.0f;
	Drops.Color = GetLook().Color;
	Drops.Glow = 2.5f;
	Drops.bSoft = true;
	Drops.bLandOnGround = false;
	AVaelDebrisBurst::Spawn(this, FVector(GetActorLocation().X, GetActorLocation().Y, GetGroundZ() + 10.0f), Drops);
}
