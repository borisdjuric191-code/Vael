// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelPickupOrb.generated.h"

class AVaelCharacter;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

/** What an orb gives back */
UENUM(BlueprintType)
enum class EVaelOrbKind : uint8
{
	Health,
	Mana
};

/**
 *  A glowing moth that rises from fallen creatures and gives health or mana back to the first player who comes close:
 *  it flutters over to them and dissolves. Health moths stay where they are while every player close to them is at full health.
 *  Red with round wings for health, blue with swallowtails for mana; the wings are sheets built in code that beat on hinges.
 */
UCLASS()
class AVaelPickupOrb : public AActor
{
	GENERATED_BODY()

	/** Lights the ground below the moth */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPointLightComponent> Light;

protected:

	/** What the orb gives back */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Orb")
	EVaelOrbKind Kind = EVaelOrbKind::Health;

public:

	/** Constructor */
	AVaelPickupOrb();

	/** Drops an orb on the ground below a location */
	static AVaelPickupOrb* DropOrb(UWorld* World, const FVector& Location, EVaelOrbKind InKind);

	/** Initialization */
	virtual void BeginPlay() override;

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

private:

	/** Where the moth is in its short life */
	enum class EMothState : uint8
	{
		Rising,
		Hovering,
		Flying,
		Dissolving
	};

	/** Builds wings, body and motes in the colours of the kind */
	void BuildMoth();

	/** The closest player within reach who can take what the moth gives */
	AVaelCharacter* FindTaker() const;

	/** Gives its health or mana to a player */
	void GiveTo(AVaelCharacter* Player);

	/** Beats the wings, lets the glow breathe and the motes drift */
	void Animate(float DeltaSeconds);

	/** Turns the moth along the way it just moved */
	void FaceAlong(const FVector& Moved, float DeltaSeconds);

	/** Hinge of each wing: left, right */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> Hinges;

	/** Small sparks around the moth */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Motes;

	/** Bright middle of the wings, breathes with the glow */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> WingCoreMaterial;

	/** Body along the middle of the wings */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Body;

	/** Player the moth flies to or dissolves into */
	TWeakObjectPtr<AVaelCharacter> Taker;

	EMothState State = EMothState::Rising;

	/** Where it rose from and the place it hovers around */
	FVector RiseStart = FVector::ZeroVector;
	FVector HoverHome = FVector::ZeroVector;

	/** Seconds in the current state and in its life */
	float StateTime = 0.0f;
	float Clock = 0.0f;

	/** Angle of the beat and how fast it chases its player in cm/s */
	float BeatPhase = 0.0f;
	float FlySpeed = 0.0f;

	/** Keeps the moths out of step with each other */
	float Seed = 0.0f;
};
