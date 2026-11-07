// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelBrewTable.h"

#include "Components/PointLightComponent.h"
#include "Player/VaelCharacter.h"
#include "Player/VaelPlayerController.h"

#define LOCTEXT_NAMESPACE "VaelBrewTable"

AVaelBrewTable::AVaelBrewTable()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	KettleGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("KettleGlow"));
	KettleGlow->SetupAttachment(RootComponent);
	KettleGlow->SetRelativeLocation(FVector(0.0f, 0.0f, 230.0f));
	KettleGlow->SetIntensityUnits(ELightUnits::Candelas);
	KettleGlow->SetIntensity(5.0f);
	KettleGlow->SetLightColor(FLinearColor(1.0f, 0.62f, 0.3f));
	KettleGlow->SetAttenuationRadius(420.0f);
	KettleGlow->SetCastShadows(false);
}

void AVaelBrewTable::BeginPlay()
{
	Super::BeginPlay();
	UVaelInteractionSubsystem::Register(this);
}

void AVaelBrewTable::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UVaelInteractionSubsystem::Unregister(this);
	Super::EndPlay(EndPlayReason);
}

bool AVaelBrewTable::CanInteract(const AVaelCharacter* Player) const
{
	return Player != nullptr && Player->GetController<AVaelPlayerController>() != nullptr;
}

void AVaelBrewTable::Interact(AVaelCharacter* Player)
{
	if (AVaelPlayerController* Controller = Player != nullptr ? Player->GetController<AVaelPlayerController>() : nullptr)
	{
		Controller->OpenBrewing();
	}
}

FText AVaelBrewTable::GetInteractPrompt() const
{
	return LOCTEXT("Brew", "Tränke brauen");
}

#undef LOCTEXT_NAMESPACE
