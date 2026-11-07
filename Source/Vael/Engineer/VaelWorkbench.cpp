// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engineer/VaelWorkbench.h"

#include "Components/PointLightComponent.h"
#include "Engineer/VaelEngineerComponent.h"
#include "Player/VaelCharacter.h"
#include "Player/VaelPlayerController.h"

#define LOCTEXT_NAMESPACE "VaelWorkbench"

AVaelWorkbench::AVaelWorkbench()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	LampGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("LampGlow"));
	LampGlow->SetupAttachment(RootComponent);
	LampGlow->SetRelativeLocation(FVector(0.0f, 0.0f, 200.0f));
	LampGlow->SetIntensityUnits(ELightUnits::Candelas);
	LampGlow->SetIntensity(4.0f);
	LampGlow->SetLightColor(FLinearColor(1.0f, 0.7f, 0.4f));
	LampGlow->SetAttenuationRadius(380.0f);
	LampGlow->SetCastShadows(false);
}

void AVaelWorkbench::BeginPlay()
{
	Super::BeginPlay();
	UVaelInteractionSubsystem::Register(this);
}

void AVaelWorkbench::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UVaelInteractionSubsystem::Unregister(this);
	Super::EndPlay(EndPlayReason);
}

bool AVaelWorkbench::CanInteract(const AVaelCharacter* Player) const
{
	return Player != nullptr && Player->IsEngineer() && Player->GetController<AVaelPlayerController>() != nullptr;
}

void AVaelWorkbench::Interact(AVaelCharacter* Player)
{
	if (AVaelPlayerController* Controller = Player != nullptr ? Player->GetController<AVaelPlayerController>() : nullptr)
	{
		Player->GetEngineer()->Restock();
		Controller->OpenCrafting(true);
	}
}

FText AVaelWorkbench::GetInteractPrompt() const
{
	return LOCTEXT("Build", "An der Werkbank bauen");
}

#undef LOCTEXT_NAMESPACE
