// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelEngineerStage2Actors.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UVaelEngineerComponent;
class AVaelCharacterBase;

/**
 *  A Glutei of the Glutkrone (Schwarzglut II, Boris 26.11.: "eher Minen statt Brut, zielsuchend nach Wärme"): laid by a full
 *  brood chamber of the armour, or along the way of the rolling Panzerkugel. It lies still until a warm enemy comes near
 *  (burning ones first; wet and frozen ones it does not feel), then crawls to it and bursts into fire.
 */
UCLASS()
class AVaelGlutEgg : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UPointLightComponent> Glow;

public:

	AVaelGlutEgg();

	virtual void Tick(float DeltaSeconds) override;

	static AVaelGlutEgg* Lay(UVaelEngineerComponent* Engineer, const FVector& Location, float Damage);

	/** The enemy it crawls to, none while it lies still */
	AVaelCharacterBase* GetPrey() const;
	bool HasBurst() const { return bBurst; }
	/** Shot under the earth by the sting (26.11.): it burrows unseen, a trail of dust above it, and breaks out under its prey */
	void SetUnderground(bool bUnder);
	bool IsUnderground() const { return bUnderground; }
	/** Carried on the tip of the sting (it does nothing of its own then), and pushed into the ground: it sinks in, then digs */
	void SetCarried(bool bInCarried) { bCarried = bInCarried; }
	void PushInto(const FVector& Ground);

	/** For tests: the warmest enemy it would go for right now */
	AVaelCharacterBase* FindPrey() const;

private:

	TWeakObjectPtr<UVaelEngineerComponent> Engineer;
	TWeakObjectPtr<AVaelCharacterBase> Prey;
	float Damage = 30.0f;
	float Age = 0.0f;
	float NextLook = 0.0f;
	bool bBurst = false;
	bool bUnderground = false;
	bool bCarried = false;
	float SinkAge = -1.0f;
	FVector SinkFrom = FVector::ZeroVector;
	FVector SinkTo = FVector::ZeroVector;
	float NextDust = 0.0f;

	void Burst();
};

/**
 *  A wind devil of the Aschenjaeger II (Boris 26.11.: "Klingenteufeltornados"): a small whirl of blades he lets go from the
 *  Drehtanz, or the big Aschenteufel he releases after a long dance. It wanders along its way, cuts what it passes, pulls light
 *  enemies into its middle and takes up fire or smoke it crosses. Players are never touched.
 */
UCLASS()
class AVaelWindDevil : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> BladeA;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> BladeB;

public:

	AVaelWindDevil();

	virtual void Tick(float DeltaSeconds) override;

	/** bBig: the Aschenteufel (bigger, longer, harder); Direction: its way over the ground */
	static AVaelWindDevil* Release(UVaelEngineerComponent* Engineer, const FVector& From, const FVector& Direction, float Damage, bool bBig);

	int32 GetCuts() const { return Cuts; }
	bool IsBurning() const { return bBurning; }

private:

	TWeakObjectPtr<UVaelEngineerComponent> Engineer;
	FVector Direction = FVector::ForwardVector;
	float Damage = 10.0f;
	float Radius = 150.0f;
	float Life = 3.0f;
	float Speed = 550.0f;
	float Age = 0.0f;
	float NextCut = 0.0f;
	float NextPuff = 0.0f;
	int32 Cuts = 0;
	bool bBig = false;
	bool bBurning = false;
	bool bSmoking = false;
};
