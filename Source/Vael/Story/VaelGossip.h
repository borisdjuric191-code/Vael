// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
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
};

/**
 *  Gerede: what people mutter in the background while players walk by, shown over their heads without stopping the game.
 *  Lines that belong to a decision of the group are said as soon as it was made.
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

	/** Picks a line that may be said now and is not the last one said; lines of a decision come half of the time. Returns -1 if there is none. */
	int32 PickLine(const UObject* WorldContext, int32 LastIndex) const;
};
