// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelSnowDrift.h"
#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelCombatStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Magic/VaelDebrisBurst.h"
#include "Magic/VaelElementSource.h"
#include "Magic/VaelGroundArea.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelSpellEffects.h"
#include "Magic/VaelWaterPuddle.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraSystem.h"
#include "ProceduralMeshComponent.h"
#include "Vael.h"
#include "VaelAssets.h"
#include "World/VaelAtmosphere.h"
#include "World/VaelFireSpot.h"
#include "World/VaelWorldSettings.h"

namespace
{
	/** Rings and steps around of the mound, its height in the middle in cm; the rim sinks a little into the ground */
	constexpr int32 MoundRings = 7;
	constexpr int32 MoundSteps = 28;
	constexpr float MoundHeight = 30.0f;
	constexpr float MoundSink = 4.0f;

	/** Seconds it takes to pile up, and share of its life over which it thaws at the end */
	constexpr float PileSeconds = 0.25f;
	constexpr float ThawShare = 0.3f;

	/** Seconds between two looks for fire */
	constexpr float SnowFireCheckInterval = 0.4f;

	/** Not quite white: in full sun brighter snow burns out to a flat white shape */
	const FLinearColor SnowColor = FLinearColor(0.55f, 0.6f, 0.68f);
}

AVaelSnowDrift::AVaelSnowDrift()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AVaelSnowDrift* AVaelSnowDrift::Spawn(UWorld* World, const FVector& Ground, float InRadius, APawn* InInstigator)
{
	if (World == nullptr)
	{
		return nullptr;
	}
	FActorSpawnParameters Parameters;
	Parameters.Instigator = InInstigator;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVaelSnowDrift* Drift = World->SpawnActor<AVaelSnowDrift>(AVaelSnowDrift::StaticClass(), FTransform(FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), Ground), Parameters);
	if (Drift != nullptr)
	{
		Drift->Radius = InRadius;
		Drift->Build();
		// Snow cools the air around it
		UVaelAtmosphere::Add(World, Ground, EVaelAir::Cold, UVaelWorldSettings::Get()->HailIceCold * FMath::Clamp(InRadius / 80.0f, 0.3f, 2.0f));
	}
	return Drift;
}

void AVaelSnowDrift::Build()
{
	Mound = NewObject<UProceduralMeshComponent>(this);
	Mound->SetupAttachment(RootComponent);
	Mound->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mound->bReceivesDecals = false;
	Mound->RegisterComponent();

	// An uneven heap: wavy outline, highest a little off the middle, sinking into the ground at the rim
	const float OutlinePhase = FMath::FRandRange(0.0f, UE_TWO_PI);
	const float BumpPhase = FMath::FRandRange(0.0f, UE_TWO_PI);
	TArray<FVector> Vertices;
	Vertices.Add(FVector(0.0f, 0.0f, MoundHeight));
	for (int32 Ring = 1; Ring <= MoundRings; ++Ring)
	{
		const float Share = Ring / static_cast<float>(MoundRings);
		for (int32 Step = 0; Step < MoundSteps; ++Step)
		{
			const float Angle = Step * UE_TWO_PI / MoundSteps;
			const float Edge = Radius * (1.0f + 0.12f * FMath::Sin(3.0f * Angle + OutlinePhase) + 0.07f * FMath::Sin(5.0f * Angle + OutlinePhase * 1.7f));
			const float Bump = 1.0f + 0.3f * Share * FMath::Sin(4.0f * Angle + BumpPhase) * FMath::Sin(Share * 5.0f + BumpPhase)
				+ 0.15f * Share * FMath::Sin(9.0f * Angle + BumpPhase * 2.0f);
			const float Height = (MoundHeight + MoundSink) * FMath::Pow(FMath::Max(1.0f - Share * Share, 0.0f), 0.8f) * Bump - MoundSink;
			Vertices.Add(FVector(FMath::Cos(Angle) * Edge * Share, FMath::Sin(Angle) * Edge * Share, Height));
		}
	}

	TArray<int32> Triangles;
	const auto RingVertex = [](int32 Ring, int32 Step) { return 1 + (Ring - 1) * MoundSteps + (Step % MoundSteps); };
	for (int32 Step = 0; Step < MoundSteps; ++Step)
	{
		Triangles.Append({ 0, RingVertex(1, Step + 1), RingVertex(1, Step) });
	}
	for (int32 Ring = 1; Ring < MoundRings; ++Ring)
	{
		for (int32 Step = 0; Step < MoundSteps; ++Step)
		{
			const int32 A = RingVertex(Ring, Step);
			const int32 B = RingVertex(Ring, Step + 1);
			const int32 C = RingVertex(Ring + 1, Step);
			const int32 D = RingVertex(Ring + 1, Step + 1);
			Triangles.Append({ A, B, C, B, D, C });
		}
	}

	// Smooth normals from the faces around each point
	TArray<FVector> Normals;
	Normals.Init(FVector::ZeroVector, Vertices.Num());
	for (int32 Index = 0; Index + 2 < Triangles.Num(); Index += 3)
	{
		const FVector Face = FVector::CrossProduct(Vertices[Triangles[Index + 2]] - Vertices[Triangles[Index]], Vertices[Triangles[Index + 1]] - Vertices[Triangles[Index]]);
		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			Normals[Triangles[Index + Corner]] += Face;
		}
	}
	for (FVector& Normal : Normals)
	{
		Normal = Normal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		Normal = Normal.Z < 0.0f ? -Normal : Normal;
	}

	// Both windings, so it shows whichever way the engine faces it
	const int32 OneSide = Triangles.Num();
	for (int32 Index = 0; Index < OneSide; Index += 3)
	{
		Triangles.Append({ Triangles[Index], Triangles[Index + 2], Triangles[Index + 1] });
	}

	TArray<FVector2D> UVs;
	for (const FVector& Vertex : Vertices)
	{
		UVs.Add(FVector2D(Vertex.X, Vertex.Y) / (Radius * 2.0f));
	}
	Mound->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);

	// Lit, so the heap shows its shape in light and shadow
	if (UMaterialInterface* Plain = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		UMaterialInstanceDynamic* Snow = UMaterialInstanceDynamic::Create(Plain, this);
		Snow->SetVectorParameterValue(TEXT("Color"), SnowColor);
		Mound->SetMaterial(0, Snow);
	}
	Mound->SetRelativeScale3D(FVector(1.0f, 1.0f, 0.01f));
}

float AVaelSnowDrift::GetSpeedMultiplierAt(const AActor* Victim)
{
	const UWorld* World = Victim != nullptr ? Victim->GetWorld() : nullptr;
	const AVaelCharacterBase* Character = Cast<AVaelCharacterBase>(Victim);
	if (World == nullptr || (Character != nullptr && Character->IsAboveGround()))
	{
		return 1.0f;
	}
	for (TActorIterator<AVaelSnowDrift> It(World); It; ++It)
	{
		if (It->MeltAge < 0.0f && FVector::Dist2D(It->GetActorLocation(), Victim->GetActorLocation()) <= It->Radius
			&& (It->GetInstigator() == nullptr || UVaelCombatStatics::CanDamage(It->GetInstigator(), Victim)))
		{
			return UVaelMagicSettings::Get()->SnowSpeedMultiplier;
		}
	}
	return 1.0f;
}

void AVaelSnowDrift::MeltAround(const UWorld* World, const FVector& Location, float InRadius)
{
	for (TActorIterator<AVaelSnowDrift> It(World); World != nullptr && It; ++It)
	{
		if (It->MeltAge < 0.0f && FVector::Dist2D(It->GetActorLocation(), Location) <= InRadius + It->Radius)
		{
			It->MeltAge = 0.0f;
		}
	}
}

void AVaelSnowDrift::MeltAlong(const UWorld* World, const FVector& Start, const FVector& End, float Width)
{
	for (TActorIterator<AVaelSnowDrift> It(World); World != nullptr && It; ++It)
	{
		// (on the ground plane: a beam runs at chest height over snow lying on the ground)
		const FVector Flat(1.0f, 1.0f, 0.0f);
		if (It->MeltAge < 0.0f && FMath::PointDistToSegment(It->GetActorLocation() * Flat, Start * Flat, End * Flat) <= Width + It->Radius)
		{
			It->MeltAge = 0.0f;
		}
	}
}

bool AVaelSnowDrift::IsFireClose() const
{
	const UWorld* World = GetWorld();
	if (AVaelGroundArea::IsFireNear(World, GetActorLocation(), Radius * 0.5f))
	{
		return true;
	}
	for (TActorIterator<AVaelFireSpot> It(World); It; ++It)
	{
		if (FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) <= Radius)
		{
			return true;
		}
	}
	return false;
}

void AVaelSnowDrift::BecomeWater()
{
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	const FVector Ground = GetActorLocation();
	UWorld* World = GetWorld();

	VaelEffects::SpawnAt(this, VaelAssets::LoadOptional(MagicSettings->SteamEffect), Ground + FVector(0.0f, 0.0f, 20.0f), FRotator::ZeroRotator, MagicSettings->SteamColor, Radius * 0.6f);
	UVaelAtmosphere::Add(this, Ground, EVaelAir::Moisture, UVaelWorldSettings::Get()->IceMeltMoisture);

	FVaelDebris Drops;
	Drops.Count = 8;
	Drops.Spread = Radius * 0.5f;
	Drops.Speed = 160.0f;
	Drops.Lift = 200.0f;
	Drops.Lifetime = 0.45f;
	Drops.MinSize = 2.0f;
	Drops.MaxSize = 4.0f;
	Drops.Color = FLinearColor(0.45f, 0.7f, 0.95f);
	Drops.Glow = 0.3f;
	AVaelDebrisBurst::Spawn(this, Ground + FVector(0.0f, 0.0f, 10.0f), Drops);

	// The meltwater: a puddle the mage can draw on (as the mage's water, or it has no look), and everyone in it is wet
	UVaelElementSource* MageWater = VaelAssets::LoadOptional(MagicSettings->MageWaterSource);
	AVaelWaterPuddle::AddSplash(World, Ground + FVector(0.0f, 0.0f, 30.0f), MagicSettings->SnowMeltWater, MageWater, GetInstigator());
	int32 NumWet = 0;
	for (TActorIterator<AVaelCharacterBase> It(World); It; ++It)
	{
		if (!It->IsAboveGround() && FVector::Dist2D(It->GetActorLocation(), Ground) <= Radius && UVaelCombatStatics::CanDamage(GetInstigator(), *It))
		{
			UVaelCombatStatics::ApplyStatus(GetInstigator(), *It, EVaelStatus::Wet, MagicSettings->SnowWetSeconds);
			++NumWet;
		}
	}
	UE_LOG(LogVael, Log, TEXT("Snow melts into water at %s, %d wet"), *Ground.ToCompactString(), NumWet);
	Destroy();
}

void AVaelSnowDrift::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	Age += DeltaSeconds;

	// Fire close by sets it melting
	NextFireCheck -= DeltaSeconds;
	if (MeltAge < 0.0f && NextFireCheck <= 0.0f)
	{
		NextFireCheck = SnowFireCheckInterval;
		if (IsFireClose())
		{
			MeltAge = 0.0f;
		}
	}

	// Melting in fire it sinks in moments and runs into a puddle; left alone it piles up, lies and thaws away
	float Height = FMath::Clamp(Age / PileSeconds, 0.0f, 1.0f);
	float Width = 1.0f;
	if (MeltAge >= 0.0f)
	{
		MeltAge += DeltaSeconds;
		const float Melted = FMath::Clamp(MeltAge / MagicSettings->SnowMeltSeconds, 0.0f, 1.0f);
		if (Melted >= 1.0f)
		{
			BecomeWater();
			return;
		}
		Height *= 1.0f - Melted;
		Width = 1.0f - 0.2f * Melted;
	}
	else
	{
		const float ThawStart = MagicSettings->SnowLieSeconds * (1.0f - ThawShare);
		const float Thawed = FMath::Clamp((Age - ThawStart) / (MagicSettings->SnowLieSeconds - ThawStart), 0.0f, 1.0f);
		if (Thawed >= 1.0f)
		{
			Destroy();
			return;
		}
		Height *= 1.0f - Thawed;
		Width = 1.0f - 0.25f * Thawed;
	}

	if (Mound != nullptr)
	{
		Mound->SetRelativeScale3D(FVector(Width, Width, FMath::Max(Height, 0.01f)));
	}
}
