// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelDebrisBurst.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelSpellEffects.h"
#include "VaelAssets.h"
#include "World/VaelGround.h"

namespace
{
	/** Diameter of the engine sphere and largest side of the rock mesh in cm */
	constexpr float DebrisSphereSize = 100.0f;
	constexpr float DebrisRockMeshSize = 230.0f;

	/** Latest start of a piece after the burst, so they don't all leave in the same frame */
	constexpr float DebrisLatestStart = 0.05f;
}

AVaelDebrisBurst::AVaelDebrisBurst()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	PrimaryActorTick.bCanEverTick = true;
}

AVaelDebrisBurst* AVaelDebrisBurst::Spawn(const UObject* WorldContext, const FVector& Location, const FVaelDebris& Debris)
{
	UWorld* World = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
	if (World == nullptr || Debris.Count <= 0)
	{
		return nullptr;
	}

	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AVaelDebrisBurst* Burst = World->SpawnActor<AVaelDebrisBurst>(AVaelDebrisBurst::StaticClass(), FTransform(Location), Parameters);
	if (Burst != nullptr)
	{
		Burst->Build(Debris);
	}

	return Burst;
}

void AVaelDebrisBurst::Build(const FVaelDebris& InDebris)
{
	Debris = InDebris;

	UMaterialInterface* CoreMaterial = nullptr;
	UMaterialInterface* GlowMaterial = nullptr;
	VaelEffects::LoadLookMaterials(CoreMaterial, GlowMaterial);

	UStaticMesh* RockMesh = Debris.RockShare > 0.0f ? VaelAssets::LoadOptional(UVaelMagicSettings::Get()->ElementOrbRockMesh) : nullptr;

	const FVector Center = GetActorLocation();

	FHitResult GroundHit;
	if (Debris.bLandOnGround && VaelGround::TraceGround(GetWorld(), Center + FVector(0.0f, 0.0f, 50.0f), Center - FVector(0.0f, 0.0f, 600.0f), GroundHit))
	{
		GroundZ = GroundHit.Location.Z;
	}

	for (int32 PieceIndex = 0; PieceIndex < Debris.Count; ++PieceIndex)
	{
		// Rocks are spread evenly among the other pieces
		const bool bRock = Debris.RockShare > 0.0f && FMath::Fmod(PieceIndex * Debris.RockShare, 1.0f) + Debris.RockShare >= 1.0f;
		const float Size = FMath::FRandRange(Debris.MinSize, Debris.MaxSize) * (bRock ? 3.0f : 1.0f);

		UStaticMeshComponent* Piece = VaelEffects::AddLookShape(this, Debris.bSoft ? GlowMaterial : CoreMaterial, Debris.Color, Debris.Glow, Debris.bSoft ? 0.3f : 0.0f);
		Piece->SetUsingAbsoluteLocation(true);
		Piece->SetUsingAbsoluteRotation(true);
		Piece->SetUsingAbsoluteScale(true);

		// A rock keeps the material of its pack; without the pack it stays a dark sphere
		if (bRock && RockMesh != nullptr)
		{
			Piece->SetStaticMesh(RockMesh);
			Piece->EmptyOverrideMaterials();
		}

		const float Angle = FMath::FRandRange(0.0f, UE_TWO_PI);
		const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
		const float Share = FMath::FRandRange(0.5f, 1.0f);

		Pieces.Add(Piece);
		Origins.Add(Center + Out * Debris.Spread * FMath::Sqrt(FMath::FRand()));
		Velocities.Add(Out * Debris.Speed * Share + FVector(0.0f, 0.0f, Debris.Lift * FMath::FRandRange(0.5f, 1.0f)));
		Tumbles.Add(FRotator(FMath::FRandRange(-720.0f, 720.0f), FMath::FRandRange(-540.0f, 540.0f), 0.0f));
		Sizes.Add(Size / (bRock && RockMesh != nullptr ? DebrisRockMeshSize : DebrisSphereSize));
		Delays.Add(FMath::FRandRange(0.0f, DebrisLatestStart));
	}

	Tick(0.0f);
}

void AVaelDebrisBurst::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Elapsed += DeltaSeconds;

	if (Elapsed >= Debris.Lifetime + DebrisLatestStart)
	{
		Destroy();
		return;
	}

	for (int32 PieceIndex = 0; PieceIndex < Pieces.Num(); ++PieceIndex)
	{
		UStaticMeshComponent* Piece = Pieces[PieceIndex];
		const float Age = Elapsed - Delays[PieceIndex];

		Piece->SetVisibility(Age > 0.0f && Age < Debris.Lifetime);
		if (Age <= 0.0f || Age >= Debris.Lifetime)
		{
			continue;
		}

		// Flung off, slowed by the air, pulled down until they lie on the ground
		const float Carried = Debris.Drag > 0.0f ? (1.0f - FMath::Exp(-Debris.Drag * Age)) / Debris.Drag : Age;
		FVector Location = Origins[PieceIndex] + Velocities[PieceIndex] * Carried - FVector(0.0f, 0.0f, 0.5f * Debris.Gravity * Age * Age);

		const float Life = Age / Debris.Lifetime;
		const bool bLanded = Location.Z <= GroundZ;
		Location.Z = FMath::Max(Location.Z, GroundZ);

		Piece->SetWorldLocation(Location);

		if (!bLanded)
		{
			Piece->SetWorldRotation(Tumbles[PieceIndex] * Age);
		}

		// Soft and glowing pieces fade and puffs swell, solid ones keep their size until they are nearly gone
		if (Debris.bSoft || Debris.Glow > 0.0f)
		{
			VaelEffects::SetLookGlow(Piece, Debris.Glow * (1.0f - Life));
		}

		const float Scale = Debris.bSoft ? 0.5f + Life : FMath::Min(1.0f, (1.0f - Life) * 4.0f);
		Piece->SetWorldScale3D(FVector(Sizes[PieceIndex] * Scale));
	}
}
