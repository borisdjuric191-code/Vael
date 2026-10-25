// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelDebrisWhirl.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Magic/VaelMagicSettings.h"
#include "Magic/VaelSpellEffects.h"
#include "NiagaraComponent.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
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
	BuildShells();
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
	if (TotalShare <= 0.0f)
	{
		Kinds.Reset();
	}

	// Bands of sand racing round and up: the steam plume in the color of the dust, its trail left in the air draws the spiral
	UNiagaraSystem* Sand = VaelAssets::LoadOptional(MagicSettings->SteamEffect);
	const int32 NumBands = bFunnel ? MagicSettings->WhirlFunnelBands : MagicSettings->WhirlStormBands;
	for (int32 Index = 0; Sand != nullptr && Index < NumBands; ++Index)
	{
		if (UNiagaraComponent* Band = VaelEffects::Attach(Sand, this, NAME_None, DustColor, Radius * MagicSettings->WhirlBandSize, false))
		{
			Band->SetAllowScalability(false);
			FPiece Path;
			Path.Angle = Index * UE_TWO_PI / NumBands;
			Path.Height = Height * (Index % 3) / 3.0f;
			Path.AngularSpeed = MagicSettings->WhirlBandTurnSpeed * (bFunnel ? 1.3f : 1.0f);
			Path.RiseSpeed = Height * 0.5f;
			Path.Offset = -0.1f;
			Band->SetRelativeLocation(PlaceOf(Path));
			Bands.Add(Band);
			BandPaths.Add(Path);
		}
	}

	const int32 Count = bFunnel ? MagicSettings->WhirlFunnelPieces : MagicSettings->WhirlStormPieces;
	for (int32 Index = 0; !Kinds.IsEmpty() && Index < Count; ++Index)
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

void UVaelDebrisWhirlComponent::BuildShells()
{
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	UMaterialInterface* Material = VaelAssets::LoadOptional(MagicSettings->WhirlMaterial);
	if (Material == nullptr)
	{
		return;
	}
	Shells = NewObject<UProceduralMeshComponent>(GetOwner());
	Shells->SetupAttachment(this);
	Shells->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Shells->SetCastShadow(false);
	Shells->RegisterComponent();

	// Each shell a tube following the shape of the whirl, inner shells narrower; U goes round in turns, V up.
	// The seam gets its own column of points so U can run on without jumping back
	const int32 NumRings = MagicSettings->WhirlShellRings;
	const int32 NumSegments = MagicSettings->WhirlShellSegments;
	TArray<int32> Triangles;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;
	for (int32 Shell = 0; Shell < MagicSettings->WhirlShells; ++Shell)
	{
		const float Inner = MagicSettings->WhirlShells > 1 ? float(Shell) / (MagicSettings->WhirlShells - 1) : 0.0f;
		const int32 First = ShellVertices.Num();
		for (int32 Ring = 0; Ring <= NumRings; ++Ring)
		{
			const float Up = float(Ring) / NumRings;
			for (int32 Segment = 0; Segment <= NumSegments; ++Segment)
			{
				ShellVertices.Add(FVector::ZeroVector);
				UVs.Add(FVector2D(float(Segment) / NumSegments * (2.0f + Shell), Up));
				// Thin at the bottom and the top, the inner shells denser
				const float Solid = FMath::Pow(FMath::Sin(Up * UE_PI), 0.6f) * FMath::Lerp(0.55f, 1.0f, Inner) * (bFunnel ? 1.0f : FMath::Lerp(1.0f, 0.5f, Up));
				Colors.Add(FLinearColor(Inner, 0.0f, 0.0f, Solid));
			}
		}
		for (int32 Ring = 0; Ring < NumRings; ++Ring)
		{
			for (int32 Segment = 0; Segment < NumSegments; ++Segment)
			{
				const int32 A = First + Ring * (NumSegments + 1) + Segment;
				const int32 C = A + NumSegments + 1;
				Triangles.Append({ A, C, A + 1, A + 1, C, C + 1 });
			}
		}
	}
	UpdateShells();

	// Normals point out from the middle (a lit material needs them, or the shells don't show at all)
	TArray<FVector> Normals;
	for (const FVector& Vertex : ShellVertices)
	{
		Normals.Add(FVector(Vertex.X, Vertex.Y, 0.0f).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector));
	}
	Shells->CreateMeshSection_LinearColor(0, ShellVertices, Triangles, Normals, UVs, Colors, TArray<FProcMeshTangent>(), false);
	Shells->SetMaterial(0, Material);
}

void UVaelDebrisWhirlComponent::UpdateShells()
{
	if (ShellVertices.IsEmpty())
	{
		return;
	}
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	const int32 NumRings = MagicSettings->WhirlShellRings;
	const int32 NumSegments = MagicSettings->WhirlShellSegments;
	int32 Index = 0;
	for (int32 Shell = 0; Shell < MagicSettings->WhirlShells; ++Shell)
	{
		const float Inner = MagicSettings->WhirlShells > 1 ? float(Shell) / (MagicSettings->WhirlShells - 1) : 0.0f;
		for (int32 Ring = 0; Ring <= NumRings; ++Ring)
		{
			// The whirl sways: the higher, the further its middle swings round
			FPiece Point;
			Point.Height = Height * Ring / NumRings;
			const float Up = float(Ring) / NumRings;
			// and breathes: bulges run up the shells like gusts
			Point.Offset = FMath::Lerp(0.05f, -0.3f, Inner) + 0.07f * FMath::Sin(Age * 3.1f - Up * 6.0f + Shell);
			const FVector Sway = FVector(FMath::Sin(Age * 1.7f + Up * 2.0f), FMath::Cos(Age * 1.3f + Up * 2.6f), 0.0f) * Radius * MagicSettings->WhirlSway * Up;
			for (int32 Segment = 0; Segment <= NumSegments; ++Segment)
			{
				// The outline is not round: a lopsided ripple runs round the shell
				Point.Angle = Segment * UE_TWO_PI / NumSegments;
				const float Ripple = 1.0f + 0.06f * FMath::Sin(Point.Angle * 3.0f + Age * 2.2f + Up * 3.0f);
				ShellVertices[Index++] = PlaceOf(Point) * FVector(Ripple, Ripple, 1.0f) + Sway;
			}
		}
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
	Age += DeltaTime;

	if (Shells != nullptr)
	{
		UpdateShells();
		Shells->UpdateMeshSection_LinearColor(0, ShellVertices, TArray<FVector>(), TArray<FVector2D>(), TArray<FLinearColor>(), TArray<FProcMeshTangent>());
	}

	// The bands race round and up and start again at the bottom
	for (int32 Index = 0; Index < Bands.Num(); ++Index)
	{
		FPiece& Path = BandPaths[Index];
		Path.Angle += Path.AngularSpeed * DeltaTime;
		Path.Height += Path.RiseSpeed * DeltaTime;
		if (Path.Height > Height)
		{
			Path.Height = 0.0f;
		}
		if (Bands[Index] != nullptr)
		{
			Bands[Index]->SetRelativeLocation(PlaceOf(Path));
		}
	}

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
		// On one of the turning spiral arms, twisted further the higher it is, so the clouds draw the whirl and don't hide it
		FPiece Spot;
		const int32 Arm = FMath::RandRange(0, MagicSettings->WhirlArms - 1);
		Spot.Height = FMath::FRandRange(0.0f, Height * (bFunnel ? 0.9f : 0.7f));
		Spot.Angle = Arm * UE_TWO_PI / MagicSettings->WhirlArms + Age * MagicSettings->WhirlBandTurnSpeed + Spot.Height / Height * 2.2f + FMath::FRandRange(-0.15f, 0.15f);
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
