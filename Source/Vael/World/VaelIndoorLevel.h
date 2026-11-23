// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelIndoorLevel.generated.h"

class AVaelCreature;
class ATargetPoint;
class UVaelWorldMap;

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

	/** True when the whole level is an indoor place (a marker with bWholeLevel) */
	static bool IsIndoor(const UWorld* World);

	/** True when a place is indoors: the whole level is, or the place lies in the box of a bounded marker (the caves of the
	 *  Eisenschlund inside the open world L_Vael, 2026-11-23) */
	static bool IsIndoorAt(const UWorld* World, const FVector& Location);

	/** The marker of a whole indoor level, or null */
	static const AVaelIndoorLevel* Find(const UWorld* World);

	/** The marker whose place a location belongs to: the whole-level one, or the bounded one around it; null outdoors */
	static const AVaelIndoorLevel* FindAt(const UWorld* World, const FVector& Location);

	/** True when the world holds any indoor marker, whole or bounded (it has caves to fill with their dwellers) */
	static bool HasIndoorPlaces(const UWorld* World);

	/** False: the marker stands for a box only (Extent around it), inside a bigger level that keeps its sky and weather */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Indoor")
	bool bWholeLevel = true;

	/** Half size of the indoor box (cm) when not the whole level */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Indoor", meta = (EditCondition = "!bWholeLevel"))
	FVector Extent = FVector(50000.0f, 50000.0f, 10000.0f);

	/** The map shown while inside the box (a cave's own map, in world coordinates) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Indoor", meta = (EditCondition = "!bWholeLevel"))
	TSoftObjectPtr<UVaelWorldMap> PlaceMap;

	/** True when a place lies in this marker's box (always for a whole level) */
	bool Covers(const FVector& Location) const;

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

	/** Walking from one cave of the joined caves into the next shows its name and a line about it */
	virtual void Tick(float DeltaSeconds) override;

private:

	/** Ceilings over the way (rock bridges and what hangs from them, skull arches; tag VaelDecke) that hide a player from the camera
	 *  are hidden while they do */
	void FadeCeilings();

	TWeakObjectPtr<const AActor> ShownRegion;
	bool bRegionKnown = false;
	float NoticeWait = 0.0f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> Ceilings;

	TSet<TObjectPtr<AActor>> HiddenCeilings;

	/** Bounded marker in a streamed world: the cave's dwellers come when a player is near their place and its floor has loaded;
	 *  those far from every player are held still (their floor may have unloaded under them) */
	void TickDwellers();
	void RefreshCeilings();
	float DwellerWait = 0.0f;
	float CeilingWait = 0.0f;
	TSet<FName> UsedPlaces;
	TArray<TWeakObjectPtr<AVaelCreature>> Dwellers;
	TSet<TWeakObjectPtr<AVaelCreature>> Frozen;
};
