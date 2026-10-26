// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VaelPlantArmComponent.generated.h"

class AVaelCharacter;
class UMaterialInstanceDynamic;
class USkeletalMeshComponent;
class USplineMeshComponent;
class UStaticMeshComponent;
class UVaelPlantArmData;

/**
 *  One hand of a Floromant turned into a plant (one component per hand).
 *  Stalks grow out of the fingers (bent along splines from the forearm past the fingertips), wind around the forearm in
 *  a half turn and grow wild from the upper arm. They follow the bones of the body every frame.
 *  While the button of the hand is held the fingers fire in turn: the firing fingertip flashes, every shot takes Saft and
 *  one shot of the plant bar, the finger stalks fan out and the winding stalks spin faster the longer it fires.
 *  A new plant makes the old one wither back into the hand first (material parameter Wuchs 1 -> 0 -> 1).
 */
UCLASS()
class UVaelPlantArmComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	/** Constructor */
	UVaelPlantArmComponent();

	/** Which hand this is; set once right after creation */
	void SetHand(bool bInLeft) { bLeft = bInLeft; }
	bool IsLeft() const { return bLeft; }

	/** Turns the hand into the plant, or back into a hand with null */
	void SetPlant(UVaelPlantArmData* InPlant);

	/** The plant the hand is or is turning into; null for a bare hand */
	UVaelPlantArmData* GetPlant() const;

	/** Holds or lets go of the button of the hand */
	void SetFiring(bool bInFiring);

	/** True while the button is held and the hand is a plant */
	bool IsFiring() const;

	/** How far the arm is stretched out to fire, 0 to 1, smoothed; drives the pose */
	float GetStance() const { return Stance; }

	/** How far a shield hand (Glimmklee) is raised before the face, 0 to 1 */
	float GetShieldStance() const { return ShieldStance; }

	/** Shots left in the plant bar, and the share of the bar */
	int32 GetBarShots() const;
	float GetBarFraction() const;

	/** Fills the plant bar, for reloading */
	void RefillBar(int32 Shots);

	/** Shots the bar lacks to be full */
	int32 GetMissingShots() const;

	/** Draws the moss pad of the forearm into the stalks: the bar gains the shots once the reload is over; no firing meanwhile */
	void StartReload(int32 Shots, float Duration);
	bool IsReloading() const { return ReloadTimeLeft > 0.0f; }

	/** Share of the reload still to go, 1 right after it started, 0 when it is done */
	float GetReloadLeft() const { return ReloadDuration > 0.0f ? ReloadTimeLeft / ReloadDuration : 0.0f; }

	/** Removes all stalks at once, for example when the player becomes a mage */
	void Clear();

	/** The plant goes into the ground as a tower: the hand is bare until the plant has grown back after the seconds */
	void PlantAway(float Seconds);

	/** True while the plant grows back after a tower; share of the wait left (1 just planted, 0 grown back) */
	bool IsRegrowing() const { return RegrowTimeLeft > 0.0f; }
	float GetRegrowLeft() const { return RegrowDuration > 0.0f ? RegrowTimeLeft / RegrowDuration : 0.0f; }

	/** The plant that grows back after a tower, changed when another one is laid on the regrowing hand */
	void SetRegrowPlant(UVaelPlantArmData* InPlant) { RegrowPlant = InPlant; }

	/** Where the hand is in world space, for the knot of crossed hands */
	FVector GetHandLocation() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	AVaelCharacter* GetCharacter() const;
	USkeletalMeshComponent* GetBody() const;

	/** Makes the stalk components for the plant now in Plant */
	void BuildStalks();

	/** Moves every stalk to the bones of the body as they are posed now */
	void UpdateStalks();

	/** Sets the material parameters every stalk shares */
	void UpdateMaterials(float DeltaTime);

	/** Fires the next finger if it is time */
	void UpdateFiring(float DeltaTime);

	/** One shot from the finger; -1 shoots out of the barrel */
	void Shoot(int32 Finger);

	USplineMeshComponent* MakeStalk(UStaticMesh* Mesh, UMaterialInstanceDynamic* Material);

	/** Name of a bone of this side, like "lowerarm" -> "lowerarm_l" */
	FName Bone(const TCHAR* Name) const;

	/** Location of a bone in the space of the body mesh */
	FVector BoneLocation(const TCHAR* Name) const;

	bool bLeft = false;

	/** The plant the stalks show now, and the one that grows next once the old ones have withered */
	UPROPERTY(Transient)
	TObjectPtr<UVaelPlantArmData> Plant;

	UPROPERTY(Transient)
	TObjectPtr<UVaelPlantArmData> NextPlant;

	bool bChanging = false;

	/** The plant that grows back after a tower, and seconds until it does */
	UPROPERTY(Transient)
	TObjectPtr<UVaelPlantArmData> RegrowPlant;

	float RegrowTimeLeft = 0.0f;
	float RegrowDuration = 0.0f;

	/** 0 = bare hand, 1 = grown plant */
	float Grow = 0.0f;

	/** Stalks of the fingers (thumb to little finger), the arm and their heads */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> FingerStalks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> FingerTips;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> WrapStalks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> WildStalks;

	/** One material per finger, so each can flash on its own, and one for the stalks of the arm */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> FingerMaterials;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ArmMaterial;

	/** Overgrowth while aiming: vines over forearm and upper arm, more wild stalks, more Keimpelz pads; first the forearm
	 *  grows (material A), then the upper arm (material B) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> ForearmVines;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> UpperArmVines;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> OvergrowthWild;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> OvergrowthPads;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> OvergrowthMaterialA;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> OvergrowthMaterialB;

	/** 0 = only the plant of the hand, 1 = the whole arm overrun */
	float Overgrowth = 0.0f;

	/** Moves the vines, wild stalks and pads of the overgrowth to the bones */
	void UpdateOvergrowth(const FVector& Shoulder, const FVector& Elbow, const FVector& Wrist);

	/** Random shape of each overgrowth stalk: where on the arm, angle, length */
	TArray<FVector> OvergrowthShapes;

	/** The Keimpelz pad on the back of the forearm; its size shows how much the pelt holds */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> KeimpelzPad;

	/** Places the pad and sizes it by the pelt; it shrinks while the stalks draw it in */
	void UpdatePad(const FVector& Elbow, const FVector& Wrist, const FVector& Out);

	// --- Funnel leaves, pump and water (Taublatt)

	/** Funnel leaves along the arm, and where each sits: share of the way from shoulder to hand, angle around the arm, size */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Leaves;

	TArray<FVector> LeafShapes;

	/** Opens the leaves at rest and lays them against the arm while aiming; the pump wave swells them in turn */
	void UpdateLeaves(const FVector& Shoulder, const FVector& Elbow, const FVector& Wrist);

	/** Where the pump wave is now on the way from shoulder (0) to hand (1); far outside while there is none */
	float GetPumpPosition() const;

	/** How much the wave swells something at the share of the way */
	float GetPumpSwell(float Share) const;

	/** Starts a pump wave at the shoulder; a looping one keeps coming while the jet runs */
	void StartPump(bool bLoop);

	float PumpStartTime = -100.0f;
	bool bPumpLoop = false;

	/** Taps and holds of a water hand: a tap pumps one Tauperle, holding turns into the jet */
	void UpdateWaterFiring(float DeltaTime);

	/** Ends the jet, letting it tear off the barrel */
	void StopJet();

	UPROPERTY(Transient)
	TObjectPtr<class AVaelSapJet> Jet;

	/** Seconds the button has been held, whether this press has pumped its shot, and when the pumped shot leaves the barrel */
	float HoldTime = 0.0f;
	bool bPumpedThisPress = false;
	float PendingShotTime = -1.0f;

	/** 0 = funnels on their stalks, 1 = nested into the barrel in front of the hand; for a rose: 0 open, 1 folded shut */
	float Barrel = 0.0f;

	// --- Rose (Taublatt): petals at the hand, open at rest, folded shut around a hole while aiming

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Petals;

	/** Every petal: ring (0 inner, 1 outer), angle around the rose, own size share */
	TArray<FVector> PetalShapes;

	/** True for a plant whose hand carries a rose */
	bool HasRose() const;

	/** Places the petals around the rose's foot and axis (space of the body mesh); returns where the shots leave */
	FVector UpdateRose(const FVector& Base, const FVector& Axis);

	/** The front of the barrel and the aim, in world space */
	FVector BarrelMuzzle = FVector::ZeroVector;
	FVector BarrelDirection = FVector::ForwardVector;

	/** Shots that come with the reload under way, its seconds left and its whole length */
	int32 ReloadShots = 0;
	float ReloadTimeLeft = 0.0f;
	float ReloadDuration = 0.0f;

	/** Random roll, angle and length of every arm stalk, fixed when they grow */
	TArray<FVector> WildShapes;

	/** Where the fingertips fired from last, in world space */
	TArray<FVector> Muzzles;
	TArray<FVector> MuzzleDirections;

	/** Flash of every fingertip, 1 right after its shot */
	TArray<float> Flashes;

	bool bFiring = false;
	float FireTime = 0.0f;
	float FireCountdown = 0.0f;
	int32 NextFinger = 0;

	/** Shots in the bar, kept as a float so it can refill slowly; and when the hand fired last */
	float BarShots = 0.0f;
	float LastShotTime = -100.0f;

	float Stance = 0.0f;
	float ShieldStance = 0.0f;
	float Fan = 0.0f;
	float SpinTurns = 0.0f;
	float SpinRate = 0.0f;
};
