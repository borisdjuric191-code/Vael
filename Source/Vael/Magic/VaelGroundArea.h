// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Magic/VaelElementTypes.h"
#include "VaelGroundArea.generated.h"

class UNiagaraSystem;
class UStaticMeshComponent;

/**
 *  A round patch of an element on the ground: a fire, a puddle, mud or steam.
 *  Mages standing in or next to it draw its element from the environment.
 *  Fires left by a caster burn the caster's enemies, can be spread by wind and put out by water.
 *  Can be placed in levels by hand or spawned by formulas. Placeholder look: a flat disc in the color of the element.
 */
UCLASS()
class AVaelGroundArea : public AActor
{
	GENERATED_BODY()

private:

	/** Placeholder look */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Disc;

protected:

	/** Element of the area */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Area")
	EVaelElement Element = EVaelElement::Fire;

	/** Radius in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Area", meta = (ClampMin = 1))
	float Radius = 150.0f;

	/** Seconds until the area disappears, 0 for never */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Area", meta = (ClampMin = 0))
	float Lifetime = 0.0f;

	/** Damage per second to the enemies of whoever created the area. Areas placed in a level hurt nobody. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Area", meta = (ClampMin = 0))
	float DamagePerSecond = 0.0f;

	/** If false, water can't put this fire out, like a camp fire */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Area")
	bool bExtinguishable = true;

	/** What the area does to the enemies of its creator standing in it. Areas placed in a level affect every creature. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Area")
	EVaelGroundEffect Effect = EVaelGroundEffect::None;

	/** Own look of this area. Empty: the look of steam, mud or its element from the magic settings. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Area")
	TObjectPtr<UNiagaraSystem> VisualOverride;

public:

	/** Constructor */
	AVaelGroundArea();

	/** Applies the properties to the placeholder look */
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Initialization */
	virtual void BeginPlay() override;

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

	/** Spawns an area on the ground. The instigator decides who its damage hurts. */
	static AVaelGroundArea* SpawnArea(UWorld* World, const FVector& Location, EVaelElement InElement, float InRadius, float InLifetime, float InDamagePerSecond, APawn* InInstigator, bool bInExtinguishable = true, EVaelGroundEffect InEffect = EVaelGroundEffect::None, UNiagaraSystem* InVisual = nullptr);

	/** Finds what the areas the actor stands in do to it: whether it is blinded and how fast it can walk (1 for normal speed) */
	static void GetEffectsOn(const AActor* Victim, bool& bOutBlinded, float& OutSpeedMultiplier);

	/** True if the target stands in mist of an ally and the observer is too far away to notice them through it */
	static bool IsHiddenInMist(const AActor* Target, const AActor* Observer);

	/** Puts out every fire that touches the given circle. Returns the number of fires put out. */
	static int32 ExtinguishFires(const UWorld* World, const FVector& Location, float InRadius);

	/** True if a fire burns at the location or closer to it than the extra distance */
	static bool IsFireNear(const UWorld* World, const FVector& Location, float ExtraDistance = 0.0f);

	/** Carries the fires inside a cone further in its direction. Returns the number of new fires. */
	static int32 SpreadFires(APawn* Caster, const FVector& Origin, const FVector& Direction, float Range, float HalfAngleDegrees);

	/** True if a location is inside the area or closer to its edge than the extra distance */
	bool IsInRange(const FVector& Location, float ExtraDistance = 0.0f) const;

	EVaelElement GetElement() const { return Element; }
	float GetRadius() const { return Radius; }
	EVaelGroundEffect GetEffect() const { return Effect; }

	/** True if mages near the area can draw the element from it: its own, or fire and water from a hot spring; steam, mist and mud provide none */
	bool ProvidesElement(EVaelElement InElement) const
	{
		if (Effect == EVaelGroundEffect::HotSpring)
		{
			return InElement == EVaelElement::Fire || InElement == EVaelElement::Water;
		}

		if (Effect == EVaelGroundEffect::Blackwater)
		{
			return InElement == EVaelElement::Mark;
		}

		return Effect == EVaelGroundEffect::None && Element == InElement;
	}

	/** Condition the area lays on the enemies of its caster standing in it, None for most */
	EVaelStatus GetInflictedStatus() const;

private:

	/** Sizes and colors the disc */
	void RefreshLook();

	/** Color of the area: pale for steam, otherwise its element */
	FLinearColor GetLookColor() const;

	/** Starts the effect of the area and hides the placeholder disc, if an effect exists */
	void SpawnVisual();

	/** Seconds since the last damage step */
	float DamageStepTime = 0.0f;
};
