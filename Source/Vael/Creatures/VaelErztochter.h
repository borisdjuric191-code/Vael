// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Creatures/VaelSchuerfameise.h"
#include "VaelErztochter.generated.h"

/**
 *  Erztochter of the Eisenschlund: young queen under the Erzmutter, twice the size of a worker, with a bone crest,
 *  an armoured saddle, a huge abdomen and two pairs of crystal-crusted wing stumps. She carries the two ores of her
 *  pair: the lead ore crowns the crest, fills the middle of the saddle and every other band of the abdomen; the
 *  companion ore fills the rims, the head and the foot of the horns. Horns (lead) and wing stumps (companion) are
 *  overgrown by a coat of long crystals in close rows that follows their shape; a few small crystals sit on the legs
 *  (design Boris 27.11.).
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

	/** Horns and wing stumps are overgrown by a coat of crystals in close rows: length of each piece times this */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter", meta = (ClampMin = 0.5))
	float CoatLength = 1.6f;

	/** Randomness of the coat (0 = strict rows, 1 = like the settings) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter", meta = (ClampMin = 0, ClampMax = 1))
	float CoatJitter = 0.25f;

	/** How deep the foot of a crystal sits in the body, times its radius: the coat (lying flat) and the other crystals */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter", meta = (ClampMin = 0))
	float CoatSink = 0.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter", meta = (ClampMin = 0))
	float CrystalSink = 0.25f;

	/** Bent crystal pieces for the coat (tip curls to local -X), from Tools/Ameisen/kristalle_biegen.py */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter")
	TArray<FVaelAmeisenKristall> BentKristalle;

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

	/** Middle of each eye on the surface and its outward axis, Blender model space (metres); measured on the model */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter|Augen")
	TArray<FVector> EyeLocations;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter|Augen")
	TArray<FVector> EyeNormals;

	/** Radius of an eye, metres */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter|Augen", meta = (ClampMin = 0.005))
	float EyeRadius = 0.034f;

	/** Milky glow of a free daughter, and the sick green of one the Kobolde fed with Tropfseuche */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter|Augen")
	FLinearColor EyeColor = FLinearColor(0.80f, 0.88f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter|Augen")
	FLinearColor EnslavedEyeColor = FLinearColor(0.45f, 1.0f, 0.25f);

	/** Glow at rest and when she fights */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter|Augen", meta = (ClampMin = 0))
	float EyeGlow = 1.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter|Augen", meta = (ClampMin = 0))
	float AngryEyeGlow = 4.0f;
};

UCLASS()
class AVaelErztochter : public AVaelSchuerfameise
{
	GENERATED_BODY()

public:

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

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
	virtual void OnDamageTaken(float Damage, const FGameplayTagContainer& DamageTags) override;

	/** Puts the living eyes on the head: a milky eyeball that breathes light and a brighter core that drifts and turns to her target */
	void BuildEyes();

	/** Breathing, gaze, anger and the flare when hit */
	void UpdateEyes(float DeltaSeconds);

	/** Ore pair 1–5 */
	UPROPERTY(EditAnywhere, Category="Erztochter", meta = (ClampMin = 1, ClampMax = 5))
	int32 Pair = 1;

private:

	struct FEye
	{
		UStaticMeshComponent* Ball = nullptr;   // kept alive by EyeParts
		UStaticMeshComponent* Core = nullptr;
		FVector Centre = FVector::ZeroVector;   // mesh space, cm
		FVector Normal = FVector::UpVector;
		FTransform Bone;                        // head bone at build time, mesh space
		FVector Gaze = FVector::ZeroVector;
	};

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> EyeParts;

	TArray<FEye> Eyes;
	float EyeTime = 0.0f;
	float EyeFlare = 0.0f;
	float EyeAnger = 0.0f;
	float EyeLife = 1.0f;
};
