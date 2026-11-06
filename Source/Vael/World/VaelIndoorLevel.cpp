// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelIndoorLevel.h"

#include "Components/BillboardComponent.h"
#include "EngineUtils.h"

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
