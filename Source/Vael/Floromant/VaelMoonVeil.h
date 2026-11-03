// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "VaelMoonVeil.generated.h"

class AVaelCharacterBase;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;
class UVaelPlantArmData;

/**
 *  The veil of the Mondtraufe arm (Boris 2026-11-02): a curtain of translucent rose-pearl light with Mondtropfen hanging from
 *  its top, standing where the Floromant cast it, across her aim. Enemy shots flying into it are stopped (the light ripples
 *  and the drops flare), and what enemies strike through it is swallowed; shots of her side through it hit harder, and
 *  allies walking through it are cleansed of burning, pitch and the mark of the Nid.
 */
UCLASS()
class AVaelMoonVeil : public AActor
{
	GENERATED_BODY()

public:

	AVaelMoonVeil();

	/** Raises a veil standing on the ground point, facing along the direction */
	static AVaelMoonVeil* Raise(APawn* InOwner, const UVaelPlantArmData* InPlant, const FVector& Ground, const FVector& Facing);

	virtual void Tick(float DeltaSeconds) override;

	/** Enemy shots it has stopped, for the smoke test */
	int32 GetStoppedShots() const { return StoppedShots; }

	/** Damage to a target after the veils between it and the attacker. Called for every hit by the attribute set */
	static float ScaleDamage(AVaelCharacterBase* Target, const AActor* Attacker, float Damage, const FGameplayTagContainer& DamageTags);

private:

	/** True if the segment crosses the curtain (within its width and height) */
	bool Crosses(const FVector& From, const FVector& To) const;

	/** Side of the curtain a point is on (+1 in front, -1 behind) */
	float SideOf(const FVector& Location) const;

	/** Makes the light ripple out from the point */
	void Ripple(const FVector& Location);

	void BuildLook();
	void StopShots();
	void Cleanse();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Curtain;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Threads;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Drops;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> DropLooks;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CurtainLook;

	UPROPERTY(Transient)
	TObjectPtr<const UVaelPlantArmData> Plant;

	/** Which side of the curtain each shot was on last tick */
	TMap<TWeakObjectPtr<AActor>, float> ShotSides;

	/** Allies already cleansed by this veil, so each is cleansed once per pass */
	TMap<TWeakObjectPtr<AActor>, float> AllySides;

	TArray<float> DropLengths;
	TArray<float> DropFlares;
	int32 StoppedShots = 0;
	float Age = 0.0f;
	float Rise = 0.0f;
	float RippleStrength = 0.0f;
	FVector RippleLocal = FVector::ZeroVector;
};
