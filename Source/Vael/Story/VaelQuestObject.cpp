// Copyright Epic Games, Inc. All Rights Reserved.

#include "Story/VaelQuestObject.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Player/VaelCharacter.h"
#include "Player/VaelPlayerController.h"
#include "Story/VaelQuestSubsystem.h"

AVaelQuestObject::AVaelQuestObject()
{
	// A small box that blocks the way like a piece of furniture
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Mesh->SetRelativeScale3D(FVector(0.7f, 0.45f, 0.4f));
	RootComponent = Mesh;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}
	if (ShapeMaterial.Succeeded())
	{
		Mesh->SetMaterial(0, ShapeMaterial.Object);
	}

	Prompt = NSLOCTEXT("VaelStory", "QuestObjectPrompt", "Durchsuchen");
}

void AVaelQuestObject::BeginPlay()
{
	Super::BeginPlay();

	if (UMaterialInstanceDynamic* Material = Mesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), Color);
	}

	UVaelInteractionSubsystem::Register(this);
}

void AVaelQuestObject::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UVaelInteractionSubsystem::Unregister(this);

	Super::EndPlay(EndPlayReason);
}

bool AVaelQuestObject::CanInteract(const AVaelCharacter* Player) const
{
	const UVaelQuestSubsystem* Quests = UVaelQuestSubsystem::Get(this);
	return Quests != nullptr && Quests->IsWaitingForEvent(EventName);
}

void AVaelQuestObject::Interact(AVaelCharacter* Player)
{
	AVaelPlayerController* PlayerController = Player != nullptr ? Player->GetController<AVaelPlayerController>() : nullptr;
	UVaelQuestSubsystem* Quests = UVaelQuestSubsystem::Get(this);
	if (PlayerController == nullptr || Quests == nullptr)
	{
		return;
	}

	// What they find is read like a talk; the notice of the quest shows once the game runs again
	PlayerController->StartDialogue(Title, Lines);
	Quests->NotifyEvent(EventName);
}
