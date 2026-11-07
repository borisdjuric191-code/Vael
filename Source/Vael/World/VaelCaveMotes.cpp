// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelCaveMotes.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Magic/VaelMagicSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "VaelAssets.h"

namespace
{
	/** Size of the engine sphere in cm */
	constexpr float MoteSphereSize = 100.0f;

	/** Look of each kind: colour and glow of the hail material, mote size in cm */
	const FLinearColor MoteDustColor(1.0f, 0.86f, 0.62f);
	const FLinearColor MoteEmberColor(1.0f, 0.38f, 0.07f);
	constexpr float MoteDustGlow = 2.2f;
	constexpr float MoteEmberGlow = 9.0f;
	constexpr float MoteDustSize = 1.6f;
	constexpr float MoteEmberSize = 2.4f;

	/** Dust sinks slowly and sways; embers rise, wobble and go out after a few seconds */
	constexpr float MoteDustSink = 6.0f;
	constexpr float MoteDustSway = 9.0f;
	constexpr float MoteEmberRise = 70.0f;
	constexpr float MoteEmberWobble = 28.0f;
	constexpr float MoteEmberLifeMin = 2.5f;
	constexpr float MoteEmberLifeMax = 5.5f;
}

AVaelCaveMotes::AVaelCaveMotes()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.0f;

	Mesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Motes"));
	RootComponent = Mesh;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->bReceivesDecals = false;
	Mesh->SetMobility(EComponentMobility::Movable);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		Mesh->SetStaticMesh(Sphere.Object);
	}
}

void AVaelCaveMotes::BeginPlay()
{
	Super::BeginPlay();

	const bool bEmber = Kind == EVaelCaveMote::Ember;
	if (UMaterialInterface* Ice = VaelAssets::LoadOptional(UVaelMagicSettings::Get()->HailIceMaterial))
	{
		Material = UMaterialInstanceDynamic::Create(Ice, this);
		Material->SetVectorParameterValue(TEXT("Farbe"), bEmber ? MoteEmberColor : MoteDustColor);
		Material->SetScalarParameterValue(TEXT("Leuchten"), (bEmber ? MoteEmberGlow : MoteDustGlow) * Brightness);
		Mesh->SetMaterial(0, Material);
	}

	Mesh->ClearInstances();
	Motes.SetNum(Count);
	Transforms.SetNum(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Respawn(Motes[Index], true);
		Transforms[Index] = FTransform(Motes[Index].Location);
	}
	Mesh->AddInstances(Transforms, false, false);
}

void AVaelCaveMotes::Respawn(FMote& Mote, bool bAnywhere) const
{
	const bool bEmber = Kind == EVaelCaveMote::Ember;
	const FVector2D Disc = FMath::RandPointInCircle(Radius);
	const float Z = bEmber && !bAnywhere ? FMath::FRandRange(0.0f, Height * 0.1f) : FMath::FRandRange(0.0f, Height);
	Mote.Location = FVector(Disc.X, Disc.Y, Z);
	Mote.Phase = FMath::FRandRange(0.0f, UE_TWO_PI);
	Mote.Size = (bEmber ? MoteEmberSize : MoteDustSize) * FMath::FRandRange(0.6f, 1.3f);
	Mote.Life = FMath::FRandRange(MoteEmberLifeMin, MoteEmberLifeMax);
	Mote.Age = bAnywhere ? FMath::FRandRange(0.0f, Mote.Life) : 0.0f;
	Mote.Velocity = bEmber ? FVector(0.0f, 0.0f, MoteEmberRise * FMath::FRandRange(0.7f, 1.4f)) : FVector(0.0f, 0.0f, -MoteDustSink * FMath::FRandRange(0.5f, 1.5f));
}

void AVaelCaveMotes::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Motes.IsEmpty() || Mesh->GetInstanceCount() != Motes.Num())
	{
		return;
	}

	const bool bEmber = Kind == EVaelCaveMote::Ember;
	const float Time = GetWorld()->GetTimeSeconds();
	for (int32 Index = 0; Index < Motes.Num(); ++Index)
	{
		FMote& Mote = Motes[Index];
		Mote.Age += DeltaSeconds;
		const float Swing = bEmber ? MoteEmberWobble : MoteDustSway;
		const FVector Drift(FMath::Sin(Time * 0.7f + Mote.Phase) * Swing, FMath::Cos(Time * 0.53f + Mote.Phase * 1.7f) * Swing, 0.0f);
		Mote.Location += (Mote.Velocity + Drift) * DeltaSeconds;

		const bool bOut = Mote.Location.Z < 0.0f || Mote.Location.Z > Height || FVector2D(Mote.Location.X, Mote.Location.Y).SizeSquared() > FMath::Square(Radius * 1.2f);
		if (bOut || (bEmber && Mote.Age > Mote.Life))
		{
			Respawn(Mote, !bEmber);
		}

		// Dust twinkles as it turns in the light; embers shrink as they cool
		const float Shown = bEmber ? FMath::Clamp(1.0f - Mote.Age / Mote.Life, 0.0f, 1.0f) * (0.8f + 0.2f * FMath::Sin(Time * 13.0f + Mote.Phase))
			: 0.55f + 0.45f * FMath::Max(0.0f, FMath::Sin(Time * 1.3f + Mote.Phase * 3.0f));
		const float Scale = Mote.Size * Shown / MoteSphereSize;
		Transforms[Index] = FTransform(FQuat::Identity, Mote.Location, FVector(Scale));
	}
	Mesh->BatchUpdateInstancesTransforms(0, Transforms, false, true, true);
}
