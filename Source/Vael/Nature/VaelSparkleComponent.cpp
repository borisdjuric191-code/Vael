// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nature/VaelSparkleComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "StaticMeshResources.h"

namespace
{
	const TCHAR* SparkleStarMeshPath = TEXT("/Game/Vael/Requisiten/Effekte/SM_Funkeln.SM_Funkeln");

	/** Stars alive at once on one model, places remembered on its surface, width of the star mesh in cm */
	constexpr int32 SparkleMaxGlints = 10;
	constexpr int32 SparkleMaxPlaces = 160;
	constexpr float SparkleStarMeshSize = 100.0f;
}

UVaelSparkleComponent::UVaelSparkleComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

UInstancedStaticMeshComponent* UVaelSparkleComponent::AddStars(const FLinearColor& Color)
{
	UStaticMesh* Star = LoadObject<UStaticMesh>(nullptr, SparkleStarMeshPath);
	if (Star == nullptr)
	{
		return nullptr;
	}

	UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(GetOwner());
	Component->SetupAttachment(this);
	Component->SetStaticMesh(Star);
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
		Material->SetVectorParameterValue(TEXT("Farbe"), Color);
		Material->SetScalarParameterValue(TEXT("Glut"), Sparkle.Brightness);
	}
	const FTransform Hidden(FQuat::Identity, GetComponentLocation(), FVector::ZeroVector);
	for (int32 Index = 0; Index < SparkleMaxGlints; ++Index)
	{
		Component->AddInstance(Hidden, true);
	}
	return Component;
}

void UVaelSparkleComponent::SamplePlaces()
{
	// Corners of the model that look up or outward, in its own space: a star never flares on the underside
	Places.Reset();
	const UStaticMesh* Mesh = Model != nullptr ? Model->GetStaticMesh() : nullptr;
	const FStaticMeshRenderData* RenderData = Mesh != nullptr ? Mesh->GetRenderData() : nullptr;
	if (RenderData == nullptr || RenderData->LODResources.Num() == 0)
	{
		return;
	}

	const FStaticMeshVertexBuffers& Buffers = RenderData->LODResources[0].VertexBuffers;
	const int32 NumVertices = Buffers.PositionVertexBuffer.GetNumVertices();
	if (NumVertices == 0 || Buffers.PositionVertexBuffer.GetVertexData() == nullptr || Buffers.StaticMeshVertexBuffer.GetTangentData() == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Sparkle: %s keeps no vertices in memory (allow CPU access on the mesh)"), *GetNameSafe(Mesh));
		return;
	}

	const FBox MeshBox = Mesh->GetBoundingBox();
	const float MinZ = MeshBox.Max.Z - (MeshBox.Max.Z - MeshBox.Min.Z) * Sparkle.TopShare;
	FRandomStream Random(GetTypeHash(GetOwner()->GetActorLocation()));
	for (int32 Try = 0; Try < SparkleMaxPlaces * 12 && Places.Num() < SparkleMaxPlaces; ++Try)
	{
		const int32 Index = Random.RandHelper(NumVertices);
		const FVector Place(Buffers.PositionVertexBuffer.VertexPosition(Index));
		const FVector Normal(Buffers.StaticMeshVertexBuffer.VertexTangentZ(Index));
		if (Place.Z >= MinZ && Normal.Z > -0.25f)
		{
			Places.Add(Place + Normal * 1.5f);
		}
	}
}

void UVaelSparkleComponent::Show(UStaticMeshComponent* InModel, const FVaelSparkle& InSparkle, float InStrength)
{
	const bool bNewModel = InModel != Model || (InModel != nullptr && Places.IsEmpty());
	Model = InModel;
	Sparkle = InSparkle;
	Strength = InStrength;

	if (Model == nullptr || (Sparkle.Rate <= 0.0f && Sparkle.Pulse <= 0.0f))
	{
		SetComponentTickEnabled(false);
		return;
	}

	if (bNewModel)
	{
		Seed = FMath::FRandRange(0.0f, 100.0f);
		Glints.SetNum(SparkleMaxGlints);
		SamplePlaces();
	}
	if (Stars == nullptr && Sparkle.Rate > 0.0f)
	{
		Stars = AddStars(Sparkle.Color);
		SecondStars = Sparkle.SecondShare > 0.0f ? AddStars(Sparkle.SecondColor) : nullptr;
	}
	SetComponentTickEnabled(true);
}

void UVaelSparkleComponent::SetGlow(UMaterialInstanceDynamic* Material, float Glow)
{
	GlowMaterial = Material;
	BaseGlow = Glow;
}

void UVaelSparkleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Model == nullptr)
	{
		return;
	}

	// Nothing to do for a model nobody sees; its stars are put away once
	const bool bSeen = Model->IsVisible() && Model->WasRecentlyRendered(0.3f);
	const float Time = GetWorld()->GetTimeSeconds();

	if (GlowMaterial != nullptr && Sparkle.Pulse > 0.0f && bSeen)
	{
		const float Beat = 0.5f + 0.5f * FMath::Sin(UE_TWO_PI * Time / Sparkle.PulsePeriod + Seed);
		GlowMaterial->SetScalarParameterValue(TEXT("GlutStaerke"), BaseGlow * (1.0f - Sparkle.Pulse + 2.0f * Sparkle.Pulse * Beat * Beat));
	}

	if (Stars == nullptr || Places.IsEmpty())
	{
		return;
	}

	if (bSeen && Strength > 0.0f)
	{
		Credit = FMath::Min(Credit + Sparkle.Rate * Strength * DeltaTime, 2.0f);
	}

	const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	const FVector Camera = PlayerController != nullptr && PlayerController->PlayerCameraManager != nullptr ? PlayerController->PlayerCameraManager->GetCameraLocation() : GetComponentLocation() + FVector(0.0f, 0.0f, 1000.0f);
	const FTransform ModelTransform = Model->GetComponentTransform();
	const FTransform Hidden(FQuat::Identity, ModelTransform.GetLocation(), FVector::ZeroVector);

	TArray<FTransform> First;
	TArray<FTransform> Second;
	First.Init(Hidden, Glints.Num());
	Second.Init(Hidden, SecondStars != nullptr ? Glints.Num() : 0);
	bool bAnyAlive = false;

	for (int32 Index = 0; Index < Glints.Num(); ++Index)
	{
		FGlint& Glint = Glints[Index];
		if (Glint.Age >= Glint.Life)
		{
			if (Credit < 1.0f || !bSeen)
			{
				continue;
			}
			Credit -= 1.0f;
			Glint.Place = Places[FMath::RandHelper(Places.Num())];
			Glint.Age = 0.0f;
			Glint.Life = FMath::FRandRange(0.35f, 0.75f);
			Glint.Size = Sparkle.Size * FMath::FRandRange(0.6f, 1.3f);
			Glint.Turn = FMath::FRandRange(0.0f, 90.0f);
			Glint.bSecond = SecondStars != nullptr && FMath::FRand() < Sparkle.SecondShare;
		}

		Glint.Age += DeltaTime;
		if (Glint.Age >= Glint.Life)
		{
			continue;
		}
		bAnyAlive = true;

		// Flares up quickly, dies away slowly, turning a little; the flat star always faces the camera
		const float Life = Glint.Age / Glint.Life;
		const float Shown = Life < 0.3f ? FMath::Sin(Life / 0.3f * UE_HALF_PI) : FMath::Square(1.0f - (Life - 0.3f) / 0.7f);
		const FVector Place = ModelTransform.TransformPosition(Glint.Place);
		const FVector ToCamera = (Camera - Place).GetSafeNormal();
		const FQuat Facing = FRotationMatrix::MakeFromX(ToCamera).ToQuat() * FQuat(FVector::ForwardVector, FMath::DegreesToRadians(Glint.Turn + 40.0f * Life));
		const FTransform Star(Facing, Place + ToCamera * 3.0f, FVector(Glint.Size * Shown / SparkleStarMeshSize));
		(Glint.bSecond ? Second : First)[Index] = Star;
	}

	Stars->BatchUpdateInstancesTransforms(0, First, true, true, true);
	if (SecondStars != nullptr)
	{
		SecondStars->BatchUpdateInstancesTransforms(0, Second, true, true, true);
	}

	// Out of the picture and every star spent: sleeps until the look is refreshed or it is seen again
	if (!bSeen && !bAnyAlive)
	{
		Credit = 0.0f;
	}
}
