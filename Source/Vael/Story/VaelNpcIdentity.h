// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VaelNpcIdentity.generated.h"

/** What a person thinks of the Prophet: the group with the glowing hands, risen from the fever */
UENUM(BlueprintType)
enum class EVaelProphetAttitude : uint8
{
	/** Not set; gossip lines that need an attitude are never said */
	None,
	/** Loves them, hopes, spreads the word */
	Follower UMETA(DisplayName="Anhänger"),
	/** Hates or fears them, calls them false */
	Enemy UMETA(DisplayName="Gegner"),
	/** Wants to believe, does not dare yet */
	Doubter UMETA(DisplayName="Zweifler"),
	/** Has other worries */
	Indifferent UMETA(DisplayName="Gleichgültig"),
	/** A story person: their dialogue decides */
	Story UMETA(DisplayName="Story")
};

/** Age of a person; the placeholder body follows it */
UENUM(BlueprintType)
enum class EVaelNpcAge : uint8
{
	Adult UMETA(DisplayName="Erwachsen"),
	Child UMETA(DisplayName="Kind"),
	Young UMETA(DisplayName="Jung"),
	Old UMETA(DisplayName="Alt")
};

/** Build of a person; the placeholder body follows it */
UENUM(BlueprintType)
enum class EVaelNpcBuild : uint8
{
	Medium UMETA(DisplayName="Mittel"),
	Slim UMETA(DisplayName="Schmal"),
	Strong UMETA(DisplayName="Kräftig")
};

/**
 *  Who a person of Vael is (Strandgut der Welt: their families came from many shores). Written by the settlement
 *  scripts from Scripts/Data/Welt/AM_Siedlungen.json; names come from Tools/Welt/namen.py.
 */
USTRUCT(BlueprintType)
struct FVaelNpcIdentity
{
	GENERATED_BODY()

	/** Name shown over the head of people without a dialogue of their own */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Npc")
	FText Name;

	/** Culture the given name comes from, like "westafrika" (Scripts/Data/Namen/Kulturen.json) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Npc")
	FName Culture;

	/** What the person does, like "Wache", "Bauer", "Pilger"; gossip lines can belong to one role */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Npc")
	FName Role;

	/** Faction, like "Ueberlebende", "Gilde", "Orden", "Passwache", "Freie" */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Npc")
	FName Faction;

	/** Key of the settlement, like "lager"; people of one settlement talk about each other */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Npc")
	FName Settlement;

	/** Name of the settlement, for {Ort} in gossip */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Npc")
	FText SettlementName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Npc")
	EVaelNpcAge Age = EVaelNpcAge::Adult;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Npc")
	EVaelNpcBuild Build = EVaelNpcBuild::Medium;

	/** What the person thinks of the Prophet; picks the gossip lines that fit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Npc")
	EVaelProphetAttitude Prophet = EVaelProphetAttitude::None;
};
