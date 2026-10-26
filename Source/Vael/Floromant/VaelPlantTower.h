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
 *  Two towers whose feet stand closer than the symbiosis radius grow together: vines of both reach to a knot between them,
 *  and from then on the pair acts as one (VaelSymbiosis), standing a while longer.
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

	/** Where the crown looks, eased towards the target */
	FVector LookDirection = FVector::ForwardVector;
	TWeakObjectPtr<AActor> Target;
};
