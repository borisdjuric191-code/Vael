// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Story/VaelNpcIdentity.h"
#include "VaelGossip.generated.h"

/** One thing a person mutters when players are near */
USTRUCT(BlueprintType)
struct FVaelGossipLine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gossip", meta = (MultiLine = true))
	FText Line;

	/** Only said once the story remembers this decision, like "Beweis.Orden"; none: always */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gossip")
	FName RequiredFlag;

	/** Only said by people of this role, like "Wache"; none: by anyone */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gossip")
	FName Role;

	/** Only said by people who think this of the Prophet; none: by anyone */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gossip")
	EVaelProphetAttitude Prophet = EVaelProphetAttitude::None;
};

/**
 *  Gerede: what people mutter in the background while players walk by, shown over their heads without stopping the game.
 *  Lines that belong to a decision of the group are said as soon as it was made.
 *  Lines can belong to one role or one attitude to the Prophet, and may hold {Name} (the speaker), {Ort} (their
 *  settlement) and {Nachbar} (someone else of the settlement). A settlement's gossip includes the shared gossip of all
 *  people (DA_Gossip_Leute) and, where there is one, the older gossip of the place.
 */
UCLASS(BlueprintType)
class UVaelGossip : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** Name shown over people who have no dialogue of their own, like "Überlebende" */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gossip")
	FText SpeakerName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gossip")
	TArray<FVaelGossipLine> Lines;

	/** More gossip said by the same people, like the lines all people share; looked through after the own lines */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gossip")
	TArray<TObjectPtr<UVaelGossip>> Includes;

	/** Players this close are muttered at, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gossip", meta = (ClampMin = 50))
	float Range = 450.0f;

	/** Seconds a line stays over the head */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gossip", meta = (ClampMin = 1))
	float ShowSeconds = 4.5f;

	/** Seconds of silence between two lines, picked at random in this span */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gossip", meta = (ClampMin = 1))
	float MinPause = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gossip", meta = (ClampMin = 1))
	float MaxPause = 24.0f;

	/** Picks a line this person may say now that is not the last one said; lines of a decision come half of the time.
	 *  The index counts through the own lines, then the included ones. Returns -1 if there is none. */
	int32 PickLine(const UObject* WorldContext, int32 LastIndex, const FVaelNpcIdentity* Speaker = nullptr) const;

	/** The line under an index of PickLine, or null */
	const FVaelGossipLine* GetLine(int32 Index) const;

	/** All lines in the order PickLine counts them: own lines, then the included gossip (each asset once) */
	void GatherLines(TArray<const FVaelGossipLine*>& OutLines) const;

private:

	void GatherLines(TArray<const FVaelGossipLine*>& OutLines, TArray<const UVaelGossip*>& Visited) const;
};
