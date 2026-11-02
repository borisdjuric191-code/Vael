// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/VaelMaterialDrop.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Items/VaelItemSettings.h"
#include "Items/VaelMaterial.h"
#include "Items/VaelMaterialBag.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Player/VaelCharacter.h"
#include "UI/VaelCombatTextSubsystem.h"
#include "World/VaelGround.h"

#define LOCTEXT_NAMESPACE "VaelItems"

namespace
{
	/** Models of the loot kit are small (a crystal 5-13 cm, a bottle 14 cm); on the ground they are drawn larger to be seen from the iso camera */
	constexpr float DropModelScale = 2.2f;

	/** Turns per second, and how far it rises and sinks while turning (cm) */
	constexpr float DropTurnSpeed = 0.12f;
	constexpr float DropBobHeight = 2.5f;
	constexpr float DropBobSpeed = 0.7f;

	/** Glow: candela at full breath, radius, breaths per second and how deep it fades */
	constexpr float DropGlowIntensity = 6.0f;
	constexpr float DropGlowRadius = 180.0f;
	constexpr float DropGlowSpeed = 0.55f;
	constexpr float DropGlowDepth = 0.45f;
}

AVaelMaterialDrop::AVaelMaterialDrop()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Spinner = CreateDefaultSubobject<USceneComponent>(TEXT("Spinner"));
	Spinner->SetupAttachment(RootComponent);

	const auto MakeModel = [this](const TCHAR* Name)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(Spinner);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->bReceivesDecals = false;
		Part->SetRelativeScale3D(FVector(DropModelScale));
		return Part;
	};

	Model = MakeModel(TEXT("Model"));
	InnerModel = MakeModel(TEXT("InnerModel"));

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(RootComponent);
	Glow->SetRelativeLocation(FVector(0.0f, 0.0f, 25.0f));
	Glow->SetAttenuationRadius(DropGlowRadius);
	Glow->SetCastShadows(false);
	Glow->SetIntensityUnits(ELightUnits::Candelas);
	Glow->SetIntensity(0.0f);

	PrimaryActorTick.bCanEverTick = true;
}

AVaelMaterialDrop* AVaelMaterialDrop::DropMaterial(UWorld* World, const FVector& Location, const UVaelMaterial* InMaterial, int32 InCount, AVaelCharacter* InOwner)
{
	if (World == nullptr || InMaterial == nullptr || InCount <= 0 || InOwner == nullptr)
	{
		return nullptr;
	}

	FVector Ground = Location;
	FHitResult GroundHit;
	if (VaelGround::TraceGround(World, Location + FVector(0.0f, 0.0f, 80.0f), Location - FVector(0.0f, 0.0f, 500.0f), GroundHit, nullptr))
	{
		Ground = GroundHit.Location;
	}

	const FTransform SpawnTransform(FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), Ground + FVector(0.0f, 0.0f, 1.0f));
	AVaelMaterialDrop* Drop = World->SpawnActorDeferred<AVaelMaterialDrop>(AVaelMaterialDrop::StaticClass(), SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Drop != nullptr)
	{
		Drop->Material = InMaterial;
		Drop->Count = InCount;
		Drop->OwningPlayer = InOwner;
		Drop->FinishSpawning(SpawnTransform);
	}

	return Drop;
}

void AVaelMaterialDrop::BeginPlay()
{
	Super::BeginPlay();

	Phase = FMath::FRandRange(0.0f, 10.0f);
	if (Material == nullptr)
	{
		return;
	}

	const auto Dress = [](UStaticMeshComponent* Part, const TSoftObjectPtr<UStaticMesh>& MeshAsset, const TSoftObjectPtr<UMaterialInterface>& MaterialAsset)
	{
		UStaticMesh* Mesh = MeshAsset.IsNull() ? nullptr : MeshAsset.LoadSynchronous();
		Part->SetStaticMesh(Mesh);
		Part->SetVisibility(Mesh != nullptr);
		if (Mesh == nullptr)
		{
			return;
		}

		if (UMaterialInterface* Look = MaterialAsset.IsNull() ? nullptr : MaterialAsset.LoadSynchronous())
		{
			Part->SetMaterial(0, Look);
		}
		// Every slot of the loot material knows it lies on the ground and adds its glint
		for (int32 Index = 0; Index < Part->GetNumMaterials(); ++Index)
		{
			if (UMaterialInstanceDynamic* Dynamic = Part->CreateAndSetMaterialInstanceDynamic(Index))
			{
				Dynamic->SetScalarParameterValue(TEXT("AmBoden"), 1.0f);
			}
		}
	};

	Dress(Model, Material->Mesh, Material->MeshMaterial);
	Dress(InnerModel, Material->InnerMesh, Material->InnerMaterial);

	GlowIntensity = DropGlowIntensity * (1.0f + 0.35f * static_cast<int32>(Material->Rarity));
	Glow->SetLightColor(Material->Color);
}

void AVaelMaterialDrop::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;
	const float Time = Age + Phase;
	Spinner->SetRelativeRotation(FRotator(0.0f, Time * DropTurnSpeed * 360.0f, 0.0f));
	Spinner->SetRelativeLocation(FVector(0.0f, 0.0f, DropBobHeight * (0.5f + 0.5f * FMath::Sin(Time * DropBobSpeed * UE_TWO_PI))));
	Glow->SetIntensity(GlowIntensity * (1.0f - DropGlowDepth * (0.5f + 0.5f * FMath::Sin(Time * DropGlowSpeed * UE_TWO_PI))));

	AVaelCharacter* DropOwner = OwningPlayer.Get();
	if (DropOwner == nullptr || Material == nullptr)
	{
		// The owner left the game, nobody else may take it
		Destroy();
		return;
	}

	if (DropOwner->IsDowned() || FVector::Dist2D(DropOwner->GetActorLocation(), GetActorLocation()) > UVaelItemSettings::Get()->ItemPickupRadius)
	{
		return;
	}

	// The bag never runs full
	DropOwner->GetMaterialBag()->AddMaterial(Material, Count);
	const FText Label = Count > 1 ? FText::Format(LOCTEXT("DropCount", "{0} ×{1}"), Material->DisplayName, Count) : Material->DisplayName;
	UVaelCombatTextSubsystem::PostPickup(DropOwner, Label, Material->Color);
	Destroy();
}

#undef LOCTEXT_NAMESPACE
