// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelAvalanche.h"
#include "Combat/VaelHitFeedbackSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Magic/VaelCloudCards.h"
#include "Magic/VaelFlakes.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelSnowDrift.h"
#include "Magic/VaelSpellEffects.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "VaelAssets.h"
#include "World/VaelGround.h"

namespace
{
	/** Seconds the front takes to roll out to the end of the cone, and where it starts in front of the caster */
	constexpr float RollSeconds = 0.7f;
	constexpr float FrontStart = 70.0f;

	/** Cards of the powder wall: across the cone and above each other, and trail cards left behind in rows */
	constexpr int32 FrontColumns = 11;
	constexpr int32 FrontRows = 2;
	constexpr int32 TrailColumns = 9;
	const float TrailRows[] = { 0.3f, 0.55f, 0.8f };

	/** Seconds the wall lingers after it stopped, and the powder left behind */
	constexpr float WallLinger = 0.6f;
	constexpr float TrailLinger = 1.6f;

	/** Lumps of ice and of rock in the snow, their size in cm */
	constexpr int32 IceLumps = 6;
	constexpr int32 RockLumps = 6;
	constexpr float LumpMinSize = 18.0f;
	constexpr float LumpMaxSize = 38.0f;
	const FLinearColor IceColor = FLinearColor(0.75f, 0.88f, 1.0f);
	const FLinearColor RockColor = FLinearColor(0.28f, 0.25f, 0.22f);

	/** Snow thrown up at the front per second, and the trail of every lump */
	constexpr float FrontSnowRate = 90.0f;
	constexpr float LumpSnowEvery = 0.07f;

	/** Drifts in the fan: rows by share of the reach and how many in each, their radius against the reach */
	const float DriftRows[] = { 0.38f, 0.62f, 0.86f };
	constexpr float DriftRadiusShare = 0.21f;

	/** Crust of ice on a caught enemy: pieces, their size in cm, seconds to melt away */
	constexpr int32 CrustPieces = 5;
	constexpr float CrustMelt = 0.3f;
	constexpr float CrustMaxDelay = 0.3f;

	/** Shake when it breaks loose */
	constexpr float AvalancheShake = 0.25f;
}

AVaelAvalanche::AVaelAvalanche()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AVaelAvalanche* AVaelAvalanche::Spawn(APawn* Caster, const FVector& Direction, float InRange, float InHalfAngle, const TArray<AActor*>& Caught, float FrozenSeconds)
{
	UWorld* World = Caster != nullptr ? Caster->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}

	// From the ground at the caster's feet
	const FVector Feet = Caster->GetActorLocation() - FVector(0.0f, 0.0f, Caster->GetSimpleCollisionHalfHeight());
	FHitResult GroundHit;
	const FVector Ground = VaelGround::TraceGround(World, Feet + FVector(0.0f, 0.0f, 100.0f), Feet - FVector(0.0f, 0.0f, 300.0f), GroundHit, Caster) ? GroundHit.Location : Feet;

	FActorSpawnParameters Parameters;
	Parameters.Instigator = Caster;
	Parameters.Owner = Caster;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVaelAvalanche* Avalanche = World->SpawnActor<AVaelAvalanche>(AVaelAvalanche::StaticClass(), FTransform(Ground), Parameters);
	if (Avalanche != nullptr)
	{
		Avalanche->Forward = Direction.GetSafeNormal2D();
		Avalanche->Range = InRange;
		Avalanche->HalfAngle = InHalfAngle;
		Avalanche->Life = FMath::Max(3.2f, CrustMaxDelay + FrozenSeconds + CrustMelt + 0.5f);
		Avalanche->SetLifeSpan(Avalanche->Life);
		Avalanche->Build(Caught, FrozenSeconds);
		UVaelHitFeedbackSubsystem::Shake(Caster, AvalancheShake);
	}
	return Avalanche;
}

FVector AVaelAvalanche::Along(float Angle) const
{
	return Forward.RotateAngleAxis(Angle, FVector::UpVector);
}

float AVaelAvalanche::FrontDistance() const
{
	const float Rolled = FMath::Clamp(Age / RollSeconds, 0.0f, 1.0f);
	return FMath::Lerp(FrontStart, Range, 1.0f - FMath::Square(1.0f - Rolled));
}

float AVaelAvalanche::GroundAt(const FVector& Location) const
{
	FHitResult GroundHit;
	const FVector Above(Location.X, Location.Y, GetActorLocation().Z + 150.0f);
	return VaelGround::TraceGround(GetWorld(), Above, Above - FVector(0.0f, 0.0f, 450.0f), GroundHit, GetInstigator()) ? GroundHit.Location.Z : GetActorLocation().Z;
}

UStaticMeshComponent* AVaelAvalanche::MakeLump(const FLinearColor& Color, float Glow)
{
	UMaterialInterface* CoreMaterial = nullptr;
	UMaterialInterface* GlowMaterial = nullptr;
	VaelEffects::LoadLookMaterials(CoreMaterial, GlowMaterial);

	UStaticMeshComponent* Lump = NewObject<UStaticMeshComponent>(this);
	Lump->SetupAttachment(RootComponent);
	Lump->SetUsingAbsoluteLocation(true);
	Lump->SetUsingAbsoluteRotation(true);
	Lump->SetUsingAbsoluteScale(true);
	Lump->SetStaticMesh(RockMesh);
	Lump->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Lump->SetCastShadow(false);
	Lump->bReceivesDecals = false;
	Lump->SetWorldScale3D(FVector::ZeroVector);
	Lump->RegisterComponent();

	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(CoreMaterial, this);
	Material->SetVectorParameterValue(VaelEffects::LookColorParameter, Color);
	Material->SetScalarParameterValue(VaelEffects::LookGlowParameter, Glow);
	Material->SetScalarParameterValue(VaelEffects::LookRimParameter, Glow > 0.0f ? 0.6f : 0.0f);
	Lump->SetMaterial(0, Material);
	return Lump;
}

void AVaelAvalanche::Build(const TArray<AActor*>& Caught, float FrozenSeconds)
{
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();

	RockMesh = VaelAssets::LoadOptional(MagicSettings->ElementOrbRockMesh);
	if (RockMesh == nullptr)
	{
		RockMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	}
	RockSize = FMath::Max(RockMesh->GetBounds().BoxExtent.GetMax() * 2.0f, 1.0f);

	// The powder: a wall of white cloud cards at the front, rows of trail cards it leaves behind
	if (UMaterialInterface* CloudMaterial = VaelAssets::LoadOptional(MagicSettings->CloudCardMaterial))
	{
		Cloud = NewObject<UVaelCloudCardsComponent>(this);
		Cloud->SetupAttachment(RootComponent);
		Cloud->SetUsingAbsoluteRotation(true);
		Cloud->RegisterComponent();
		Cloud->Init(CloudMaterial);
		const auto AddCard = [this](float Angle, float Distance, int32 Row, float Size)
		{
			Cloud->AddCard(FVector::ZeroVector, Size, 0.0f);
			CardAngles.Add(Angle);
			CardDistances.Add(Distance);
			CardRows.Add(Row);
			CardSizes.Add(Size);
			CardPhases.Add(FMath::FRandRange(0.0f, UE_TWO_PI));
		};
		for (int32 Row = 0; Row < FrontRows; ++Row)
		{
			for (int32 Column = 0; Column < FrontColumns; ++Column)
			{
				const float Angle = FMath::Lerp(-HalfAngle, HalfAngle, Column / (FrontColumns - 1.0f)) + FMath::FRandRange(-3.0f, 3.0f);
				AddCard(Angle, 0.0f, Row, FMath::FRandRange(140.0f, 180.0f) + Row * 40.0f);
			}
		}
		for (const float Share : TrailRows)
		{
			for (int32 Column = 0; Column < TrailColumns; ++Column)
			{
				const float Angle = FMath::Lerp(-HalfAngle, HalfAngle, Column / (TrailColumns - 1.0f)) * 0.9f + FMath::FRandRange(-4.0f, 4.0f);
				AddCard(Angle, Range * Share + FMath::FRandRange(-30.0f, 30.0f), 0, FMath::FRandRange(120.0f, 170.0f));
			}
		}
		Cloud->Commit();
	}

	// Lumps of ice and rock spread over the wall, some right at its front, some behind
	for (int32 Index = 0; Index < IceLumps + RockLumps; ++Index)
	{
		const bool bIce = Index < IceLumps;
		FLump& Lump = Lumps.AddDefaulted_GetRef();
		Lump.Mesh = bIce ? MakeLump(IceColor, 0.6f) : MakeLump(RockColor, 0.0f);
		Lump.Angle = FMath::FRandRange(-HalfAngle, HalfAngle) * 0.85f;
		Lump.Lag = FMath::FRandRange(20.0f, 90.0f);
		Lump.Size = FMath::FRandRange(LumpMinSize, LumpMaxSize);
		Lump.Bounce = FMath::FRandRange(8.0f, 25.0f);
		Lump.Start = FMath::VRand().ToOrientationQuat();
		Lump.NextFlake = FMath::FRandRange(0.0f, LumpSnowEvery);
	}

	// Crust of ice around the legs of everyone it caught, when the wall reaches them
	for (AActor* Target : Caught)
	{
		if (Target == nullptr)
		{
			continue;
		}
		FCrust& Crust = Crusts.AddDefaulted_GetRef();
		Crust.Target = Target;
		const float Reached = FMath::Clamp(FVector::Dist2D(Target->GetActorLocation(), GetActorLocation()) / Range, 0.0f, 1.0f) * RollSeconds;
		Crust.Appear = FMath::Min(Reached, CrustMaxDelay);
		Crust.Melt = Crust.Appear + FrozenSeconds;
		for (int32 Piece = 0; Piece < CrustPieces; ++Piece)
		{
			Crust.Pieces.Add(MakeLump(IceColor, 0.5f));
			Crust.Sizes.Add(FMath::FRandRange(22.0f, 34.0f) * FMath::Clamp(Target->GetSimpleCollisionRadius() / 40.0f, 0.7f, 2.0f));
		}
	}

	// Drifts where the snow comes to rest: one near, two in the middle, three at the far end
	for (int32 Row = 0; Row < UE_ARRAY_COUNT(DriftRows); ++Row)
	{
		const int32 Count = Row + 1;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Across = Count == 1 ? 0.0f : FMath::Lerp(-0.65f, 0.65f, Index / (Count - 1.0f)) * HalfAngle;
			DriftSpots.Add(FVector2D(Across + FMath::FRandRange(-4.0f, 4.0f), Range * DriftRows[Row]));
		}
	}

	Flakes = NewObject<UVaelFlakesComponent>(this);
	Flakes->SetupAttachment(RootComponent);
	Flakes->RegisterComponent();
	Flakes->SetWorldLocation(GetActorLocation());
}

void AVaelAvalanche::UpdateCloud(float Front)
{
	if (Cloud == nullptr)
	{
		return;
	}
	const float Grow = Front / Range;
	const float Stopped = FMath::Max(Age - RollSeconds, 0.0f);
	const float WallShown = FMath::Clamp(Age / 0.12f, 0.0f, 1.0f) * FMath::Clamp(1.0f - Stopped / WallLinger, 0.0f, 1.0f);

	for (int32 Index = 0; Index < CardAngles.Num(); ++Index)
	{
		const float Phase = CardPhases[Index] + Age * 3.0f;
		FVector Place;
		float Size;
		float Solid;
		if (CardDistances[Index] <= 0.0f)
		{
			// The wall rolls on its own front, billowing up; once stopped it runs out a little further and thins
			const float Distance = Front + Stopped * 60.0f + FMath::Sin(Phase) * 15.0f;
			const float Height = 40.0f + CardRows[Index] * 70.0f + Stopped * 50.0f + FMath::Sin(Phase * 1.3f) * 12.0f;
			Place = Along(CardAngles[Index]) * Distance + FVector(0.0f, 0.0f, Height);
			Size = CardSizes[Index] * (0.45f + 0.55f * Grow) * (1.0f + Stopped * 0.4f);
			Solid = 0.9f * WallShown * (CardRows[Index] == 0 ? 1.0f : 0.75f);
		}
		else
		{
			// Powder left behind: shows when the wall has passed, settles slowly and thins away
			const float Since = Age - CardDistances[Index] / Range * RollSeconds;
			const float Left = Since / TrailLinger;
			Place = Along(CardAngles[Index]) * CardDistances[Index] + FVector(0.0f, 0.0f, 30.0f + 60.0f * FMath::Clamp(Left, 0.0f, 1.0f));
			Size = CardSizes[Index] * (1.0f + 0.4f * FMath::Clamp(Left, 0.0f, 1.0f));
			Solid = Front >= CardDistances[Index] ? 0.55f * FMath::Clamp(1.0f - Left, 0.0f, 1.0f) : 0.0f;
		}
		Cloud->SetCard(Index, Place, Size, Solid);
	}
	Cloud->Commit();
}

void AVaelAvalanche::UpdateLumps(float Front, float DeltaSeconds)
{
	const FVector Center = GetActorLocation();
	const float Sink = FMath::Clamp((Age - (Life - 1.0f)) / 0.5f, 0.0f, 1.0f);

	for (FLump& Lump : Lumps)
	{
		// Behind the front, rolling over and over as far as it has come, hopping on the snow
		const float Distance = FMath::Max(Front - Lump.Lag, 30.0f);
		const FVector Direction = Along(Lump.Angle);
		const float Roll = Distance / (Lump.Size * 0.5f);
		const float Hop = Age < RollSeconds ? FMath::Abs(FMath::Sin(Roll * 0.5f)) * Lump.Bounce : 0.0f;
		FVector Location = Center + Direction * Distance;
		if (Age < RollSeconds + 0.3f)
		{
			Lump.GroundZ = GroundAt(Location);
		}
		Location.Z = Lump.GroundZ + Lump.Size * 0.4f + Hop - Lump.Size * 0.4f * Sink;

		const FVector Axis = FVector::CrossProduct(FVector::UpVector, Direction);
		Lump.Mesh->SetWorldLocationAndRotation(Location, FQuat(Axis, Roll) * Lump.Start);
		Lump.Mesh->SetWorldScale3D(FVector(Lump.Size / RockSize * (1.0f - Sink) * FMath::Clamp(Age / 0.15f, 0.0f, 1.0f)));

		// It trails snow while it rolls
		Lump.NextFlake -= DeltaSeconds;
		if (Age < RollSeconds && Lump.NextFlake <= 0.0f && Flakes != nullptr)
		{
			Lump.NextFlake = LumpSnowEvery;
			const FVector Throw = -Direction * FMath::FRandRange(40.0f, 120.0f) + FVector(0.0f, 0.0f, FMath::FRandRange(120.0f, 260.0f));
			Flakes->Add(EVaelFlake::Snow, Location, Throw, FMath::FRandRange(4.0f, 7.0f), Location.Z - Lump.Size * 0.4f);
		}
	}
}

void AVaelAvalanche::UpdateCrusts()
{
	for (FCrust& Crust : Crusts)
	{
		const AActor* Target = Crust.Target.Get();
		const float Grown = FMath::Clamp((Age - Crust.Appear) / 0.12f, 0.0f, 1.0f) * FMath::Clamp(1.0f - (Age - Crust.Melt) / CrustMelt, 0.0f, 1.0f);
		for (int32 Piece = 0; Piece < Crust.Pieces.Num(); ++Piece)
		{
			UStaticMeshComponent* Mesh = Crust.Pieces[Piece];
			if (Target == nullptr || Grown <= 0.0f)
			{
				Mesh->SetWorldScale3D(FVector::ZeroVector);
				continue;
			}
			// Around the legs and up to the hips, leaning outwards like ice that grew there
			const float Radius = Target->GetSimpleCollisionRadius();
			const float HalfHeight = Target->GetSimpleCollisionHalfHeight();
			const float Angle = Piece * UE_TWO_PI / Crust.Pieces.Num() + Piece * 0.4f;
			const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
			const float Height = -HalfHeight + 10.0f + (Piece % 3) * HalfHeight * 0.3f;
			const FVector Location = Target->GetActorLocation() + Out * Radius * 0.75f + FVector(0.0f, 0.0f, Height);
			Mesh->SetWorldLocationAndRotation(Location, FRotationMatrix::MakeFromZ(Out + FVector(0.0f, 0.0f, 1.2f)).ToQuat());
			const float Size = Crust.Sizes[Piece] * Grown / RockSize;
			Mesh->SetWorldScale3D(FVector(Size, Size, Size * 1.6f));
		}
	}
}

void AVaelAvalanche::ThrowSnow(float Front, float DeltaSeconds)
{
	if (Flakes == nullptr || Age >= RollSeconds)
	{
		return;
	}
	const FVector Center = GetActorLocation();
	SnowCredit += FrontSnowRate * DeltaSeconds;
	for (; SnowCredit >= 1.0f; SnowCredit -= 1.0f)
	{
		// Flung up at the front and forwards, they come down after the wall
		const FVector Direction = Along(FMath::FRandRange(-HalfAngle, HalfAngle));
		FVector Location = Center + Direction * Front;
		const float GroundZ = GroundAt(Location);
		Location.Z = GroundZ + FMath::FRandRange(20.0f, 100.0f);
		const FVector Throw = Direction * FMath::FRandRange(150.0f, 400.0f) + FVector(0.0f, 0.0f, FMath::FRandRange(150.0f, 380.0f));
		Flakes->Add(EVaelFlake::Snow, Location, Throw, FMath::FRandRange(4.0f, 8.0f), GroundZ);
	}
}

void AVaelAvalanche::LayDrifts(float Front)
{
	const FVector Center = GetActorLocation();
	for (int32 Index = DriftSpots.Num() - 1; Index >= 0; --Index)
	{
		if (Front < DriftSpots[Index].Y)
		{
			continue;
		}
		FVector Location = Center + Along(DriftSpots[Index].X) * DriftSpots[Index].Y;
		Location.Z = GroundAt(Location);
		AVaelSnowDrift::Spawn(GetWorld(), Location, Range * DriftRadiusShare, GetInstigator());
		DriftSpots.RemoveAt(Index);
	}
}

void AVaelAvalanche::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;

	const float Front = FrontDistance();
	UpdateCloud(Front);
	UpdateLumps(Front, DeltaSeconds);
	UpdateCrusts();
	ThrowSnow(Front, DeltaSeconds);
	LayDrifts(Front);

	// The snow it threw fades with it at the end; the drifts stay on their own
	if (Flakes != nullptr)
	{
		Flakes->SetFade(FMath::Clamp((Life - Age) / 0.6f, 0.0f, 1.0f));
	}
}
