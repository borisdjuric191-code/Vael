// Copyright Epic Games, Inc. All Rights Reserved.

#include "Story/VaelNpc.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Player/VaelCharacter.h"
#include "Player/VaelPlayerController.h"
#include "Story/VaelDialogue.h"
#include "Story/VaelQuestSubsystem.h"
#include "World/VaelProgressSubsystem.h"

namespace
{
	/** Size of the person in cm */
	constexpr float NpcRadius = 40.0f;
	constexpr float NpcHalfHeight = 90.0f;

	/** Color of the placeholder skin */
	const FLinearColor NpcSkinColor(0.62f, 0.48f, 0.38f);
}

AVaelNpc::AVaelNpc()
{
	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(NpcRadius, NpcHalfHeight);
	Capsule->SetCollisionProfileName(TEXT("Pawn"));
	RootComponent = Capsule;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	// A robe as a cone, a head as a sphere on top
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(RootComponent);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.5f));
	Body->SetRelativeLocation(FVector(0.0f, 0.0f, -15.0f));

	Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	Head->SetupAttachment(RootComponent);
	Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Head->SetRelativeScale3D(FVector(0.32f));
	Head->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));

	if (ConeMesh.Succeeded())
	{
		Body->SetStaticMesh(ConeMesh.Object);
	}
	if (SphereMesh.Succeeded())
	{
		Head->SetStaticMesh(SphereMesh.Object);
	}
	if (ShapeMaterial.Succeeded())
	{
		Body->SetMaterial(0, ShapeMaterial.Object);
		Head->SetMaterial(0, ShapeMaterial.Object);
	}
}

void AVaelNpc::BeginPlay()
{
	Super::BeginPlay();

	if (UMaterialInstanceDynamic* RobeMaterial = Body->CreateAndSetMaterialInstanceDynamic(0))
	{
		RobeMaterial->SetVectorParameterValue(TEXT("Color"), RobeColor);
	}
	if (UMaterialInstanceDynamic* SkinMaterial = Head->CreateAndSetMaterialInstanceDynamic(0))
	{
		SkinMaterial->SetVectorParameterValue(TEXT("Color"), NpcSkinColor);
	}

	UVaelInteractionSubsystem::Register(this);
}

void AVaelNpc::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UVaelInteractionSubsystem::Unregister(this);

	Super::EndPlay(EndPlayReason);
}

void AVaelNpc::Interact(AVaelCharacter* Player)
{
	AVaelPlayerController* PlayerController = Player != nullptr ? Player->GetController<AVaelPlayerController>() : nullptr;
	UVaelProgressSubsystem* Progress = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UVaelProgressSubsystem>() : nullptr;
	if (PlayerController == nullptr || Dialogue == nullptr || Progress == nullptr)
	{
		return;
	}

	// A quest waiting for this talk speaks first
	TArray<FText> QuestLines;
	UVaelQuestSubsystem* Quests = UVaelQuestSubsystem::Get(this);
	const bool bQuestTalk = Quests != nullptr && Quests->TakeTalk(GetSpeakerKey(), QuestLines);

	// The first talk is the intro, later ones give the hints that fit now
	TArray<FText> Lines;
	if (!Progress->HasHeardIntro(GetSpeakerKey()) && !Dialogue->IntroLines.IsEmpty())
	{
		Lines = Dialogue->IntroLines;
		Progress->MarkIntroHeard(GetSpeakerKey());
	}
	else if (!bQuestTalk || QuestLines.IsEmpty())
	{
		Lines = Dialogue->GatherHints(GetWorld());
	}

	Lines.Append(QuestLines);

	if (Lines.IsEmpty())
	{
		Lines.Add(NSLOCTEXT("VaelStory", "NothingToSay", "…"));
	}

	PlayerController->StartDialogue(GetDisplayName(), Lines);
}

FText AVaelNpc::GetInteractPrompt() const
{
	return NSLOCTEXT("VaelStory", "Talk", "Sprechen");
}

bool AVaelNpc::HasNews() const
{
	const UVaelProgressSubsystem* Progress = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UVaelProgressSubsystem>() : nullptr;
	const UVaelQuestSubsystem* Quests = UVaelQuestSubsystem::Get(this);

	return Dialogue != nullptr && ((!Dialogue->IntroLines.IsEmpty() && Progress != nullptr && !Progress->HasHeardIntro(GetSpeakerKey()))
		|| (Quests != nullptr && Quests->IsWaitingForTalk(GetSpeakerKey())));
}

FText AVaelNpc::GetDisplayName() const
{
	return Dialogue != nullptr ? Dialogue->SpeakerName : FText::GetEmpty();
}

FName AVaelNpc::GetSpeakerKey() const
{
	return Dialogue != nullptr ? Dialogue->GetFName() : GetFName();
}
