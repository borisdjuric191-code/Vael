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
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"
#include "UObject/Package.h"
#include "VaelAssets.h"

namespace
{
	/** Radius of the engine sphere the orb flies as, so the bolt scales like the orb */
	constexpr float BallRadius = 50.0f;

	/** Length of the round nose against the radius of the jet: a little pointed, not a bulging ball */
	constexpr float NoseLength = 1.3f;

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
	constexpr float FlickerSpeed = 16.0f;
	constexpr float FlickerAmount = 0.09f;
	constexpr float PulseAmount = 0.04f;

	/** Degrees per second the bolt turns around its flight axis */
	constexpr float RollSpeed = 220.0f;

	/** The surface wobbles less than on the floating ball, so the shape stays clean at speed */
	constexpr float BoltWobble = 1.5f;

	/** A bolt mirrors the sky less than the floating ball, so its body keeps its color in bright light */
	constexpr float BoltWetness = 0.6f;

	/** The spiral is foamed up until it is white spray, bright over its whole width */
	constexpr float SpiralFoam = 40.0f;
	constexpr float SpiralFresnelPower = 0.5f;

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

	if (GetParticles()->GetAsset() == nullptr)
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

	if (BoltSource->Material == nullptr || GetBall()->GetStaticMesh() != BoltMesh || BoltMesh == nullptr)
	{
		Spiral->SetVisibility(false);
		return;
	}

	// White spray foams on the rim, so the water reads even at speed
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	if (BoltMaterial == nullptr || BoltMaterial->Parent != BoltSource->Material)
	{
		BoltMaterial = UMaterialInstanceDynamic::Create(BoltSource->Material, this);
	}

	BoltMaterial->SetScalarParameterValue(TEXT("FoamAmount"), MagicSettings->WaterBoltFoam);
	BoltMaterial->SetScalarParameterValue(TEXT("FresnelPower"), MagicSettings->WaterBoltFresnelPower);
	BoltMaterial->SetScalarParameterValue(TEXT("WobbleStrength"), BoltWobble);
	BoltMaterial->SetScalarParameterValue(TEXT("Wetness"), BoltWetness);

	float Murkiness = 0.0f;
	BoltSource->Material->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("Murkiness")), Murkiness);
	BoltMaterial->SetScalarParameterValue(TEXT("Murkiness"), FMath::Max(Murkiness, MagicSettings->WaterBoltMinMurkiness));

	float Emissive = 0.0f;
	BoltSource->Material->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("EmissiveStrength")), Emissive);
	BoltMaterial->SetScalarParameterValue(TEXT("EmissiveStrength"), FMath::Min(Emissive, MagicSettings->WaterBoltMaxEmissive));
	GetBall()->SetMaterial(0, BoltMaterial);

	// The spiral is the same water, foamed up to white spray
	if (Spiral->GetStaticMesh() == nullptr)
	{
		Spiral->SetVisibility(false);
		return;
	}

	if (SpiralMaterial == nullptr || SpiralMaterial->Parent != BoltSource->Material)
	{
		SpiralMaterial = UMaterialInstanceDynamic::Create(BoltSource->Material, this);
	}

	SpiralMaterial->SetScalarParameterValue(TEXT("FoamAmount"), SpiralFoam);
	SpiralMaterial->SetScalarParameterValue(TEXT("FresnelPower"), SpiralFresnelPower);
	SpiralMaterial->SetScalarParameterValue(TEXT("WobbleStrength"), 0.0f);
	SpiralMaterial->SetScalarParameterValue(TEXT("Murkiness"), 0.0f);
	Spiral->SetMaterial(0, SpiralMaterial);
	Spiral->SetVisibility(true);
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
