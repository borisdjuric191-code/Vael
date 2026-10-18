// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/VaelCharacterBase.h"
#include "GameFramework/Actor.h"
#include "Floromant/VaelPlantData.h"
#include "VaelPlant.generated.h"

class AVaelCharacter;
class AVaelCreature;
class UBoxComponent;
class UMaterialInstanceDynamic;
class UProceduralMeshComponent;
class UStaticMeshComponent;

/** Builds gnarled roots and vines as tubes, for the wall, the grip and the whip */
namespace VaelRoots
{
	/** Adds one root along the curve: thick at its start, thinner along it (to a point for spikes), knotted */
	void AddRoot(TArray<FVector>& Vertices, TArray<FVector>& Normals, TArray<FVector2D>& UVs, TArray<int32>& Triangles,
		TFunctionRef<FVector(float)> PointAt, float Thickness, bool bPointed, float Seed);
}

/**
 *  A plant of the Floromant. Sown plants grow out of the ground in a second or two, sway and stand still until the Ruf des Gartens
 *  makes them act for a few seconds; Wuchern turns them into their giant form. Plants growing wild in the world answer the call too:
 *  for them an invisible plant acts in their place. Only the fire of enemies hurts a plant, never other players.
 *  All its movement is procedural: the material fades it in from the bottom up and sways it, the code pops its size, squashes and flashes it.
 */
UCLASS()
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

	/** Grows a sown plant on the ground for the Floromant, in its light or shadow form. Returns null if it can't be spawned. */
	static AVaelPlant* GrowPlant(UVaelPlantData* Data, AVaelCharacter* Gardener, const FVector& GroundLocation, float Yaw, bool bShadow);

	/** Lets a plant growing wild in the world act: an invisible plant does it in its place for the seconds */
	static AVaelPlant* ActForWildPlant(UVaelPlantData* Data, AVaelCharacter* Caller, const FVector& GroundLocation, bool bShadow, float Duration, bool bGiant);

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

	/** Makes the plant act for the seconds, in its light or shadow form */
	void Call(float Duration, bool bActAsShadow);

	/** Turns it into its giant form for the seconds: bigger and stronger */
	void MakeGiant(float Duration);

	UVaelPlantData* GetData() const { return Data; }
	AVaelCharacter* GetGardener() const { return Gardener.Get(); }
	bool IsShadowForm() const { return bShadow; }
	bool IsWild() const { return bWild; }
	bool IsActing() const { return CallTimeLeft > 0.0f; }

	/** Lets the plant wither and vanish */
	void Wither();

	/** True once it withers */
	bool IsWithering() const { return bWithering; }

	/** Share of its health left, for the health bar */
	float GetHealthFraction() const { return GetMaxHealth() > 0.0f ? GetHealth() / GetMaxHealth() : 0.0f; }

	/** For the smoke test: shots fired, and pulses that reached an ally or an enemy */
	int32 GetShotCount() const { return ShotCount; }
	int32 GetPulseCount() const { return PulseCount; }

	//~Begin AVaelCharacterBase
	virtual bool IsOnPlayerSide() const override { return true; }
	virtual bool IsDefeated() const override { return bWithering || bWild; }
	virtual float GetIncomingDamageMultiplier(const FGameplayTagContainer& DamageTags) const override;
	virtual void ApplyKnockback(const FVector& Direction, float Speed) override {}
	virtual void ApplyPull(const FVector& Location, float Speed, float DeltaSeconds) override {}
	virtual bool CanReceiveStatus(EVaelStatus Status) const override { return Status == EVaelStatus::Burning; }
	//~End AVaelCharacterBase

protected:

	/** Builds the look and starts to grow */
	virtual void BeginPlay() override;

	/** Withers at zero health, ducks when hurt */
	virtual void OnHealthChanged(float OldValue, float NewValue) override;

private:

	/** The model scaled to its height, set on the ground, its material and the shadow overlay */
	void BuildLook();

	/** What it does each frame while it acts */
	void TickCall(float DeltaSeconds);

	/** Sets how far it has grown in, the flash, the squash and the giant form on the materials and the model */
	void UpdateLook(float DeltaSeconds);

	/** Lets the plant glow up in its colour for the seconds; a telegraph before it acts */
	void Flash(float Seconds, float Strength = 1.0f);

	/** Squash: below 1 it ducks and widens, above 1 it stretches up; lean tips it forwards in degrees. Both ease back on their own. */
	void Squash(float Amount, float LeanDegrees = 0.0f);

	/** Nearest living enemy within the range, null if there is none */
	AVaelCreature* FindEnemy(float Range) const;

	/** Living enemies and players on their feet within the radius */
	TArray<AVaelCreature*> FindEnemies(float Radius) const;
	TArray<AVaelCharacter*> FindAllies(float Radius) const;

	/** Shoots a seed of fire or a pollen dart at the enemy */
	void Shoot(AActor* Target, bool bFire);

	/** Soft puffs of light around it, the visible pulse of an effect */
	void Puff(const FLinearColor& Color, float Spread, int32 Count, float Lift) const;

	/** Effect strength now: stronger in the giant form */
	float GetPower() const;
	EVaelPlantEffect GetActiveEffect() const;
	FLinearColor GetColor() const;

	UPROPERTY(Transient)
	TObjectPtr<UVaelPlantData> Data;

	TWeakObjectPtr<AVaelCharacter> Gardener;
	bool bShadow = false;
	bool bWild = false;

	/** World height of the ground it grows from */
	float GroundZ = 0.0f;

	/** Seconds since it was planted, seconds it still acts and whether it acts in its shadow form, seconds of the giant form left */
	float Age = 0.0f;
	float CallTimeLeft = 0.0f;
	bool bActAsShadow = false;
	float GiantTimeLeft = 0.0f;
	float GiantAmount = 0.0f;

	/** Seconds until the next pulse or shot, and whether the start of the call has been handled */
	float PulseCountdown = 0.0f;
	bool bCallStarted = false;
	bool bWindingUp = false;
	TWeakObjectPtr<AActor> WindupTarget;
	int32 ShotCount = 0;
	int32 PulseCount = 0;

	/** Part of the look that pops, squashes and leans, and its size once grown */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> GrowingPart;
	FVector GrownScale = FVector::OneVector;

	/** Instances of the material of every part */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;

	float FlashStrength = 0.0f;
	float FlashFade = 1.0f;
	float SquashAmount = 1.0f;
	float LeanAmount = 0.0f;

	bool bWithering = false;
	float WitherTime = 0.0f;
};

/** A wall of vines a Schlotwinde raises across the way to the enemies: holds back enemies, players walk through, shots fly through its gaps */
UCLASS()
class AVaelVineWall : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> Barrier;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UProceduralMeshComponent> Roots;

public:

	AVaelVineWall();

	/** Raises a wall on the ground, across the yaw, for the seconds; bigger for a giant plant */
	static AVaelVineWall* Raise(UWorld* World, const FVector& GroundLocation, float Yaw, float Lifetime, float Scale, APawn* InInstigator);

	virtual void Tick(float DeltaSeconds) override;

	/** True while the wall holds back enemies */
	bool IsHolding() const;

private:

	void Build(float Scale);
	void LetPlayersThrough();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;

	float Age = 0.0f;
	float Lifetime = 8.0f;
	float GroundZ = 0.0f;
	float PassCountdown = 0.0f;
};

/** Wurzelgriff: roots shoot out of the ground at a spot, bind the enemies there and choke them */
UCLASS()
class AVaelRootGrip : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UProceduralMeshComponent> Roots;

public:

	AVaelRootGrip();

	static AVaelRootGrip* Spawn(AVaelCharacter* Caster, const FVector& GroundLocation);

	virtual void Tick(float DeltaSeconds) override;

	/** Enemies it has held, for the smoke test */
	int32 GetHeldCount() const { return HeldCount; }

private:

	void Build();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;

	TWeakObjectPtr<AVaelCharacter> Caster;
	float Age = 0.0f;
	float GroundZ = 0.0f;
	float TickCountdown = 0.0f;
	int32 HeldCount = 0;
};

/** Dornenpeitsche: a thorny vine lashes from the hand to the target and back */
UCLASS()
class AVaelThornWhip : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UProceduralMeshComponent> Vine;

public:

	AVaelThornWhip();

	static AVaelThornWhip* Lash(AActor* Caster, const FVector& From, const FVector& To);

	virtual void Tick(float DeltaSeconds) override;

private:

	void Rebuild();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;

	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	float Age = 0.0f;
	float Seed = 0.0f;
};
