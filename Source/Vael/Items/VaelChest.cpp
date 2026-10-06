// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/VaelChest.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Items/VaelItemData.h"
#include "Items/VaelItemDrop.h"
#include "Items/VaelItemSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Player/VaelCharacter.h"
#include "Vael.h"

namespace
{
	/** Size of the placeholder box in cm */
	const FVector ChestSize(90.0f, 60.0f, 55.0f);

	/** Items fall in a ring around the chest at this distance */
	constexpr float ChestDropDistance = 110.0f;

	const FLinearColor ClosedChestColor(0.28f, 0.16f, 0.07f);
	const FLinearColor OpenChestColor(0.08f, 0.05f, 0.03f);
}

AVaelChest::AVaelChest()
{
	// The actor stands on the ground, the box sits on top of that point
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Box = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Box"));
	Box->SetupAttachment(RootComponent);
	Box->SetCollisionProfileName(TEXT("BlockAll"));
	Box->SetRelativeScale3D(ChestSize / 100.0f);
	Box->SetRelativeLocation(FVector(0.0f, 0.0f, ChestSize.Z * 0.5f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Box->SetStaticMesh(CubeMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CubeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (CubeMaterial.Succeeded())
	{
		Box->SetMaterial(0, CubeMaterial.Object);
	}
}

void AVaelChest::BeginPlay()
{
	Super::BeginPlay();

	RefreshLook();
	UVaelInteractionSubsystem::Register(this);
}

void AVaelChest::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UVaelInteractionSubsystem::Unregister(this);

	Super::EndPlay(EndPlayReason);
}

void AVaelChest::Interact(AVaelCharacter* Player)
{
	if (bOpened)
	{
		return;
	}

	bOpened = true;
	RefreshLook();

	// Loot is personal: every player gets their own items, spread around the chest
	TArray<AVaelCharacter*> Players;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (AVaelCharacter* Each = It->Get() != nullptr ? It->Get()->GetPawn<AVaelCharacter>() : nullptr)
		{
			Players.Add(Each);
		}
	}

	int32 DropIndex = 0;
	const int32 NumDrops = Players.Num() * (FixedItems.Num() + RolledItems);

	const auto Drop = [&](const FVaelItem& Item, AVaelCharacter* ItemOwner)
	{
		const float Angle = 2.0f * PI * DropIndex++ / FMath::Max(NumDrops, 1);
		const FVector Offset(FMath::Cos(Angle) * ChestDropDistance, FMath::Sin(Angle) * ChestDropDistance, 0.0f);
		AVaelItemDrop::DropItem(GetWorld(), GetActorLocation() + Offset, Item, ItemOwner);
	};

	for (AVaelCharacter* ItemOwner : Players)
	{
		for (const UVaelItemData* ItemData : FixedItems)
		{
			if (ItemData != nullptr)
			{
				Drop(ItemData->MakeItem(), ItemOwner);
			}
		}

		for (int32 RollIndex = 0; RollIndex < RolledItems; ++RollIndex)
		{
			Drop(UVaelItemSettings::Get()->RollItem(MinRarity), ItemOwner);
		}
	}

	UE_LOG(LogVael, Log, TEXT("Chest '%s' opened by player %d: %d items"), *GetNameSafe(this), Player != nullptr ? Player->GetPlayerNumber() : 0, NumDrops);
}

FText AVaelChest::GetInteractPrompt() const
{
	return NSLOCTEXT("VaelItems", "OpenChest", "Truhe öffnen");
}

void AVaelChest::RefreshLook()
{
	if (UMaterialInstanceDynamic* Material = Box->CreateAndSetMaterialInstanceDynamic(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), bOpened ? OpenChestColor : ClosedChestColor);
	}
}
