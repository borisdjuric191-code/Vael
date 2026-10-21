// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelLurkingEyes.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UStaticMeshComponent;

/**
 *  A pair of glowing eyes just under the surface of murky water: something lives in there.
 *  Now and then, while a player is in sight but not close, the eyes surface, watch the nearest player, blink once
 *  and sink again; a player who comes too close makes them sink at once. Placed at the water surface by
 *  Scripts/BuildMoorWasser.py, where the nixies of the Pfuhlmoor will lurk later. Pure scenery: no collision, no fight.
 */
UCLASS()
class AVaelLurkingEyes : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> LeftEye;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> RightEye;

protected:

	/** Material of the eyes; its scalar parameter "Glow" is driven from 0 (sunk) to 1 (watching) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Lurking Eyes")
	TObjectPtr<UMaterialInterface> EyeMaterial;

	/** Seconds between two appearances, picked at random in this range */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Lurking Eyes", meta = (ClampMin = 1))
	FVector2D AppearDelay = FVector2D(14.0f, 40.0f);

	/** Seconds the eyes stay up and watch */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Lurking Eyes", meta = (ClampMin = 0.5))
	float WatchTime = 3.5f;

	/** Seconds to surface or to sink */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Lurking Eyes", meta = (ClampMin = 0.05))
	float FadeTime = 0.9f;

	/** The eyes only surface while a player is within this distance (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Lurking Eyes", meta = (ClampMin = 0))
	float WatchRange = 4000.0f;

	/** A player closer than this (cm) makes them sink at once, and they don't surface for him */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Lurking Eyes", meta = (ClampMin = 0))
	float ShyRange = 800.0f;

	/** Distance between the eyes in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Lurking Eyes", meta = (ClampMin = 1))
	float EyeSpacing = 18.0f;

	/** Diameter of an eye in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Lurking Eyes", meta = (ClampMin = 1))
	float EyeSize = 9.0f;

	/** How deep the eyes lie under the surface while sunk, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Lurking Eyes", meta = (ClampMin = 0))
	float SunkDepth = 14.0f;

public:

	/** Constructor */
	AVaelLurkingEyes();

	/** Places the eyes, so the editor shows them */
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Initialization */
	virtual void BeginPlay() override;

	/** Surfaces, watches, blinks and sinks */
	virtual void Tick(float DeltaSeconds) override;

private:

	enum class EPhase : uint8 { Sunk, Rising, Watching, Sinking };

	/** Nearest player pawn and its distance, or null */
	APawn* FindNearestPlayer(float& OutDistance) const;

	/** Applies how far the eyes are up (0..1) and the blink to the eye components */
	void ApplyLook(float Up, float Blink);

	/** Picks the time until the next appearance */
	void ScheduleNext();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> EyeMaterials;

	EPhase Phase = EPhase::Sunk;
	float PhaseTime = 0.0f;
	float NextAppear = 0.0f;
	float BlinkAt = 0.0f;
};
