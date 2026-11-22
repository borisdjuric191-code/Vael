// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelIndoorLevel.generated.h"

/**
 *  Marks a level as an indoor place, like the refugia under the Wurzelpfade (the Glutkammer below the crater).
 *  One of these in a level is enough: the game gives it no sky that follows the clock (no sun, no moon), no local weather
 *  (fog banks, storms, rain patches), and no training dummies or test areas of the test level. The level brings its own light.
 */
UCLASS(meta = (DisplayName = "Vael Indoor Level"))
class AVaelIndoorLevel : public AActor
{
	GENERATED_BODY()

public:

	/** Constructor */
	AVaelIndoorLevel();

	/** True when the world holds an indoor marker */
	static bool IsIndoor(const UWorld* World);

	/** The indoor marker of the world, or null */
	static const AVaelIndoorLevel* Find(const UWorld* World);

	/**
	 *  Which cave a place belongs to, like "L_Gildenmine". In the joined caves of the Eisenschlund (one map since 2026-11-22) every
	 *  cave is a region tagged with its old level name; anywhere else (and outside those regions) the name of the map
	 */
	static FString PlaceKeyAt(const UWorld* World, const FVector& Location);

	/** The places for enemies of a cave fill with what lives there (the Schleimgaenger of the Tropfseuche) */
	virtual void BeginPlay() override;

	/** The name of the place, shown where the region name stands (a cave of the Eisenschlund: "Kernerz-Stollen") */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Indoor")
	FText PlaceName;
};
