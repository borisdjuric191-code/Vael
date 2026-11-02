// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelSpellOrbit.h"
#include "Combat/VaelCharacterBase.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Magic/VaelDebrisBurst.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelSpellEffects.h"
#include "Magic/VaelSpellProjectile.h"
#include "VaelAssets.h"

namespace
{
	/** Diameter of the engine sphere in cm */
	constexpr float OrbitSphereSize = 100.0f;

	/** Seconds the stones need to grow to full size */
	constexpr float OrbitGrowTime = 0.2f;

	/** Seconds before a stone can hit the same enemy again */
	constexpr float OrbitHitCooldown = 0.5f;

	/** The stones bob up and down by this much in cm, and the slabs rock on their edge by up to these degrees */
	constexpr float OrbitBob = 6.0f;
	constexpr float OrbitSlabRock = 6.0f;
}

AVaelSpellOrbit::AVaelSpellOrbit()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	PrimaryActorTick.bCanEverTick = true;
}

AVaelSpellOrbit* AVaelSpellOrbit::Start(APawn* Caster, const FVaelSpellHit& InHit, const FVaelOrbitSettings& InSettings)
{
	UWorld* World = Caster != nullptr ? Caster->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}

	// One circle per caster: a new one replaces the old
	for (TActorIterator<AVaelSpellOrbit> It(World); It; ++It)
	{
		if (It->GetInstigator() == Caster)
		{
			It->Destroy();
		}
	}

	const FTransform SpawnTransform(Caster->GetActorLocation());

	AVaelSpellOrbit* Orbit = World->SpawnActorDeferred<AVaelSpellOrbit>(AVaelSpellOrbit::StaticClass(), SpawnTransform, Caster, Caster, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Orbit != nullptr)
	{
		Orbit->Hit = InHit;
		Orbit->Settings = InSettings;
		Orbit->Settings.Count = FMath::Max(Orbit->Settings.Count, 1);
		Orbit->InitialLifeSpan = InSettings.Lifetime;
		Orbit->FinishSpawning(SpawnTransform);
	}

	return Orbit;
}

void AVaelSpellOrbit::BeginPlay()
{
	Super::BeginPlay();

	UMaterialInterface* CoreMaterial = nullptr;
	UMaterialInterface* GlowMaterial = nullptr;
	VaelEffects::LoadLookMaterials(CoreMaterial, GlowMaterial);

	// A shield of flat slabs standing on edge (Boris 2026-11-02): more and bigger stones than the formula's rocks, pressed flat,
	// facing outwards. The rock of the earth orb pressed into shape; brown spheres stand in for it while the pack is missing
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	Settings.Count = FMath::Max(Settings.Count, MagicSettings->OrbitShieldStones);
	SlabSize = MagicSettings->OrbitSlabShape * Settings.StoneSize;
	UStaticMesh* RockMesh = VaelAssets::LoadOptional(MagicSettings->ElementOrbRockMesh);
	const FVector MeshSize = RockMesh != nullptr ? RockMesh->GetBounds().BoxExtent.ComponentMax(FVector(0.5f)) * 2.0f : FVector(OrbitSphereSize);
	StoneScale = SlabSize / MeshSize;

	for (int32 StoneIndex = 0; StoneIndex < Settings.Count; ++StoneIndex)
	{
		UStaticMeshComponent* Stone = VaelEffects::AddLookShape(this, CoreMaterial, UVaelMagicSettings::Get()->GetElementColor(EVaelElement::Earth) * 0.6f, 0.0f);
		if (RockMesh != nullptr)
		{
			Stone->SetStaticMesh(RockMesh);
			Stone->EmptyOverrideMaterials();
		}

		Stone->SetUsingAbsoluteLocation(true);
		Stone->SetUsingAbsoluteRotation(true);
		Stone->SetCastShadow(true);
		Stone->SetVisibility(true);

		Stones.Add(Stone);
		// No tumbling: a slab only rocks a little on its edge, so it keeps its face to the outside
		Tumbles.Add(FRotator(FMath::FRandRange(-1.0f, 1.0f), FMath::FRandRange(0.0f, 360.0f), FMath::FRandRange(-1.0f, 1.0f)));
	}

	Tick(0.0f);
}

FVector AVaelSpellOrbit::GetStoneLocation(int32 StoneIndex) const
{
	const float Angle = FMath::DegreesToRadians(Age * Settings.Speed + StoneIndex * 360.0f / Settings.Count);
	const float Bob = FMath::Sin(Age * 5.0f + StoneIndex * 1.7f) * OrbitBob;

	return GetActorLocation() + FVector(FMath::Cos(Angle) * Settings.Radius, FMath::Sin(Angle) * Settings.Radius, Bob);
}

void AVaelSpellOrbit::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	APawn* Caster = GetInstigator();
	if (Caster == nullptr || !IsValid(Caster))
	{
		Destroy();
		return;
	}

	// The circle follows the caster
	Age += DeltaSeconds;
	SetActorLocation(Caster->GetActorLocation());

	const float Grow = FMath::Clamp(Age / OrbitGrowTime, 0.0f, 1.0f);
	const float Now = GetWorld()->GetTimeSeconds();

	for (int32 StoneIndex = 0; StoneIndex < Stones.Num(); ++StoneIndex)
	{
		const FVector StoneLocation = GetStoneLocation(StoneIndex);

		// Face out from the caster, rocking a little on its edge with its own rhythm
		const float Facing = Age * Settings.Speed + StoneIndex * 360.0f / Settings.Count;
		const FRotator& Rock = Tumbles[StoneIndex];
		const float Phase = Age * 4.0f + Rock.Yaw;
		Stones[StoneIndex]->SetWorldLocation(StoneLocation);
		Stones[StoneIndex]->SetWorldRotation(FRotator(OrbitSlabRock * FMath::Sin(Phase) * Rock.Pitch, Facing, OrbitSlabRock * FMath::Sin(Phase * 1.3f) * Rock.Roll));
		Stones[StoneIndex]->SetWorldScale3D(StoneScale * Grow);

		// Enemies the stone touches are hit and thrown outwards
		for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
		{
			AVaelCharacterBase* Enemy = *It;
			if (!UVaelCombatStatics::CanDamage(Caster, Enemy))
			{
				continue;
			}

			const float Reach = SlabSize.Y * 0.5f + Enemy->GetSimpleCollisionRadius();
			if (FVector::DistSquared2D(StoneLocation, Enemy->GetActorLocation()) > FMath::Square(Reach)
				|| FMath::Abs(StoneLocation.Z - Enemy->GetActorLocation().Z) > Enemy->GetSimpleCollisionHalfHeight() + SlabSize.Z * 0.5f)
			{
				continue;
			}

			const float* LastHit = LastHitTimes.Find(Enemy);
			if (LastHit != nullptr && Now - *LastHit < OrbitHitCooldown)
			{
				continue;
			}

			LastHitTimes.Add(Enemy, Now);
			UVaelCombatStatics::ApplySpellHit(Caster, Enemy, Hit, Enemy->GetActorLocation() - GetActorLocation());
		}
	}

	BlockProjectiles(DeltaSeconds);
}

void AVaelSpellOrbit::BlockProjectiles(float DeltaSeconds)
{
	// Projectiles of enemies that reach the ring break on a slab, if one is there or whirls past within the frame; the slabs
	// spin so fast and stand so close that hardly anything gets through. Checked on the ring itself, so a fast shot that would
	// jump over a thin slab between two frames is still caught.
	const APawn* Caster = GetInstigator();
	const FVector Center = GetActorLocation();
	const float Grow = FMath::Clamp(Age / OrbitGrowTime, 0.0f, 1.0f);
	const float SpinPerFrame = FMath::Abs(Settings.Speed) * DeltaSeconds;
	for (TActorIterator<AVaelSpellProjectile> It(GetWorld()); It; ++It)
	{
		AVaelSpellProjectile* Projectile = *It;
		if (!UVaelCombatStatics::CanDamage(Projectile->GetInstigator(), Caster))
		{
			continue;
		}
		const FVector Offset = Projectile->GetActorLocation() - Center;
		const float Size = Projectile->GetSimpleCollisionRadius();
		const float Travel = Projectile->GetVelocity().Size() * DeltaSeconds;
		if (FMath::Abs(Offset.Size2D() - Settings.Radius) > SlabSize.X * 0.5f + Size + Travel * 0.5f || FMath::Abs(Offset.Z) > SlabSize.Z * 0.5f * Grow + Size)
		{
			continue;
		}
		const float Bearing = FMath::RadiansToDegrees(FMath::Atan2(Offset.Y, Offset.X));
		const float HalfSlab = FMath::RadiansToDegrees((SlabSize.Y * 0.5f * Grow + Size) / FMath::Max(Settings.Radius, 1.0f));
		for (int32 StoneIndex = 0; StoneIndex < Stones.Num(); ++StoneIndex)
		{
			const float Facing = Age * Settings.Speed + StoneIndex * 360.0f / Settings.Count;
			if (FMath::Abs(FMath::FindDeltaAngleDegrees(Facing, Bearing)) <= HalfSlab + SpinPerFrame * 0.5f)
			{
				Projectile->Block();
				++NumBlocked;
				break;
			}
		}
	}
}

void AVaelSpellOrbit::LifeSpanExpired()
{
	Shatter();
}

void AVaelSpellOrbit::Shatter()
{
	// Each stone breaks into pebbles that fly on outwards
	for (int32 StoneIndex = 0; StoneIndex < Stones.Num(); ++StoneIndex)
	{
		FVaelDebris Pebbles;
		Pebbles.Count = 6;
		Pebbles.Spread = Settings.StoneSize * 0.3f;
		Pebbles.Speed = 220.0f;
		Pebbles.Lift = 200.0f;
		Pebbles.MinSize = 6.0f;
		Pebbles.MaxSize = 12.0f;
		Pebbles.RockShare = 0.3f;
		AVaelDebrisBurst::Spawn(this, GetStoneLocation(StoneIndex), Pebbles);
	}

	Destroy();
}
