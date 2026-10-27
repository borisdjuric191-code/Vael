// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Floromant/VaelSymbiosis.h"
#include "VaelPlantTower.generated.h"

class AVaelCharacter;
class UMaterialInstanceDynamic;
class USplineMeshComponent;
class UStaticMeshComponent;
class UVaelPlantArmData;
class UVaelSymbiosisKnotComponent;

/**
 *  A tower of the Floromant (L2 / R2, Boris 2026-10-26): she sticks the plant of a hand into the ground and it grows into a
 *  column of its twisted vines with the plant's crown on top (finger stalks, heads or the open rose), which fires the shot of
 *  its plant at the closest enemy, slower and a little bigger than the hand.
 *  Two towers whose feet stand closer than the symbiosis radius grow together: they lean in and twist into one trunk with a mixed
 *  blossom of both crowns on top, and from then on the pair acts as one (VaelSymbiosis), standing a while longer.
 */
UCLASS()
class AVaelPlantTower : public AActor
{
	GENERATED_BODY()

public:

	AVaelPlantTower();

	/** Plants a tower of the plant on the ground at the location for the Floromant; fuses it with a tower close by */
	static AVaelPlantTower* PlantTower(AVaelCharacter* Gardener, UVaelPlantArmData* InPlant, const FVector& GroundLocation);

	virtual void Tick(float DeltaSeconds) override;

	/** True until it starts to wither */
	bool IsStanding() const { return !bWithering; }

	/** Lets it wither and vanish */
	void Wither();

	/** Turmruf: grows into its giant form for the seconds, taller and firing faster */
	void CallUp(float Seconds);

	/** Fires at once, stronger, like when the wave of the root net reaches it */
	void FireNow(float Power);

	/** The plant of the tower */
	UVaelPlantArmData* GetPlant() const { return Plant; }

	/** Top of the column, where the crown sits */
	FVector GetCrown() const;

	/** The Floromant who planted it */
	AVaelCharacter* GetGardener() const;

private:

	/** Builds the column and the crown */
	void Build();

	/** Moves column and crown as far as the tower has grown, the crown turned a little towards its target */
	void UpdateLook(float DeltaSeconds);

	/** Joins the tower with another into a symbiosis; this one leads and acts for both */
	void FuseWith(AVaelPlantTower* Other);

	/** Fires its own shot, or the pair's, at the target */
	void Fire(AActor* Target, float Power);

	USplineMeshComponent* MakeStalk(UStaticMesh* Mesh);

	UPROPERTY(Transient)
	TObjectPtr<UVaelPlantArmData> Plant;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> Column;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> CrownStalks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> CrownPieces;

	/** Knot of the symbiosis, only on the tower that leads it */
	UPROPERTY(Transient)
	TObjectPtr<UVaelSymbiosisKnotComponent> Knot;

	/** The other tower of a symbiosis, and whether this one leads it */
	TWeakObjectPtr<AVaelPlantTower> Partner;
	bool bLeads = false;
	EVaelSymbiosis Symbiosis = EVaelSymbiosis::None;
	float FuseGrow = 0.0f;

	bool bShadow = false;
	bool bWithering = false;
	float Grow = 0.0f;
	float LifeLeft = 0.0f;
	float FireCountdown = 0.5f;
	float RoseBeatCountdown = 1.0f;

	/** Glass dome of a Glimmklee tower, or of a pair with a Glimmklee */
	TWeakObjectPtr<class AVaelGlimmDome> Dome;

	/** Direction of the crown on top of the bent column */
	FVector GetCrownAxis() const;

	/** Lets the column bend like a spring towards where it should lean (aim, sway), kicked by its shots */
	void UpdateBend(float DeltaSeconds);

	/** True for the plants that bend their column towards the target and shoot where the crown looks (like a Hydra) */
	bool AimsLikeHydra() const;

	/**
	 *  The shared trunk of a symbiosis (Boris 2026-10-26): both towers lean in, their columns meet in the middle and twist round
	 *  each other into one trunk twice as thick, which carries both crowns up as one mixed blossom. False while not fused.
	 *  Center is the foot of the trunk, Bend the leader's, Fuse how far it has grown together (0 to 1).
	 */
	bool GetTrunk(FVector& OutCenter, FVector& OutBend, float& OutLength, float& OutFuse) const;

	/** Crown and its direction of the tower standing alone */
	FVector GetOwnCrown() const;
	FVector GetOwnCrownAxis() const;

	/**
	 *  The crown stalks fire in turn like the fingers of the hand (Boris 2026-10-26): aiming they draw together into a bundle, the
	 *  stalk whose turn it is glows up in a ball and draws back, its head flies off as the shot and grows again
	 */
	TArray<float> StalkCharges;
	TArray<float> StalkRegrow;
	TArray<float> StalkKicks;
	TArray<FVector> StalkTips;
	TArray<FVector> StalkDirections;
	int32 NextStalk = 0;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> ChargeOrbs;

	/** Charging, regrowing and the glow balls of the crown stalks */
	void UpdateStalkShots(float DeltaSeconds);

	/**
	 *  The mixed blossom charges before the pair acts (only on the leader): the parts of both plants light up, sparks of light
	 *  run from them into the heart, the heart swells and fires
	 */
	float PairCharge = 0.0f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> StreamOrbs;

	void UpdatePairCharge(float DeltaSeconds);

	/** Twisted pieces of each column vine up the shared trunk, made when the tower fuses */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> Braid;

	/** Where the crown looks, eased towards the target */
	FVector LookDirection = FVector::ForwardVector;
	TWeakObjectPtr<AActor> Target;

	/** Bend of the column: flat direction times the angle in radians, and its speed (Boris 2026-10-26) */
	FVector Bend = FVector::ZeroVector;
	FVector BendVelocity = FVector::ZeroVector;

	/** 0 to 1 while the tower aims at a target; the bud of a Mohnmörser opening at a throw */
	float AimBlend = 0.0f;

	/** Seconds of the Turmruf left, and how far it has grown into its giant form (0 to 1) */
	float CallLeft = 0.0f;
	float Giant = 0.0f;
	float BudOpen = 0.0f;
};
