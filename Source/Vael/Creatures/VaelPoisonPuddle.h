// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelPoisonPuddle.generated.h"

class UMaterialInstanceDynamic;
class UStaticMeshComponent;
struct FVaelSpellHit;

/** What a poison puddle is right now */
UENUM(BlueprintType)
enum class EVaelPoisonState : uint8
{
	/** Still in the air as a spat gob */
	Falling,
	/** Lies on the ground and poisons whoever stands in it */
	Liquid,
	/** Frozen by ice: harmless, nothing can wallow in it, thaws after a while */
	Frozen,
	/** Set on fire: boils off into a poison cloud that hurts everyone in it, snakes too */
	Cloud
};

/**
 *  A puddle of the Pfuhlnatter's poison. Gobs landing on or beside it make it bigger, it seeps away slowly.
 *  Players standing in it take poison damage; the snake heals by wallowing in it (more in a corrupted region, where the
 *  poison turns violet). Water washes it away, ice freezes it, fire turns it into a poison cloud that hurts everyone.
 *  Values in UVaelPfuhlnatterData. Look: overlapping soft blobs of M_Pfuhlnatter_Gift on the ground.
 */
UCLASS()
class AVaelPoisonPuddle : public AActor
{
	GENERATED_BODY()

public:

	/** Constructor */
	AVaelPoisonPuddle();

	/** Lands, seeps away, poisons, freezes and thaws, burns off */
	virtual void Tick(float DeltaSeconds) override;

	/** A gob of poison that lands at the location after the delay: splashes whoever is close, then joins a puddle lying
	 *  there or starts a new one. The instigator is the snake that spat it */
	static AVaelPoisonPuddle* Spit(UWorld* World, const FVector& Location, float Delay, AActor* InInstigator);

	/** A spell hit the ground: water washes poison away, ice freezes it, fire sets it off as a poison cloud */
	static void NotifySpellImpact(UWorld* World, const FVector& Location, float Radius, const FVaelSpellHit& Hit);

	/** The liquid puddle closest to a location within the distance (to wallow in), or null */
	static AVaelPoisonPuddle* FindNearest(const UWorld* World, const FVector& Location, float MaxDistance);

	/** A snake wallowing takes up some poison; returns how much it got */
	float Drain(float Amount);

	/** True if the location is on the puddle, or closer to its rim than the extra distance */
	bool Covers(const FVector& Location, float ExtraDistance = 0.0f) const;

	/** Middle of the puddle on the ground, and its rough radius in cm */
	FVector GetMiddle() const;
	float GetRadius() const;

	EVaelPoisonState GetPoisonState() const { return State; }

	/** Poison in a corrupted region: violet, and heals a wallowing snake more */
	bool IsCorrupted() const { return bCorrupted; }

	/** For tests: puddles lying in the world */
	static int32 CountPuddles(const UWorld* World);

private:

	/** Pours poison onto the puddle: onto the blob it lands on, or as a new blob beside it */
	void AddPoison(const FVector& GroundLocation, float Amount);

	/** Ground below a location, or false if there is none */
	static bool FindGround(const UWorld* World, const FVector& Location, FVector& OutGround);

	void Freeze();
	void Ignite();
	void Wash(const FVector& Location, float Radius);

	/** Sizes the blobs after their poison and colours them after the state */
	void RefreshLook(float DeltaSeconds);

	/** Hits everyone standing in the puddle (players) or in the cloud (everyone) */
	void HurtThoseInside(float Damage, float ExtraDistance, bool bEveryone);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Blobs;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;

	TArray<FVector> BlobCenters;
	TArray<float> BlobPoison;
	TArray<float> BlobShownRadius;

	TWeakObjectPtr<AActor> SpitBy;
	EVaelPoisonState State = EVaelPoisonState::Falling;
	float StateTime = 0.0f;
	float FallDelay = 0.0f;
	FVector LandAt = FVector::ZeroVector;
	float HurtCountdown = 0.0f;
	float Age = 0.0f;
	bool bCorrupted = false;
	FLinearColor ShownColor = FLinearColor::Black;
};
