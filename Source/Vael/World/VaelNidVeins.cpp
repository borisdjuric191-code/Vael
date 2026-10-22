// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelNidVeins.h"
#include "Combat/VaelCombatStatics.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Player/VaelCharacter.h"
#include "ProceduralMeshComponent.h"
#include "VaelAssets.h"
#include "Vael.h"

namespace
{
	/** Distance between two points of a vein in cm (Scripts/PlaceNidVeins.py lays them this far apart) */
	constexpr float VeinStep = 100.0f;

	/** Profile across a vein: the liquid bulges a little above the ground, its edges sink into it */
	constexpr float LiquidBulge = 0.03f;
	constexpr float LiquidSink = 0.12f;

	/** A vein under the ground only swells it, a little wider than the vein */
	constexpr float RidgeBulge = 0.05f;
	constexpr float RidgeSink = 0.2f;
	constexpr float RidgeWidth = 1.3f;

	/** Points across a vein */
	constexpr int32 VeinAcross = 7;

	const FName FlowParameter(TEXT("Fliessen"));

	template<typename T>
	TSoftObjectPtr<T> NidAsset(const TCHAR* Name)
	{
		return TSoftObjectPtr<T>(FSoftObjectPath(FString::Printf(TEXT("/Game/Vael/Nid/%s.%s"), Name, Name)));
	}
}

AVaelNidVeins::AVaelNidVeins()
{
	PrimaryActorTick.bCanEverTick = true;

	Veins = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Veins"));
	Veins->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Veins->SetCastShadow(false);
	Veins->SetCanEverAffectNavigation(false);
	Veins->bUseAsyncCooking = false;
	RootComponent = Veins;

	Bubbles = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Bubbles"));
	Bubbles->SetupAttachment(Veins);
	Bubbles->SetUsingAbsoluteLocation(true);
	Bubbles->SetUsingAbsoluteRotation(true);
	Bubbles->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bubbles->SetCastShadow(false);
	Bubbles->NumCustomDataFloats = 1;

	Mist = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Mist"));
	Mist->SetupAttachment(Veins);
	Mist->SetUsingAbsoluteLocation(true);
	Mist->SetUsingAbsoluteRotation(true);
	Mist->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mist->SetCastShadow(false);
	Mist->NumCustomDataFloats = 1;

	LiquidMaterial = NidAsset<UMaterialInterface>(TEXT("M_Nid_Ader"));
	RidgeMaterial = NidAsset<UMaterialInterface>(TEXT("M_Nid_Wulst"));
	BubbleMaterial = NidAsset<UMaterialInterface>(TEXT("M_Nid_Blase"));
	MistMaterial = NidAsset<UMaterialInterface>(TEXT("M_Nid_Nebel"));
	BubbleMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Sphere.Sphere")));
}

void AVaelNidVeins::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildMesh();
}

void AVaelNidVeins::BeginPlay()
{
	Super::BeginPlay();
	BuildMesh();
	const FVaelNidVeinPoint* Widest = nullptr;
	for (const FVaelNidVeinPoint& Point : OpenPoints)
	{
		Widest = Widest == nullptr || Point.Width > Widest->Width ? &Point : Widest;
	}
	UE_LOG(LogVael, Display, TEXT("Nid veins %s: %d points, %.0f m open, widest open at %s"), *GetName(), Points.Num(), OpenMetres,
		Widest == nullptr ? TEXT("-") : *Widest->Location.ToCompactString());
	if (UStaticMesh* Sphere = VaelAssets::LoadOptional(BubbleMesh))
	{
		Bubbles->SetStaticMesh(Sphere);
		Mist->SetStaticMesh(Sphere);
		Bubbles->SetMaterial(0, VaelAssets::LoadOptional(BubbleMaterial));
		Mist->SetMaterial(0, VaelAssets::LoadOptional(MistMaterial));
	}
}

void AVaelNidVeins::BuildMesh()
{
	Veins->ClearAllMeshSections();
	struct FStrip
	{
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FVector2D> UV0;
		TArray<FVector2D> UV1;
		TArray<FProcMeshTangent> Tangents;
	};
	FStrip Liquid;
	FStrip Ridge;
	const FVector Origin = GetActorLocation();
	OpenPoints.Reset();
	OpenWidthSum = 0.0f;
	OpenMetres = 0.0f;
	MaxOpenWidth = 1.0f;

	// The points come vein after vein
	TArray<TArray<FVaelNidVeinPoint>> Branches;
	for (const FVaelNidVeinPoint& Point : Points)
	{
		if (Branches.IsEmpty() || Branches.Last().Last().Vein != Point.Vein)
		{
			Branches.AddDefaulted();
		}
		Branches.Last().Add(Point);
	}

	for (int32 BranchIndex = 0; BranchIndex < Branches.Num(); ++BranchIndex)
	{
		const TArray<FVaelNidVeinPoint>& Line = Branches[BranchIndex];
		const float BranchRandom = FMath::Frac(BranchIndex * 0.6180339f);
		int32 First = 0;
		while (First < Line.Num() - 1)
		{
			// A stretch of the same kind, sharing its last point with the next stretch so nothing gaps
			const bool bOpen = Line[First].bOpen;
			int32 Last = First;
			while (Last + 1 < Line.Num() && Line[Last + 1].bOpen == bOpen)
			{
				++Last;
			}
			Last = FMath::Min(Last + 1, Line.Num() - 1);
			FStrip& Strip = bOpen ? Liquid : Ridge;
			const float Bulge = bOpen ? LiquidBulge : RidgeBulge;
			const float Sink = bOpen ? LiquidSink : RidgeSink;
			const float Wider = bOpen ? 1.0f : RidgeWidth;
			const int32 Base = Strip.Vertices.Num();
			for (int32 PointIndex = First; PointIndex <= Last; ++PointIndex)
			{
				const FVaelNidVeinPoint& Point = Line[PointIndex];
				const FVector Before = Line[FMath::Max(PointIndex - 1, 0)].Location;
				const FVector After = Line[FMath::Min(PointIndex + 1, Line.Num() - 1)].Location;
				const FVector Forward = FVector(After.X - Before.X, After.Y - Before.Y, 0.0f).GetSafeNormal();
				const FVector Side = FVector::CrossProduct(FVector::UpVector, Forward);
				const float Width = Point.Width * Wider;
				for (int32 Across = 0; Across < VeinAcross; ++Across)
				{
					const float Y = Across / float(VeinAcross - 1) * 2.0f - 1.0f;
					const float Height = Width * (Bulge * (1.0f - Y * Y) - Sink * Y * Y);
					Strip.Vertices.Add(Point.Location - Origin + Side * Y * Width * 0.5f + FVector(0.0f, 0.0f, Height));
					Strip.Normals.Add((FVector::UpVector + Side * 4.0f * Y * (Bulge + Sink)).GetSafeNormal());
					// u across the vein, v along its way from the spring in metres; UV1: its width in metres, and which vein it is
					Strip.UV0.Add(FVector2D((Y + 1.0f) * 0.5f, Point.Along / 100.0f));
					Strip.UV1.Add(FVector2D(Point.Width / 100.0f, BranchRandom));
					Strip.Tangents.Add(FProcMeshTangent(Forward, false));
				}
				if (PointIndex > First)
				{
					const int32 Row = Base + (PointIndex - First) * VeinAcross;
					for (int32 Across = 0; Across < VeinAcross - 1; ++Across)
					{
						const int32 A = Row - VeinAcross + Across;
						const int32 B = Row + Across;
						Strip.Triangles.Append({ A, A + 1, B, A + 1, B + 1, B });
					}
				}
				if (bOpen)
				{
					OpenPoints.Add(Point);
					OpenWidthSum += Point.Width;
					MaxOpenWidth = FMath::Max(MaxOpenWidth, Point.Width);
				}
			}
			if (bOpen)
			{
				OpenMetres += (Last - First) * VeinStep / 100.0f;
			}
			First = Last;
			if (Last == Line.Num() - 1)
			{
				break;
			}
		}
	}

	if (!Liquid.Vertices.IsEmpty())
	{
		Veins->CreateMeshSection_LinearColor(0, Liquid.Vertices, Liquid.Triangles, Liquid.Normals, Liquid.UV0, Liquid.UV1, TArray<FVector2D>(), TArray<FVector2D>(),
			TArray<FLinearColor>(), Liquid.Tangents, false);
		if (UMaterialInterface* Base = VaelAssets::LoadOptional(LiquidMaterial))
		{
			UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, this);
			Material->SetScalarParameterValue(FlowParameter, FlowSpeed / 100.0f);
			Veins->SetMaterial(0, Material);
		}
	}
	if (!Ridge.Vertices.IsEmpty())
	{
		const int32 Section = Liquid.Vertices.IsEmpty() ? 0 : 1;
		Veins->CreateMeshSection_LinearColor(Section, Ridge.Vertices, Ridge.Triangles, Ridge.Normals, Ridge.UV0, Ridge.UV1, TArray<FVector2D>(), TArray<FVector2D>(),
			TArray<FLinearColor>(), Ridge.Tangents, false);
		if (UMaterialInterface* Base = VaelAssets::LoadOptional(RidgeMaterial))
		{
			UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, this);
			Material->SetScalarParameterValue(FlowParameter, FlowSpeed / 100.0f);
			Veins->SetMaterial(Section, Material);
		}
	}
}

void AVaelNidVeins::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdatePuffs(DeltaSeconds);
	HurtWaders(DeltaSeconds);
}

void AVaelNidVeins::UpdatePuffs(float DeltaSeconds)
{
	if (OpenPoints.IsEmpty() || Bubbles->GetStaticMesh() == nullptr)
	{
		return;
	}

	// New bubbles rise here and there, more often where a vein is thick
	BubbleDebt += DeltaSeconds * BubblesPer10Metres * OpenMetres / 10.0f;
	while (BubbleDebt >= 1.0f)
	{
		BubbleDebt -= 1.0f;
		for (int32 Try = 0; Try < 6; ++Try)
		{
			const FVaelNidVeinPoint& Point = OpenPoints[FMath::RandRange(0, OpenPoints.Num() - 1)];
			if (FMath::FRand() * MaxOpenWidth > Point.Width)
			{
				continue;
			}
			FPuff Bubble;
			const FVector Side = FVector(FMath::FRandRange(-1.0f, 1.0f), FMath::FRandRange(-1.0f, 1.0f), 0.0f) * Point.Width * 0.25f;
			Bubble.Location = Point.Location + Side + FVector(0.0f, 0.0f, Point.Width * LiquidBulge * 0.8f);
			Bubble.Size = FMath::Clamp(Point.Width * FMath::FRandRange(0.12f, 0.3f), 4.0f, 40.0f);
			Bubble.Life = FMath::FRandRange(1.2f, 2.6f);
			LiveBubbles.Add(Bubble);
			break;
		}
	}

	// A bubble swells slowly as a dome out of the liquid, then bursts; most of them let out a puff of poisonous mist
	TArray<FTransform> Placed;
	TArray<float> Shown;
	for (int32 Index = LiveBubbles.Num() - 1; Index >= 0; --Index)
	{
		FPuff& Bubble = LiveBubbles[Index];
		Bubble.Age += DeltaSeconds;
		if (Bubble.Age >= Bubble.Life)
		{
			if (FMath::FRand() < MistShare)
			{
				FPuff Puff;
				Puff.Location = Bubble.Location;
				Puff.Size = Bubble.Size * FMath::FRandRange(2.0f, 3.5f);
				Puff.Life = FMath::FRandRange(3.0f, 5.5f);
				LiveMist.Add(Puff);
			}
			LiveBubbles.RemoveAtSwap(Index);
			continue;
		}
		const float Life = Bubble.Age / Bubble.Life;
		const float Swell = FMath::SmoothStep(0.0f, 0.85f, Life) + 0.25f * FMath::SmoothStep(0.88f, 1.0f, Life);
		const float Scale = Bubble.Size * (0.15f + 0.85f * Swell) / 100.0f;
		Placed.Add(FTransform(FQuat::Identity, Bubble.Location, FVector(Scale, Scale, Scale * 0.7f)));
		Shown.Add(Life);
	}
	Bubbles->ClearInstances();
	Bubbles->AddInstances(Placed, false, true);
	for (int32 Index = 0; Index < Shown.Num(); ++Index)
	{
		Bubbles->SetCustomDataValue(Index, 0, Shown[Index], Index == Shown.Num() - 1);
	}

	// The mist rises, spreads and thins away
	Placed.Reset();
	Shown.Reset();
	for (int32 Index = LiveMist.Num() - 1; Index >= 0; --Index)
	{
		FPuff& Puff = LiveMist[Index];
		Puff.Age += DeltaSeconds;
		if (Puff.Age >= Puff.Life)
		{
			LiveMist.RemoveAtSwap(Index);
			continue;
		}
		const float Life = Puff.Age / Puff.Life;
		const float Scale = Puff.Size * (0.5f + 1.5f * FMath::Sqrt(Life)) / 100.0f;
		Placed.Add(FTransform(FQuat::Identity, Puff.Location + FVector(0.0f, 0.0f, Puff.Size * 1.5f * Life), FVector(Scale, Scale, Scale * 0.6f)));
		Shown.Add(Life);
	}
	Mist->ClearInstances();
	Mist->AddInstances(Placed, false, true);
	for (int32 Index = 0; Index < Shown.Num(); ++Index)
	{
		Mist->SetCustomDataValue(Index, 0, Shown[Index], Index == Shown.Num() - 1);
	}
}

void AVaelNidVeins::HurtWaders(float DeltaSeconds)
{
	// Who stands in an open vein is hurt by the Nid, a few times a second
	constexpr float Interval = 0.5f;
	DamageClock += DeltaSeconds;
	if (DamageClock < Interval || DamagePerSecond <= 0.0f)
	{
		return;
	}
	DamageClock -= Interval;
	for (TActorIterator<AVaelCharacter> It(GetWorld()); It; ++It)
	{
		const FVector Feet = It->GetActorLocation();
		for (const FVaelNidVeinPoint& Point : OpenPoints)
		{
			const float Near = Point.Width * 0.5f + 25.0f;
			if (FMath::Abs(Feet.X - Point.Location.X) < Near && FMath::Abs(Feet.Y - Point.Location.Y) < Near
				&& FVector::Dist2D(Feet, Point.Location) < Near && FMath::Abs(Feet.Z - Point.Location.Z) < 200.0f)
			{
				UVaelCombatStatics::DealDamage(nullptr, *It, DamagePerSecond * Interval, EVaelElement::Mark);
				break;
			}
		}
	}
}
