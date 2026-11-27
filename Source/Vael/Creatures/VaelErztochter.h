// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Creatures/VaelSchuerfameise.h"
#include "VaelErztochter.generated.h"

/**
 *  Erztochter of the Eisenschlund: young queen under the Erzmutter, twice the size of a worker, with a bone crest,
 *  an armoured saddle, a huge abdomen and two pairs of crystal-crusted wing stumps. She carries the two ores of her
 *  pair: the lead ore crowns the crest, fills the middle of the saddle and every other band of the abdomen; the
 *  companion ore fills the rims, the head, the foot of the horns and splinters along the torn wing edges; a few small
 *  crystals of both sit on the legs (design Boris 27.11.).
 *  Values are first proposals; the fight (Resonanz, Splitterschlag, leap, breakable stumps) follows.
 */
UCLASS(BlueprintType)
class UVaelErztochterData : public UVaelSchuerfameiseData
{
	GENERATED_BODY()

public:

	UVaelErztochterData();

	/** Growth of her crystals (like an Ahnenpanzer): 0 = fills the setting */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter", meta = (ClampMin = 0))
	float CrystalGrowth = 0.9f;

	/** The crest crystals are long spikes: length times this */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter", meta = (ClampMin = 0.5))
	float CrestLength = 1.8f;

	/** The splinters along the torn edges of the wing stumps: length times this */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter", meta = (ClampMin = 0.5))
	float WingLength = 2.4f;

	/** Chance that a setting takes the other ore of the pair, so the two ores interlock instead of splitting the body */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter", meta = (ClampMin = 0, ClampMax = 0.5))
	float MixChance = 0.2f;

	/** Settings nearer to the middle line of the saddle than this take the lead ore, metres in the Blender model */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter", meta = (ClampMin = 0))
	float SaddleCore = 0.25f;

	/** Width of the bands on the abdomen that switch between the two ores, metres in the Blender model */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter", meta = (ClampMin = 0.05))
	float AbdomenBand = 0.2f;
};

UCLASS()
class AVaelErztochter : public AVaelSchuerfameise
{
	GENERATED_BODY()

public:

	virtual void BeginPlay() override;

	/** Ore pair 1–5 (1 Schwefelkies+Funkenerz, 2 Rosteisen+Salzspat, 3 Gruenspan+Drahtsilber, 4 Blutzinnober+Siechglas,
	 *  5 Kobaltbluete+Rippenspat; the first is the lead ore) and whether the Kobolde enslaved her */
	void SetupDaughter(int32 InPair, bool bInEnslaved);

	/** Lead and companion ore of a pair */
	static void GetPairOres(int32 InPair, FName& OutLead, FName& OutCompanion);

	int32 GetPair() const { return Pair; }

protected:

	virtual TSubclassOf<UVaelCreatureData> GetDefaultDataClass() const override;
	virtual void BuildCrystals() override;
	virtual float GetStatusTextHeight() const override { return 190.0f * GetActorScale3D().Z; }

	/** Ore pair 1–5 */
	UPROPERTY(EditAnywhere, Category="Erztochter", meta = (ClampMin = 1, ClampMax = 5))
	int32 Pair = 1;
};
