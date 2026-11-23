// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Creatures/VaelNightCreature.h"
#include "VaelKobold.generated.h"

class ATargetPoint;

class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;
class UVaelMaterial;

/** What a living Kobold does in a fight (proposal 2026-11-21, waits for Boris and team) */
UENUM(BlueprintType)
enum class EVaelKoboldRolle : uint8
{
	/** Scurries in, grabs, steals materials and runs; killed before it gets away it drops all of it */
	Dieb,
	/** Holds its ground and strikes hard */
	Waechter,
	/** Keeps away, makes false copies of itself and lays signs that confuse */
	Zauberer,
	/** Slave driver and ant herder: cracks the whip from afar and throws vials of Tropfseuche that infest */
	Treiber,
	/** Blast master: throws crystal charges and lays blast traps */
	Sprengmeister,
	/** Lantern bearer: lures with false light and confuses; hit, it puts the light out and hides */
	Laternentraeger
};

/**
 *  A living Kobold of the Eisenschlund (Boris and team, 2026-11-13/21): small, nasty, cunning and very clever; their
 *  magic is contracts, false images, misleading signs and caught voices; weak to air and to the Russquarz lens. The
 *  bodies are the Tripo Kobolde (Schleimkratzer, Hortwaechter, Kontoraeltester) with movements of their own
 *  (Tools/Nachtwesen/kobold_lebend_clips.py). Their story (first allies, then betrayal and robbery, the hoard) comes later.
 */
UCLASS(BlueprintType)
class UVaelKoboldData : public UVaelNightCreatureData
{
	GENERATED_BODY()

public:

	UVaelKoboldData();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Kobold")
	EVaelKoboldRolle Rolle = EVaelKoboldRolle::Waechter;

	/** Thief: materials taken per grab; how long it runs before it is gone with them (s); how fast it runs (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Kobold|Dieb", meta = (ClampMin = 1))
	int32 StealCount = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Kobold|Dieb", meta = (ClampMin = 1))
	float FleeSeconds = 9.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Kobold|Dieb", meta = (ClampMin = 1))
	float FleeSpeed = 320.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Kobold|Dieb")
	FName RunClip = TEXT("Rennen");

	/** Caster: false copies per spell and how long they last (s); the misleading sign: radius (cm), lifetime and the
	 *  confusion it leaves (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Kobold|Zauberer", meta = (ClampMin = 0))
	int32 IllusionCount = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Kobold|Zauberer", meta = (ClampMin = 1))
	float IllusionSeconds = 14.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Kobold|Zauberer", meta = (ClampMin = 10))
	float SignRadius = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Kobold|Zauberer", meta = (ClampMin = 1))
	float SignSeconds = 9.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Kobold|Zauberer", meta = (ClampMin = 0))
	float SignConfusion = 3.0f;

	/** Thrown things (a vial, a charge): flight time (s), arc height and the radius where they land (cm), damage */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Kobold|Wurf", meta = (ClampMin = 0.2))
	float ThrowFlight = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Kobold|Wurf", meta = (ClampMin = 0))
	float ThrowApex = 180.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Kobold|Wurf", meta = (ClampMin = 10))
	float ThrowRadius = 200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Kobold|Wurf", meta = (ClampMin = 0))
	float ThrowDamage = 20.0f;

	/** Blast trap: trigger radius (cm), seconds until armed, fuse (s), how long it lies, its blast */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Kobold|Falle", meta = (ClampMin = 10))
	float TrapRadius = 110.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Kobold|Falle", meta = (ClampMin = 0))
	float TrapArmSeconds = 1.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Kobold|Falle", meta = (ClampMin = 0.1))
	float TrapFuse = 0.7f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Kobold|Falle", meta = (ClampMin = 1))
	float TrapSeconds = 40.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Kobold|Falle", meta = (ClampMin = 0))
	float TrapDamage = 26.0f;

	/** Lantern: confusion of the lure (s); light out and hiding after a hit (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Kobold|Laterne", meta = (ClampMin = 0))
	float LureConfusion = 3.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Kobold|Laterne", meta = (ClampMin = 0))
	float LightOutSeconds = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Kobold|Laterne", meta = (ClampMin = 0))
	float HideSeconds = 3.0f;
};

/** Schleimkratzer body: the thief */
UCLASS(BlueprintType)
class UVaelKoboldDiebData : public UVaelKoboldData
{
	GENERATED_BODY()

public:

	UVaelKoboldDiebData();
};

/** Hortwaechter body: the guard */
UCLASS(BlueprintType)
class UVaelKoboldWaechterData : public UVaelKoboldData
{
	GENERATED_BODY()

public:

	UVaelKoboldWaechterData();
};

/** Kontoraeltester body: the caster */
UCLASS(BlueprintType)
class UVaelKoboldZaubererData : public UVaelKoboldData
{
	GENERATED_BODY()

public:

	UVaelKoboldZaubererData();
};

/** Sklaventreiber body: the slave driver */
UCLASS(BlueprintType)
class UVaelKoboldTreiberData : public UVaelKoboldData
{
	GENERATED_BODY()

public:

	UVaelKoboldTreiberData();
};

/** Sprengmeister body: the blast master */
UCLASS(BlueprintType)
class UVaelKoboldSprengmeisterData : public UVaelKoboldData
{
	GENERATED_BODY()

public:

	UVaelKoboldSprengmeisterData();
};

/** Laternentraeger body: the lantern bearer */
UCLASS(BlueprintType)
class UVaelKoboldLaternentraegerData : public UVaelKoboldData
{
	GENERATED_BODY()

public:

	UVaelKoboldLaternentraegerData();
};

/** A blast trap of the Sprengmeister: a crystal charge on a tripwire that goes off shortly after someone comes near */
UCLASS()
class AVaelSprengfalle : public AActor
{
	GENERATED_BODY()

public:

	AVaelSprengfalle();

	virtual void Tick(float DeltaSeconds) override;

	static AVaelSprengfalle* Lay(AActor* Owner, const FVector& GroundLocation, const UVaelKoboldData* Data);

private:

	UPROPERTY(VisibleAnywhere, Category="Trap")
	TObjectPtr<UStaticMeshComponent> Charge;

	UPROPERTY(VisibleAnywhere, Category="Trap")
	TObjectPtr<UPointLightComponent> Glow;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Look;

	TWeakObjectPtr<AActor> Layer;
	/** Colour of its ore plug */
	FLinearColor Color = FLinearColor(1.0f, 0.2f, 0.6f);
	float Radius = 110.0f;
	float ArmSeconds = 1.2f;
	float Fuse = 0.7f;
	float Lifetime = 40.0f;
	float Damage = 26.0f;
	float Age = 0.0f;
	float FuseLeft = -1.0f;
};

/** A misleading sign of a Kobold on the ground: whoever steps on it while it glows loses the direction */
UCLASS()
class AVaelIrrzeichen : public AActor
{
	GENERATED_BODY()

public:

	AVaelIrrzeichen();

	virtual void Tick(float DeltaSeconds) override;

	static AVaelIrrzeichen* Place(AActor* Caster, const FVector& GroundLocation, float Radius, float Seconds, float Confusion);

private:

	UPROPERTY(VisibleAnywhere, Category="Sign")
	TObjectPtr<UStaticMeshComponent> Disc;

	UPROPERTY(VisibleAnywhere, Category="Sign")
	TObjectPtr<UPointLightComponent> Glow;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Look;

	TWeakObjectPtr<AActor> Caster;
	float Radius = 150.0f;
	float Lifetime = 9.0f;
	float Confusion = 3.0f;
	float Age = 0.0f;
	float NextCheck = 0.0f;
};

/**
 *  A living Kobold. Thieves steal and run, guards strike, casters make false copies of themselves and lay misleading
 *  signs. A false copy dies at the first hit and does no harm; its blows are only show.
 */
UCLASS()
class AVaelKobold : public AVaelNightCreature
{
	GENERATED_BODY()

public:

	AVaelKobold();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual float GetOutgoingDamageMultiplier() const override;

	/** Makes it a false copy before it is born */
	void MakeIllusion(float Seconds) { bIllusion = true; IllusionLeft = Seconds; }

	bool IsIllusion() const { return bIllusion; }

protected:

	virtual TSubclassOf<UVaelCreatureData> GetDefaultDataClass() const override;
	virtual void TickBehavior(float DeltaSeconds) override;
	virtual void OnPreyHit(AVaelCharacter* Player, const FVaelNightAttack& Attack) override;
	virtual void OnAttackLanded(const FVaelNightAttack& Attack) override;
	virtual void OnDamageTaken(float Damage, const FGameplayTagContainer& DamageTags) override;
	virtual void Die() override;

private:

	struct FVaelStolenGoods
	{
		TWeakObjectPtr<AVaelCharacter> Owner;
		TWeakObjectPtr<const UVaelMaterial> Material;
		int32 Count = 0;
	};

	void Steal(AVaelCharacter* Player);
	void ReturnLoot();
	void CastIllusions();
	void CastSign();
	void Vanish();
	void ThrowVial();
	void ThrowCharge();
	void LayTrap();
	void Lure();
	void TickLantern(float DeltaSeconds);

	/** Sklaventreiber: the whip hangs coiled at the hip and is in the hand only while it lashes (two material slots of its rig) */
	void TickWhip();
	void ShowSlot(FName Slot, bool bShown);
	bool bWhipInHand = true;

	TArray<FVaelStolenGoods> Stolen;
	bool bFleeing = false;
	float FleeLeft = 0.0f;
	/** Running off without loot to come back (the lantern bearer after a hit) */
	bool bHiding = false;
	float HideLeft = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> LanternLight;

	float LightOutLeft = 0.0f;
	float LightShown = 1.0f;
	float LanternFlare = 0.0f;
	float LanternTime = 0.0f;
	bool bIllusion = false;
	float IllusionLeft = 0.0f;
};

/** Living Kobolde in their caves */
class UVaelKobolde
{
public:

	/** Fills the places for enemies of the Kobold caves (Kobold-Stollen, Ameisenbau) with living Kobolde */
	static void SpawnInCaves(UWorld* World, TFunctionRef<bool(const ATargetPoint*)> Take = [](const ATargetPoint*) { return true; },
		TArray<TWeakObjectPtr<AVaelCreature>>* Born = nullptr);

	/** Data of a role; about every second one is the variant of its body (<AssetName>_B), when that was built */
	static UVaelKoboldData* GetData(EVaelKoboldRolle Rolle);

	/** Data of a role, always the body itself */
	static UVaelKoboldData* GetBaseData(EVaelKoboldRolle Rolle);

	/**
	 *  Now and then the variant of a Kobold body instead of the body itself (Boris 2026-11-22: more variety): the same creature with
	 *  the assets <AssetName>_B (bent and recoloured in Tools/Nachtwesen/kobold_variante.py). Returns the base when there is no variant.
	 */
	static UVaelNightCreatureData* MaybeVariant(UVaelNightCreatureData* Base, float Chance = 0.5f);
};
