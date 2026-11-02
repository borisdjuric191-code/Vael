// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelFlakes.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Magic/VaelMagicSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "VaelAssets.h"
#include "World/VaelRegion.h"

namespace
{
	constexpr int32 FlakeKinds = 4;

	/** Size of the engine sphere in cm; a flake is a disc this thin against its width */
	constexpr float SphereSize = 100.0f;
	constexpr float FlakeThickness = 0.16f;
	constexpr float LyingThickness = 0.1f;

	/** Most flakes at once; beyond that the oldest lying ones go first */
	constexpr int32 MaxFlakes = 900;

	/** How a kind falls: sinking speed in cm/s, how wide and how often it sways, how fast it tumbles (turns per second) */
	struct FFlakeMotion
	{
		float Fall;
		float Sway;
		float SwayRate;
		float SpinRate;
	};
	const FFlakeMotion Motions[FlakeKinds] =
	{
		{ 85.0f, 16.0f, 1.6f, 1.2f },	// Snow
		{ 120.0f, 18.0f, 1.0f, 0.8f },	// Soot: turns slowly, flipping fast looks like confetti
		{ 260.0f, 6.0f, 3.0f, 5.0f },	// Ember
		{ 150.0f, 12.0f, 1.8f, 2.5f }	// Ash
	};

	/** How a kind looks: colour and glow of the ice material */
	const FLinearColor Colors[FlakeKinds] =
	{
		FLinearColor(0.92f, 0.95f, 1.0f),
		FLinearColor(0.03f, 0.03f, 0.035f),
		FLinearColor(1.0f, 0.42f, 0.08f),
		FLinearColor(0.32f, 0.31f, 0.3f)
	};
	const float Glows[FlakeKinds] = { 0.85f, 1.0f, 6.0f, 0.7f };

	/** Share of the wind a flake drifts with, and how quickly it takes on the falling speed and the wind */
	constexpr float WindShare = 0.35f;
	constexpr float FallCatchUp = 2.5f;
	constexpr float DriftCatchUp = 1.2f;

	/** An ember still this hot on landing can light a fire; landed it glows on for this long before it is ash */
	constexpr float HotLanding = 0.35f;
	constexpr float EmberAfterglow = 0.8f;
}

UVaelFlakesComponent::UVaelFlakesComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetUsingAbsoluteLocation(true);
	SetUsingAbsoluteRotation(true);
	SetUsingAbsoluteScale(true);
}

void UVaelFlakesComponent::Build()
{
	UMaterialInterface* IceMaterial = VaelAssets::LoadOptional(UVaelMagicSettings::Get()->HailIceMaterial);
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	Frames.SetNum(FlakeKinds);
	for (int32 Kind = 0; Kind < FlakeKinds; ++Kind)
	{
		UInstancedStaticMeshComponent* Mesh = NewObject<UInstancedStaticMeshComponent>(GetOwner());
		Mesh->SetStaticMesh(Sphere);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		Mesh->bReceivesDecals = false;
		Mesh->SetupAttachment(this);
		Mesh->SetUsingAbsoluteLocation(true);
		Mesh->SetUsingAbsoluteRotation(true);
		Mesh->SetUsingAbsoluteScale(true);
		Mesh->SetWorldTransform(FTransform::Identity);
		Mesh->RegisterComponent();
		if (UMaterialInstanceDynamic* Material = IceMaterial != nullptr ? UMaterialInstanceDynamic::Create(IceMaterial, this) : nullptr)
		{
			Material->SetVectorParameterValue(TEXT("Farbe"), Colors[Kind]);
			Material->SetScalarParameterValue(TEXT("Leuchten"), Glows[Kind]);
			Mesh->SetMaterial(0, Material);
		}
		Meshes.Add(Mesh);
	}
}

void UVaelFlakesComponent::Add(EVaelFlake Kind, const FVector& Location, const FVector& Velocity, float Size, float GroundZ)
{
	if (Meshes.IsEmpty())
	{
		Build();
	}

	// Full: the oldest flake lying on the ground makes room, or else the oldest of all
	if (Flakes.Num() >= MaxFlakes)
	{
		const int32 Lying = Flakes.IndexOfByPredicate([](const FFlake& Flake) { return Flake.Landed >= 0.0f; });
		Flakes.RemoveAt(Lying != INDEX_NONE ? Lying : 0);
	}

	FFlake& Flake = Flakes.AddDefaulted_GetRef();
	Flake.Kind = Kind;
	Flake.Location = Location;
	Flake.Velocity = Velocity;
	Flake.SpinAxis = FMath::VRand();
	Flake.Spin = FMath::FRandRange(0.0f, UE_TWO_PI);
	Flake.Phase = FMath::FRandRange(0.0f, UE_TWO_PI);
	Flake.Size = Size;
	Flake.GroundZ = GroundZ;
	Flake.Heat = Kind == EVaelFlake::Ember ? 1.0f : 0.0f;
	Flake.CoolTime = FMath::FRandRange(1.0f, 2.2f);
	Flake.LandYaw = FMath::FRandRange(0.0f, 360.0f);
}

int32 UVaelFlakesComponent::NumFalling() const
{
	int32 Count = 0;
	for (const FFlake& Flake : Flakes)
	{
		Count += Flake.Landed < 0.0f ? 1 : 0;
	}
	return Count;
}

void UVaelFlakesComponent::Fall(FFlake& Flake, float DeltaTime, const FVector& Wind)
{
	const FFlakeMotion& Motion = Motions[static_cast<int32>(Flake.Kind)];

	// Sinks at its own pace and drifts with part of the wind; what was thrown up slows and turns over
	Flake.Velocity.Z = FMath::FInterpTo(Flake.Velocity.Z, -Motion.Fall, DeltaTime, FallCatchUp);
	const FVector Drift = FMath::VInterpTo(FVector(Flake.Velocity.X, Flake.Velocity.Y, 0.0f), Wind * WindShare, DeltaTime, DriftCatchUp);
	Flake.Velocity.X = Drift.X;
	Flake.Velocity.Y = Drift.Y;
	Flake.Location += Flake.Velocity * DeltaTime;
	Flake.Spin += DeltaTime * Motion.SpinRate * UE_TWO_PI;

	// An ember cools to ash on the way
	if (Flake.Kind == EVaelFlake::Ember)
	{
		Flake.Heat -= DeltaTime / Flake.CoolTime;
		if (Flake.Heat <= 0.0f)
		{
			Flake.Kind = EVaelFlake::Ash;
		}
	}

	if (Flake.Location.Z > Flake.GroundZ + 1.0f)
	{
		return;
	}

	Flake.Location.Z = Flake.GroundZ + 1.0f;
	Flake.Landed = 0.0f;
	if (Flake.Kind == EVaelFlake::Ember && Flake.Heat >= HotLanding && OnHotLanding)
	{
		OnHotLanding(Flake.Location);
	}
}

FTransform UVaelFlakesComponent::Place(const FFlake& Flake) const
{
	const float Width = Flake.Size * Fade / SphereSize;
	if (Flake.Landed >= 0.0f)
	{
		return FTransform(FRotator(0.0f, Flake.LandYaw, 0.0f), Flake.Location, FVector(Width, Width, Width * LyingThickness));
	}

	// Sails from side to side, less close to the ground so it settles where it lands
	const FFlakeMotion& Motion = Motions[static_cast<int32>(Flake.Kind)];
	const float Settle = FMath::Clamp((Flake.Location.Z - Flake.GroundZ) / 40.0f, 0.0f, 1.0f);
	const float Swing = FMath::Sin(Age * Motion.SwayRate * UE_TWO_PI * 0.5f + Flake.Phase);
	const FVector Side = FVector(FMath::Cos(Flake.Phase * 0.7f), FMath::Sin(Flake.Phase), 0.0f) * Motion.Sway * Swing * Settle;

	// Embers burn smaller as they cool
	const float Shrink = Flake.Kind == EVaelFlake::Ember ? 0.6f + 0.4f * Flake.Heat : 1.0f;
	return FTransform(FQuat(Flake.SpinAxis, Flake.Spin), Flake.Location + Side, FVector(Width, Width, Width * FlakeThickness) * Shrink);
}

void UVaelFlakesComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Meshes.IsEmpty())
	{
		return;
	}

	Age += DeltaTime;
	const FVector Wind = AVaelRegion::GetWindAt(GetWorld(), GetComponentLocation());

	for (TArray<FTransform>& Frame : Frames)
	{
		Frame.Reset();
	}

	for (FFlake& Flake : Flakes)
	{
		if (Flake.Landed < 0.0f)
		{
			Fall(Flake, DeltaTime, Wind);
		}
		else
		{
			Flake.Landed += DeltaTime;
			// A hot ember glows on a moment where it fell, then it is grey ash
			if (Flake.Kind == EVaelFlake::Ember && Flake.Landed >= EmberAfterglow)
			{
				Flake.Kind = EVaelFlake::Ash;
			}
		}
		Frames[static_cast<int32>(Flake.Kind)].Add(Place(Flake));
	}

	// Every kind keeps its instances; the ones not needed this frame are shrunk to nothing
	for (int32 Kind = 0; Kind < FlakeKinds; ++Kind)
	{
		UInstancedStaticMeshComponent* Mesh = Meshes[Kind];
		TArray<FTransform>& Frame = Frames[Kind];
		const int32 Shown = Frame.Num();
		const int32 Existing = Mesh->GetInstanceCount();
		if (Shown == 0 && Existing == 0)
		{
			continue;
		}
		for (int32 Index = Existing; Index < Shown; ++Index)
		{
			Mesh->AddInstance(Frame[Index], true);
		}
		for (int32 Index = Shown; Index < Existing; ++Index)
		{
			Frame.Add(FTransform(FQuat::Identity, GetComponentLocation(), FVector::ZeroVector));
		}
		Mesh->BatchUpdateInstancesTransforms(0, Frame, true, true, true);
	}
}
