// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nature/VaelHarvestable.h"
#include "UObject/ConstructorHelpers.h"
#include "Compendium/VaelCompendiumSettings.h"
#include "Compendium/VaelCompendiumSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Items/VaelMaterialBag.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Nature/VaelHarvestableData.h"
#include "Player/VaelCharacter.h"
#include "TimerManager.h"
#include "Vael.h"

#define LOCTEXT_NAMESPACE "VaelNature"

namespace
{
	/** Seconds between two looks for researchers nearby */
	constexpr float HarvestableResearchInterval = 0.5f;

	/** A gathered plant or fungus keeps this share of its stem */
	constexpr float HarvestedStemShare = 0.35f;

	/** A mined stone darkens to this share of its color */
	constexpr float MinedStoneShade = 0.55f;

	/** The engine's basic shapes are 100 cm across */
	constexpr float BasicShapeSize = 100.0f;
}

AVaelHarvestable::AVaelHarvestable()
{
	PrimaryActorTick.bCanEverTick = false;

	// The actor stands on the ground, the shapes grow up from that point
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Base = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Base"));
	Base->SetupAttachment(RootComponent);
	Base->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Base->SetCanEverAffectNavigation(false);

	Crown = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Crown"));
	Crown->SetupAttachment(RootComponent);
	Crown->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Crown->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	CylinderMesh = CylinderFinder.Object;
	SphereMesh = SphereFinder.Object;
	ConeMesh = ConeFinder.Object;

	if (ShapeMaterialFinder.Succeeded())
	{
		Base->SetMaterial(0, ShapeMaterialFinder.Object);
		Crown->SetMaterial(0, ShapeMaterialFinder.Object);
	}
}

void AVaelHarvestable::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	RefreshLook();
}

void AVaelHarvestable::BeginPlay()
{
	Super::BeginPlay();

	if (Data == nullptr)
	{
		UE_LOG(LogVael, Warning, TEXT("Harvestable '%s' has no data and can't be gathered"), *GetNameSafe(this));
		return;
	}

	RefreshLook();
	UVaelInteractionSubsystem::Register(this);

	// Spread the looks of many harvestables over the interval
	GetWorldTimerManager().SetTimer(ResearchTimer, this, &AVaelHarvestable::UpdateResearch, HarvestableResearchInterval, true, FMath::FRandRange(0.0f, HarvestableResearchInterval));
}

void AVaelHarvestable::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UVaelInteractionSubsystem::Unregister(this);
	GetWorldTimerManager().ClearAllTimersForObject(this);

	Super::EndPlay(EndPlayReason);
}

FName AVaelHarvestable::GetCompendiumId() const
{
	return Data != nullptr ? Data->CompendiumId : NAME_None;
}

FText AVaelHarvestable::GetInteractPrompt() const
{
	if (Data == nullptr)
	{
		return FText::GetEmpty();
	}

	switch (Data->Kind)
	{
	case EVaelHarvestKind::Stone:	return FText::Format(LOCTEXT("Mine", "{0} abbauen"), Data->DisplayName);
	case EVaelHarvestKind::Fungus:	return FText::Format(LOCTEXT("Gather", "{0} sammeln"), Data->DisplayName);
	default:						return FText::Format(LOCTEXT("Pick", "{0} pflücken"), Data->DisplayName);
	}
}

void AVaelHarvestable::Interact(AVaelCharacter* Player)
{
	if (!CanInteract(Player))
	{
		return;
	}

	bHarvested = true;
	RefreshLook();

	UVaelMaterialBag::GiveToGroup(this, Data->Loot);

	if (UVaelCompendiumSubsystem* Compendium = UVaelCompendiumSubsystem::Get(this))
	{
		Compendium->AddSample(Data->CompendiumId);
	}

	if (Data->RegrowSeconds > 0.0f)
	{
		GetWorldTimerManager().SetTimer(RegrowTimer, this, &AVaelHarvestable::Regrow, Data->RegrowSeconds, false);
	}

	UE_LOG(LogVael, Log, TEXT("'%s' (%s) gathered by player %d"), *GetNameSafe(this), *Data->CompendiumId.ToString(), Player != nullptr ? Player->GetPlayerNumber() : 0);
}

void AVaelHarvestable::Regrow()
{
	bHarvested = false;
	RefreshLook();
}

void AVaelHarvestable::UpdateResearch()
{
	UVaelCompendiumSubsystem* Compendium = UVaelCompendiumSubsystem::Get(this);
	const FName SubjectId = GetCompendiumId();
	if (Compendium == nullptr || Compendium->FindEntry(SubjectId) == nullptr)
	{
		GetWorldTimerManager().ClearTimer(ResearchTimer);
		return;
	}

	// Once the kind is observed, sighting and watching have nothing more to teach
	if (Compendium->GetProgress(SubjectId).Stage >= EVaelResearchStage::Observed)
	{
		GetWorldTimerManager().ClearTimer(ResearchTimer);
		return;
	}

	// Seen from afar; watched from close by, or from further through the spyglass, every watcher counting.
	// Plants and stones never flee, so unlike creatures they need no calm.
	const UVaelCompendiumSettings* Settings = UVaelCompendiumSettings::Get();
	const FVector Location = GetActorLocation();
	const float Reach = Settings->SpyglassFocusRadius + GetSimpleCollisionRadius();

	bool bSeen = false;
	float Watched = 0.0f;

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const AVaelCharacter* Player = It->Get() != nullptr ? It->Get()->GetPawn<AVaelCharacter>() : nullptr;
		if (Player == nullptr)
		{
			continue;
		}

		const float Distance = FVector::Dist2D(Player->GetActorLocation(), Location);
		bSeen |= Distance <= Settings->SightRange;

		if (Player->IsObserving() && FVector::Dist2D(Player->GetFocusLocation(), Location) <= Reach)
		{
			Watched += HarvestableResearchInterval * Settings->SpyglassObserveRate;
		}
		else if (Distance <= Settings->ObserveRange)
		{
			Watched += HarvestableResearchInterval;
		}
	}

	if (bSeen)
	{
		Compendium->Sight(SubjectId);
	}

	if (Watched > 0.0f)
	{
		Compendium->Observe(SubjectId, Watched);
	}
}

void AVaelHarvestable::RefreshLook()
{
	if (Data == nullptr)
	{
		return;
	}

	const float H = Data->Height;
	const bool bStone = Data->Kind == EVaelHarvestKind::Stone;

	// Stones block the way like any rock; plants and fungi can be walked through
	Base->SetCollisionProfileName(bStone ? TEXT("BlockAll") : TEXT("NoCollision"));
	Crown->SetRelativeRotation(FRotator::ZeroRotator);

	switch (Data->Kind)
	{
	case EVaelHarvestKind::Stone:
	{
		// A squat boulder with a crystal breaking out of its top
		Base->SetStaticMesh(SphereMesh);
		Base->SetRelativeScale3D(FVector(H * 1.4f, H * 1.15f, H) / BasicShapeSize);
		Base->SetRelativeLocation(FVector(0.0f, 0.0f, H * 0.4f));

		Crown->SetStaticMesh(ConeMesh);
		Crown->SetRelativeScale3D(FVector(H * 0.35f, H * 0.35f, H * 0.75f) / BasicShapeSize);
		Crown->SetRelativeLocation(FVector(H * 0.15f, 0.0f, H * 0.95f));
		Crown->SetRelativeRotation(FRotator(0.0f, 0.0f, 12.0f));
		break;
	}
	case EVaelHarvestKind::Fungus:
	{
		// A thick stalk under a flat, wide cap
		const float StalkHeight = H * 0.65f * (bHarvested ? HarvestedStemShare : 1.0f);

		Base->SetStaticMesh(CylinderMesh);
		Base->SetRelativeScale3D(FVector(H * 0.22f, H * 0.22f, StalkHeight) / BasicShapeSize);
		Base->SetRelativeLocation(FVector(0.0f, 0.0f, StalkHeight * 0.5f));

		Crown->SetStaticMesh(SphereMesh);
		Crown->SetRelativeScale3D(FVector(H * 0.85f, H * 0.85f, H * 0.35f) / BasicShapeSize);
		Crown->SetRelativeLocation(FVector(0.0f, 0.0f, H * 0.7f));
		break;
	}
	default:
	{
		// A thin stem under a round crown
		const float StemHeight = H * 0.8f * (bHarvested ? HarvestedStemShare : 1.0f);

		Base->SetStaticMesh(CylinderMesh);
		Base->SetRelativeScale3D(FVector(FMath::Max(H * 0.08f, 3.0f), FMath::Max(H * 0.08f, 3.0f), StemHeight) / BasicShapeSize);
		Base->SetRelativeLocation(FVector(0.0f, 0.0f, StemHeight * 0.5f));

		Crown->SetStaticMesh(SphereMesh);
		Crown->SetRelativeScale3D(FVector(H * 0.4f) / BasicShapeSize);
		Crown->SetRelativeLocation(FVector(0.0f, 0.0f, H * 0.8f));
		break;
	}
	}

	// What is taken is gone until it grows back; a mined stone stays, but dull
	Crown->SetVisibility(!bHarvested);

	if (UMaterialInstanceDynamic* BaseMaterial = Base->CreateAndSetMaterialInstanceDynamic(0))
	{
		BaseMaterial->SetVectorParameterValue(TEXT("Color"), bStone && bHarvested ? Data->BaseColor * MinedStoneShade : Data->BaseColor);
	}

	if (UMaterialInstanceDynamic* CrownMaterial = Crown->CreateAndSetMaterialInstanceDynamic(0))
	{
		CrownMaterial->SetVectorParameterValue(TEXT("Color"), Data->GlowColor);
	}
}

#undef LOCTEXT_NAMESPACE
