// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Creatures/VaelNightCreature.h"
#include "Creatures/VaelTropfseuche.h"
#include "VaelSchleimgaenger.generated.h"

class UMaterialInstanceDynamic;

/**
 *  A Schleimgaenger: a dead body the Tropfseuche moves like Cordyceps moves an ant (Boris and team, 2026-11-21). The
 *  first one is the walking skeleton made from the Fab skeleton (Tools/Nachtwesen/schleimgaenger_*.py); Kobolde,
 *  Gildenleute and the elite dead of earlier regions follow on the same mannequin skeleton.
 */
UCLASS(BlueprintType)
class UVaelSchleimgaengerData : public UVaelNightCreatureData
{
	GENERATED_BODY()

public:

	UVaelSchleimgaengerData();
};

/**
 *  Lies as a corpse until the living come near, is hauled up and staggers at them. Its blows infest; its film shows the
 *  strain (colour, pulse, flow). When it falls, the Seuche raises it again after a few seconds, at most a few times,
 *  unless water reached the body. Values of the strains in UVaelTropfseucheSettings.
 */
UCLASS()
class AVaelSchleimgaenger : public AVaelNightCreature
{
	GENERATED_BODY()

public:

	AVaelSchleimgaenger();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual float GetOutgoingDamageMultiplier() const override;

	/** Sets the strain before it is born (otherwise it rolls one for its place) */
	void SetStrain(EVaelSeuchenStamm InStrain) { Strain = InStrain; bStrainSet = true; }

	EVaelSeuchenStamm GetStrain() const { return Strain; }

	/** How often the Seuche has raised this body already */
	int32 Rises = 0;

protected:

	virtual TSubclassOf<UVaelCreatureData> GetDefaultDataClass() const override;
	virtual void Die() override;
	virtual void OnPreyHit(AVaelCharacter* Player, const FVaelNightAttack& Attack) override;
	virtual TArray<FVaelLootEntry> GetLoot() const override;

private:

	/** The corpse gets up again as a new host, or lies still if water reached it */
	void Rise();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Film;

	EVaelSeuchenStamm Strain = EVaelSeuchenStamm::Verwesung;
	bool bStrainSet = false;

	/** Dead and waiting to rise; or washed: the film fades */
	bool bWaitingToRise = false;
	bool bWashed = false;
	float FilmShown = 1.0f;
	FTimerHandle RiseTimer;
};
