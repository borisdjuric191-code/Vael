// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Creatures/VaelCreature.h"
#include "VaelAshRoller.generated.h"

class USceneComponent;
class UStaticMeshComponent;

/** What an ash roller is doing */
UENUM(BlueprintType)
enum class EVaelRollerState : uint8
{
	/** Walks towards its target or back home */
	Roam,
	/** Curls up and shows the lane it will take */
	Curl,
	/** Rolls down the lane */
	Roll,
	/** Uncurls after a roll */
	Uncurl,
	/** Rolled into something solid: lies stunned on its back, belly up */
	OnBack
};

/**
 *  Aschenroller: armadillo of the Aschenmark, a defender. Curls up, shows its lane on the ground and rolls down it,
 *  throwing aside every player in the way. Curled up its shell takes most of a hit. A roll that ends at a rock wall
 *  or anything else solid leaves it on its back for a while, where it takes more damage.
 *  Looks: built in code from engine shapes until the creature factory has a model for it.
 */
UCLASS()
class AVaelAshRoller : public AVaelCreature
{
	GENERATED_BODY()

public:

	/** Constructor */
	AVaelAshRoller();

	/** Builds the look */
	virtual void BeginPlay() override;

	/** Moves the shell, head and feet */
	virtual void Tick(float DeltaSeconds) override;

	/** The shell protects it while curled; on its back it is soft */
	virtual float GetIncomingDamageMultiplier(const FGameplayTagContainer& DamageTags) const override;

	/** What the roller is doing */
	UFUNCTION(BlueprintPure, Category="Creature")
	EVaelRollerState GetRollerState() const { return State; }

	/** Rolls started so far, for tests */
	int32 GetRollCount() const { return RollCount; }

	/** Curls up at once to roll in the direction, for tests */
	void StartRoll(const FVector& Direction);

protected:

	virtual TSubclassOf<UVaelCreatureData> GetDefaultDataClass() const override;
	virtual void TickBehavior(float DeltaSeconds) override;
	virtual void OnBodyColorShown(const FLinearColor& Color, bool bHitFlash) override;
	virtual void Die() override;

private:

	/** Switches to a new state */
	void EnterState(EVaelRollerState NewState);

	/** Rolls on, runs over players and looks for something solid ahead */
	void TickRoll(float DeltaSeconds);

	/** True if something solid stands right ahead in the lane */
	bool IsBlockedAhead(float Distance) const;

	/** The roll ended at something solid */
	void Crash();

	/** Turns towards a location at the turn speed */
	void TurnTowards(const FVector& Location, float DeltaSeconds);

	/** Builds shell, belly, head, tail, feet and the lane on the ground */
	void BuildRig();

	/** Builds the placeholder look from engine shapes, used while the kind has no mesh of the creature factory */
	void BuildShapes();

	/** Poses the factory mesh: the roll, the turn onto its back, the legs drawn in */
	void PoseFactoryMesh(class UPoseableMeshComponent* RollerMesh, float Rock, float Shiver);

	/** Adds a part of the look to the rig */
	UStaticMeshComponent* AddRigPart(const FLinearColor& Color, const TCHAR* MeshPath = nullptr);

	/** Shows the lane on the ground while curling, hides it otherwise */
	void UpdateLane();

	EVaelRollerState State = EVaelRollerState::Roam;

	/** Seconds in the current state */
	float StateTime = 0.0f;

	/** Seconds until the next roll may start */
	float RollCooldown = 0.0f;

	/** Direction and start of the current lane */
	FVector RollDirection = FVector::ForwardVector;
	FVector LaneStart = FVector::ZeroVector;

	/** Players the current roll has run over */
	TArray<TWeakObjectPtr<AActor>> RollHits;

	int32 RollCount = 0;

	/** How far it is curled up, 0 walking to 1 a ball, and how far it has turned onto its back, 0 to 1 */
	float CurlShown = 0.0f;
	float FlipShown = 0.0f;

	/** Turn of the rolling ball in degrees, and the way walked for the swing of the feet in cm */
	float SpinDegrees = 0.0f;
	float WalkedDistance = 0.0f;

	/** Where it stood in the last frame, to measure the way it rolled and walked */
	FVector LastLocation = FVector::ZeroVector;
	bool bHasLastLocation = false;

	/** Colors shown right now */
	FLinearColor ShellColor = FLinearColor::Gray;
	bool bFlashing = false;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Rig;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Shell;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Belly;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Bands;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Head;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Snout;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Tail;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Feet;

	/** The roller as a closed ball, shown instead of the walking mesh while it curls and rolls; null without a ball model */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Ball;

	UPROPERTY(Transient)
	TObjectPtr<class UMaterialInstanceDynamic> BallMaterial;

	/** Middle and half size of the ball model at scale 1, in cm: it is turned around its middle and pressed into a true sphere */
	FVector BallMiddle = FVector::ZeroVector;
	FVector BallHalfSize = FVector(50.0f);

	/** The lane on the ground and the brighter strip that fills it until the roll starts */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Lane;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> LaneFill;
};
