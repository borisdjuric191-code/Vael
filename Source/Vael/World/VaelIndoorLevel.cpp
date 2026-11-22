// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelIndoorLevel.h"

#include "Components/BillboardComponent.h"
#include "Creatures/VaelTropfseuche.h"
#include "Creatures/VaelKobold.h"
#include "EngineUtils.h"
#include "World/VaelRegion.h"

AVaelIndoorLevel::AVaelIndoorLevel()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

#if WITH_EDITORONLY_DATA
	// Something to click in the editor; not visible in the game
	if (UBillboardComponent* Sprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite")))
	{
		Sprite->SetupAttachment(RootComponent);
		Sprite->bIsScreenSizeScaled = true;
	}
#endif
}

bool AVaelIndoorLevel::IsIndoor(const UWorld* World)
{
	return World != nullptr && TActorIterator<AVaelIndoorLevel>(const_cast<UWorld*>(World));
}

const AVaelIndoorLevel* AVaelIndoorLevel::Find(const UWorld* World)
{
	if (World == nullptr)
	{
		return nullptr;
	}
	TActorIterator<AVaelIndoorLevel> It(const_cast<UWorld*>(World));
	return It ? *It : nullptr;
}

FString AVaelIndoorLevel::PlaceKeyAt(const UWorld* World, const FVector& Location)
{
	if (World == nullptr)
	{
		return FString();
	}
	for (TActorIterator<AVaelRegion> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (It->Tags.Num() > 0 && It->Tags[0].ToString().StartsWith(TEXT("L_")) && It->Contains(Location))
		{
			return It->Tags[0].ToString();
		}
	}
	return UWorld::RemovePIEPrefix(World->GetMapName());
}

void AVaelIndoorLevel::BeginPlay()
{
	Super::BeginPlay();
	UVaelTropfseuche::SpawnHosts(GetWorld());
	UVaelKobolde::SpawnInCaves(GetWorld());
}
