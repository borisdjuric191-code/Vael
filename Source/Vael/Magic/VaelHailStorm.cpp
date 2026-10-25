// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelHailStorm.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Magic/VaelCloudCards.h"
#include "Magic/VaelDebrisBurst.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelSpellEffects.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "VaelAssets.h"
#include "World/VaelGround.h"

namespace
{
	/** Size of the engine cone in cm; the cloud gathers and thins over these seconds; the hail stops this long before the end */
	constexpr float HailConeSize = 100.0f;
	constexpr float HailCloudFade = 0.4f;
	constexpr float HailTail = 0.7f;
}

AVaelHailStorm::AVaelHailStorm()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AVaelHailStorm* AVaelHailStorm::Spawn(AActor* Caster, const FVector& Center, float InRadius, const TArray<FVector>& Points, const TArray<float>& Delays)
{
	UWorld* World = Caster != nullptr ? Caster->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}
	FActorSpawnParameters Parameters;
	Parameters.Owner = Caster;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVaelHailStorm* Storm = World->SpawnActor<AVaelHailStorm>(AVaelHailStorm::StaticClass(), FTransform(Center), Parameters);
	if (Storm != nullptr)
	{
		Storm->Radius = InRadius;
		Storm->StrikePoints = Points;
		Storm->StrikeDelays = Delays;
		Storm->VolleysDone.Init(false, Points.Num());
		for (const float Delay : Delays)
		{
			Storm->LastStrike = FMath::Max(Storm->LastStrike, Delay);
		}
		Storm->SetLifeSpan(Storm->LastStrike + HailTail + HailCloudFade + 0.3f);
		Storm->Build();
	}
	return Storm;
}

void AVaelHailStorm::Build()
{
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();

	// The storm cloud: dark cards of rolling cloud over the patch, a little wider than it
	if (UMaterialInterface* CloudMaterial = VaelAssets::LoadOptional(MagicSettings->StormCloudMaterial))
	{
		Cloud = NewObject<UVaelCloudCardsComponent>(this);
		Cloud->SetupAttachment(RootComponent);
		Cloud->RegisterComponent();
		Cloud->Init(CloudMaterial);
		for (int32 Index = 0; Index < MagicSettings->HailCloudCards; ++Index)
		{
			const FVector2D Spot = FMath::RandPointInCircle(Radius * 1.15f);
			const FVector Place(Spot.X, Spot.Y, MagicSettings->HailCloudHeight + FMath::FRandRange(-70.0f, 70.0f));
			const float Size = Radius * FMath::FRandRange(0.6f, 0.95f);
			Cloud->AddCard(Place, Size, 0.0f);
			CloudPlaces.Add(Place);
			CloudSizes.Add(Size);
		}
		Cloud->Commit();
	}

	// The lances: small, pale, glowing cones of ice (a material made for instances)
	UMaterialInterface* IceMaterial = VaelAssets::LoadOptional(MagicSettings->HailIceMaterial);
	Lances = NewObject<UInstancedStaticMeshComponent>(this);
	Lances->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone")));
	Lances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Lances->SetCastShadow(false);
	Lances->SetupAttachment(RootComponent);
	Lances->SetUsingAbsoluteScale(true);
	Lances->RegisterComponent();
	if (UMaterialInstanceDynamic* Ice = IceMaterial != nullptr ? UMaterialInstanceDynamic::Create(IceMaterial, this) : nullptr)
	{
		Ice->SetVectorParameterValue(TEXT("Farbe"), MagicSettings->HailColor);
		Lances->SetMaterial(0, Ice);
	}
	Stones.SetNum(MagicSettings->HailMaxLances);
	for (int32 Index = 0; Index < Stones.Num(); ++Index)
	{
		StoneTransforms.Add(FTransform(FQuat::Identity, GetActorLocation(), FVector::ZeroVector));
		Lances->AddInstance(StoneTransforms.Last(), true);
	}
}

void AVaelHailStorm::Drop(const FVector& Landing)
{
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	for (FStone& Stone : Stones)
	{
		if (Stone.bActive)
		{
			continue;
		}
		// Slanting down a little with the storm's wind, from the cloud
		const FVector Velocity = FVector(MagicSettings->HailSlant.X, MagicSettings->HailSlant.Y, -1.0f).GetSafeNormal() * MagicSettings->HailSpeed;
		const float FallTime = (MagicSettings->HailCloudHeight - 40.0f) / -Velocity.Z;
		Stone.Location = Landing - Velocity * FallTime;
		Stone.Velocity = Velocity * FMath::FRandRange(0.9f, 1.1f);
		Stone.GroundZ = Landing.Z;
		Stone.Length = FMath::FRandRange(MagicSettings->HailLength.X, MagicSettings->HailLength.Y);
		Stone.bActive = true;
		return;
	}
}

void AVaelHailStorm::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	Age += DeltaSeconds;
	UWorld* World = GetWorld();
	const FVector Center = GetActorLocation();
	const float FallTime = (MagicSettings->HailCloudHeight - 40.0f) / MagicSettings->HailSpeed;

	// The cloud gathers, churns darkly and thins away after the last strike
	if (Cloud != nullptr)
	{
		const float Life = GetLifeSpan() > 0.0f ? GetLifeSpan() : 1.0f;
		const float Shown = FMath::Min(FMath::Clamp(Age / HailCloudFade, 0.0f, 1.0f), FMath::Clamp(Life / HailCloudFade, 0.0f, 1.0f));
		for (int32 Index = 0; Index < CloudPlaces.Num(); ++Index)
		{
			const float Phase = Age * 0.5f + Index * 1.7f;
			const FVector Churn = FVector(FMath::Sin(Phase), FMath::Cos(Phase * 0.8f), 0.0f) * 30.0f;
			Cloud->SetCard(Index, CloudPlaces[Index] * FMath::Lerp(0.6f, 1.0f, Shown) + Churn, CloudSizes[Index], 0.8f * Shown);
		}
		Cloud->Commit();
	}

	// Hail all over the patch while the strikes come, starting as soon as it can reach the ground with the first one
	if (Age < LastStrike + 0.2f)
	{
		DropCredit += MagicSettings->HailRate * DeltaSeconds;
		while (DropCredit >= 1.0f)
		{
			DropCredit -= 1.0f;
			const FVector2D Spot = FMath::RandPointInCircle(Radius);
			const FVector Above = Center + FVector(Spot.X, Spot.Y, 200.0f);
			FHitResult GroundHit;
			const FVector Landing = VaelGround::TraceGround(World, Above, Above - FVector(0.0f, 0.0f, 600.0f), GroundHit, GetOwner()) ? GroundHit.Location : FVector(Above.X, Above.Y, Center.Z);
			Drop(Landing);
		}
	}

	// A volley on each strike, landing just as it hits
	for (int32 Index = 0; Index < StrikePoints.Num(); ++Index)
	{
		if (!VolleysDone[Index] && Age >= StrikeDelays[Index] - FallTime)
		{
			VolleysDone[Index] = true;
			for (int32 Lance = 0; Lance < MagicSettings->HailVolley; ++Lance)
			{
				const FVector2D Spot = FMath::RandPointInCircle(MagicSettings->HailVolleySpread);
				Drop(StrikePoints[Index] + FVector(Spot.X, Spot.Y, 0.0f));
			}
		}
	}

	// The lances fall, point first; on the ground they burst into a few splinters now and then
	for (int32 Index = 0; Index < Stones.Num(); ++Index)
	{
		FStone& Stone = Stones[Index];
		if (Stone.bActive)
		{
			Stone.Location += Stone.Velocity * DeltaSeconds;
			if (Stone.Location.Z <= Stone.GroundZ)
			{
				Stone.bActive = false;
				if (++NumLanded % MagicSettings->HailSplinterEvery == 0)
				{
					FVaelDebris Splinters;
					Splinters.Count = 3;
					Splinters.Spread = 6.0f;
					Splinters.Speed = 160.0f;
					Splinters.Lift = 150.0f;
					Splinters.Lifetime = 0.35f;
					Splinters.MinSize = 2.0f;
					Splinters.MaxSize = 4.0f;
					Splinters.Color = MagicSettings->HailColor;
					Splinters.Glow = 0.4f;
					AVaelDebrisBurst::Spawn(this, FVector(Stone.Location.X, Stone.Location.Y, Stone.GroundZ + 4.0f), Splinters);
				}
			}
		}
		if (Stone.bActive)
		{
			const FVector Down = Stone.Velocity.GetSafeNormal();
			const float Width = MagicSettings->HailWidth;
			StoneTransforms[Index] = FTransform(FRotationMatrix::MakeFromZ(Down).ToQuat(), Stone.Location, FVector(Width, Width, Stone.Length) / HailConeSize);
		}
		else
		{
			StoneTransforms[Index].SetScale3D(FVector::ZeroVector);
		}
	}
	Lances->BatchUpdateInstancesTransforms(0, StoneTransforms, true, true, true);
}
