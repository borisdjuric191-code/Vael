// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelLightningBolt.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Magic/VaelMagicSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "VaelAssets.h"

namespace
{
	/** The channel strays sideways by this share of its length at most, never more than the cap in cm; each halving of a piece keeps this share of the stray */
	constexpr float BoltStrayShare = 0.16f;
	constexpr float BoltStrayCap = 130.0f;
	constexpr float BoltStrayKeep = 0.56f;

	/** Pieces of the channel get about this long in cm */
	constexpr float BoltPieceLength = 14.0f;
	constexpr int32 BoltMaxDepth = 7;

	/** Chance of a branch at a point of the channel, how long it gets against what is left of the channel, at most in cm */
	constexpr float BoltBranchChance = 0.09f;
	constexpr float BoltBranchShare = 0.42f;
	constexpr float BoltBranchMax = 340.0f;
	constexpr int32 BoltMaxLines = 14;

	/** Width of a ribbon in cm: the white-hot core is a thin line in the middle of its halo */
	constexpr float BoltWidth = 46.0f;

	/** Glow of the ribbons; the halo of a dark bolt stays low so its black core shows */
	constexpr float BoltGlow = 10.0f;
	constexpr float BoltDarkGlow = 4.0f;

	/** Share of the duration the leader takes, at most in seconds */
	constexpr float BoltLeaderShare = 0.22f;
	constexpr float BoltLeaderMax = 0.06f;

	/** How fast a strike fades, and the second and third strike against the first */
	constexpr float BoltStrokeDecay = 16.0f;
	constexpr float BoltRestrike = 0.7f;

	/** Seconds the channel glows out after the bolt */
	constexpr float BoltAfterglow = 0.28f;

	/** Sparks where it strikes, seconds they fly, how they fall in cm/s², length of their streak in seconds */
	constexpr int32 BoltSparkCount = 16;
	constexpr float BoltSparkLifetime = 0.5f;
	constexpr float BoltSparkGravity = 900.0f;
	constexpr float BoltSparkStreak = 0.035f;

	/** Small arcs around the place it hit: how many, how far in cm, how long they crackle on, seconds between their shapes */
	constexpr int32 BoltCrackleCount = 3;
	constexpr float BoltCrackleRadius = 70.0f;
	constexpr float BoltCrackleTime = 0.45f;
	constexpr float BoltCrackleInterval = 0.04f;

	/** Size in cm of the flash where it hits and where it starts */
	constexpr float BoltHitFlash = 300.0f;
	constexpr float BoltStartFlash = 130.0f;

	/** Brightness of the light at the ends in candela */
	constexpr float BoltLightIntensity = 22.0f;

	/** Thinner bolts than this are small arcs: they bring no light of their own */
	constexpr float BoltMinLitThickness = 0.5f;
}

AVaelLightningBolt::AVaelLightningBolt()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	PrimaryActorTick.bCanEverTick = true;
}

AVaelLightningBolt* AVaelLightningBolt::Spawn(AActor* Caster, const FVector& From, const FVector& To, const FLinearColor& Color, float Duration, float Thickness, bool bSparks, bool bDark)
{
	UWorld* World = Caster != nullptr ? Caster->GetWorld() : nullptr;
	if (World == nullptr || FVector::DistSquared(From, To) < 1.0f)
	{
		return nullptr;
	}

	const FTransform SpawnTransform(From);

	AVaelLightningBolt* Bolt = World->SpawnActorDeferred<AVaelLightningBolt>(AVaelLightningBolt::StaticClass(), SpawnTransform, Caster, Cast<APawn>(Caster), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Bolt != nullptr)
	{
		Bolt->From = From;
		Bolt->To = To;
		Bolt->BoltColor = Color;
		Bolt->Duration = FMath::Max(Duration, 0.05f);
		Bolt->Thickness = Thickness;
		Bolt->bSparks = bSparks;
		Bolt->bDark = bDark;
		Bolt->FinishSpawning(SpawnTransform);
	}

	return Bolt;
}

void AVaelLightningBolt::BeginPlay()
{
	Super::BeginPlay();

	// The geometry lies in the world, not on the actor
	Mesh = NewObject<UProceduralMeshComponent>(this);
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetUsingAbsoluteLocation(true);
	Mesh->SetUsingAbsoluteRotation(true);
	Mesh->SetUsingAbsoluteScale(true);
	Mesh->SetWorldTransform(FTransform::Identity);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->bReceivesDecals = false;
	Mesh->RegisterComponent();

	if (UMaterialInterface* Parent = VaelAssets::LoadOptional(UVaelMagicSettings::Get()->LightningMaterial))
	{
		Material = UMaterialInstanceDynamic::Create(Parent, this);
		Material->SetVectorParameterValue(TEXT("Farbe"), BoltColor);
		Material->SetVectorParameterValue(TEXT("KernFarbe"), bDark ? FLinearColor::Black : FMath::Lerp(BoltColor, FLinearColor::White, 0.8f));
		Material->SetScalarParameterValue(TEXT("Dunkel"), bDark ? 1.0f : 0.0f);
		Material->SetScalarParameterValue(TEXT("Leuchten"), bDark ? BoltDarkGlow : BoltGlow);
		Mesh->SetMaterial(0, Material);
	}

	const FLinearColor LightColor = bDark ? BoltColor : FMath::Lerp(BoltColor, FLinearColor::White, 0.6f);
	const bool bLit = Thickness >= BoltMinLitThickness;
	for (TObjectPtr<UPointLightComponent>* Light : { &StartLight, &EndLight })
	{
		*Light = NewObject<UPointLightComponent>(this);
		(*Light)->SetupAttachment(RootComponent);
		(*Light)->SetUsingAbsoluteLocation(true);
		(*Light)->SetMobility(EComponentMobility::Movable);
		(*Light)->SetIntensityUnits(ELightUnits::Candelas);
		(*Light)->SetIntensity(0.0f);
		(*Light)->SetVisibility(bLit);
		(*Light)->SetLightColor(LightColor);
		(*Light)->SetAttenuationRadius(650.0f);
		(*Light)->SetCastShadows(false);
		(*Light)->RegisterComponent();
	}

	StartLight->SetWorldLocation(From);
	EndLight->SetWorldLocation(To);

	// A long bolt strikes more often than a short one; the first strike comes when the leader arrives
	StrokeTimes.Add(0.0f);
	const float Rest = Duration * (1.0f - BoltLeaderShare);
	if (Rest > 0.12f)
	{
		StrokeTimes.Add(Rest * FMath::FRandRange(0.35f, 0.5f));
	}
	if (Rest > 0.22f)
	{
		StrokeTimes.Add(Rest * FMath::FRandRange(0.7f, 0.85f));
	}

	for (int32 SparkIndex = 0; bSparks && SparkIndex < BoltSparkCount; ++SparkIndex)
	{
		// Upwards more than down
		FVector Direction = FMath::VRand();
		Direction.Z = FMath::Abs(Direction.Z);
		SparkVelocities.Add(Direction * FMath::FRandRange(350.0f, 950.0f) + FVector(0.0f, 0.0f, 150.0f));
	}

	BuildChannel();
	BuildCrackle();
	Tick(0.0f);
}

void AVaelLightningBolt::AddJagged(TArray<FVector>& Points, const FVector& Start, const FVector& End, float Stray, int32 Depth) const
{
	if (Depth <= 0)
	{
		Points.Add(End);
		return;
	}

	// The middle of every piece strays sideways, away from the way the piece runs
	const FVector Direction = (End - Start).GetSafeNormal();
	FVector Side = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();
	if (Side.IsNearlyZero())
	{
		Side = FVector::RightVector;
	}
	const FVector Lift = FVector::CrossProduct(Side, Direction);
	const FVector Middle = (Start + End) * 0.5f + (Side * FMath::FRandRange(-1.0f, 1.0f) + Lift * FMath::FRandRange(-0.6f, 0.6f)) * Stray;

	AddJagged(Points, Start, Middle, Stray * BoltStrayKeep, Depth - 1);
	AddJagged(Points, Middle, End, Stray * BoltStrayKeep, Depth - 1);
}

void AVaelLightningBolt::AddBranch(const FVector& Start, const FVector& Direction, float Length, float Width, float Brightness, float StartAt, int32 ForksLeft)
{
	if (Lines.Num() >= BoltMaxLines)
	{
		return;
	}

	FVaelBoltLine Branch;
	Branch.Width = Width;
	Branch.Brightness = Brightness;
	Branch.StartAt = StartAt;
	Branch.Points.Add(Start);

	const int32 Depth = FMath::Clamp(FMath::CeilLogTwo(FMath::CeilToInt(Length / BoltPieceLength)), 2, BoltMaxDepth - 2);
	AddJagged(Branch.Points, Start, Start + Direction * Length, Length * BoltStrayShare * 1.3f, Depth);

	const int32 ForkAt = Branch.Points.Num() / 2;
	const FVector ForkPoint = Branch.Points[ForkAt];
	Lines.Add(MoveTemp(Branch));

	if (ForksLeft > 0)
	{
		const FVector ForkDirection = (Direction + FMath::VRand() * 0.8f).GetSafeNormal();
		AddBranch(ForkPoint, ForkDirection, Length * 0.5f, Width * 0.7f, Brightness * 0.75f, StartAt + 0.1f, ForksLeft - 1);
	}
}

void AVaelLightningBolt::BuildChannel()
{
	Lines.Reset();

	const FVector Line = To - From;
	const float Length = Line.Size();
	const FVector Direction = Line / Length;
	const int32 Depth = FMath::Clamp(FMath::CeilLogTwo(FMath::CeilToInt(Length / BoltPieceLength)), 3, BoltMaxDepth);

	FVaelBoltLine Main;
	Main.Width = 1.0f;
	Main.Brightness = 1.0f;
	Main.Points.Add(From);
	AddJagged(Main.Points, From, To, FMath::Min(Length * BoltStrayShare, BoltStrayCap), Depth);

	const TArray<FVector> Points = Main.Points;
	Lines.Add(MoveTemp(Main));

	// Branches leave towards where the bolt is going, thin and dim; they end in the air
	for (int32 PointIndex = 2; PointIndex < Points.Num() - 2; ++PointIndex)
	{
		if (FMath::FRand() > BoltBranchChance)
		{
			continue;
		}

		const float Along = PointIndex / static_cast<float>(Points.Num() - 1);
		const FVector BranchDirection = (Direction + FMath::VRand() * 0.9f).GetSafeNormal();
		const float BranchLength = FMath::Min(Length * (1.0f - Along) * BoltBranchShare, BoltBranchMax) * FMath::FRandRange(0.5f, 1.0f);
		if (BranchLength > 20.0f)
		{
			AddBranch(Points[PointIndex], BranchDirection, BranchLength, 0.5f, 0.6f, Along, 1);
		}
	}
}

void AVaelLightningBolt::BuildCrackle()
{
	Crackles.Reset();
	for (int32 ArcIndex = 0; bSparks && ArcIndex < BoltCrackleCount; ++ArcIndex)
	{
		TArray<FVector>& Arc = Crackles.AddDefaulted_GetRef();
		const FVector Start = To + FMath::VRand() * BoltCrackleRadius * 0.25f;
		const FVector End = To + FMath::VRand() * BoltCrackleRadius * FMath::FRandRange(0.6f, 1.0f) * Thickness;
		Arc.Add(Start);
		AddJagged(Arc, Start, End, BoltCrackleRadius * 0.3f, 3);
	}
}

void AVaelLightningBolt::AddRibbon(const TArray<FVector>& Points, float Share, float Width, float Brightness, float Core)
{
	const int32 NumPoints = FMath::CeilToInt(Points.Num() * FMath::Clamp(Share, 0.0f, 1.0f));
	if (NumPoints < 2 || Brightness <= 0.0f)
	{
		return;
	}

	const int32 First = Vertices.Num();
	for (int32 Index = 0; Index < NumPoints; ++Index)
	{
		const FVector& Point = Points[Index];
		const FVector Tangent = Points[FMath::Min(Index + 1, Points.Num() - 1)] - Points[FMath::Max(Index - 1, 0)];
		const FVector Side = FVector::CrossProduct(Tangent, CameraLocation - Point).GetSafeNormal();

		// Thins out towards the far end, like a line drawn with a running out brush
		const float Along = Index / static_cast<float>(NumPoints - 1);
		const float HalfWidth = Width * 0.5f * FMath::Lerp(1.0f, 0.45f, Along * Along);

		Vertices.Add(Point - Side * HalfWidth);
		Vertices.Add(Point + Side * HalfWidth);
		UVs.Add(FVector2D(Along, 0.0f));
		UVs.Add(FVector2D(Along, 1.0f));
		const FLinearColor Color(Brightness, Core, 0.0f, 1.0f);
		Colors.Add(Color);
		Colors.Add(Color);
	}

	for (int32 Index = 0; Index < NumPoints - 1; ++Index)
	{
		const int32 A = First + Index * 2;
		Triangles.Append({ A, A + 1, A + 2, A + 1, A + 3, A + 2 });
	}
}

void AVaelLightningBolt::AddFlash(const FVector& Center, float Size, float InBrightness, float Core)
{
	if (InBrightness <= 0.0f)
	{
		return;
	}

	// The flash of a small arc is only a spark
	const float Brightness = InBrightness * FMath::Min(Thickness, 1.0f);

	const FVector ToCamera = (CameraLocation - Center).GetSafeNormal();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, ToCamera).GetSafeNormal() * Size * 0.5f;
	const FVector Up = FVector::CrossProduct(ToCamera, Right).GetSafeNormal() * Size * 0.5f;

	const int32 First = Vertices.Num();
	Vertices.Append({ Center - Right - Up, Center + Right - Up, Center + Right + Up, Center - Right + Up });
	UVs.Append({ FVector2D(0.0f, 0.0f), FVector2D(1.0f, 0.0f), FVector2D(1.0f, 1.0f), FVector2D(0.0f, 1.0f) });
	const FLinearColor Color(Brightness, Core, 1.0f, 1.0f);
	Colors.Append({ Color, Color, Color, Color });
	Triangles.Append({ First, First + 1, First + 2, First, First + 2, First + 3 });
}

void AVaelLightningBolt::CommitMesh()
{
	if (Vertices.IsEmpty())
	{
		Mesh->SetVisibility(false);
		return;
	}

	Mesh->SetVisibility(true);
	Mesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, TArray<FVector>(), UVs, Colors, TArray<FProcMeshTangent>(), false);
	if (Material != nullptr)
	{
		Mesh->SetMaterial(0, Material);
	}
}

void AVaelLightningBolt::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Elapsed += DeltaSeconds;

	const float LeaderTime = FMath::Min(Duration * BoltLeaderShare, BoltLeaderMax);
	const float SinceStrike = Elapsed - LeaderTime;
	const float EndTime = LeaderTime + FMath::Max(Duration * (1.0f - BoltLeaderShare) + BoltAfterglow, bSparks ? FMath::Max(BoltSparkLifetime, BoltCrackleTime) : 0.0f);
	if (Elapsed >= EndTime)
	{
		Destroy();
		return;
	}

	const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	CameraLocation = PlayerController != nullptr && PlayerController->PlayerCameraManager != nullptr ? PlayerController->PlayerCameraManager->GetCameraLocation() : To + FVector(-600.0f, 0.0f, 900.0f);

	Vertices.Reset();
	UVs.Reset();
	Colors.Reset();
	Triangles.Reset();

	// A new strike runs along a channel that has changed a little
	if (StrokeTimes.IsValidIndex(StrokeIndex + 1) && SinceStrike >= StrokeTimes[StrokeIndex + 1])
	{
		++StrokeIndex;
		BuildChannel();
	}

	if (SinceStrike < 0.0f)
	{
		// The leader feels its way: thin and dim, the branches growing as it passes them
		const float Head = FMath::Clamp(Elapsed / FMath::Max(LeaderTime, 0.001f), 0.0f, 1.0f);
		for (const FVaelBoltLine& Line : Lines)
		{
			AddRibbon(Line.Points, (Head - Line.StartAt) / FMath::Max(1.0f - Line.StartAt, 0.01f), BoltWidth * 0.7f * Thickness * Line.Width, 0.45f * Line.Brightness, 0.5f);
		}
		AddFlash(From, BoltStartFlash * 0.6f * Thickness, 0.5f, 0.3f);
		StartLight->SetIntensity(BoltLightIntensity * 0.25f);
		EndLight->SetIntensity(0.0f);
		CommitMesh();
		return;
	}

	// Every strike flashes up and decays fast; the last one leaves the channel glowing out in its color
	const float StrokeAge = SinceStrike - StrokeTimes[StrokeIndex];
	const float Strength = StrokeIndex == 0 ? 1.0f : BoltRestrike;
	const float Strike = Strength * FMath::Exp(-StrokeAge * BoltStrokeDecay);
	const float Glowing = 1.0f - FMath::Clamp((SinceStrike - Duration * (1.0f - BoltLeaderShare)) / BoltAfterglow, 0.0f, 1.0f);
	const float Residual = 0.22f * Glowing * Glowing;
	const float Brightness = FMath::Max(Strike, Residual) * FMath::FRandRange(0.85f, 1.0f);

	const float Core = FMath::Clamp(Strike * 3.0f, bDark ? 0.8f : 0.0f, 1.0f);
	for (const FVaelBoltLine& Line : Lines)
	{
		AddRibbon(Line.Points, 1.0f, BoltWidth * Thickness * Line.Width * FMath::Lerp(0.6f, 1.15f, Strike), Brightness * Line.Brightness, Core);
	}

	// Flash where it hit and, weaker, where it left
	AddFlash(To, BoltHitFlash * Thickness * FMath::Lerp(0.55f, 1.0f, Strike), Strike * 1.1f, Core);
	AddFlash(From, BoltStartFlash * Thickness, Strike * 0.7f, Core);

	if (bSparks)
	{
		// Small arcs crackle on around the place it hit
		CrackleCountdown -= DeltaSeconds;
		if (CrackleCountdown <= 0.0f)
		{
			CrackleCountdown = BoltCrackleInterval * FMath::FRandRange(0.7f, 1.3f);
			BuildCrackle();
		}

		const float Crackle = 1.0f - FMath::Clamp(SinceStrike / BoltCrackleTime, 0.0f, 1.0f);
		for (const TArray<FVector>& Arc : Crackles)
		{
			if (FMath::FRand() < 0.75f)
			{
				AddRibbon(Arc, 1.0f, BoltWidth * 0.35f, Crackle * 0.7f, 0.7f);
			}
		}

		// Sparks fly off as short streaks, slow down and fall
		const auto SparkAt = [this](const FVector& Velocity, float Seconds)
		{
			const float Carried = (1.0f - FMath::Exp(-2.5f * Seconds)) / 2.5f;
			return To + Velocity * Carried - FVector(0.0f, 0.0f, 0.5f * BoltSparkGravity * Seconds * Seconds);
		};

		const float SparkLife = FMath::Clamp(SinceStrike / BoltSparkLifetime, 0.0f, 1.0f);
		for (int32 SparkIndex = 0; SparkLife < 1.0f && SparkIndex < SparkVelocities.Num(); ++SparkIndex)
		{
			const TArray<FVector> Streak = { SparkAt(SparkVelocities[SparkIndex], SinceStrike), SparkAt(SparkVelocities[SparkIndex], FMath::Max(SinceStrike - BoltSparkStreak, 0.0f)) };
			AddRibbon(Streak, 1.0f, 6.0f, (1.0f - SparkLife) * 1.1f, 1.0f);
		}
	}

	const float Light = BoltLightIntensity * FMath::Max(Strike, Residual * 0.5f) * FMath::FRandRange(0.8f, 1.0f);
	StartLight->SetIntensity(Light * 0.4f);
	EndLight->SetIntensity(Light);

	CommitMesh();
}
