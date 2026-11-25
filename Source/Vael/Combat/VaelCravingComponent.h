// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VaelCravingComponent.generated.h"

class AVaelCharacter;

/** The deadly sins the shadow sets carry (Boris 25.11.: only Nid or shadow-path armour wears one, about two sets per sin) */
UENUM(BlueprintType)
enum class EVaelSin : uint8
{
	None,
	/** Gier: claiming the Nid (Prediger-Satz), hoarding (Kobold-Hort-Satz) */
	Greed,
	/** Völlerei: eating (Moder-Satz) and drinking (Blut-Falter) */
	Gluttony,
	/** Hochmut (Drachenbrut) */
	Pride,
	Wrath,
	Envy,
	Sloth,
	Lust
};

/**
 *  What a craving does (tuning, data): how fast it grows in a fight and with the corruption, what feeds it, what it gives
 *  while fed, what it costs while it starves, and when it takes the wearer over
 */
USTRUCT(BlueprintType)
struct FVaelCravingSpec
{
	GENERATED_BODY()

	/** Hunger, Durst … (shown in the HUD and the combat text) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Craving")
	FText Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Craving")
	EVaelSin Sin = EVaelSin::None;

	/** Growth per second while enemies are near, from 0 (fed) to 1 (starving); times 1 + corruption / CorruptionScale */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Craving", meta = (ClampMin = 0))
	float GrowthPerSecond = 0.035f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Craving", meta = (ClampMin = 1))
	float CorruptionScale = 50.0f;

	/** Out of a fight it settles this fast */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Craving", meta = (ClampMin = 0))
	float CalmPerSecond = 0.02f;

	/** An enemy dying this close feeds it this much */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Craving", meta = (ClampMin = 0))
	float FeedPerKill = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Craving", meta = (ClampMin = 0))
	float FeedReach = 900.0f;

	/** Each feeding heals the wearer this much (the Blut-Falter drinks, the Moder-Satz eats) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Craving", meta = (ClampMin = 0))
	float HealPerFeed = 6.0f;

	/** Fed (below SatedBelow) the wearer hits this much harder */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Craving", meta = (ClampMin = 0, ClampMax = 1))
	float SatedBelow = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Craving", meta = (ClampMin = 1))
	float SatedDamageScale = 1.2f;

	/** Starving (above StarvingAbove) his corruption rises by this per second: the price, never damage to himself */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Craving", meta = (ClampMin = 0, ClampMax = 1))
	float StarvingAbove = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Craving", meta = (ClampMin = 0))
	float CorruptionPerSecond = 0.25f;

	/** Starving at this corruption or more it takes him over: it lashes out at the nearest enemy by itself and he cannot take it off */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Craving", meta = (ClampMin = 0, ClampMax = 100))
	float PossessAtCorruption = 85.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Craving", meta = (ClampMin = 0))
	float LashEvery = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Craving", meta = (ClampMin = 0))
	float LashReach = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Craving", meta = (ClampMin = 0))
	float LashDamage = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Craving")
	FLinearColor Color = FLinearColor(0.55f, 0.2f, 0.65f);
};

namespace VaelCravings
{
	/** Völlerei, eating: the mouths of the Moder-Satz */
	FVaelCravingSpec Hunger();
	/** Völlerei, drinking: the Blut-Falter */
	FVaelCravingSpec Thirst();
}

/**
 *  The craving of a shadow set (Boris 25.11.): it grows in every fight and the faster the higher the wearer's corruption,
 *  nearby deaths feed it (and heal him), fed it makes him stronger, starving it raises his corruption, and starving at high
 *  corruption it takes him over. The sets switch it on and off; it does nothing while none is set.
 */
UCLASS(ClassGroup=(Vael), meta=(BlueprintSpawnableComponent))
class UVaelCravingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVaelCravingComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Starts a craving (a shadow set was put on); the meter starts fed */
	void SetCraving(const FVaelCravingSpec& Spec);

	/** Ends it (the set was taken off) */
	void ClearCraving();

	bool HasCraving() const { return bActive; }
	const FVaelCravingSpec& GetSpec() const { return Spec; }

	/** 0 fed … 1 starving */
	float GetMeter() const { return Meter; }
	void SetMeter(float Value) { Meter = FMath::Clamp(Value, 0.0f, 1.0f); }

	/** Feeds it (a kill, a drink); heals the wearer by the feeding */
	void Feed(float Amount);

	bool IsSated() const { return bActive && Meter < Spec.SatedBelow; }
	bool IsStarving() const { return bActive && Meter > Spec.StarvingAbove; }
	bool IsPossessed() const { return bPossessed; }

	/** How much harder he hits because of it */
	float GetDamageScale() const { return IsSated() ? Spec.SatedDamageScale : 1.0f; }

	/** The set won't come off while it has him, or while he fights hungry */
	bool CanTakeOff() const { return !bPossessed; }

	/** For the tests: feedings and lashes so far */
	int32 GetFeedings() const { return Feedings; }
	int32 GetLashes() const { return Lashes; }

private:
	AVaelCharacter* GetPlayer() const;
	bool IsInFight() const;

	FVaelCravingSpec Spec;
	bool bActive = false;
	float Meter = 0.0f;
	bool bPossessed = false;
	float NextLash = 0.0f;
	int32 Feedings = 0;
	int32 Lashes = 0;
	/** Creatures near him that were alive at the last look, to notice who died close by */
	TSet<TWeakObjectPtr<class AVaelCreature>> NearAlive;
};
