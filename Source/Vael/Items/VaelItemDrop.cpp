// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/VaelItemDrop.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Items/VaelInventory.h"
#include "Items/VaelItemSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Player/VaelCharacter.h"
#include "Player/VaelPlayerController.h"
#include "UI/VaelCombatTextSubsystem.h"
#include "UI/VaelNoticeSubsystem.h"
#include "VaelGameMode.h"
#include "World/VaelGround.h"

namespace
{
	/** Size of the engine cylinder used as placeholder */
	constexpr float DropCylinderRadius = 50.0f;
	constexpr float DropCylinderHeight = 100.0f;

	/** Pillar: thin, taller for rarer items */
	constexpr float DropPillarRadius = 7.0f;
	constexpr float DropPillarBaseHeight = 60.0f;
	constexpr float DropPillarHeightPerRarity = 30.0f;

	/** Disc in the color of the owner */
	constexpr float DropDiscRadius = 32.0f;
}

AVaelItemDrop::AVaelItemDrop()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	const auto MakePart = [&](const TCHAR* Name)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(RootComponent);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCastShadow(false);
		Part->bReceivesDecals = false;

		if (CylinderMesh.Succeeded())
		{
			Part->SetStaticMesh(CylinderMesh.Object);
		}
		if (ShapeMaterial.Succeeded())
		{
			Part->SetMaterial(0, ShapeMaterial.Object);
		}

		return Part;
	};

	Pillar = MakePart(TEXT("Pillar"));
	OwnerDisc = MakePart(TEXT("OwnerDisc"));
	OwnerDisc->SetRelativeScale3D(FVector(DropDiscRadius / DropCylinderRadius, DropDiscRadius / DropCylinderRadius, 0.02f));

	PrimaryActorTick.bCanEverTick = true;
}

AVaelItemDrop* AVaelItemDrop::DropItem(UWorld* World, const FVector& Location, const FVaelItem& InItem, AVaelCharacter* InOwner)
{
	if (World == nullptr || !InItem.IsValid() || InOwner == nullptr)
	{
		return nullptr;
	}

	// The item lies on the ground below the spot
	FVector Ground = Location;
	FHitResult GroundHit;
	if (VaelGround::TraceGround(World, Location + FVector(0.0f, 0.0f, 80.0f), Location - FVector(0.0f, 0.0f, 500.0f), GroundHit, nullptr))
	{
		Ground = GroundHit.Location;
	}

	const FTransform SpawnTransform(Ground + FVector(0.0f, 0.0f, 2.0f));

	AVaelItemDrop* Drop = World->SpawnActorDeferred<AVaelItemDrop>(AVaelItemDrop::StaticClass(), SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Drop != nullptr)
	{
		Drop->Item = InItem;
		Drop->OwningPlayer = InOwner;
		Drop->FinishSpawning(SpawnTransform);
	}

	return Drop;
}

void AVaelItemDrop::BeginPlay()
{
	Super::BeginPlay();

	const float PillarHeight = DropPillarBaseHeight + static_cast<int32>(Item.Rarity) * DropPillarHeightPerRarity;
	Pillar->SetRelativeScale3D(FVector(DropPillarRadius / DropCylinderRadius, DropPillarRadius / DropCylinderRadius, PillarHeight / DropCylinderHeight));
	Pillar->SetRelativeLocation(FVector(0.0f, 0.0f, PillarHeight * 0.5f));

	if (UMaterialInstanceDynamic* PillarMaterial = Pillar->CreateAndSetMaterialInstanceDynamic(0))
	{
		PillarMaterial->SetVectorParameterValue(TEXT("Color"), VaelItems::GetRarityColor(Item.Rarity));
	}

	// The disc shows whose item it is
	const AVaelCharacter* ItemOwner = OwningPlayer.Get();
	const AVaelPlayerController* OwnerController = ItemOwner != nullptr ? ItemOwner->GetController<AVaelPlayerController>() : nullptr;
	const AVaelGameMode* GameMode = GetWorld()->GetAuthGameMode<AVaelGameMode>();

	if (UMaterialInstanceDynamic* DiscMaterial = OwnerDisc->CreateAndSetMaterialInstanceDynamic(0))
	{
		DiscMaterial->SetVectorParameterValue(TEXT("Color"), GameMode != nullptr && OwnerController != nullptr ? GameMode->GetPlayerColor(OwnerController->GetPlayerSlot()) : FLinearColor::White);
	}
}

void AVaelItemDrop::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	AVaelCharacter* ItemOwner = OwningPlayer.Get();
	if (ItemOwner == nullptr)
	{
		// The owner left the game, nobody else may take it
		Destroy();
		return;
	}

	if (ItemOwner->IsDowned() || FVector::Dist2D(ItemOwner->GetActorLocation(), GetActorLocation()) > UVaelItemSettings::Get()->ItemPickupRadius)
	{
		bToldBackpackFull = false;
		return;
	}

	if (ItemOwner->GetInventory()->AddItem(Item))
	{
		UVaelCombatTextSubsystem::PostPickup(ItemOwner, Item.DisplayName, VaelItems::GetRarityColor(Item.Rarity));
		Destroy();
	}
	else if (!bToldBackpackFull)
	{
		bToldBackpackFull = true;
		UVaelNoticeSubsystem::Post(this, NSLOCTEXT("VaelItems", "BackpackFull", "Rucksack voll"),
			NSLOCTEXT("VaelItems", "BackpackFullDetail", "Mach Platz, dann liegt der Gegenstand noch hier."), FLinearColor(FColor(158, 143, 125)), 2.5f);
	}
}
