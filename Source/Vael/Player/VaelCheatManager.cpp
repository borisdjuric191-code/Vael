// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/VaelCheatManager.h"
#include "GameFramework/PlayerController.h"
#include "Magic/VaelElementComponent.h"
#include "Player/VaelCharacter.h"
#include "Vael.h"

void UVaelCheatManager::VaelCast(const FString& Elements, float AimYaw)
{
	APlayerController* PlayerController = GetOuterAPlayerController();
	const AVaelCharacter* VaelCharacter = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
	if (VaelCharacter == nullptr)
	{
		return;
	}

	UVaelElementComponent* ElementComponent = VaelCharacter->GetElementComponent();
	ElementComponent->ClearQueue();

	for (const TCHAR Letter : Elements.ToUpper())
	{
		switch (Letter)
		{
		case TEXT('F'): ElementComponent->AddElement(EVaelElement::Fire); break;
		case TEXT('W'): ElementComponent->AddElement(EVaelElement::Water); break;
		case TEXT('E'): ElementComponent->AddElement(EVaelElement::Earth); break;
		case TEXT('L'): ElementComponent->AddElement(EVaelElement::Air); break;
		case TEXT('M'): ElementComponent->AddElement(EVaelElement::Mark); break;
		default: break;
		}
	}

	PlayerController->SetControlRotation(FRotator(0.f, AimYaw, 0.f));

	const EVaelCastResult Result = ElementComponent->CastQueue();

	UE_LOG(LogVael, Log, TEXT("VaelCast %s: %s, mana now %.1f"), *Elements, *UEnum::GetValueAsString(Result), VaelCharacter->GetMana());
}
