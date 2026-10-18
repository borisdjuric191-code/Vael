// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Floromant/VaelFloromantSettings.h"
#include "VaelHeroSubsystem.generated.h"

/** Class and body a player has chosen */
USTRUCT(BlueprintType)
struct FVaelHeroChoice
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Hero")
	EVaelHeroClass Class = EVaelHeroClass::Mage;

	/** True for the woman, false for the man */
	UPROPERTY(EditAnywhere, Category="Hero")
	bool bFemale = false;
};

/**
 *  Remembers which class and body each player slot plays, for the whole session: the choice survives travelling and
 *  a player who leaves and joins again plays the same. Chosen on the page "Gestalt" of the menu.
 */
UCLASS()
class UVaelHeroSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	/** Returns the subsystem of the game the object lives in, null outside a game */
	static UVaelHeroSubsystem* Get(const UObject* WorldContext);

	/** Choice of the slot; until a player chooses, the slots take turns so mage and Floromant can be tested side by side */
	FVaelHeroChoice GetChoice(int32 PlayerSlot) const;

	/** Stores the choice of the slot */
	void SetChoice(int32 PlayerSlot, const FVaelHeroChoice& Choice) { Choices.Add(PlayerSlot, Choice); }

	/** The four choices in the order the page "Gestalt" lists them */
	static FVaelHeroChoice GetListedChoice(int32 Index);
	static constexpr int32 NumListedChoices = 4;

	/** Name of a choice for the players, for example "Floromantin" */
	static FText GetChoiceName(const FVaelHeroChoice& Choice);

private:

	TMap<int32, FVaelHeroChoice> Choices;
};
