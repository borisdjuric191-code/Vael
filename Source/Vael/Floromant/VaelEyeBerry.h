// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Floromant/VaelSymbiosis.h"
#include "VaelEyeBerry.generated.h"

class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;
class UVaelPlantArmData;
class AVaelCharacterBase;

/**
 *  The Beerenwurf of the Wachbeere arm (Boris 2026-11-03): an eye berry thrown from a fingertip flies in an arc, lands and
 *  opens as a big blinking eye with a ring of turquoise light around it. Every time it looks around, a random share of the
 *  enemies in its gaze (never all) wander off confused. On the shadow path (Lauerbeere) it is dark wine with a red pupil and
 *  its stare paralyses them instead; now and then it falls on a player, as the shadow's price.
 */
UCLASS()
class AVaelEyeBerry : public AActor
{
	GENERATED_BODY()

public:

	AVaelEyeBerry();

	/** Throws a berry from the muzzle towards the target on the ground */
	static AVaelEyeBerry* Throw(APawn* InThrower, const UVaelPlantArmData* InPlant, const FVector& Muzzle, const FVector& Target, bool bInShadow);

	virtual void Tick(float DeltaSeconds) override;

	/** Grown together with another plant (VaelSymbiosis, Boris 2026-11-04): the partner touch on every look, how far it sees
	 *  and how long it stays open against a single berry, and the way a Blinzelwind eye rolls */
	void SetPair(EVaelSymbiosis InPair, const UVaelPlantArmData* InOther, float RadiusScale = 1.0f, float SecondsScale = 1.0f, const FVector& InRoll = FVector::ZeroVector);

	/** Looks it has made and enemies it has caught, for the smoke test */
	int32 GetLooks() const { return Looks; }
	int32 GetCaught() const { return Caught; }
	bool HasLanded() const { return bLanded; }

private:

	/** Confuses or stares down a share of the enemies around */
	void LookAround();

	/** What the partner adds to one caught enemy, and to every look */
	void Touch(AVaelCharacterBase* Enemy);
	void LookPair();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Berry;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Ring;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BerryLook;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RingLook;

	UPROPERTY(Transient)
	TObjectPtr<const UVaelPlantArmData> Plant;

	FVector Start = FVector::ZeroVector;
	FVector Target = FVector::ZeroVector;
	bool bShadow = false;
	bool bLanded = false;
	float Age = 0.0f;
	float OpenAge = 0.0f;
	float NextLook = 0.0f;
	float Flash = 0.0f;
	int32 Looks = 0;
	int32 Caught = 0;

	UPROPERTY(Transient)
	TObjectPtr<const UVaelPlantArmData> Other;

	EVaelSymbiosis Pair = EVaelSymbiosis::None;
	FVector Roll = FVector::ZeroVector;
	float GazeRadius = 450.0f;
	float OpenSeconds = 5.0f;
};
