// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nature/VaelPlantLifeComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "StaticMeshResources.h"
#include "World/VaelGround.h"
#include "World/VaelRegion.h"

namespace
{
	/** The drop and blot built for the Moderglocke serve every dripping plant; the game colours them */
	const TCHAR* PlantDropMeshPath = TEXT("/Game/Vael/Kreaturen/Wurzelforst/Moderglocke/Effekte/SM_Moderglocke_Gifttropfen.SM_Moderglocke_Gifttropfen");
	const TCHAR* PlantBlotMeshPath = TEXT("/Game/Vael/Kreaturen/Wurzelforst/Moderglocke/Effekte/SM_Moderglocke_Giftlache.SM_Moderglocke_Giftlache");

	constexpr int32 PlantMaxDrops = 8;
	constexpr int32 PlantMaxBlots = 10;
	constexpr int32 PlantMaxPlaces = 80;

	/** Seconds a drop swells before it lets go, and a blot stays */
	constexpr float PlantDropSwell = 0.9f;
	constexpr float PlantBlotLife = 2.2f;

	/** Width and length of the drop mesh, width of the blot mesh in cm */
	constexpr float PlantDropMeshWidth = 32.0f;
	constexpr float PlantDropMeshLength = 100.0f;
	constexpr float PlantBlotMeshSize = 80.0f;
}

UVaelPlantLifeComponent::UVaelPlantLifeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

UInstancedStaticMeshComponent* UVaelPlantLifeComponent::AddPieces(const TCHAR* MeshPath, float Roughness)
{
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, MeshPath);
	if (Mesh == nullptr)
	{
		return nullptr;
	}

	UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(GetOwner());
	Component->SetupAttachment(this);
	Component->SetStaticMesh(Mesh);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCastShadow(false);
	Component->bReceivesDecals = false;
	Component->SetCanEverAffectNavigation(false);
	Component->SetUsingAbsoluteLocation(true);
	Component->SetUsingAbsoluteRotation(true);
	Component->SetUsingAbsoluteScale(true);
	Component->RegisterComponent();
	Component->SetWorldTransform(FTransform::Identity);
	if (UMaterialInstanceDynamic* Material = Component->CreateDynamicMaterialInstance(0))
	{
		Material->SetVectorParameterValue(TEXT("Farbe"), Life.DripColor);
		Material->SetScalarParameterValue(TEXT("Glut"), Life.DripGlow);
		Material->SetScalarParameterValue(TEXT("Rauheit"), Roughness);
	}
	const FTransform Hidden(FQuat::Identity, GetComponentLocation(), FVector::ZeroVector);
	for (int32 Index = 0; Index < FMath::Max(PlantMaxDrops, PlantMaxBlots); ++Index)
	{
		Component->AddInstance(Hidden, true);
	}
	return Component;
}

void UVaelPlantLifeComponent::SamplePlaces()
{
	// Corners that look down, well above the foot: the mouths of the bells and the tips of hanging leaves
	Places.Reset();
	const UStaticMesh* Mesh = Model->GetStaticMesh();
	const FStaticMeshRenderData* RenderData = Mesh != nullptr ? Mesh->GetRenderData() : nullptr;
	if (RenderData == nullptr || RenderData->LODResources.Num() == 0)
	{
		return;
	}
	const FStaticMeshVertexBuffers& Buffers = RenderData->LODResources[0].VertexBuffers;
	const int32 NumVertices = Buffers.PositionVertexBuffer.GetNumVertices();
	if (NumVertices == 0 || Buffers.PositionVertexBuffer.GetVertexData() == nullptr || Buffers.StaticMeshVertexBuffer.GetTangentData() == nullptr)
	{
		return;
	}

	const FBox MeshBox = Mesh->GetBoundingBox();
	const float MinZ = MeshBox.Min.Z + (MeshBox.Max.Z - MeshBox.Min.Z) * 0.3f;
	FRandomStream Random(GetTypeHash(GetOwner()->GetActorLocation()));
	for (int32 Try = 0; Try < PlantMaxPlaces * 40 && Places.Num() < PlantMaxPlaces; ++Try)
	{
		const int32 Index = Random.RandHelper(NumVertices);
		const FVector Place(Buffers.PositionVertexBuffer.VertexPosition(Index));
		const FVector Normal(Buffers.StaticMeshVertexBuffer.VertexTangentZ(Index));
		if (Place.Z >= MinZ && Normal.Z < -0.75f)
		{
			Places.Add(Place);
		}
	}
}

void UVaelPlantLifeComponent::Show(UStaticMeshComponent* InModel, const FVaelPlantLife& InLife)
{
	Model = InModel;
	Life = InLife;
	if (Model == nullptr)
	{
		SetComponentTickEnabled(false);
		return;
	}

	Seed = FMath::FRandRange(0.0f, 100.0f);
	RestRotation = Model->GetRelativeRotation();
	Hanging.Init(FDrop(), PlantMaxDrops);
	Lying.Init(FVector4(0.0f, 0.0f, 0.0f, PlantBlotLife), PlantMaxBlots);
	FHitResult GroundHit;
	const FVector Foot = Model->GetComponentLocation();
	GroundZ = VaelGround::TraceGround(GetWorld(), Foot + FVector(0.0f, 0.0f, 100.0f), Foot - FVector(0.0f, 0.0f, 400.0f), GroundHit) ? GroundHit.Location.Z : Foot.Z;

	if (Life.DripRate > 0.0f && Drops == nullptr)
	{
		SamplePlaces();
		Drops = AddPieces(PlantDropMeshPath, 0.1f);
		Blots = AddPieces(PlantBlotMeshPath, 0.1f);
	}
	if (Drops != nullptr)
	{
		Drops->SetVisibility(true);
		Blots->SetVisibility(true);
	}
	SetComponentTickEnabled(true);
}

void UVaelPlantLifeComponent::Stop()
{
	if (Model != nullptr)
	{
		Model->SetRelativeRotation(RestRotation);
	}
	if (Drops != nullptr)
	{
		Drops->SetVisibility(false);
		Blots->SetVisibility(false);
	}
	Model = nullptr;
	SetComponentTickEnabled(false);
}

void UVaelPlantLifeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Model == nullptr || !Model->WasRecentlyRendered(0.3f))
	{
		return;
	}

	// Rocks about its foot in two slow uneven waves, and leans away from the wind, gusts and all
	const float Time = GetWorld()->GetTimeSeconds() + Seed;
	const FVector Wind = AVaelRegion::GetWindAt(GetWorld(), Model->GetComponentLocation());
	const float Gust = 0.7f + 0.3f * FMath::Sin(Time * 1.9f) * FMath::Sin(Time * 0.63f);
	const FVector Lean = Wind.GetSafeNormal2D() * FMath::Clamp(Wind.Size2D() / 500.0f, 0.0f, 1.0f) * 3.5f * Gust;
	const float Step = UE_TWO_PI / Life.SwayPeriod;
	const float AlongX = Life.Sway * (0.7f * FMath::Sin(Time * Step) + 0.3f * FMath::Sin(Time * Step * 2.3f + 1.0f)) + Lean.X;
	const float AlongY = Life.Sway * (0.7f * FMath::Sin(Time * Step * 0.83f + 2.0f) + 0.3f * FMath::Sin(Time * Step * 1.9f)) + Lean.Y;
	// Leaning towards +X is a pitch down, towards +Y a roll
	Model->SetRelativeRotation((FQuat(FRotator(-AlongX, 0.0f, AlongY)) * RestRotation.Quaternion()).Rotator());

	if (Drops == nullptr || Blots == nullptr || Places.IsEmpty())
	{
		return;
	}

	Credit = FMath::Min(Credit + Life.DripRate * DeltaTime, 1.5f);
	const FTransform ModelTransform = Model->GetComponentTransform();
	const FTransform Hidden(FQuat::Identity, ModelTransform.GetLocation(), FVector::ZeroVector);
	TArray<FTransform> DropTransforms;
	TArray<FTransform> BlotTransforms;
	DropTransforms.Init(Hidden, FMath::Max(PlantMaxDrops, PlantMaxBlots));
	BlotTransforms.Init(Hidden, FMath::Max(PlantMaxDrops, PlantMaxBlots));
	const float Width = Life.DripSize;

	for (int32 Index = 0; Index < Hanging.Num(); ++Index)
	{
		FDrop& Drop = Hanging[Index];
		if (Drop.Age < 0.0f)
		{
			if (Credit < 1.0f)
			{
				continue;
			}
			Credit -= 1.0f;
			Drop.Place = Places[FMath::RandHelper(Places.Num())];
			Drop.Age = 0.0f;
			Drop.Speed = 0.0f;
			Drop.Size = FMath::FRandRange(0.75f, 1.25f);
			Drop.bFalling = false;
		}

		Drop.Age += DeltaTime;
		if (!Drop.bFalling)
		{
			// Swells where it hangs and goes with the plant; heavy enough, it lets go
			const float Grown = FMath::Clamp(Drop.Age / PlantDropSwell, 0.0f, 1.0f);
			Drop.Fallen = ModelTransform.TransformPosition(Drop.Place);
			const float Thick = Width * Drop.Size * Grown;
			DropTransforms[Index] = FTransform(FQuat::Identity, Drop.Fallen - FVector(0.0f, 0.0f, Thick * 2.2f),
				FVector(Thick / PlantDropMeshWidth, Thick / PlantDropMeshWidth, Thick * 2.2f / PlantDropMeshLength));
			Drop.bFalling = Drop.Age >= PlantDropSwell;
			continue;
		}

		Drop.Speed += 980.0f * DeltaTime;
		Drop.Fallen.Z -= Drop.Speed * DeltaTime;
		if (Drop.Fallen.Z <= GroundZ)
		{
			Lying[NextBlot] = FVector4(Drop.Fallen.X, Drop.Fallen.Y, Drop.Size, 0.0f);
			NextBlot = (NextBlot + 1) % Lying.Num();
			Drop.Age = -1.0f;
			continue;
		}
		const float Thick = Width * Drop.Size;
		const float Long = Thick * (2.2f + Drop.Speed / 250.0f);
		DropTransforms[Index] = FTransform(FQuat::Identity, Drop.Fallen - FVector(0.0f, 0.0f, Long),
			FVector(Thick / PlantDropMeshWidth, Thick / PlantDropMeshWidth, Long / PlantDropMeshLength));
	}

	for (int32 Index = 0; Index < Lying.Num(); ++Index)
	{
		FVector4& Blot = Lying[Index];
		if (Blot.W >= PlantBlotLife)
		{
			continue;
		}
		Blot.W += DeltaTime;
		const float Spread = FMath::InterpEaseOut(0.3f, 1.0f, FMath::Clamp(Blot.W / 0.15f, 0.0f, 1.0f), 2.0f) * FMath::Clamp((PlantBlotLife - Blot.W) / 0.9f, 0.0f, 1.0f);
		const float Size = Width * 3.2f * Blot.Z * Spread;
		BlotTransforms[Index] = FTransform(FRotator(0.0f, Index * 77.0f, 0.0f).Quaternion(), FVector(Blot.X, Blot.Y, GroundZ + 0.8f), FVector(Size / PlantBlotMeshSize, Size / PlantBlotMeshSize, 0.5f));
	}

	Drops->BatchUpdateInstancesTransforms(0, DropTransforms, true, true, true);
	Blots->BatchUpdateInstancesTransforms(0, BlotTransforms, true, true, true);
}
