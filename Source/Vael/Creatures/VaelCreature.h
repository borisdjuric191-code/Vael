// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/VaelCharacterBase.h"
#include "Magic/VaelElementTypes.h"
#include "VaelCreature.generated.h"

class AVaelCharacter;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UVaelCreatureData;
class AVaelCreature;

DECLARE_MULTICAST_DELEGATE_OneParam(FVaelOnCreatureDied, AVaelCreature* /*Creature*/);

/**
 *  Base of every hostile creature. Reads its values from a creature data asset,
 *  moves with simple steering towards its targets (no navigation mesh needed yet) and dies at zero health.
 *  Subclasses implement their behavior in TickBehavior, which only runs while the creature is free to act.
 */
UCLASS(Abstract)
class AVaelCreature : public AVaelCharacterBase
{
	GENERATED_BODY()

private:

	/** Placeholder look, sized to the collision capsule */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Body;

protected:

	/** Values of this kind of creature. Empty: the defaults of the data class of the creature, which are the prototype values. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature")
	TObjectPtr<UVaelCreatureData> CreatureData;

	/** Debug help: shows name, health and conditions as text above the creature once it is hurt */
	UPROPERTY(EditAnywhere, Category="Appearance")
	bool bShowStatusText = false;

	/** Seconds the body lights up after a hit */
	UPROPERTY(EditAnywhere, Category="Appearance", meta = (ClampMin = 0))
	float HitFlashDuration = 0.12f;

	/** Seconds a dead creature stays visible */
	UPROPERTY(EditAnywhere, Category="Appearance", meta = (ClampMin = 0))
	float CorpseDuration = 0.6f;

	/** Vector parameter that colors the placeholder body */
	UPROPERTY(EditAnywhere, Category="Appearance")
	FName BodyColorParameter = TEXT("Color");

public:

	/** Constructor */
	AVaelCreature();

	/** Applies the creature data to collision, attributes and movement */
	virtual void PostInitializeComponents() override;

	/** Initialization */
	virtual void BeginPlay() override;

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

	/** Spawns the creature described by the data on the ground at the given location. Returns null if it can't be spawned. */
	static AVaelCreature* SpawnCreature(UWorld* World, UVaelCreatureData* Data, const FVector& GroundLocation, const FRotator& Rotation = FRotator::ZeroRotator);

	/** Finds the ground below a location. Returns false if there is none within a few meters. */
	static bool FindGround(const UWorld* World, const FVector& Location, FVector& OutGroundLocation);

	/** Returns the values of this creature */
	const UVaelCreatureData* GetCreatureData() const { return ActiveData.Get(); }

	/** Name shown to the players */
	UFUNCTION(BlueprintPure, Category="Creature")
	FText GetCreatureName() const;

	/** True once the creature has died */
	UFUNCTION(BlueprintPure, Category="Creature")
	bool IsDead() const { return bDead; }

	//~Begin AVaelCharacterBase
	virtual bool IsDefeated() const override { return bDead; }
	virtual float GetIncomingDamageMultiplier(const FGameplayTagContainer& DamageTags) const override;
	virtual bool CanReceiveStatus(EVaelStatus Status) const override;
	virtual void ApplyKnockback(const FVector& Direction, float Speed) override;
	virtual void ApplyStun(float Duration) override;
	virtual void ApplyPull(const FVector& Location, float Speed, float DeltaSeconds) override;
	virtual void ApplyBlind(float Duration) override;
	//~End AVaelCharacterBase

	/** True while the creature is a boss in a running fight; the HUD then shows its health at the top */
	virtual bool IsBossFightActive() const { return false; }

	/** True for a stronger version sent by a corrupted region */
	UFUNCTION(BlueprintPure, Category="Creature")
	bool IsMarked() const { return bMarked; }

	/** Lets the creature stumble after a hit: it stops acting for a moment and its body tips over. Bosses barely stagger. */
	void Stagger(float Duration);

	/** Makes the creature a marked one: more health, more damage, tinted by the Mark */
	void SetMarked();

	virtual float GetOutgoingDamageMultiplier() const override;

	/** Height above the actor at which the HUD shows the health bar */
	float GetHealthBarHeight() const { return GetStatusTextHeight(); }

	/** Called when the creature dies */
	FVaelOnCreatureDied OnDied;

protected:

	/** Data class used when no data asset is set */
	virtual TSubclassOf<UVaelCreatureData> GetDefaultDataClass() const;

	/** Behavior of the creature. Only called while it is alive, not frozen and not stunned. */
	virtual void TickBehavior(float DeltaSeconds) {}

	/** Height of the status text above the actor */
	virtual float GetStatusTextHeight() const;

	/** True if the status text is shown even at full health */
	virtual bool AlwaysShowStatusText() const { return false; }

	/** Reacts to damage and dies at zero health */
	virtual void OnHealthChanged(float OldValue, float NewValue) override;

	/** Kills the creature: stops it, teaches its formula and removes it after a moment */
	virtual void Die();

	/** Returns the creature data as its subclass. Never null after PostInitializeComponents. */
	template <typename DataType>
	const DataType* GetData() const { return CastChecked<DataType>(ActiveData.Get()); }

	/** Returns the closest player who isn't down, or null if none is within the distance */
	AVaelCharacter* FindNearestPlayer(float MaxDistance, float* OutDistance = nullptr) const;

	/** Calls the function for every player in the level who isn't down */
	void ForEachActivePlayer(TFunctionRef<void(AVaelCharacter*)> Function) const;

	/** Horizontal distance to another actor */
	float GetDistanceTo2D(const AActor* Other) const;

	/** Walks or flies in a horizontal direction at the given speed in cm/s */
	void MoveInDirection(const FVector& Direction, float Speed);

	/** Walks or flies towards a location at the given speed in cm/s */
	void MoveTowards(const FVector& Location, float Speed);

	/** Turns the creature towards a location right away */
	void FaceTowards(const FVector& Location);

	/** Start of a projectile: at chest height of a player, the distance in front of the edge of the capsule */
	FVector GetAttackLocation(const FVector& Direction, float DistanceBeyondCapsule = 20.0f) const;

	/** Hurts a player with an attack of the creature. Returns false if the player can't be hurt right now, for example while dodging. */
	bool HitPlayer(AActor* Target, float Damage, EVaelElement Element);

	/** Teaches the players the formula made of the elements, if it exists and isn't known yet. Returns true if it was learned. */
	bool TeachFormula(TConstArrayView<EVaelElement> Elements, const FText& Reason) const;

	/** Keeps the creature from acting for some seconds */
	void Stun(float Duration);

	/** True while the creature is blinded by steam or a sandstorm, or has just left it: it can't see and doesn't start attacks */
	bool IsBlinded() const;

	/** Sets the color the body shows outside of hit flashes */
	void SetBodyColor(const FLinearColor& Color);

	/** Returns a random number between X and Y of the range */
	static float RandomInRange(const FVector2D& Range) { return FMath::FRandRange(Range.X, Range.Y); }

	/** Placeholder body */
	UStaticMeshComponent* GetBody() const { return Body.Get(); }

	/** Where the creature started */
	FVector HomeLocation = FVector::ZeroVector;

private:

	/** Applies the body color, or the flash color while a hit flash lasts */
	void RefreshBodyColor();

	/** Values in use: the data asset, or the defaults of the data class without one */
	UPROPERTY(Transient)
	TObjectPtr<UVaelCreatureData> ActiveData;

	/** Material of the placeholder body */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;

	/** Color of the body outside of hit flashes */
	FLinearColor BodyColor = FLinearColor::Gray;

	/** World time until which the body flashes */
	float HitFlashEndTime = 0.0f;

	/** True while the body shows the flash color */
	bool bShowingHitFlash = false;

	/** World time until which the creature can't act */
	float StunEndTime = 0.0f;

	/** True once the creature has died */
	bool bDead = false;

	/** True for a stronger version sent by a corrupted region */
	bool bMarked = false;

	/** World time at which the current stagger ends */
	float StaggerEndTime = 0.0f;

	/** Seconds the current stagger lasts */
	float StaggerLength = 0.0f;

	/** Tip of the body during a stagger: roll and pitch in degrees */
	FVector2D StaggerTipDirection = FVector2D::ZeroVector;

	/** Share of its speed at which the creature walks, lowered by mud */
	float GroundSpeedMultiplier = 1.0f;

	/** World time until which the creature is blind */
	float BlindEndTime = 0.0f;

	/** Reads what the patches on the ground the creature stands in do to it */
	void UpdateGroundEffects();
};
