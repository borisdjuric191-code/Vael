// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/VaelFireComponent.h"
#include "VaelFireSpot.generated.h"

/**
 *  A fire on its own: placed in a level for campfires, torches, braziers and other fire lights, or spawned where fire
 *  lands, on the ground or on a wall. Lives for a while (or for ever), then dies down and leaves its scorch mark to cool.
 */
UCLASS()
class AVaelFireSpot : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UVaelFireComponent> Fire;

public:

	/** Constructor */
	AVaelFireSpot();

	/** Shape of a placed fire: a campfire by default */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fire")
	FVaelFireShape Shape;

	/** Seconds it burns, 0 for ever (placed fires) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fire", meta = (ClampMin = 0))
	float BurnTime = 0.0f;

	/** Lights a fire where something burning landed; a wall normal makes it a fire on the wall */
	static AVaelFireSpot* Spawn(UWorld* World, const FVector& Location, const FVaelFireShape& InShape, float InBurnTime);

	/** Lights a fire, or joins one that already burns this close: that one takes the fire in, grows and burns on (at least
	 *  for the burn time) instead of a second fire burning beside it. With bUser the caller holds it, until it calls ReleaseUser. */
	static AVaelFireSpot* SpawnOrJoin(UWorld* World, const FVector& Location, const FVaelFireShape& InShape, float InBurnTime, bool bUser);

	/** Fire landed on this patch: it grows by part of it, moves a little towards where it landed and burns on */
	void Feed(const FVector& Location, float Radius);

	/** Someone who keeps the fire burning lets go; the last one puts it out */
	void ReleaseUser();

	/** A blast of flames where something burning explodes: they bloom out wide in a moment and are gone */
	static AVaelFireSpot* SpawnBurst(UWorld* World, const FVector& Location, float Radius);

	/** Lets it die down now */
	void PutOut(float FadeOut = 0.8f);

	/** True while a mage can draw fire from it: burning, not a blast, not dying down */
	bool CanGiveFire() const { return !bBurst && !bPutOut; }

	/** A mage at Towards draws fire from it: the flames lean to them and it burns lower. A campfire grows back to its size after a while, a Brandherd drawn too low goes out */
	void GiveFire(const FVector& Towards);

	UVaelFireComponent* GetFire() const { return Fire; }

protected:

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:

	float Age = 0.0f;
	bool bPutOut = false;

	/** Who keep it burning (fire areas); it goes out when the last one lets go and its burn time is over */
	int32 Users = 0;

	/** A blast: blooms out over its burn time instead of burning steadily */
	bool bBurst = false;

	/** Size it was lit with, the size it grows towards, and where its middle moves to */
	float BaseRadius = 0.0f;
	float TargetRadius = 0.0f;
	FVector TargetLocation = FVector::ZeroVector;

	/** Seconds since it was last fed */
	float SinceFed = 0.0f;
};
