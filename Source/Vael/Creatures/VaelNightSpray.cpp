// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelNightSpray.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Creatures/VaelPollenBurst.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "World/VaelGround.h"

namespace
{
	const TCHAR* SporeMeshPath = TEXT("/Game/Vael/Kreaturen/Wurzelforst/Moderglocke/Effekte/SM_Moderglocke_Spore.SM_Moderglocke_Spore");
	const TCHAR* DropMeshPath = TEXT("/Game/Vael/Kreaturen/Wurzelforst/Moderglocke/Effekte/SM_Moderglocke_Gifttropfen.SM_Moderglocke_Gifttropfen");
	const TCHAR* BlotMeshPath = TEXT("/Game/Vael/Kreaturen/Wurzelforst/Moderglocke/Effekte/SM_Moderglocke_Giftlache.SM_Moderglocke_Giftlache");

	/** Spores: rust red with a darker heart, glowing a little like the cap (the Distelspinne's puffs are pink) */
	const FLinearColor SporeColor(0.5f, 0.09f, 0.04f);
	const FLinearColor SporeCloudColor(0.36f, 0.09f, 0.08f);
	const FLinearColor SporeCloudAccent(0.62f, 0.1f, 0.07f);

	/** Poison: the cap's ichor, a murky brown-olive bile that shines wet; it rots what it touches, it doesn't burn */
	const FLinearColor VenomColor(0.06f, 0.047f, 0.012f);

	constexpr int32 SprayNumSpores = 90;
	constexpr int32 SprayNumDrops = 64;

	/** Seconds a spore drifts, and a blot of poison lies before it seeps away */
	constexpr float SporeLife = 3.4f;
	constexpr float BlotLife = 3.2f;
	constexpr float VenomGravity = 980.0f;

	/** Largest side of the meshes in cm: burr, drop (its length) and blot */
	constexpr float SporeMeshSize = 80.0f;
	constexpr float DropMeshLength = 100.0f;
	constexpr float DropMeshWidth = 32.0f;
	constexpr float BlotMeshSize = 80.0f;
}

AVaelNightSpray::AVaelNightSpray()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AVaelNightSpray* AVaelNightSpray::Spawn(UWorld* World, EVaelNightSpray Kind, const FVector& From, float RimRadius, float Reach, float Scale)
{
	if (World == nullptr || Kind == EVaelNightSpray::None)
	{
		return nullptr;
	}

	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVaelNightSpray* Spray = World->SpawnActor<AVaelNightSpray>(AVaelNightSpray::StaticClass(), FTransform(From), Parameters);
	if (Spray != nullptr)
	{
		Spray->Build(Kind, From, RimRadius, Reach, Scale);
	}
	return Spray;
}

UInstancedStaticMeshComponent* AVaelNightSpray::AddPieces(const TCHAR* MeshPath, const FLinearColor& Color, float Glow, float Roughness)
{
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, MeshPath);
	if (Mesh == nullptr)
	{
		return nullptr;
	}

	UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(this);
	Component->SetupAttachment(RootComponent);
	Component->SetStaticMesh(Mesh);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCastShadow(false);
	Component->bReceivesDecals = false;
	Component->SetUsingAbsoluteLocation(true);
	Component->SetUsingAbsoluteRotation(true);
	Component->SetUsingAbsoluteScale(true);
	Component->RegisterComponent();
	Component->SetWorldTransform(FTransform::Identity);
	if (UMaterialInstanceDynamic* Material = Component->CreateDynamicMaterialInstance(0))
	{
		Material->SetVectorParameterValue(TEXT("Farbe"), Color);
		Material->SetScalarParameterValue(TEXT("Glut"), Glow);
		Material->SetScalarParameterValue(TEXT("Rauheit"), Roughness);
	}

	// Out of sight until its piece starts
	const FTransform Hidden(FQuat::Identity, GetActorLocation(), FVector::ZeroVector);
	for (int32 Index = 0; Index < Pieces.Num(); ++Index)
	{
		Component->AddInstance(Hidden, true);
	}
	return Component;
}

void AVaelNightSpray::Build(EVaelNightSpray InKind, const FVector& From, float RimRadius, float Reach, float Scale)
{
	Kind = InKind;

	FHitResult GroundHit;
	GroundZ = VaelGround::TraceGround(GetWorld(), From, From - FVector(0.0f, 0.0f, 1200.0f), GroundHit) ? GroundHit.Location.Z : From.Z - 250.0f;

	if (Kind == EVaelNightSpray::Spores)
	{
		// Blown out of the cap in every direction, more up than down, then the air holds them
		Lifetime = SporeLife + 0.4f;
		Pieces.SetNum(SprayNumSpores);
		for (FPiece& Piece : Pieces)
		{
			const FVector Out = FVector(FMath::VRand().X, FMath::VRand().Y, FMath::FRandRange(-0.25f, 0.9f)).GetSafeNormal();
			Piece.Start = From + Out * RimRadius * FMath::FRandRange(0.2f, 0.8f);
			Piece.Velocity = Out * Reach * FMath::FRandRange(1.2f, 3.0f);
			Piece.Tumble = FRotator(FMath::FRandRange(-200.0f, 200.0f), FMath::FRandRange(-200.0f, 200.0f), FMath::FRandRange(-200.0f, 200.0f));
			Piece.Delay = FMath::FRandRange(0.12f, 0.4f);
			Piece.Size = FMath::FRandRange(2.5f, 6.5f) * FMath::Sqrt(Scale);
			Piece.Seed = FMath::FRandRange(0.0f, 100.0f);
		}
		Flying = AddPieces(SporeMeshPath, SporeColor, 2.2f, 0.85f);
		AVaelPollenBurst::Spawn(GetWorld(), From, Reach * 0.65f, SporeCloudColor, SporeCloudAccent, 2.4f);
	}
	else
	{
		// Flung from the rim of the cap, each drop in its own arc to its own spot around the creature
		Lifetime = 2.0f + BlotLife;
		Pieces.SetNum(SprayNumDrops);
		for (FPiece& Piece : Pieces)
		{
			const float RimAngle = FMath::FRandRange(0.0f, UE_TWO_PI);
			const float LandAngle = RimAngle + FMath::FRandRange(-0.5f, 0.5f);
			const float LandOut = FMath::Max(RimRadius * 1.3f, Reach * FMath::Sqrt(FMath::FRand()));
			Piece.Start = From + FVector(FMath::Cos(RimAngle), FMath::Sin(RimAngle), 0.0f) * RimRadius * FMath::FRandRange(0.8f, 1.0f);
			const FVector Land(From.X + FMath::Cos(LandAngle) * LandOut, From.Y + FMath::Sin(LandAngle) * LandOut, GroundZ);
			Piece.FlightTime = FMath::FRandRange(0.5f, 0.8f);
			Piece.Velocity = (Land - Piece.Start) / Piece.FlightTime;
			Piece.Velocity.Z += 0.5f * VenomGravity * Piece.FlightTime;
			Piece.Delay = FMath::FRandRange(0.0f, 0.55f);
			Piece.Size = FMath::FRandRange(0.7f, 1.3f) * FMath::Sqrt(Scale);
			Piece.Seed = FMath::FRandRange(0.0f, 360.0f);
		}
		Flying = AddPieces(DropMeshPath, VenomColor, 1.2f, 0.12f);
		Landed = AddPieces(BlotMeshPath, VenomColor * 0.8f, 0.7f, 0.08f);
	}

	if (Flying == nullptr)
	{
		Destroy();
		return;
	}
	Tick(0.0f);
}

void AVaelNightSpray::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;
	if (Age >= Lifetime)
	{
		Destroy();
		return;
	}

	const FTransform Hidden(FQuat::Identity, GetActorLocation(), FVector::ZeroVector);
	TArray<FTransform> FlyingTransforms;
	TArray<FTransform> LandedTransforms;
	FlyingTransforms.Init(Hidden, Pieces.Num());
	LandedTransforms.Init(Hidden, Landed != nullptr ? Pieces.Num() : 0);

	for (int32 Index = 0; Index < Pieces.Num(); ++Index)
	{
		const FPiece& Piece = Pieces[Index];
		const float Time = Age - Piece.Delay;
		if (Time <= 0.0f)
		{
			continue;
		}

		if (Kind == EVaelNightSpray::Spores)
		{
			if (Time >= SporeLife)
			{
				continue;
			}
			// The burst is spent within half a second; after it they hang, sway and sink
			const float Drag = 3.5f;
			const FVector Blown = Piece.Velocity * (1.0f - FMath::Exp(-Drag * Time)) / Drag;
			const FVector Sway(FMath::Sin(Time * 1.7f + Piece.Seed) * 9.0f, FMath::Cos(Time * 1.3f + Piece.Seed * 1.9f) * 9.0f, -14.0f * Time);
			FVector Place = Piece.Start + Blown + Sway * FMath::Min(Time, 1.0f);
			Place.Z = FMath::Max(Place.Z, GroundZ + 2.0f);
			const float Shown = FMath::Min(FMath::Clamp(Time / 0.12f, 0.0f, 1.0f), FMath::Clamp((SporeLife - Time) / 0.9f, 0.0f, 1.0f));
			FlyingTransforms[Index] = FTransform((Piece.Tumble * Time).Quaternion(), Place, FVector(Piece.Size * Shown / SporeMeshSize));
			continue;
		}

		if (Time < Piece.FlightTime)
		{
			// The heavy end leads, the thread trails; the faster it falls, the longer it is drawn
			const FVector Place = Piece.Start + Piece.Velocity * Time - FVector(0.0f, 0.0f, 0.5f * VenomGravity * Time * Time);
			const FVector Speed = Piece.Velocity - FVector(0.0f, 0.0f, VenomGravity * Time);
			const float Width = 5.0f * Piece.Size * FMath::Clamp(Time / 0.08f, 0.0f, 1.0f);
			const float Length = Width * (2.2f + Speed.Size() / 320.0f);
			FlyingTransforms[Index] = FTransform(FRotationMatrix::MakeFromZ(-Speed).ToQuat(), Place, FVector(Width / DropMeshWidth, Width / DropMeshWidth, Length / DropMeshLength));
		}
		else if (Landed != nullptr && Time < Piece.FlightTime + BlotLife)
		{
			// Splashes out wide at once, lies there and seeps away
			const float Lying = Time - Piece.FlightTime;
			const float Spread = FMath::InterpEaseOut(0.25f, 1.0f, FMath::Clamp(Lying / 0.18f, 0.0f, 1.0f), 2.5f) * FMath::Clamp((BlotLife - Lying) / 0.8f, 0.0f, 1.0f);
			const FVector Place = Piece.Start + Piece.Velocity * Piece.FlightTime - FVector(0.0f, 0.0f, 0.5f * VenomGravity * Piece.FlightTime * Piece.FlightTime);
			const float Width = 34.0f * Piece.Size * Spread;
			LandedTransforms[Index] = FTransform(FRotator(0.0f, Piece.Seed, 0.0f).Quaternion(), FVector(Place.X, Place.Y, GroundZ + 1.0f), FVector(Width / BlotMeshSize, Width / BlotMeshSize, 0.6f));
		}
	}

	Flying->BatchUpdateInstancesTransforms(0, FlyingTransforms, true, true, true);
	if (Landed != nullptr)
	{
		Landed->BatchUpdateInstancesTransforms(0, LandedTransforms, true, true, true);
	}
}
