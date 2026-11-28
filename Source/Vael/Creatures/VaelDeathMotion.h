// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Magic/VaelElementTypes.h"
#include "VaelDeathMotion.generated.h"

class AVaelCreature;
class UMaterialInstanceDynamic;
class UMeshComponent;
class UMaterialInterface;

/** How a creature dies, picked from the killing blow */
UENUM(BlueprintType)
enum class EVaelDeathStyle : uint8
{
	/** Topples over (tall bodies) or rolls onto its back (flat ones) */
	Collapse,
	/** Flung away by a heavy blow, tumbles, bounces and slides */
	Fling,
	/** Falls out of the air and hits the ground */
	Fall,
	/** Charred by fire: blackens, embers glow out, crumbles to ash */
	Burn,
	/** Frozen solid and shattered into shards */
	Shatter,
	/** Lightning or storm: twitches with sparks, then collapses */
	Shock,
	/** Crushed flat by earth */
	Crush,
	/** Taken by the Nid: dissolves into violet mist */
	Dissolve
};

/** What the killing blow was, gathered by the creature in the last moments before it fell */
USTRUCT(BlueprintType)
struct FVaelKillingBlow
{
	GENERATED_BODY()

	/** Element of the last damage, and whether it had one */
	EVaelElement Element = EVaelElement::Earth;
	bool bHasElement = false;

	/** Damage of the last hit and the health before it */
	float Damage = 0.0f;
	float HealthBefore = 0.0f;

	/** Where the blow came from (the attacker), zero if unknown */
	FVector From = FVector::ZeroVector;

	/** Knockback that came with the blow, in cm/s, and its direction */
	float Knockback = 0.0f;
	FVector KnockbackDirection = FVector::ZeroVector;

	/** Conditions at death */
	bool bBurning = false;
	bool bFrozen = false;
};

/**
 * The death of a creature (Boris 2026-11-27: "alle Gegner brauchen eine Animation, in der sie sterben; manche werden weggeschleudert,
 * wenn genug Wucht ist, durch Feuer werden sie verkohlt"). Added by AVaelCreature::Die; moves the whole actor (collision and movement
 * are off on a corpse), tints all its meshes with an overlay material and throws debris. Works on every creature, whatever its body is
 * made of; creatures with a death clip of their own keep it and only get the element look, the flight and the sinking.
 * The look materials live under /Game/Vael/VFX/Tod (Scripts/CreateDeathMaterials.py); without them only the motion plays.
 */
UCLASS(ClassGroup=(Vael))
class UVaelDeathMotion : public UActorComponent
{
	GENERATED_BODY()

public:
	UVaelDeathMotion();

	/** Starts the death of the creature; returns how many seconds the corpse should stay */
	static float Start(AVaelCreature* Creature, const FVaelKillingBlow& Blow, bool bOwnClip);

	/** A knockback that arrives right after the killing hit (damage comes first) still throws the corpse */
	void AddBlow(const FVector& Direction, float Speed);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	EVaelDeathStyle GetStyle() const { return Style; }

private:
	void Choose();
	void BeginStyle();
	void TickTopple(float DeltaTime);
	void TickFlight(float DeltaTime);
	void TickLying(float DeltaTime);
	void SetOverlay(UMaterialInterface* Material);
	void SetOverlayParameter(FName Name, float Value);
	void Debris(const FVector& Where, const FLinearColor& Color, float Glow, int32 Count, float Speed, float Lift, float Gravity, float MinSize,
		float MaxSize, float Lifetime, bool bDust, bool bSoft, float RockShare = 0.0f) const;
	float TraceGround(const FVector& At) const;

	FVaelKillingBlow Blow;
	EVaelDeathStyle Style = EVaelDeathStyle::Collapse;
	bool bChosen = false;
	bool bOwnClip = false;
	bool bFlat = false;
	bool bSettled = false;
	float Age = 0.0f;
	float Duration = 4.5f;
	float HalfHeight = 60.0f;
	float Radius = 40.0f;
	float GroundZ = 0.0f;
	FVector StartLocation = FVector::ZeroVector;
	FQuat StartRotation = FQuat::Identity;
	FVector StartScale = FVector::OneVector;
	FVector FallDirection = FVector::ForwardVector;
	FVector Pivot = FVector::ZeroVector;
	float ToppleAngle = 88.0f;

	// flight
	FVector Velocity = FVector::ZeroVector;
	FVector SpinAxis = FVector::RightVector;
	float SpinSpeed = 0.0f;
	int32 Bounces = 0;
	float LieBlend = 0.0f;
	FQuat LieFrom = FQuat::Identity;
	FQuat LieTo = FQuat::Identity;
	float LieZFrom = 0.0f;

	// look
	float NextEmber = 0.0f;
	bool bShattered = false;
	bool bCrumbled = false;
	float SinkFrom = 0.0f;
	bool bSinking = false;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Overlay;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMeshComponent>> Meshes;
};
