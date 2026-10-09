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

	/** The stones bob up and down by this much in cm */
	constexpr float OrbitBob = 12.0f;
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

	// The rock of the earth orb; brown spheres stand in for it while the pack is missing
	UStaticMesh* RockMesh = VaelAssets::LoadOptional(UVaelMagicSettings::Get()->ElementOrbRockMesh);
	const float MeshSize = RockMesh != nullptr ? FMath::Max(RockMesh->GetBounds().BoxExtent.GetMax() * 2.0f, 1.0f) : OrbitSphereSize;
	StoneScale = FVector(Settings.StoneSize / MeshSize);

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
		Tumbles.Add(FRotator(FMath::FRandRange(-200.0f, 200.0f), FMath::FRandRange(-300.0f, 300.0f), FMath::FRandRange(-150.0f, 150.0f)));
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

		Stones[StoneIndex]->SetWorldLocation(StoneLocation);
		Stones[StoneIndex]->SetWorldRotation(Tumbles[StoneIndex] * Age);
		Stones[StoneIndex]->SetWorldScale3D(StoneScale * Grow);

		// Enemies the stone touches are hit and thrown outwards
		for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
		{
			AVaelCharacterBase* Enemy = *It;
			if (!UVaelCombatStatics::CanDamage(Caster, Enemy))
			{
				continue;
			}

			const float Reach = Settings.StoneSize * 0.5f + Enemy->GetSimpleCollisionRadius();
			if (FVector::DistSquared2D(StoneLocation, Enemy->GetActorLocation()) > FMath::Square(Reach)
				|| FMath::Abs(StoneLocation.Z - Enemy->GetActorLocation().Z) > Enemy->GetSimpleCollisionHalfHeight() + Settings.StoneSize)
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

		// Projectiles of enemies break on the stones
		for (TActorIterator<AVaelSpellProjectile> It(GetWorld()); It; ++It)
		{
			if (UVaelCombatStatics::CanDamage(It->GetInstigator(), Caster)
				&& FVector::DistSquared(StoneLocation, It->GetActorLocation()) <= FMath::Square(Settings.StoneSize * 0.5f + It->GetSimpleCollisionRadius()))
			{
				It->Block();
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
