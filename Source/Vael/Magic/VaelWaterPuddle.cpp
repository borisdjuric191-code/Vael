// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelWaterPuddle.h"
#include "Combat/VaelCombatStatics.h"
#include "Combat/VaelCharacterBase.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Magic/VaelElementSource.h"
#include "Magic/VaelMagicSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Vael.h"
#include "World/VaelRegion.h"

namespace
{
	/** Radius of the engine sphere */
	constexpr float SphereMeshRadius = 50.0f;

	/** Height of a blob in cm; half of it sinks into the ground */
	constexpr float BlobHeight = 4.0f;

	/** How far down water looks for ground to land on */
	constexpr float GroundSearchDistance = 500.0f;

	/** Seconds between two wetting steps */
	constexpr float WetStepInterval = 0.5f;

	/** A puddle with less water than this is gone */
	constexpr float DriedOutWater = 0.05f;

	/** Most blobs in one puddle; more splashes make the nearest blob bigger */
	constexpr int32 MaxBlobs = 12;

	/** The soft edge of the water material fades within this many cm of the ground, so a flat blob doesn't vanish into it */
	constexpr float PuddleContactFade = 1.5f;

	/** A puddle barely wobbles */
	constexpr float PuddleWobble = 0.4f;
}

AVaelWaterPuddle::AVaelWaterPuddle()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	BlobMesh = SphereMesh.Object;
}

AVaelWaterPuddle* AVaelWaterPuddle::AddSplash(UWorld* World, const FVector& Location, float Amount, UVaelElementSource* InSource, APawn* InInstigator)
{
	if (World == nullptr || Amount <= 0.0f)
	{
		return nullptr;
	}

	// The water falls straight down onto the ground, also when the ball burst on a body or in the air
	FHitResult Ground;
	if (!World->LineTraceSingleByObjectType(Ground, Location, Location - FVector(0.0f, 0.0f, GroundSearchDistance), FCollisionObjectQueryParams(ECC_WorldStatic)))
	{
		return nullptr;
	}

	const FVector GroundLocation = Ground.ImpactPoint;
	const float MergeDistance = UVaelMagicSettings::Get()->PuddleMergeDistance;

	// Join a puddle of the same water whose maker is on the same side
	for (TActorIterator<AVaelWaterPuddle> It(World); It; ++It)
	{
		AVaelWaterPuddle* Puddle = *It;
		if (Puddle->Source != InSource || !Puddle->Covers(GroundLocation, MergeDistance))
		{
			continue;
		}

		const bool bSameSide = Puddle->GetInstigator() == InInstigator || (Puddle->GetInstigator() != nullptr && InInstigator != nullptr && !UVaelCombatStatics::CanDamage(Puddle->GetInstigator(), InInstigator));
		if (bSameSide)
		{
			Puddle->AddWater(GroundLocation, Amount);
			return Puddle;
		}
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Instigator = InInstigator;
	SpawnParameters.Owner = InInstigator;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AVaelWaterPuddle* Puddle = World->SpawnActor<AVaelWaterPuddle>(AVaelWaterPuddle::StaticClass(), FTransform(GroundLocation), SpawnParameters);
	if (Puddle == nullptr)
	{
		return nullptr;
	}

	Puddle->Source = InSource;
	if (InSource != nullptr && InSource->Material != nullptr)
	{
		Puddle->Material = UMaterialInstanceDynamic::Create(InSource->Material, Puddle);
		Puddle->Material->SetScalarParameterValue(TEXT("ContactFade"), PuddleContactFade);
		Puddle->Material->SetScalarParameterValue(TEXT("WobbleStrength"), PuddleWobble);
	}

	Puddle->AddWater(GroundLocation, Amount);
	return Puddle;
}

bool AVaelWaterPuddle::IsDeepWaterAt(const UWorld* World, const FVector& Location)
{
	if (World == nullptr)
	{
		return false;
	}

	for (TActorIterator<AVaelWaterPuddle> It(World); It; ++It)
	{
		if (It->IsDeepEnough() && It->Covers(Location))
		{
			return true;
		}
	}

	return false;
}

float AVaelWaterPuddle::GetWater() const
{
	float Water = 0.0f;
	for (const float Amount : BlobWater)
	{
		Water += Amount;
	}

	return Water;
}

bool AVaelWaterPuddle::IsDeepEnough() const
{
	return GetWater() >= UVaelMagicSettings::Get()->PuddleWaterToCount;
}

bool AVaelWaterPuddle::Covers(const FVector& Location, float ExtraDistance) const
{
	for (int32 Index = 0; Index < Blobs.Num(); ++Index)
	{
		if (FVector::DistSquared2D(Location, Blobs[Index]->GetComponentLocation()) <= FMath::Square(GetBlobRadius(BlobWater[Index]) + ExtraDistance))
		{
			return true;
		}
	}

	return false;
}

void AVaelWaterPuddle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();

	// Rain keeps puddles, a drought dries them faster
	float DryPerSecond = MagicSettings->PuddleDryPerSecond;
	if (const AVaelRegion* Region = AVaelRegion::GetRegionAt(GetWorld(), GetActorLocation()))
	{
		DryPerSecond *= Region->GetWeather() == EVaelWeather::Rain ? 0.0f : Region->GetWeather() == EVaelWeather::Drought ? MagicSettings->PuddleDroughtDryMultiplier : 1.0f;
	}

	// Every blob shrinks by the same share, so the puddle keeps its shape while it dries
	const float Water = GetWater();
	const float Keep = Water > 0.0f ? FMath::Max(Water - DryPerSecond * DeltaSeconds, 0.0f) / Water : 0.0f;
	for (int32 Index = 0; Index < Blobs.Num(); ++Index)
	{
		BlobWater[Index] *= Keep;
		RefreshBlob(Index);
	}

	if (Water * Keep < DriedOutWater)
	{
		UE_LOG(LogVael, Verbose, TEXT("Puddle %s has dried out"), *GetName());
		Destroy();
		return;
	}

	WetStepTime += DeltaSeconds;
	if (WetStepTime < WetStepInterval || !IsDeepEnough())
	{
		return;
	}

	WetStepTime = 0.0f;

	// Whoever stands in deep enough water gets wet; a puddle of a caster only wets the caster's enemies
	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		if (!Covers(It->GetActorLocation()))
		{
			continue;
		}

		if (GetInstigator() == nullptr || UVaelCombatStatics::CanDamage(GetInstigator(), *It))
		{
			UVaelCombatStatics::ApplyStatus(GetInstigator(), *It, EVaelStatus::Wet, MagicSettings->PuddleWetDuration);
		}
	}
}

void AVaelWaterPuddle::AddWater(const FVector& GroundLocation, float Amount)
{
	const float MaxWater = UVaelMagicSettings::Get()->PuddleMaxWater;
	Amount = FMath::Min(Amount, FMath::Max(MaxWater - GetWater(), 0.0f));

	// Water that lands on a blob makes it bigger, otherwise it starts a blob of its own beside the others
	int32 Nearest = INDEX_NONE;
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	for (int32 Index = 0; Index < Blobs.Num(); ++Index)
	{
		const float DistanceSquared = FVector::DistSquared2D(GroundLocation, Blobs[Index]->GetComponentLocation());
		if (DistanceSquared < NearestDistanceSquared)
		{
			Nearest = Index;
			NearestDistanceSquared = DistanceSquared;
		}
	}

	const bool bOnBlob = Nearest != INDEX_NONE && NearestDistanceSquared <= FMath::Square(GetBlobRadius(BlobWater[Nearest]) * 0.5f);
	if (bOnBlob || Blobs.Num() >= MaxBlobs)
	{
		BlobWater[Nearest] += Amount;
		RefreshBlob(Nearest);
	}
	else
	{
		UStaticMeshComponent* Blob = NewObject<UStaticMeshComponent>(this);
		Blob->SetStaticMesh(BlobMesh);
		Blob->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Blob->SetCastShadow(false);
		Blob->SetupAttachment(RootComponent);
		Blob->RegisterComponent();
		Blob->SetWorldLocation(GroundLocation);
		Blob->SetWorldRotation(FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f));

		if (Material != nullptr)
		{
			Blob->SetMaterial(0, Material);
		}

		Blobs.Add(Blob);
		BlobWater.Add(Amount);
		RefreshBlob(Blobs.Num() - 1);
	}

	UE_LOG(LogVael, Verbose, TEXT("Puddle %s holds %.2f water in %d blobs%s"), *GetName(), GetWater(), Blobs.Num(), IsDeepEnough() ? TEXT(", deep enough to count") : TEXT(""));
}

float AVaelWaterPuddle::GetBlobRadius(float Water)
{
	// The puddle covers ground in step with its water
	return UVaelMagicSettings::Get()->PuddleRadiusPerWater * FMath::Sqrt(FMath::Max(Water, 0.0f));
}

void AVaelWaterPuddle::RefreshBlob(int32 Index)
{
	// Slightly oval, so neighbouring blobs read as one puddle and not as coins
	const float Radius = GetBlobRadius(BlobWater[Index]);
	Blobs[Index]->SetWorldScale3D(FVector(Radius * 1.15f, Radius * 0.9f, BlobHeight * 0.5f) / SphereMeshRadius);
}
