// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelDebrisWhirl.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelSpellEffects.h"
#include "NiagaraSystem.h"
#include "VaelAssets.h"

namespace
{
	/** Height of a funnel and of a storm band, compared with their radius */
	constexpr float WhirlFunnelHeightShare = 2.4f;
	constexpr float WhirlStormHeightShare = 0.9f;

	/** Seconds between two puffs of dust at the bottom */
	constexpr float WhirlDustInterval = 0.12f;
}

UVaelDebrisWhirlComponent::UVaelDebrisWhirlComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UVaelDebrisWhirlComponent::Build(float InRadius, bool bInFunnel, const FLinearColor& InDustColor)
{
	Radius = InRadius;
	bFunnel = bInFunnel;
	Height = Radius * (bFunnel ? WhirlFunnelHeightShare : WhirlStormHeightShare);
	DustColor = InDustColor;

	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	float TotalShare = 0.0f;
	TArray<const FVaelWhirlPiece*> Loaded;
	TArray<UStaticMesh*> Meshes;
	for (const FVaelWhirlPiece& Kind : MagicSettings->WhirlPieces)
	{
		if (UStaticMesh* Mesh = VaelAssets::LoadOptional(Kind.Mesh))
		{
			UInstancedStaticMeshComponent* Instances = NewObject<UInstancedStaticMeshComponent>(GetOwner());
			Instances->SetStaticMesh(Mesh);
			Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Instances->SetCastShadow(false);
			Instances->SetupAttachment(this);
			Instances->RegisterComponent();
			Kinds.Add(Instances);
			Loaded.Add(&Kind);
			Meshes.Add(Mesh);
			TotalShare += Kind.Share;
		}
	}
	if (Kinds.IsEmpty() || TotalShare <= 0.0f)
	{
		return;
	}

	const int32 Count = bFunnel ? MagicSettings->WhirlFunnelPieces : MagicSettings->WhirlStormPieces;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		// The kinds in their shares, spread evenly over the pieces
		float Pick = (Index + 0.5f) / Count * TotalShare;
		int32 Kind = 0;
		while (Kind < Loaded.Num() - 1 && Pick > Loaded[Kind]->Share)
		{
			Pick -= Loaded[Kind]->Share;
			++Kind;
		}

		FPiece Piece;
		Piece.Kind = Kind;
		Piece.Angle = FMath::FRandRange(0.0f, UE_TWO_PI);
		Piece.Height = FMath::FRandRange(0.0f, Height);
		Piece.Offset = FMath::FRandRange(-0.25f, 0.2f);
		// Small grit whirls fast, twigs are slower and heavier
		const float Size = FMath::FRandRange(Loaded[Kind]->MinSize, Loaded[Kind]->MaxSize);
		Piece.AngularSpeed = FMath::FRandRange(2.2f, 4.2f) * (bFunnel ? 1.3f : 1.0f) / FMath::Max(1.0f, FMath::Sqrt(Size / 6.0f));
		Piece.RiseSpeed = FMath::FRandRange(0.25f, 0.6f) * Height;
		Piece.Scale = Size / FMath::Max(Meshes[Kind]->GetBounds().SphereRadius * 2.0f, 1.0f);
		Piece.Tumble = FRotator(FMath::FRandRange(0.0f, 360.0f), FMath::FRandRange(0.0f, 360.0f), FMath::FRandRange(0.0f, 360.0f));
		Piece.TumbleSpeed = FRotator(FMath::FRandRange(-400.0f, 400.0f), FMath::FRandRange(-300.0f, 300.0f), FMath::FRandRange(-400.0f, 400.0f));
		Piece.Instance = Kinds[Kind]->AddInstance(FTransform(Piece.Tumble, PlaceOf(Piece), FVector(Piece.Scale)));
		Pieces.Add(Piece);
	}
}

FVector UVaelDebrisWhirlComponent::PlaceOf(const FPiece& Piece) const
{
	// A funnel narrows towards the ground, a storm bulges in the middle of its band
	const float Up = Piece.Height / Height;
	const float Reach = bFunnel ? FMath::Lerp(0.18f, 1.0f, FMath::Pow(Up, 0.8f)) : FMath::Lerp(0.7f, 1.0f, FMath::Sin(Up * UE_PI));
	const float Distance = Radius * FMath::Max(Reach + Piece.Offset, 0.05f);
	return FVector(FMath::Cos(Piece.Angle) * Distance, FMath::Sin(Piece.Angle) * Distance, Piece.Height);
}

void UVaelDebrisWhirlComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	for (FPiece& Piece : Pieces)
	{
		// Spiral up, and at the top fall back in at the bottom
		Piece.Angle += Piece.AngularSpeed * DeltaTime;
		Piece.Height += Piece.RiseSpeed * DeltaTime;
		if (Piece.Height > Height)
		{
			Piece.Height = FMath::FRandRange(0.0f, Height * 0.15f);
			Piece.Offset = FMath::FRandRange(-0.25f, 0.2f);
		}
		Piece.Tumble += Piece.TumbleSpeed * DeltaTime;
		Kinds[Piece.Kind]->UpdateInstanceTransform(Piece.Instance, FTransform(Piece.Tumble, PlaceOf(Piece), FVector(Piece.Scale)), false, false, true);
	}
	for (UInstancedStaticMeshComponent* Instances : Kinds)
	{
		Instances->MarkRenderStateDirty();
	}

	// The body: clouds of dust like those falling off a rock wall, all over the shape of the whirl, carried round with it
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	NextBodyCountdown -= DeltaTime;
	while (NextBodyCountdown <= 0.0f)
	{
		NextBodyCountdown += bFunnel ? MagicSettings->WhirlFunnelDustInterval : MagicSettings->WhirlStormDustInterval;
		FPiece Spot;
		Spot.Angle = FMath::FRandRange(0.0f, UE_TWO_PI);
		Spot.Height = FMath::FRandRange(0.0f, Height * (bFunnel ? 0.9f : 0.7f));
		Spot.Offset = FMath::FRandRange(-0.3f, 0.05f);
		const FVector Place = GetComponentTransform().TransformPosition(PlaceOf(Spot));
		VaelEffects::SpawnAt(this, VaelAssets::LoadOptional(MagicSettings->DustBurstEffect), Place, FRotator::ZeroRotator, DustColor, Radius * MagicSettings->WhirlDustSize);
	}

	// Dust swirls up at the bottom
	NextDustCountdown -= DeltaTime;
	if (NextDustCountdown <= 0.0f)
	{
		NextDustCountdown = WhirlDustInterval;
		const float Angle = FMath::FRandRange(0.0f, UE_TWO_PI);
		const float Distance = Radius * FMath::FRandRange(bFunnel ? 0.1f : 0.6f, bFunnel ? 0.5f : 1.0f);
		const FVector Place = GetComponentTransform().TransformPosition(FVector(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, 0.0f));
		VaelEffects::SpawnAt(this, VaelAssets::LoadOptional(UVaelMagicSettings::Get()->DustPuffEffect), Place, FRotator::ZeroRotator, DustColor, Radius * 0.3f);
	}
}
