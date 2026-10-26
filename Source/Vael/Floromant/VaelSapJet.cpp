// Copyright Epic Games, Inc. All Rights Reserved.

#include "Floromant/VaelSapJet.h"
#include "Combat/VaelCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Floromant/VaelFloromantComponent.h"
#include "Floromant/VaelPlantArmData.h"
#include "Magic/VaelElementSource.h"
#include "Player/VaelCharacter.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelWaterPuddle.h"
#include "Materials/MaterialInterface.h"
#include "Nature/VaelWaterable.h"
#include "ProceduralMeshComponent.h"
#include "VaelAssets.h"
#include "World/VaelGround.h"

namespace
{
	/** Rings along the jet and points around each ring */
	constexpr int32 JetRings = 48;
	constexpr int32 JetAround = 10;

	/** Speed at which the jet shoots out of the barrel, and at which its torn tail flies off, cm/s */
	constexpr float ShootOutSpeed = 5200.0f;
	constexpr float TearOffSpeed = 2600.0f;

	/** How far the jet sags at its end under its own weight, in cm per metre of length squared */
	constexpr float SagPerSquareMetre = 2.5f;

	/** Seconds between two pushes of everyone in the jet, and between two puddles where it ends */
	constexpr float PushInterval = 0.2f;
	constexpr float PuddleInterval = 0.35f;
	constexpr float PuddleWater = 0.18f;

	/** Damage collects on each enemy and is dealt in steps, so the numbers stay readable */
	constexpr float DamageStep = 4.0f;

	/** Spray: pool, drops per second shed along the jet and fanned where it ends, their size and life */
	constexpr int32 SprayPool = 220;
	constexpr float BodyDropRate = 60.0f;
	constexpr float EndDropRate = 140.0f;
	constexpr float DropMinSize = 2.0f;
	constexpr float DropMaxSize = 6.5f;

	/** The foam flows along the jet, in metres of texture per second */
	constexpr float FoamFlow = 3.0f;
}

AVaelSapJet::AVaelSapJet()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	Tube = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Tube"));
	Tube->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Tube->SetCastShadow(false);
	Tube->bUseAsyncCooking = false;
	RootComponent = Tube;

	Spray = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Spray"));
	Spray->SetupAttachment(Tube);
	Spray->SetUsingAbsoluteLocation(true);
	Spray->SetUsingAbsoluteRotation(true);
	Spray->SetUsingAbsoluteScale(true);
	Spray->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Spray->SetCastShadow(false);
	Spray->TranslucencySortPriority = 1;
}

AVaelSapJet* AVaelSapJet::StartJet(APawn* Gardener, const UVaelPlantArmData* Plant, const FVector& Muzzle, const FVector& Direction)
{
	if (Gardener == nullptr || Plant == nullptr)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.Owner = Gardener;
	Params.Instigator = Gardener;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVaelSapJet* Jet = Gardener->GetWorld()->SpawnActor<AVaelSapJet>(AVaelSapJet::StaticClass(), FTransform(Direction.Rotation(), Muzzle), Params);
	if (Jet == nullptr)
	{
		return nullptr;
	}

	Jet->Plant = Plant;
	Jet->Source = VaelAssets::LoadOptional(Plant->WaterSource);
	Jet->Seed = FMath::FRandRange(1.0f, 100.0f);
	Jet->Drops.Init(SprayPool);

	// The water of the plant's source; without one the waves of the mage
	UMaterialInterface* Material = Jet->Source != nullptr ? Jet->Source->WaveMaterial.Get() : nullptr;
	if (Material == nullptr)
	{
		Material = VaelAssets::LoadOptional(UVaelMagicSettings::Get()->WaveMaterial);
	}
	Jet->Tube->SetMaterial(0, Material);
	Jet->Spray->SetMaterial(0, Material);
	Jet->Aim(Muzzle, Direction, 1.0f);
	return Jet;
}

void AVaelSapJet::Aim(const FVector& Muzzle, const FVector& Direction, float InPressure)
{
	if (bStopping)
	{
		return;
	}
	SetActorLocationAndRotation(Muzzle, Direction.Rotation());
	Pressure = FMath::Max(Pressure, InPressure);
}

void AVaelSapJet::Stop()
{
	bStopping = true;
}

void AVaelSapJet::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Plant == nullptr)
	{
		Destroy();
		return;
	}

	Clock += DeltaSeconds;
	Pressure = FMath::FInterpTo(Pressure, 0.0f, DeltaSeconds, 5.0f);

	// Walls stop it; it shoots out of the barrel at the start
	const FVector Start = GetActorLocation();
	const FVector Direction = GetActorForwardVector();
	Reach = Plant->JetRange;
	bEndsOnWall = false;
	FHitResult WallHit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VaelSapJet), false, GetInstigator());
	if (GetWorld()->LineTraceSingleByObjectType(WallHit, Start, Start + Direction * Reach, FCollisionObjectQueryParams(ECC_WorldStatic), QueryParams))
	{
		Reach = WallHit.Distance;
		bEndsOnWall = true;
	}
	Length = FMath::Min(Length + ShootOutSpeed * DeltaSeconds, Reach);

	// Torn off, the tail flies out until nothing is left
	if (bStopping)
	{
		TornOff += TearOffSpeed * DeltaSeconds;
		if (TornOff >= Length)
		{
			Destroy();
			return;
		}
	}

	const float Metres = Length / 100.0f;
	EndLocation = Start + Direction * Length - FVector(0.0f, 0.0f, SagPerSquareMetre * Metres * Metres);

	if (!bStopping)
	{
		UpdateHits(DeltaSeconds);
	}
	UpdateSpray(DeltaSeconds);
	BuildTube();
}

void AVaelSapJet::UpdateHits(float DeltaSeconds)
{
	APawn* Gardener = GetInstigator();
	UWorld* World = GetWorld();
	const FVector Start = GetActorLocation();
	const FVector Direction = GetActorForwardVector();

	PushCountdown -= DeltaSeconds;
	const bool bPush = PushCountdown <= 0.0f;
	if (bPush)
	{
		PushCountdown = PushInterval;
	}

	// A rooted Floromant pumps harder
	const AVaelCharacter* Character = Cast<AVaelCharacter>(Gardener);
	const float Power = Character != nullptr && Character->GetFloromant() != nullptr ? Character->GetFloromant()->GetHandPower() : 1.0f;

	FVaelSpellHit Hit;
	Hit.Element = EVaelElement::Water;
	Hit.Status = Plant->ShotStatus;
	Hit.StatusDuration = Plant->ShotStatusDuration;

	for (TActorIterator<AVaelCharacterBase> It(World); It; ++It)
	{
		AVaelCharacterBase* Target = *It;
		if (!UVaelCombatStatics::CanDamage(Gardener, Target))
		{
			continue;
		}
		const FVector ToTarget = Target->GetActorLocation() - Start;
		const float Along = FVector::DotProduct(FVector(ToTarget.X, ToTarget.Y, 0.0f), Direction);
		if (Along < 0.0f || Along > Length)
		{
			continue;
		}
		const float Across = FMath::Abs(Direction.X * ToTarget.Y - Direction.Y * ToTarget.X);
		if (Across >= Plant->JetHalfWidth + Target->GetCapsuleComponent()->GetScaledCapsuleRadius() * 0.5f)
		{
			continue;
		}

		const float Dealt = CollectedDamage.Add(Target, Plant->JetDamagePerSecond * Power * DeltaSeconds, DamageStep);
		if (Dealt > 0.0f)
		{
			Hit.Damage = Dealt;
			UVaelCombatStatics::ApplySpellHit(Gardener, Target, Hit, Direction);
		}
		// It keeps pushing: weaker the farther out the enemy stands
		if (bPush && Plant->JetPush > 0.0f)
		{
			Target->ApplyKnockback(Direction, Plant->JetPush * FMath::Lerp(1.0f, 0.5f, Along / FMath::Max(Plant->JetRange, 1.0f)));
		}
	}

	// It waters the plants it runs over, and the water gathers where it lands
	VaelWatering::WaterAlong(World, Start, EndLocation, Plant->JetHalfWidth, Plant->JetWaterPerSecond * DeltaSeconds, Gardener);
	PuddleCountdown -= DeltaSeconds;
	if (PuddleCountdown <= 0.0f && Length >= Reach - 1.0f)
	{
		PuddleCountdown = PuddleInterval;
		AVaelWaterPuddle::PourSplash(World, EndLocation, PuddleWater, Source, Gardener);
	}
}

void AVaelSapJet::UpdateSpray(float DeltaSeconds)
{
	const FVector Start = GetActorLocation();
	const FVector Direction = GetActorForwardVector();
	FHitResult GroundHit;
	const float GroundZ = VaelGround::TraceGround(GetWorld(), EndLocation + FVector(0.0f, 0.0f, 200.0f), EndLocation - FVector(0.0f, 0.0f, 1000.0f), GroundHit, this)
		? GroundHit.Location.Z : EndLocation.Z - 300.0f;
	Drops.Update(DeltaSeconds, GroundZ);

	// Along the jet it sheds a few drops that fall away in arcs; where it ends it fans into spray, back off a wall
	const FVector Side = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();
	const float Live = bStopping ? 0.3f : 1.0f;
	Drops.Debt = FMath::Min(Drops.Debt + (BodyDropRate + EndDropRate) * Live * DeltaSeconds, 20.0f);
	for (; Drops.Debt >= 1.0f; Drops.Debt -= 1.0f)
	{
		const bool bAtEnd = FMath::FRand() < EndDropRate / (BodyDropRate + EndDropRate);
		FVector Location;
		FVector Velocity;
		if (bAtEnd)
		{
			Location = EndLocation;
			const FVector Fan = Side * FMath::FRandRange(-1.0f, 1.0f) + FVector(0.0f, 0.0f, FMath::FRandRange(-0.2f, 0.8f));
			const FVector Onward = bEndsOnWall ? -Direction * FMath::FRandRange(80.0f, 260.0f) : Direction * FMath::FRandRange(150.0f, 420.0f);
			Velocity = Onward + Fan * FMath::FRandRange(120.0f, 320.0f);
		}
		else
		{
			const float Share = FMath::FRandRange(0.2f, 0.95f);
			Location = FMath::Lerp(Start, EndLocation, Share);
			Velocity = Direction * FMath::FRandRange(200.0f, 500.0f) + Side * FMath::FRandRange(-90.0f, 90.0f) + FVector(0.0f, 0.0f, FMath::FRandRange(0.0f, 80.0f));
		}
		const bool bClump = FMath::FRand() < 0.12f;
		const float Size = FMath::Lerp(DropMinSize, DropMaxSize, FMath::Pow(FMath::FRand(), 2.5f)) * (bClump ? 1.6f : 1.0f);
		if (!Drops.Throw(Location, Velocity, FMath::FRandRange(0.35f, 0.8f), Size, bClump))
		{
			break;
		}
	}
}

void AVaelSapJet::BuildTube()
{
	constexpr int32 Rows = JetAround + 1;
	TubeSection.Reset(JetRings * Rows);

	const float Metres = Length / 100.0f;
	const float Turns = Plant->JetSpiralTurns;
	const float Swell = 1.0f + 0.35f * Pressure;
	for (int32 Ring = 0; Ring < JetRings; ++Ring)
	{
		const float T = static_cast<float>(Ring) / (JetRings - 1);

		// The torn tail starts further out; the first rings close the jet at the barrel or at the tear
		const float X = FMath::Lerp(FMath::Min(TornOff, Length), Length, T);
		const float Share = Length > 1.0f ? X / Length : 0.0f;
		const float Sag = SagPerSquareMetre * FMath::Square(X / 100.0f);

		// Narrow at the barrel, wider and torn where it ends; it twists in a spiral, so it wobbles round its middle line
		const float Close = FMath::Min(T * 6.0f, 1.0f) * FMath::Min((1.0f - T) * 8.0f, 1.0f);
		const float Radius = FMath::Lerp(Plant->JetStartRadius, Plant->JetEndRadius, FMath::Pow(Share, 0.8f)) * Swell * FMath::Max(Close, 0.15f);
		const float Spin = Clock * 9.0f + X / 100.0f * Turns * UE_TWO_PI;
		const FVector Twist(0.0f, FMath::Cos(Spin), FMath::Sin(Spin));
		const FVector Middle = FVector(X, 0.0f, -Sag) + Twist * Radius * 0.25f * Share;

		// Light and clear near the barrel, foaming more where it fans out and where it is torn
		const float Light = FMath::Lerp(0.85f, 0.55f, Share);
		const float FoamBase = 0.2f + 0.6f * FMath::Pow(Share, 1.5f) + (bStopping ? 0.3f : 0.0f);
		const float Alpha = 0.92f * (1.0f - FMath::SmoothStep(0.8f, 1.0f, Share) * 0.7f);

		for (int32 Step = 0; Step < Rows; ++Step)
		{
			const float Angle = UE_TWO_PI * Step / JetAround;
			const float Cos = FMath::Cos(Angle);
			const float Sin = FMath::Sin(Angle);
			const float Lump = 1.0f + (0.12f + 0.45f * Share) * VaelWaterMesh::Noise(Cos * 1.3f + X / 60.0f - Clock * 7.0f + Seed, Sin * 1.3f + Seed * 0.5f);
			const FVector Out(0.0f, Cos, Sin);

			// The foam winds round the jet in a spiral stripe
			const float Stripe = FMath::Max(0.0f, FMath::Cos(Angle - Spin * 1.0f));
			const int32 Index = Ring * Rows + Step;
			TubeSection.Vertices[Index] = Middle + Out * Radius * FMath::Max(Lump, 0.25f);
			TubeSection.Normals[Index] = Out;
			TubeSection.UVs[Index] = FVector2D((Angle + Spin) * Radius / 100.0f, X / 100.0f - Clock * FoamFlow);
			TubeSection.Colors[Index] = FLinearColor(Light, FMath::Clamp(FoamBase + 0.35f * Stripe * Share, 0.0f, 1.0f), 0.0f, Alpha);
		}
	}

	const FVector CameraLocation = VaelWaterMesh::GetCameraLocation(GetWorld(), GetActorLocation() + FVector(-600.0f, 0.0f, 900.0f));
	const FVector ToCamera = GetActorTransform().InverseTransformVectorNoScale(CameraLocation - GetActorLocation());
	VaelWaterMesh::BuildViewSortedTubeTriangles(JetRings, JetAround, FVector::RightVector, FVector::UpVector, ToCamera, TubeSection.Triangles);
	TubeSection.Apply(Tube, 0, true);

	Drops.Build(SpraySection, FTransform::Identity, CameraLocation, 1.0f, 0.45f);
	const bool bCreate = !bSprayCreated;
	if (bCreate)
	{
		Drops.BuildTriangles(SpraySection);
		bSprayCreated = true;
	}
	SpraySection.Apply(Spray, 0, bCreate);
}
