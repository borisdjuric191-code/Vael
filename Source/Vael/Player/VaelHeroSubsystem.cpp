// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/VaelHeroSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "VaelHero"

UVaelHeroSubsystem* UVaelHeroSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	return GameInstance != nullptr ? GameInstance->GetSubsystem<UVaelHeroSubsystem>() : nullptr;
}

FVaelHeroChoice UVaelHeroSubsystem::GetChoice(int32 PlayerSlot) const
{
	if (const FVaelHeroChoice* Choice = Choices.Find(PlayerSlot))
	{
		return *Choice;
	}

	// Mage, Floromantin, mage woman, Floromant: the second player tries out the new class right away
	const int32 Slot = FMath::Max(PlayerSlot, 0) % 4;
	FVaelHeroChoice Default;
	Default.Class = Slot % 2 == 1 ? EVaelHeroClass::Floromant : EVaelHeroClass::Mage;
	Default.bFemale = Slot == 1 || Slot == 2;
	return Default;
}

FVaelHeroChoice UVaelHeroSubsystem::GetListedChoice(int32 Index)
{
	FVaelHeroChoice Choice;
	Choice.Class = Index >= 2 ? EVaelHeroClass::Floromant : EVaelHeroClass::Mage;
	Choice.bFemale = Index % 2 == 1;
	return Choice;
}

FText UVaelHeroSubsystem::GetChoiceName(const FVaelHeroChoice& Choice)
{
	if (Choice.Class == EVaelHeroClass::Floromant)
	{
		return Choice.bFemale ? LOCTEXT("Floromantin", "Floromantin") : LOCTEXT("Floromant", "Floromant");
	}
	return Choice.bFemale ? LOCTEXT("Magierin", "Magierin") : LOCTEXT("Magier", "Magier");
}

#undef LOCTEXT_NAMESPACE
