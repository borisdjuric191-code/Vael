// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelWaterBolt.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Magic/VaelElementSource.h"
#include "Magic/VaelMagicSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "ProceduralMeshComponent.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"
#include "UObject/Package.h"
#include "VaelAssets.h"
#include "World/VaelGround.h"

namespace
{
	/** Radius of the engine sphere the orb flies as, so the bolt scales like the orb */
	constexpr float BallRadius = 50.0f;

	/** Length of the round nose against the radius of the jet: a little pointed, not a bulging ball */
	constexpr float NoseLength = 1.6f;

	/** Points around the jet and along its nose and body */
	constexpr int32 NumAround = 24;
	constexpr int32 NumNoseRings = 8;
	constexpr int32 NumBodyRings = 28;

	/** Points around the spiral tube and along it */
	constexpr int32 NumSpiralAround = 8;
	constexpr int32 NumSpiralRings = 120;

	/** Where the spiral winds around the jet, in jet radii: close behind the nose, wide at its end, like the rings of a pressure jet */
	constexpr float SpiralStartRadius = 1.35f;
	constexpr float SpiralEndRadius = 3.0f;

	/** Thickness of the spiral tube, in jet radii */
	constexpr float SpiralThickness = 0.16f;

	/** Share of the body the spiral runs along */
	constexpr float SpiralReach = 0.85f;

	/** How quickly and how much the jet flickers in length, and pulses in width like the strokes of a pump */
	constexpr float FlickerSpeed = 22.0f;
	constexpr float FlickerAmount = 0.14f;
	constexpr float PulseAmount = 0.07f;

	/** Degrees per second the bolt turns around its flight axis */
	constexpr float RollSpeed = 400.0f;

	/** The surface wobbles less than on the floating ball, so the shape stays clean at speed */
	constexpr float BoltWobble = 1.5f;

	/** A bolt mirrors the sky less than the floating ball, so its body keeps its color in bright light */
	constexpr float BoltWetness = 0.45f;

	/** The spiral is foamed up until it is white spray, bright over its whole width */
	constexpr float SpiralFoam = 40.0f;
	constexpr float SpiralFresnelPower = 0.5f;

	/** Real water: points around the jet, rings along its nose and body */
	constexpr int32 JetAround = 20;
	constexpr int32 JetNoseRings = 7;
	constexpr int32 JetBodyRings = 30;

	/** Real water: drops in the pool, their size in cm and how long they fly */
	constexpr int32 JetDropCount = 150;
	constexpr float JetDropMinSize = 0.9f;
	constexpr float JetDropMaxSize = 3.6f;
	constexpr float JetDropMinLife = 0.3f;
	constexpr float JetDropMaxLife = 0.7f;

	/** Real water: how fast the foam streams back along the jet, m/s */
	constexpr float JetFoamFlow = 3.0f;

	/** Last meshes built and the shape they were built for, shared by all bolts */
	TWeakObjectPtr<UStaticMesh> CachedBoltMesh;
	TWeakObjectPtr<UStaticMesh> CachedSpiralMesh;
	FVector3f CachedShape = FVector3f::ZeroVector;
}

AVaelWaterBolt::AVaelWaterBolt()
{
	Trail = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Trail"));
	Trail->SetupAttachment(GetPivot());
	Trail->SetAutoActivate(false);

	Spiral = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Spiral"));
	Spiral->SetupAttachment(GetBall());
	Spiral->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Spiral->SetCastShadow(false);
	Spiral->SetVisibility(false);

	// The spray is drawn over the jet it winds around
	Spiral->TranslucencySortPriority = 2;

	Jet = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Jet"));
	Jet->SetupAttachment(GetPivot());
	Jet->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Jet->SetCastShadow(false);
	Jet->SetVisibility(false);

	// The drops stay where they were shed, so they live in world space
	JetDrops = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("JetDrops"));
	JetDrops->SetupAttachment(RootComponent);
	JetDrops->SetUsingAbsoluteLocation(true);
	JetDrops->SetUsingAbsoluteRotation(true);
	JetDrops->SetUsingAbsoluteScale(true);
	JetDrops->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	JetDrops->SetCastShadow(false);
	JetDrops->SetVisibility(false);
	JetDrops->TranslucencySortPriority = 1;

	// No bobbing, a shot flies straight
	BobHeight = 0.0f;
	SpinSpeed = 0.0f;
}

void AVaelWaterBolt::BeginPlay()
{
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();

	const FVector3f Shape(MagicSettings->WaterBoltTailLength, MagicSettings->WaterBoltThickness, MagicSettings->WaterBoltSpiralTurns);
	if (!CachedBoltMesh.IsValid() || (Shape.Z > 0.0f && !CachedSpiralMesh.IsValid()) || CachedShape != Shape)
	{
		CachedBoltMesh = BuildBoltMesh(Shape.X, Shape.Y);
		CachedSpiralMesh = Shape.Z > 0.0f ? BuildSpiralMesh(Shape.X, Shape.Y, Shape.Z) : nullptr;
		CachedShape = Shape;
	}

	BoltMesh = CachedBoltMesh.Get();
	GetBall()->SetStaticMesh(BoltMesh);
	Spiral->SetStaticMesh(CachedSpiralMesh.Get());

	// Real water: the jet and its drops are built every frame from the wave material, so it looks like the Welle; its own drops replace the head effect
	UMaterialInterface* WaveMaterial = MagicSettings->bWaterBoltRealWater ? VaelAssets::LoadOptional(MagicSettings->WaveMaterial) : nullptr;
	if (WaveMaterial != nullptr)
	{
		bRealWater = true;
		Seed = FMath::FRandRange(1.0f, 100.0f);
		Drops.Init(JetDropCount);
		Jet->SetMaterial(0, WaveMaterial);
		JetDrops->SetMaterial(0, WaveMaterial);
		Jet->SetVisibility(true);
		JetDrops->SetVisibility(true);
	}
	else if (GetParticles()->GetAsset() == nullptr)
	{
		GetParticles()->SetAsset(VaelAssets::LoadOptional(MagicSettings->WaterBoltHeadEffect));
	}

	if (UNiagaraSystem* TrailEffect = VaelAssets::LoadOptional(MagicSettings->WaterBoltTrailEffect))
	{
		Trail->SetAsset(TrailEffect);
		Trail->Activate(true);
	}

	Super::BeginPlay();

	ApplySource();
}

void AVaelWaterBolt::ApplySource()
{
	Super::ApplySource();

	const UVaelElementSource* BoltSource = GetSource();
	if (BoltSource == nullptr)
	{
		return;
	}

	Trail->SetVariableLinearColor(WaterColorParameter, BoltSource->ParticleColor);

	// Real water draws itself; the smooth jet and the spiral stay hidden. A source with its own wave colours paints it
	if (bRealWater)
	{
		if (BoltSource->WaveMaterial != nullptr)
		{
			Jet->SetMaterial(0, BoltSource->WaveMaterial);
			JetDrops->SetMaterial(0, BoltSource->WaveMaterial);
		}
		GetBall()->SetVisibility(false);
		Spiral->SetVisibility(false);
		return;
	}

	if (BoltSource->Material == nullptr || GetBall()->GetStaticMesh() != BoltMesh || BoltMesh == nullptr)
	{
		Spiral->SetVisibility(false);
		return;
	}

	// White spray foams on the rim, so the water reads even at speed
	if (BoltMaterial == nullptr || BoltMaterial->Parent != BoltSource->Material)
	{
		BoltMaterial = CreateJetMaterial(BoltSource->Material, this);
	}

	GetBall()->SetMaterial(0, BoltMaterial);

	// The spiral is the same water, foamed up to white spray
	if (Spiral->GetStaticMesh() == nullptr)
	{
		Spiral->SetVisibility(false);
		return;
	}

	if (SpiralMaterial == nullptr || SpiralMaterial->Parent != BoltSource->Material)
	{
		SpiralMaterial = CreateJetMaterial(BoltSource->Material, this, true);
	}

	Spiral->SetMaterial(0, SpiralMaterial);
	Spiral->SetVisibility(true);
}

UMaterialInstanceDynamic* AVaelWaterBolt::CreateJetMaterial(UMaterialInterface* SourceMaterial, UObject* Outer, bool bSpray)
{
	if (SourceMaterial == nullptr)
	{
		return nullptr;
	}

	UMaterialInstanceDynamic* Jet = UMaterialInstanceDynamic::Create(SourceMaterial, Outer);
	if (bSpray)
	{
		Jet->SetScalarParameterValue(TEXT("FoamAmount"), SpiralFoam);
		Jet->SetScalarParameterValue(TEXT("FresnelPower"), SpiralFresnelPower);
		Jet->SetScalarParameterValue(TEXT("WobbleStrength"), 0.0f);
		Jet->SetScalarParameterValue(TEXT("Murkiness"), 0.0f);
		return Jet;
	}

	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	Jet->SetScalarParameterValue(TEXT("FoamAmount"), MagicSettings->WaterBoltFoam);
	Jet->SetScalarParameterValue(TEXT("FresnelPower"), MagicSettings->WaterBoltFresnelPower);
	Jet->SetScalarParameterValue(TEXT("WobbleStrength"), BoltWobble);
	Jet->SetScalarParameterValue(TEXT("Wetness"), BoltWetness);

	float Murkiness = 0.0f;
	SourceMaterial->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("Murkiness")), Murkiness);
	Jet->SetScalarParameterValue(TEXT("Murkiness"), FMath::Max(Murkiness, MagicSettings->WaterBoltMinMurkiness));

	float Emissive = 0.0f;
	SourceMaterial->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("EmissiveStrength")), Emissive);
	Jet->SetScalarParameterValue(TEXT("EmissiveStrength"), FMath::Min(Emissive, MagicSettings->WaterBoltMaxEmissive));
	return Jet;
}

void AVaelWaterBolt::UpdateMotion(float DeltaSeconds)
{
	MotionTime += DeltaSeconds;

	// Point along the flight; before it has moved, along the shot it sits on
	const FVector Location = GetActorLocation();
	FVector Direction = bHasLastLocation ? Location - LastLocation : FVector::ZeroVector;
	if (!Direction.Normalize(0.01f))
	{
		Direction = GetPivot()->GetForwardVector();
	}

	if (bHasLastLocation && DeltaSeconds > 0.0f)
	{
		Velocity = (Location - LastLocation) / DeltaSeconds;
	}

	LastLocation = Location;
	bHasLastLocation = true;

	FRotator Rotation = Direction.Rotation();
	Rotation.Roll = MotionTime * RollSpeed;
	GetPivot()->SetWorldRotation(Rotation);

	// The jet flickers in length and pulses in width like the strokes of a pump
	const float Flicker = 1.0f + FlickerAmount * (FMath::Sin(MotionTime * FlickerSpeed) + 0.5f * FMath::Sin(MotionTime * FlickerSpeed * 2.3f + 1.0f));
	const float Pulse = 1.0f + PulseAmount * FMath::Sin(MotionTime * FlickerSpeed * 1.7f);
	GetBall()->SetRelativeScale3D(FVector(Flicker, Pulse, Pulse));

	// The spiral turns around the jet, so it seems to drill forward
	Spiral->SetRelativeRotation(FRotator(0.0f, 0.0f, MotionTime * UVaelMagicSettings::Get()->WaterBoltSpiralSpin));

	if (bRealWater)
	{
		Jet->SetRelativeScale3D(FVector(Flicker, Pulse, Pulse));
		UpdateJetDrops(DeltaSeconds);
		BuildJet();
	}
}

void AVaelWaterBolt::BuildJet()
{
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	const float Violence = MagicSettings->WaterBoltViolence;
	const float Radius = BallRadius * MagicSettings->WaterBoltThickness;
	const float Length = BallRadius * MagicSettings->WaterBoltTailLength;
	constexpr int32 Rings = JetNoseRings + 1 + JetBodyRings;
	constexpr int32 Rows = JetAround + 1;

	JetSection.Reset(Rings * Rows);

	// The outline from the front tip back: a round nose, then a body that stays thick for a while and tapers to a torn end
	FVector2D Outline[Rings];
	float Tails[Rings];
	for (int32 Ring = 0; Ring < Rings; ++Ring)
	{
		Tails[Ring] = 0.0f;
		if (Ring <= JetNoseRings)
		{
			const float Angle = HALF_PI * Ring / JetNoseRings;
			Outline[Ring] = FVector2D(Radius * NoseLength * FMath::Cos(Angle), Radius * FMath::Sin(Angle));
		}
		else
		{
			Tails[Ring] = static_cast<float>(Ring - JetNoseRings) / JetBodyRings;
			Outline[Ring] = FVector2D(-Tails[Ring] * Length, Radius * FMath::Pow(FMath::Max(1.0f - FMath::Pow(Tails[Ring], 1.6f), 0.0f), 0.9f));
		}
	}

	float Along = 0.0f;
	for (int32 Ring = 0; Ring < Rings; ++Ring)
	{
		const float Tail = Tails[Ring];
		Along += Ring > 0 ? FVector2D::Distance(Outline[Ring], Outline[Ring - 1]) / 100.0f : 0.0f;

		// Outward at right angles to the outline, straight ahead at the tip and straight back at the end
		const FVector2D Slope = (Outline[FMath::Min(Ring + 1, Rings - 1)] - Outline[FMath::Max(Ring - 1, 0)]).GetSafeNormal();

		// The tail whips about and breaks up more the further back it is
		const float Whip = Tail * Tail * Radius * 0.7f * Violence;
		const FVector Sway(0.0f, Whip * VaelWaterMesh::Noise(Tail * 2.0f - MotionTime * 4.0f, Seed), Whip * VaelWaterMesh::Noise(Seed * 1.3f, Tail * 2.0f - MotionTime * 4.0f));
		const float Torn = Violence * (0.1f + 0.4f * Tail);

		// Pale foaming water pressed together at the nose, a darker body that foams up again and tears where it runs out
		const float Light = Ring <= JetNoseRings ? FMath::Lerp(1.0f, 0.55f, static_cast<float>(Ring) / JetNoseRings) : FMath::Lerp(0.45f, 0.8f, Tail);
		const float Foam = Ring <= JetNoseRings ? FMath::Lerp(0.8f, 0.35f, static_cast<float>(Ring) / JetNoseRings) : 0.25f + 0.65f * FMath::Pow(Tail, 1.4f);
		const float Alpha = 0.95f * (1.0f - FMath::SmoothStep(0.5f, 1.0f, Tail));

		for (int32 Step = 0; Step < Rows; ++Step)
		{
			const float Angle = UE_TWO_PI * Step / JetAround;
			const float Cos = FMath::Cos(Angle);
			const float Sin = FMath::Sin(Angle);
			const float Lump = 1.0f + Torn * VaelWaterMesh::Noise(Cos * 1.2f + Tail * 6.0f - MotionTime * 9.0f + Seed, Sin * 1.2f + Seed * 0.5f);
			const float PointRadius = Outline[Ring].Y * FMath::Max(Lump, 0.2f);

			FVector Normal(Slope.Y, -Slope.X * Cos, -Slope.X * Sin);
			if (Ring == 0)
			{
				Normal = FVector::ForwardVector;
			}
			else if (Ring == Rings - 1)
			{
				Normal = FVector::BackwardVector;
			}

			const int32 Index = Ring * Rows + Step;
			JetSection.Vertices[Index] = FVector(Outline[Ring].X, PointRadius * Cos, PointRadius * Sin) + Sway;
			JetSection.Normals[Index] = Normal.GetSafeNormal();
			JetSection.UVs[Index] = FVector2D(Angle * Radius / 100.0f, Along + MotionTime * JetFoamFlow);
			JetSection.Colors[Index] = FLinearColor(Light, Foam, 0.0f, Alpha);
		}
	}

	const FVector CameraLocation = VaelWaterMesh::GetCameraLocation(GetWorld(), GetActorLocation() + FVector(-600.0f, 0.0f, 900.0f));
	Drops.Build(DropSection, FTransform::Identity, CameraLocation, 1.0f, 0.4f);

	// The jet is created anew every frame, its back drawn before its front
	const FVector ToCamera = Jet->GetComponentTransform().InverseTransformVectorNoScale(CameraLocation - GetActorLocation());
	VaelWaterMesh::BuildViewSortedTubeTriangles(Rings, JetAround, FVector::RightVector, FVector::UpVector, ToCamera, JetSection.Triangles);
	JetSection.Apply(Jet, 0, true);

	const bool bCreate = !bJetCreated;
	if (bCreate)
	{
		Drops.BuildTriangles(DropSection);
		bJetCreated = true;
	}

	DropSection.Apply(JetDrops, 0, bCreate);
}

void AVaelWaterBolt::UpdateJetDrops(float DeltaSeconds)
{
	if (DeltaSeconds <= 0.0f)
	{
		return;
	}

	// The drops fall until they hit the ground under the bolt
	const FVector Location = GetActorLocation();
	FHitResult GroundHit;
	const float GroundZ = VaelGround::TraceGround(GetWorld(), Location, Location - FVector(0.0f, 0.0f, 1000.0f), GroundHit, this) ? GroundHit.Location.Z : Location.Z - 1000.0f;
	Drops.Update(DeltaSeconds, GroundZ);

	// The body and the tail shed drops that keep some of the bolt's speed and fall away in arcs
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	const float Radius = BallRadius * MagicSettings->WaterBoltThickness;
	const float Length = BallRadius * MagicSettings->WaterBoltTailLength;
	const FTransform& JetTransform = Jet->GetComponentTransform();

	Drops.Debt = FMath::Min(Drops.Debt + MagicSettings->WaterBoltDropRate * DeltaSeconds, 10.0f);
	for (; Drops.Debt >= 1.0f; Drops.Debt -= 1.0f)
	{
		const float Tail = FMath::FRandRange(0.15f, 0.9f);
		const float Angle = FMath::FRandRange(0.0f, UE_TWO_PI);
		const FVector Out(0.0f, FMath::Cos(Angle), FMath::Sin(Angle));
		const FVector Local = FVector(-Tail * Length, 0.0f, 0.0f) + Out * Radius * (1.0f - Tail);

		const FVector WorldOut = JetTransform.TransformVectorNoScale(Out);
		const FVector DropVelocity = Velocity * FMath::FRandRange(0.1f, 0.35f) + WorldOut * FMath::FRandRange(60.0f, 180.0f) + FVector(0.0f, 0.0f, FMath::FRandRange(30.0f, 120.0f));
		const bool bClump = FMath::FRand() < 0.1f;
		const float Size = FMath::Lerp(JetDropMinSize, JetDropMaxSize, FMath::Pow(FMath::FRand(), 2.5f)) * (bClump ? 1.6f : 1.0f);

		if (!Drops.Throw(JetTransform.TransformPosition(Local), DropVelocity, FMath::FRandRange(JetDropMinLife, JetDropMaxLife), Size, bClump))
		{
			break;
		}
	}
}

UStaticMesh* AVaelWaterBolt::BuildBoltMesh(float TailLength, float Thickness)
{
	const float Radius = BallRadius * Thickness;
	const float Length = BallRadius * TailLength;

	// The outline from the front tip to the end: a slightly pointed round nose, then a body that stays thick for a while
	// and narrows to a point, like a jet of water under pressure
	TArray<FVector2f> Outline;
	for (int32 Ring = 0; Ring <= NumNoseRings; ++Ring)
	{
		const float Angle = HALF_PI * Ring / NumNoseRings;
		Outline.Add(FVector2f(Radius * NoseLength * FMath::Cos(Angle), Radius * FMath::Sin(Angle)));
	}

	for (int32 Ring = 1; Ring <= NumBodyRings; ++Ring)
	{
		const float Share = static_cast<float>(Ring) / NumBodyRings;
		Outline.Add(FVector2f(-Share * Length, Radius * FMath::Pow(FMath::Max(1.0f - FMath::Pow(Share, 1.6f), 0.0f), 0.9f)));
	}

	TArray<TArray<FVector3f>> RingPositions;
	TArray<TArray<FVector3f>> RingNormals;
	for (int32 Ring = 0; Ring < Outline.Num(); ++Ring)
	{
		const FVector2f Previous = Outline[FMath::Max(Ring - 1, 0)];
		const FVector2f Next = Outline[FMath::Min(Ring + 1, Outline.Num() - 1)];
		const FVector2f Along = (Next - Previous).GetSafeNormal();

		TArray<FVector3f>& Positions = RingPositions.AddDefaulted_GetRef();
		TArray<FVector3f>& Normals = RingNormals.AddDefaulted_GetRef();
		for (int32 Step = 0; Step < NumAround; ++Step)
		{
			const float Angle = UE_TWO_PI * Step / NumAround;
			Positions.Add(FVector3f(Outline[Ring].X, Outline[Ring].Y * FMath::Cos(Angle), Outline[Ring].Y * FMath::Sin(Angle)));

			// Outward, at right angles to the outline; straight ahead and straight back at the two tips
			FVector3f Normal(Along.Y, -Along.X * FMath::Cos(Angle), -Along.X * FMath::Sin(Angle));
			if (Ring == 0)
			{
				Normal = FVector3f(1.0f, 0.0f, 0.0f);
			}
			else if (Ring == Outline.Num() - 1)
			{
				Normal = FVector3f(-1.0f, 0.0f, 0.0f);
			}

			Normals.Add(Normal.GetSafeNormal());
		}
	}

	return BuildRingMesh(RingPositions, RingNormals);
}

UStaticMesh* AVaelWaterBolt::BuildSpiralMesh(float TailLength, float Thickness, float Turns)
{
	const float Radius = BallRadius * Thickness;
	const float Length = BallRadius * TailLength;

	// The middle line of the tube: starts just behind the nose and winds backwards, ever wider
	auto Center = [&](float Share)
	{
		const float Angle = UE_TWO_PI * Turns * Share;
		const float Wide = Radius * FMath::Lerp(SpiralStartRadius, SpiralEndRadius, Share);
		return FVector3f(Radius * 0.3f - Share * Length * SpiralReach, Wide * FMath::Cos(Angle), Wide * FMath::Sin(Angle));
	};

	TArray<TArray<FVector3f>> RingPositions;
	TArray<TArray<FVector3f>> RingNormals;
	for (int32 Ring = 0; Ring <= NumSpiralRings; ++Ring)
	{
		const float Share = static_cast<float>(Ring) / NumSpiralRings;
		const FVector3f Here = Center(Share);
		const FVector3f Tangent = (Center(FMath::Min(Share + 0.002f, 1.0f)) - Center(FMath::Max(Share - 0.002f, 0.0f))).GetSafeNormal();

		// Across the tube: away from the jet, then at right angles to both
		FVector3f Out(0.0f, Here.Y, Here.Z);
		Out = (Out - Tangent * FVector3f::DotProduct(Out, Tangent)).GetSafeNormal();
		const FVector3f Side = FVector3f::CrossProduct(Tangent, Out);

		// Thin at the start, thickest early on, thinning out at the end where the spray scatters
		const float Tube = Radius * SpiralThickness * FMath::Min(Share * 8.0f, 1.0f) * FMath::Sqrt(1.0f - Share);

		TArray<FVector3f>& Positions = RingPositions.AddDefaulted_GetRef();
		TArray<FVector3f>& Normals = RingNormals.AddDefaulted_GetRef();
		for (int32 Step = 0; Step < NumSpiralAround; ++Step)
		{
			const float Angle = UE_TWO_PI * Step / NumSpiralAround;
			const FVector3f Normal = Out * FMath::Cos(Angle) + Side * FMath::Sin(Angle);
			Positions.Add(Here + Normal * Tube);
			Normals.Add(Normal);
		}
	}

	return BuildRingMesh(RingPositions, RingNormals);
}

UStaticMesh* AVaelWaterBolt::BuildWaveMesh(float HalfWidth, float Sweep, float Thickness)
{
	// Cross-section of a breaking wave, in shares of its depth (X) and height (Z): up the gentle back, over the crest,
	// out to the tip of the lip, under it and down the steep face; the last edge closes it along the ground
	static const FVector2f Profile[] = {
		{ -0.5f, 0.0f }, { -0.4f, 0.45f }, { -0.22f, 0.85f }, { 0.0f, 1.0f }, { 0.22f, 0.95f },
		{ 0.4f, 0.75f }, { 0.27f, 0.62f }, { 0.17f, 0.35f }, { 0.27f, 0.0f } };
	constexpr int32 NumProfile = UE_ARRAY_COUNT(Profile);
	constexpr int32 NumAcross = 25;

	// Outward at every point: the profile runs clockwise, so outward is to the left of each edge
	FVector2f ProfileNormals[NumProfile];
	for (int32 Point = 0; Point < NumProfile; ++Point)
	{
		const FVector2f Before = Profile[Point] - Profile[(Point + NumProfile - 1) % NumProfile];
		const FVector2f After = Profile[(Point + 1) % NumProfile] - Profile[Point];
		const FVector2f Normal = FVector2f(-Before.Y, Before.X).GetSafeNormal() + FVector2f(-After.Y, After.X).GetSafeNormal();
		ProfileNormals[Point] = Normal.GetSafeNormal();
	}

	TArray<TArray<FVector3f>> RingPositions;
	TArray<TArray<FVector3f>> RingNormals;
	for (int32 Ring = 0; Ring < NumAcross; ++Ring)
	{
		const float Across = Ring / (NumAcross - 1.0f) * 2.0f - 1.0f;

		// Highest in the middle, falling away to nothing at both ends
		const float Shrink = FMath::Pow(FMath::Max(1.0f - Across * Across, 0.0f), 0.35f);
		const float Height = 100.0f * Shrink * (1.0f - 0.3f * Across * Across);
		const float Depth = Thickness * Shrink;
		const float Back = -Sweep * Across * Across;

		TArray<FVector3f>& Positions = RingPositions.AddDefaulted_GetRef();
		TArray<FVector3f>& Normals = RingNormals.AddDefaulted_GetRef();
		for (int32 Point = 0; Point < NumProfile; ++Point)
		{
			Positions.Add(FVector3f(Back + Profile[Point].X * Depth, Across * HalfWidth, Profile[Point].Y * Height));
			Normals.Add(FVector3f(ProfileNormals[Point].X * 100.0f, 0.0f, ProfileNormals[Point].Y * FMath::Max(Thickness, 1.0f)).GetSafeNormal());
		}
	}

	return BuildRingMesh(RingPositions, RingNormals);
}

UStaticMesh* AVaelWaterBolt::BuildRingMesh(const TArray<TArray<FVector3f>>& RingPositions, const TArray<TArray<FVector3f>>& RingNormals)
{
	FMeshDescription Description;
	FStaticMeshAttributes Attributes(Description);
	Attributes.Register();

	TVertexAttributesRef<FVector3f> Positions = Attributes.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> Normals = Attributes.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector2f> UVs = Attributes.GetVertexInstanceUVs();
	UVs.SetNumChannels(1);

	const FName SlotName(TEXT("Water"));
	const FPolygonGroupID Group = Description.CreatePolygonGroup();
	Attributes.GetPolygonGroupMaterialSlotNames()[Group] = SlotName;

	const int32 NumRings = RingPositions.Num();
	const int32 Around = RingPositions[0].Num();

	// One vertex per point; the seam gets its own instances so the texture wraps
	TArray<FVertexInstanceID> Instances;
	for (int32 Ring = 0; Ring < NumRings; ++Ring)
	{
		TArray<FVertexID> RingVertices;
		for (int32 Step = 0; Step < Around; ++Step)
		{
			const FVertexID Vertex = Description.CreateVertex();
			Positions[Vertex] = RingPositions[Ring][Step];
			RingVertices.Add(Vertex);
		}

		for (int32 Step = 0; Step <= Around; ++Step)
		{
			const FVertexInstanceID Instance = Description.CreateVertexInstance(RingVertices[Step % Around]);
			Normals[Instance] = RingNormals[Ring][Step % Around].GetSafeNormal();
			UVs.Set(Instance, 0, FVector2f(static_cast<float>(Step) / Around, static_cast<float>(Ring) / (NumRings - 1)));
			Instances.Add(Instance);
		}
	}

	// Unreal draws a triangle from the side its corners run clockwise; check once on a ring in the middle which order that is
	const int32 Stride = Around + 1;
	auto Corner = [&](int32 Ring, int32 Step) { return Instances[Ring * Stride + Step]; };
	auto Position = [&](FVertexInstanceID Instance) { return Positions[Description.GetVertexInstanceVertex(Instance)]; };

	const int32 Middle = NumRings / 2;
	const FVector3f A = Position(Corner(Middle, 0));
	const FVector3f B = Position(Corner(Middle + 1, 0));
	const FVector3f C = Position(Corner(Middle, 1));
	const bool bFlip = FVector3f::DotProduct(FVector3f::CrossProduct(B - A, C - A), Normals[Corner(Middle, 0)]) > 0.0f;

	for (int32 Ring = 0; Ring + 1 < NumRings; ++Ring)
	{
		for (int32 Step = 0; Step < Around; ++Step)
		{
			const FVertexInstanceID Here = Corner(Ring, Step);
			const FVertexInstanceID Back = Corner(Ring + 1, Step);
			const FVertexInstanceID Side = Corner(Ring, Step + 1);
			const FVertexInstanceID Diagonal = Corner(Ring + 1, Step + 1);

			if (bFlip)
			{
				Description.CreateTriangle(Group, { Here, Side, Back });
				Description.CreateTriangle(Group, { Side, Diagonal, Back });
			}
			else
			{
				Description.CreateTriangle(Group, { Here, Back, Side });
				Description.CreateTriangle(Group, { Side, Back, Diagonal });
			}
		}
	}

	FStaticMeshOperations::ComputeTriangleTangentsAndNormals(Description);
	FStaticMeshOperations::ComputeTangentsAndNormals(Description, EComputeNTBsFlags::Tangents);

	UStaticMesh* Mesh = NewObject<UStaticMesh>(GetTransientPackage(), NAME_None, RF_Transient);
	Mesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, SlotName, SlotName));

	UStaticMesh::FBuildMeshDescriptionsParams Params;
	Params.bBuildSimpleCollision = false;
	Params.bFastBuild = true;
	Mesh->BuildFromMeshDescriptions({ &Description }, Params);

	return Mesh;
}
