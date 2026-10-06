// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelFormulaScroll.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Magic/VaelFormula.h"
#include "Magic/VaelGrimoireSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Player/VaelCharacter.h"
#include "UI/VaelNoticeSubsystem.h"
#include "Vael.h"

#define LOCTEXT_NAMESPACE "VaelMagic"

namespace
{
	/** Height of the scroll above the ground */
	constexpr float ScrollHoverHeight = 45.0f;

	/** How far the scroll bobs up and down, in cm, and how fast */
	constexpr float ScrollBobAmplitude = 6.0f;
	constexpr float ScrollBobSpeed = 2.5f;

	/** Turning speed in degrees per second */
	constexpr float ScrollTurnSpeed = 40.0f;
}

AVaelFormulaScroll::AVaelFormulaScroll()
{
	PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
	PickupSphere->InitSphereRadius(PickupRadius);
	PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupSphere->SetCollisionObjectType(ECC_WorldDynamic);
	PickupSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	PickupSphere->SetGenerateOverlapEvents(true);
	RootComponent = PickupSphere;

	// Placeholder look: a cylinder lying on its side, the color of old paper
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, ScrollHoverHeight), FRotator(90.0f, 0.0f, 0.0f));
	Mesh->SetRelativeScale3D(FVector(0.18f, 0.18f, 0.5f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CylinderMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMaterial.Succeeded())
	{
		Mesh->SetMaterial(0, ShapeMaterial.Object);
	}

	PrimaryActorTick.bCanEverTick = true;
}

AVaelFormulaScroll* AVaelFormulaScroll::SpawnScroll(UWorld* World, const FVector& GroundLocation, TConstArrayView<EVaelElement> InElements, const FText& InLabel)
{
	if (World == nullptr)
	{
		return nullptr;
	}

	const FTransform SpawnTransform(GroundLocation);

	AVaelFormulaScroll* Scroll = World->SpawnActorDeferred<AVaelFormulaScroll>(AVaelFormulaScroll::StaticClass(), SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Scroll != nullptr)
	{
		Scroll->Elements = TArray<EVaelElement>(InElements);
		Scroll->Label = InLabel;
		Scroll->FinishSpawning(SpawnTransform);
	}

	return Scroll;
}

void AVaelFormulaScroll::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	PickupSphere->SetSphereRadius(PickupRadius);

	if (UMaterialInstanceDynamic* Material = Mesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), Color);
	}
}

void AVaelFormulaScroll::BeginPlay()
{
	Super::BeginPlay();

	BaseMeshHeight = Mesh->GetRelativeLocation().Z;

	PickupSphere->OnComponentBeginOverlap.AddDynamic(this, &AVaelFormulaScroll::OnPickupOverlap);

	// A player may already stand on the scroll when it appears
	TArray<AActor*> OverlappingPlayers;
	PickupSphere->GetOverlappingActors(OverlappingPlayers, AVaelCharacter::StaticClass());
	if (!OverlappingPlayers.IsEmpty())
	{
		PickUp(OverlappingPlayers[0]);
	}
}

void AVaelFormulaScroll::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Time = GetWorld()->GetTimeSeconds();
	FVector MeshLocation = Mesh->GetRelativeLocation();
	MeshLocation.Z = BaseMeshHeight + FMath::Sin(Time * ScrollBobSpeed) * ScrollBobAmplitude;

	Mesh->SetRelativeLocationAndRotation(MeshLocation, FRotator(90.0f, Time * ScrollTurnSpeed, 0.0f));
}

void AVaelFormulaScroll::OnPickupOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	const AVaelCharacter* Player = Cast<AVaelCharacter>(OtherActor);
	if (Player != nullptr && !Player->IsDowned())
	{
		PickUp(OtherActor);
	}
}

void AVaelFormulaScroll::PickUp(AActor* Player)
{
	if (IsActorBeingDestroyed())
	{
		return;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	UVaelGrimoireSubsystem* Grimoire = GameInstance != nullptr ? GameInstance->GetSubsystem<UVaelGrimoireSubsystem>() : nullptr;
	UVaelFormula* Formula = Grimoire != nullptr ? Grimoire->FindFormula(Elements) : nullptr;

	if (Formula == nullptr)
	{
		UE_LOG(LogVael, Warning, TEXT("Scroll '%s' holds a formula of %d elements that isn't in the game"), *GetNameSafe(this), Elements.Num());
	}
	else if (!Grimoire->LearnFormula(Formula, FText::Format(LOCTEXT("FormulaFromScroll", "{0} \u2013 ein Fragment des alten Wissens."), Label)))
	{
		// The group knew it already; the scroll is used up all the same
		UVaelNoticeSubsystem::Post(this, Label, FText::Format(LOCTEXT("ScrollAlreadyKnown", "{0} ist euch schon bekannt."), Formula->DisplayName), FLinearColor(0.62f, 0.56f, 0.49f));
	}

	UE_LOG(LogVael, Log, TEXT("'%s' picks up scroll '%s' (%s)"), *GetNameSafe(Player), *Label.ToString(), Formula != nullptr ? *Formula->DisplayName.ToString() : TEXT("no formula"));

	Destroy();
}

#undef LOCTEXT_NAMESPACE
