// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/VaelCheatManager.h"
#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelCombatStatics.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Magic/VaelElementComponent.h"
#include "Magic/VaelFormula.h"
#include "Magic/VaelGrimoireSubsystem.h"
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

void UVaelCheatManager::VaelLearnAll()
{
	const UGameInstance* GameInstance = GetWorld() != nullptr ? GetWorld()->GetGameInstance() : nullptr;
	UVaelGrimoireSubsystem* Grimoire = GameInstance != nullptr ? GameInstance->GetSubsystem<UVaelGrimoireSubsystem>() : nullptr;
	if (Grimoire == nullptr)
	{
		return;
	}

	int32 NumLearned = 0;
	for (UVaelFormula* Formula : Grimoire->GetAllFormulas())
	{
		NumLearned += Grimoire->LearnFormula(Formula) ? 1 : 0;
	}

	UE_LOG(LogVael, Log, TEXT("VaelLearnAll: %d formulas learned, %d known now"), NumLearned, Grimoire->GetKnownFormulas().Num());
}

void UVaelCheatManager::VaelStatus(const FString& Status, float Duration)
{
	APlayerController* PlayerController = GetOuterAPlayerController();
	APawn* PlayerPawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;

	const int64 StatusValue = StaticEnum<EVaelStatus>()->GetValueByNameString(Status);
	if (PlayerPawn == nullptr || StatusValue == INDEX_NONE)
	{
		return;
	}

	// Burning needs a strength; the value of the spark is a good test value
	const float TestBurnDamagePerSecond = 6.0f;

	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		if (UVaelCombatStatics::CanDamage(PlayerPawn, *It))
		{
			UVaelCombatStatics::ApplyStatus(PlayerPawn, *It, static_cast<EVaelStatus>(StatusValue), Duration, TestBurnDamagePerSecond);

			UE_LOG(LogVael, Log, TEXT("VaelStatus: '%s' is now: %s"), *GetNameSafe(*It), *It->GetStatusText().ToString());
		}
	}
}
