// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/VaelStorageChest.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engineer/VaelEngineerComponent.h"
#include "Engineer/VaelEngineerGear.h"
#include "Items/VaelMaterial.h"
#include "Items/VaelMaterialBag.h"
#include "Player/VaelCharacter.h"
#include "Player/VaelPlayerController.h"
#include "Story/VaelQuestSubsystem.h"
#include "UI/VaelNoticeSubsystem.h"
#include "Vael.h"

#define LOCTEXT_NAMESPACE "VaelStorageChest"

namespace
{
	const FName StarterFlag(TEXT("Ingenieur.Grundstock"));

	/** What the survivors put aside for the engineer: hulls, cores and boosters for his first formulas, metal and glass to refine */
	struct FStarter { const TCHAR* Material; int32 Count; };
	const FStarter Starter[] =
	{
		{ TEXT("Schlacke"), 10 }, { TEXT("Glutstein"), 8 }, { TEXT("Knistersamen"), 4 }, { TEXT("Distelflaum"), 3 }, { TEXT("Glutsamen"), 3 },
		{ TEXT("Glutdruese"), 2 }, { TEXT("Blatttau"), 4 }, { TEXT("Pelzwasser"), 2 }, { TEXT("Latex"), 3 }, { TEXT("Blasenhaut"), 3 },
		{ TEXT("Rauchblase"), 3 }, { TEXT("Schlackenglas"), 3 }, { TEXT("Hohlkiel"), 2 }, { TEXT("Kelchschleim"), 2 }, { TEXT("Sichelklinge"), 2 },
		{ TEXT("Rollpanzer"), 2 }, { TEXT("Russquarz"), 3 }, { TEXT("Staeublingspulver"), 2 }, { TEXT("Silberhalm"), 2 }, { TEXT("Sternmilch"), 2 },
	};
}

AVaelStorageChest::AVaelStorageChest()
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Glint = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glint"));
	Glint->SetupAttachment(RootComponent);
	Glint->SetRelativeLocation(FVector(0.0f, 0.0f, 110.0f));
	Glint->SetIntensityUnits(ELightUnits::Candelas);
	Glint->SetIntensity(3.0f);
	Glint->SetLightColor(FLinearColor(1.0f, 0.72f, 0.42f));
	Glint->SetAttenuationRadius(240.0f);
	Glint->SetCastShadows(false);
}

void AVaelStorageChest::BeginPlay()
{
	Super::BeginPlay();
	if (Mesh->GetStaticMesh() == nullptr)
	{
		Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Megascans/3D_Assets/OldWoodenChest/SM_OldWoodenChest.SM_OldWoodenChest"), nullptr, LOAD_NoWarn | LOAD_Quiet));
	}
	UVaelInteractionSubsystem::Register(this);
}

void AVaelStorageChest::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UVaelInteractionSubsystem::Unregister(this);
	Super::EndPlay(EndPlayReason);
}

bool AVaelStorageChest::CanInteract(const AVaelCharacter* Player) const
{
	return Player != nullptr && Player->GetController<AVaelPlayerController>() != nullptr;
}

int32 AVaelStorageChest::GiveStarterStock(AVaelCharacter* Player) const
{
	UVaelQuestSubsystem* Quests = UVaelQuestSubsystem::Get(this);
	if (!bStarterStock || Quests == nullptr || Quests->HasFlag(StarterFlag) || Player->GetMaterialBag() == nullptr)
	{
		return 0;
	}
	Quests->SetFlag(StarterFlag);
	int32 Kinds = 0;
	for (const FStarter& Each : Starter)
	{
		if (const UVaelMaterial* Material = VaelEngineerGear::LoadMaterial(Each.Material))
		{
			Player->GetMaterialBag()->AddMaterial(Material, Each.Count);
			++Kinds;
		}
		else
		{
			UE_LOG(LogVael, Warning, TEXT("Storage chest: the starting material %s is missing"), Each.Material);
		}
	}
	return Kinds;
}

void AVaelStorageChest::Interact(AVaelCharacter* Player)
{
	AVaelPlayerController* Controller = Player != nullptr ? Player->GetController<AVaelPlayerController>() : nullptr;
	if (Controller == nullptr)
	{
		return;
	}
	if (Player->IsEngineer())
	{
		if (const int32 Kinds = GiveStarterStock(Player); Kinds > 0)
		{
			UVaelNoticeSubsystem::Post(this, LOCTEXT("StarterTitle", "Reste aus dem Dorf"),
				FText::Format(LOCTEXT("StarterDetail", "Edda hat dir {0} Sorten Material in die Kiste gelegt: genug für die ersten Ladungen. Misch sie an Werkbank oder Esse."), Kinds),
				FLinearColor(FColor(206, 140, 70)), 6.0f);
		}
		Player->GetEngineer()->Restock();
	}
	Controller->OpenGrimoire(EVaelMenuPage::Inventory);
}

FText AVaelStorageChest::GetInteractPrompt() const
{
	return LOCTEXT("Open", "Vorratskiste öffnen");
}

#undef LOCTEXT_NAMESPACE
