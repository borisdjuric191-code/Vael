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

	// ---- Fight (design Boris 27.11.: Resonanz, Splitterschlag, leap, breakable stumps, escort; effects by ore pair) ----

	/** Workers of her lead ore that always come with her */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter|Kampf", meta = (ClampMin = 0))
	int32 EscortCount = 3;

	/** Resonanz: the stumps hum, her colony comes. Seconds of the call, time until the next, its reach, how many ants of her
	 *  colony may be around before she stops calling new ones, how many come per call */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter|Kampf", meta = (ClampMin = 0.2))
	float ResonanceCast = 2.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter|Kampf", meta = (ClampMin = 1))
	float ResonanceCooldown = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter|Kampf", meta = (ClampMin = 0))
	float ResonanceRange = 2200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter|Kampf", meta = (ClampMin = 0))
	int32 ResonanceColony = 6;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter|Kampf", meta = (ClampMin = 0))
	int32 ResonanceReinforcements = 2;

	/** Damage during the call that breaks it off, as a share of her health */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter|Kampf", meta = (ClampMin = 0, ClampMax = 1))
	float ResonanceInterrupt = 0.06f;

	/** Splitterschlag: a fan (every third time a ring) of shards of her companion ore; the effect depends on the pair */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter|Kampf", meta = (ClampMin = 0.1))
	float ShardWindup = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter|Kampf", meta = (ClampMin = 1))
	float ShardCooldown = 7.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter|Kampf", meta = (ClampMin = 1))
	int32 ShardCount = 6;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter|Kampf", meta = (ClampMin = 0))
	float ShardRange = 1100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter|Kampf", meta = (ClampMin = 0))
	float ShardCone = 55.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter|Kampf", meta = (ClampMin = 0))
	float ShardDamage = 11.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter|Kampf", meta = (ClampMin = 10))
	float ShardRadius = 120.0f;

	/** Leap onto a player: crouch, flight, shock wave where she lands */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter|Kampf", meta = (ClampMin = 0.1))
	float JumpWindup = 0.7f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter|Kampf", meta = (ClampMin = 0.2))
	float JumpFlight = 0.9f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter|Kampf", meta = (ClampMin = 1))
	float JumpCooldown = 11.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter|Kampf", meta = (ClampMin = 0))
	float JumpMinDistance = 450.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter|Kampf", meta = (ClampMin = 0))
	float JumpMaxDistance = 1250.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter|Kampf", meta = (ClampMin = 0))
	float JumpDamage = 22.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter|Kampf", meta = (ClampMin = 10))
	float JumpRadius = 280.0f;

	/** Wing stumps break side by side: hits wear them down (share of their damage, earth and frost twice), each holds this
	 *  share of her health. Without both she neither calls nor leaps, but rages: faster and biting more often */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter|Kampf", meta = (ClampMin = 0, ClampMax = 1))
	float StumpShare = 0.12f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter|Kampf", meta = (ClampMin = 0, ClampMax = 1))
	float StumpHitShare = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter|Kampf", meta = (ClampMin = 1))
	float RageSpeed = 1.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Erztochter|Kampf", meta = (ClampMin = 0.1, ClampMax = 1))
	float RageBiteInterval = 0.65f;
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
	virtual void TickBehavior(float DeltaSeconds) override;

	/** Spawns her escort of workers (once) */
	void SpawnEscort();

	/** Ants of her lead ore around her (not counting her) */
	int32 CountColony() const;

	/** Resonanz: hum, then the colony comes; broken off by enough damage */
	void StartResonance();
	void FinishResonance();

	/** Splitterschlag: wind up, then a fan or ring of shards */
	void StartShards(const AActor* Target);
	void FireShards();

	/** Leap: crouch, fly, land in a shock wave */
	void StartJump(const AActor* Target);

	/** Breaks the wing stump of one side (0 left, 1 right): it shrinks away with its crystals, shards fly, ore drops */
	void BreakStump(int32 Side);

	int32 GetIntactStumps() const { return (bStumpBroken[0] ? 0 : 1) + (bStumpBroken[1] ? 0 : 1); }
	bool IsRaging() const { return GetIntactStumps() == 0; }

	/** Raises, shakes and hides the wing bones */
	void PoseWings(float DeltaSeconds);

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

	enum class EAction : uint8 { None, Resonance, Shards, JumpWindup, JumpAir };
	EAction Action = EAction::None;
	float ActionTime = 0.0f;
	float ResonanceTimer = 4.0f;
	float ShardTimer = 2.0f;
	float JumpTimer = 5.0f;
	float BiteTimer = 0.0f;
	float CastDamage = 0.0f;
	int32 ShardCasts = 0;
	FVector ShardAim = FVector::ForwardVector;
	FVector JumpStart = FVector::ZeroVector;
	FVector JumpEnd = FVector::ZeroVector;
	float StumpDamage[2] = {0.0f, 0.0f};
	bool bStumpBroken[2] = {false, false};
	bool bEscortSpawned = false;
	float WingLift = 0.0f;
	float WingShake = 0.0f;
	float WingTime = 0.0f;

	float EyeTime = 0.0f;
	float EyeFlare = 0.0f;
	float EyeAnger = 0.0f;
	float EyeLife = 1.0f;
};
