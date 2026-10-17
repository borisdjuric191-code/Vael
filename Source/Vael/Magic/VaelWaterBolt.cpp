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
	/** Radius of the head, like the engine sphere, so the bolt scales like the orb */
	constexpr float HeadRadius = 50.0f;

	/** Points around the bolt and along its head and tail */
	constexpr int32 NumAround = 24;
	constexpr int32 NumHeadRings = 8;
	constexpr int32 NumTailRings = 24;

	/** How quickly and how much the tail flickers in length */
	constexpr float FlickerSpeed = 13.0f;
	constexpr float FlickerAmount = 0.07f;

	/** Degrees per second the bolt turns around its flight axis */
	constexpr float RollSpeed = 220.0f;

	/** The surface wobbles less than on the floating ball, so the shape stays clean at speed */
	constexpr float BoltWobble = 1.5f;

	/** A bolt mirrors the sky less than the floating ball, so its body keeps its color in bright light */
	constexpr float BoltWetness = 0.6f;

	/** Last bolt mesh built and its tail, shared by all bolts */
	TWeakObjectPtr<UStaticMesh> CachedMesh;
	float CachedTailLength = 0.0f;
}

AVaelWaterBolt::AVaelWaterBolt()
{
	Trail = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Trail"));
	Trail->SetupAttachment(GetPivot());
	Trail->SetAutoActivate(false);

	// No bobbing, a shot flies straight
	BobHeight = 0.0f;
	SpinSpeed = 0.0f;
}

void AVaelWaterBolt::BeginPlay()
{
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();

	if (!CachedMesh.IsValid() || CachedTailLength != MagicSettings->WaterBoltTailLength)
	{
		CachedMesh = BuildBoltMesh(MagicSettings->WaterBoltTailLength);
		CachedTailLength = MagicSettings->WaterBoltTailLength;
	}

	BoltMesh = CachedMesh.Get();
	GetBall()->SetStaticMesh(BoltMesh);

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

	// The tail flickers like a jet of water, the head stays round
	const float Flicker = 1.0f + FlickerAmount * (FMath::Sin(MotionTime * FlickerSpeed) + 0.5f * FMath::Sin(MotionTime * FlickerSpeed * 2.3f + 1.0f));
	GetBall()->SetRelativeScale3D(FVector(Flicker, 1.0f, 1.0f));
}

UStaticMesh* AVaelWaterBolt::BuildBoltMesh(float TailLength)
{
	// The outline from the front tip to the end of the tail: a half ball, then a tail that narrows smoothly to a point
	TArray<FVector2f> Outline;
	for (int32 Ring = 0; Ring <= NumHeadRings; ++Ring)
	{
		const float Angle = HALF_PI * Ring / NumHeadRings;
		Outline.Add(FVector2f(HeadRadius * FMath::Cos(Angle), HeadRadius * FMath::Sin(Angle)));
	}

	for (int32 Ring = 1; Ring <= NumTailRings; ++Ring)
	{
		const float Share = static_cast<float>(Ring) / NumTailRings;
		// Narrows quickly behind the head and runs out long and thin, like the tail of a falling star
		const float Smooth = Share * Share * (3.0f - 2.0f * Share);
		Outline.Add(FVector2f(-Share * TailLength * HeadRadius, HeadRadius * FMath::Pow(FMath::Max(1.0f - Smooth, 0.0f), 1.7f)));
	}

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

	// One ring of points per step of the outline; the seam gets its own instances so the texture wraps
	TArray<FVertexInstanceID> Instances;
	for (int32 Ring = 0; Ring < Outline.Num(); ++Ring)
	{
		const FVector2f Previous = Outline[FMath::Max(Ring - 1, 0)];
		const FVector2f Next = Outline[FMath::Min(Ring + 1, Outline.Num() - 1)];
		const FVector2f Along = (Next - Previous).GetSafeNormal();

		TArray<FVertexID> RingVertices;
		for (int32 Step = 0; Step < NumAround; ++Step)
		{
			const float Angle = UE_TWO_PI * Step / NumAround;
			const FVertexID Vertex = Description.CreateVertex();
			Positions[Vertex] = FVector3f(Outline[Ring].X, Outline[Ring].Y * FMath::Cos(Angle), Outline[Ring].Y * FMath::Sin(Angle));
			RingVertices.Add(Vertex);
		}

		for (int32 Step = 0; Step <= NumAround; ++Step)
		{
			const float Angle = UE_TWO_PI * Step / NumAround;
			const FVertexInstanceID Instance = Description.CreateVertexInstance(RingVertices[Step % NumAround]);

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

			Normals[Instance] = Normal.GetSafeNormal();
			UVs.Set(Instance, 0, FVector2f(static_cast<float>(Step) / NumAround, static_cast<float>(Ring) / (Outline.Num() - 1)));
			Instances.Add(Instance);
		}
	}

	// Unreal draws a triangle from the side its corners run clockwise; check once on the widest ring which order that is
	const int32 Stride = NumAround + 1;
	auto Corner = [&](int32 Ring, int32 Step) { return Instances[Ring * Stride + Step]; };
	auto Position = [&](FVertexInstanceID Instance) { return Positions[Description.GetVertexInstanceVertex(Instance)]; };

	const FVector3f A = Position(Corner(NumHeadRings, 0));
	const FVector3f B = Position(Corner(NumHeadRings + 1, 0));
	const FVector3f C = Position(Corner(NumHeadRings, 1));
	const bool bFlip = FVector3f::DotProduct(FVector3f::CrossProduct(B - A, C - A), Normals[Corner(NumHeadRings, 0)]) > 0.0f;

	for (int32 Ring = 0; Ring + 1 < Outline.Num(); ++Ring)
	{
		for (int32 Step = 0; Step < NumAround; ++Step)
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
