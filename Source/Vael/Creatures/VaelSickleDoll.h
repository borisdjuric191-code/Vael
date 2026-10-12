// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Creatures/VaelCreature.h"
#include "VaelSickleDoll.generated.h"

class USceneComponent;
class UStaticMeshComponent;

/** What a sickle doll is doing */
UENUM(BlueprintType)
enum class EVaelDollState : uint8
{
	/** Jerks towards its target, a few steps and a pause */
	Lurk,
	/** Raises its sickles; the ring on the ground shows how far they reach */
	Windup,
	/** Whirls like a top towards its target and cuts everyone in reach */
	Whirl,
	/** Staggers after the whirl and takes more damage */
	Dazed,
	/** Water made it collapse into wet straw; it lies there for a while */
	Soaked
};

/**
 *  Sichelpuppe: charred harvest doll of the fields of the Aschenmark, woken by the Mark. Moves in jerks, whirls like a top
 *  with its sickles out and is dazed for a moment afterwards. Fire does next to nothing to it; water lets it collapse into wet straw.
 *  Looks: built in code from engine shapes until the creature factory has a model for it.
 */
UCLASS()
class AVaelSickleDoll : public AVaelCreature
{
	GENERATED_BODY()

public:

	/** Constructor */
	AVaelSickleDoll();

	/** Builds the look */
	virtual void BeginPlay() override;

	/** Moves body, arms and sickles */
	virtual void Tick(float DeltaSeconds) override;

	/** Dazed and soaked dolls take more damage */
	virtual float GetIncomingDamageMultiplier(const FGameplayTagContainer& DamageTags) const override;

	/** What the doll is doing */
	UFUNCTION(BlueprintPure, Category="Creature")
	EVaelDollState GetDollState() const { return State; }

	/** Whirls started so far, for tests */
	int32 GetWhirlCount() const { return WhirlCount; }

protected:

	virtual TSubclassOf<UVaelCreatureData> GetDefaultDataClass() const override;
	virtual void TickBehavior(float DeltaSeconds) override;
	virtual void OnDamageTaken(float Damage, const FGameplayTagContainer& DamageTags) override;
	virtual void OnBodyColorShown(const FLinearColor& Color, bool bHitFlash) override;
	virtual float GetStatusTextHeight() const override { return 150.0f; }

private:

	/** Switches to a new state */
	void EnterState(EVaelDollState NewState);

	/** Builds skirt, chest, head, arms, sickles and the ring on the ground */
	void BuildRig();

	/** Builds the placeholder look from engine shapes, used while the kind has no mesh of the creature factory */
	void BuildShapes();

	/** Finds the arm bones of the factory mesh and which way they hang */
	void SetupFactoryArms();

	/** Poses the factory mesh: spin, lean and collapse of the whole doll, and its arms */
	void PoseFactoryMesh(class UPoseableMeshComponent* DollMesh, const FRotator& Lean, const FVector& Scale, float HeadDroop, float Time);

	/** Arm bones of the factory mesh, their reference pose in component space, and the sign of the turn that lets them hang */
	FName ArmBoneNames[2];
	FTransform RestArms[2];
	float ArmDownSign[2] = { 1.0f, -1.0f };

	/** Adds a part of the look to the rig */
	UStaticMeshComponent* AddRigPart(const FLinearColor& Color, float Glow = 0.0f, const TCHAR* MeshPath = nullptr);

	EVaelDollState State = EVaelDollState::Lurk;

	/** Seconds in the current state */
	float StateTime = 0.0f;

	/** Seconds until the next whirl may start, and until the sickles cut again */
	float WhirlCooldown = 0.0f;
	float CutCountdown = 0.0f;

	int32 WhirlCount = 0;

	/** Turn of the whirling body in degrees, how far the arms are raised (0 hanging to 1 out), how far it has collapsed (0 to 1) */
	float SpinDegrees = 0.0f;
	float ArmsShown = 0.0f;
	float CollapseShown = 0.0f;

	FLinearColor StrawColor = FLinearColor::Gray;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Rig;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Skirt;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Chest;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Head;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Eyes;

	/** Arm and sickle of each side, left first */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Arms;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Sickles;

	/** Ring on the ground that shows the reach of the sickles */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> ReachDisc;
};
