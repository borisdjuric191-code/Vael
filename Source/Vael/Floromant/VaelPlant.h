// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/VaelCharacterBase.h"
#include "Floromant/VaelFloromantSettings.h"
#include "VaelPlant.generated.h"

class AVaelCharacter;
class AVaelCreature;
class UBoxComponent;
class UMaterialInstanceDynamic;
class UProceduralMeshComponent;
class UStaticMeshComponent;

/**
 *  A plant grown by a Floromant from a seed. Grows out of the ground in a second or two, sways in the wind, acts once grown
 *  and withers when its time is over or its health is gone. Only the fire of enemies hurts it, never other players.
 *  All its movement is procedural: the material fades it in from the bottom up and sways it, the code pops its size, squashes and flashes it.
 */
UCLASS(abstract)
class AVaelPlant : public AVaelCharacterBase
{
	GENERATED_BODY()

protected:

	/** Model of the plant */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> Body;

public:

	/** Constructor */
	AVaelPlant();

	/** Grows a plant of the seed on the ground at the location for the Floromant, facing the yaw. Returns null if it can't be spawned. */
	static AVaelPlant* GrowPlant(EVaelSeedKind Kind, AVaelCharacter* Gardener, const FVector& GroundLocation, float Yaw);

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

	/** Seed the plant grew from */
	EVaelSeedKind GetKind() const { return Kind; }

	/** Floromant who grew it */
	AVaelCharacter* GetGardener() const { return Gardener.Get(); }

	/** Lets the plant wither and vanish */
	void Wither();

	/** True once it withers */
	bool IsWithering() const { return bWithering; }

	/** Share of its health left, for the health bar */
	float GetHealthFraction() const { return GetMaxHealth() > 0.0f ? GetHealth() / GetMaxHealth() : 0.0f; }

	//~Begin AVaelCharacterBase
	virtual bool IsOnPlayerSide() const override { return true; }
	virtual bool IsDefeated() const override { return bWithering; }
	virtual float GetIncomingDamageMultiplier(const FGameplayTagContainer& DamageTags) const override;
	virtual void ApplyKnockback(const FVector& Direction, float Speed) override {}
	virtual void ApplyPull(const FVector& Location, float Speed, float DeltaSeconds) override {}
	virtual bool CanReceiveStatus(EVaelStatus Status) const override { return Status == EVaelStatus::Burning; }
	//~End AVaelCharacterBase

protected:

	/** Builds the look and starts to grow */
	virtual void BeginPlay() override;

	/** Withers at zero health, flashes when hurt */
	virtual void OnHealthChanged(float OldValue, float NewValue) override;

	/** Shapes the look: by default the model of the plant, scaled to its height and set on the ground */
	virtual void BuildLook();

	/** What the plant does each frame once it has grown */
	virtual void TickGrown(float DeltaSeconds) {}

	/** Called once when it starts to wither */
	virtual void OnWither() {}

	/** Height of the ground below the plant in world space */
	float GetGroundZ() const { return GroundZ; }

	/** Look and values of its seed */
	const FVaelPlantLook& GetLook() const;

	/** Lets the plant glow up in its colour for the seconds; a telegraph before it acts */
	void Flash(float Seconds, float Strength = 1.0f);

	/** Squash of the plant: below 1 it ducks and widens, above 1 it stretches up. Lean tips it forwards in degrees. Both ease back on their own. */
	void Squash(float Amount, float LeanDegrees = 0.0f);

	/** Gives a material to the parts of the look; each gets its own instance, so it can grow in and flash */
	void UseMaterial(UPrimitiveComponent* Part, UMaterialInterface* Material);

	/** Nearest living enemy within the range, null if there is none */
	AVaelCreature* FindEnemy(float Range) const;

	/** Part of the look that pops, squashes and leans: the model, or the roots of the wall */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> GrowingPart;

	/** Size of the growing part once grown, as its relative scale */
	FVector GrownScale = FVector::OneVector;

	/** Seconds since it was planted */
	float Age = 0.0f;

private:

	/** Sets how far it has grown in, the flash and the squash on the materials and the model */
	void UpdateLook(float DeltaSeconds);

	EVaelSeedKind Kind = EVaelSeedKind::Thistle;
	TWeakObjectPtr<AVaelCharacter> Gardener;

	/** World height of the ground it grows from */
	float GroundZ = 0.0f;

	/** Instances of the material of every part */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;

	/** Flash: strength now and how fast it fades */
	float FlashStrength = 0.0f;
	float FlashFade = 1.0f;

	/** Squash and lean now; they ease back to 1 and 0 */
	float SquashAmount = 1.0f;
	float LeanAmount = 0.0f;

	/** True once it withers, and seconds since then */
	bool bWithering = false;
	float WitherTime = 0.0f;
};

/** Glutdistel: turns towards the nearest enemy and shoots seeds of fire that burst where they hit */
UCLASS()
class AVaelThistlePlant : public AVaelPlant
{
	GENERATED_BODY()

public:

	/** Seeds of fire shot so far */
	int32 GetShotCount() const { return ShotCount; }

protected:

	virtual void TickGrown(float DeltaSeconds) override;

private:

	/** Shoots a seed of fire at the enemy */
	void Shoot(AActor* Target);

	int32 ShotCount = 0;

	/** Seconds until the next shot, and the enemy the coming shot is aimed at while it winds up */
	float ShotCountdown = 0.3f;
	bool bWindingUp = false;
	TWeakObjectPtr<AActor> WindupTarget;
};

/** Wurzelwand: a wall of roots across the throw that holds back enemies; players walk through, and shots of both sides fly through its gaps */
UCLASS()
class AVaelRootWallPlant : public AVaelPlant
{
	GENERATED_BODY()

	/** Holds back the enemies */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> Barrier;

	/** The roots, built in code */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UProceduralMeshComponent> Roots;

public:

	/** Constructor */
	AVaelRootWallPlant();

	/** True while the wall holds back enemies */
	bool IsHolding() const;

protected:

	virtual void BuildLook() override;
	virtual void TickGrown(float DeltaSeconds) override;
	virtual void OnWither() override;

private:

	/** Lets every player walk through the wall */
	void LetPlayersThrough();

	/** Seconds until the players are looked at again; one who joins late passes too */
	float PassCountdown = 0.0f;
};

/** Tränenkelch: heals every ally around it in pulses */
UCLASS()
class AVaelChalicePlant : public AVaelPlant
{
	GENERATED_BODY()

public:

	/** Pulses that reached at least one ally so far, hurt or not */
	int32 GetPulsesReachingAllies() const { return PulsesReachingAllies; }

protected:

	virtual void TickGrown(float DeltaSeconds) override;

private:

	/** Seconds until the next pulse */
	float PulseCountdown = 0.0f;

	int32 PulsesReachingAllies = 0;
};
