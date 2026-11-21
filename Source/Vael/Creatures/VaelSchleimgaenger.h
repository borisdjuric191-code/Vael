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

	/** Lumps of slime the game sticks on a body that has none in its model (the Kobolde), at this size against a 1.8 m human */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tropfseuche", meta = (ClampMin = 0))
	int32 BodyLumps = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tropfseuche", meta = (ClampMin = 0.1))
	float LumpScale = 1.0f;
};

/**
 *  The infested Kobolde (Boris and team, 2026-11-21: the main enemies of the Eisenschlund, mostly the weak ones that had
 *  to scrape the slime). Their models are the Tripo Kobolde with the mannequin bone names (Tools/Nachtwesen/kobold_*.py)
 *  and the same movements as the skeleton, baked at their own size.
 */
UCLASS(BlueprintType)
class UVaelKoboldSchleimkratzerData : public UVaelSchleimgaengerData
{
	GENERATED_BODY()

public:

	UVaelKoboldSchleimkratzerData();
};

/** The heavy guard of the hoard, half dwarf, half orc, in battered armour */
UCLASS(BlueprintType)
class UVaelKoboldHortwaechterData : public UVaelSchleimgaengerData
{
	GENERATED_BODY()

public:

	UVaelKoboldHortwaechterData();
};

/** The old noble who keeps the accounts, in a robe hung with crystals */
UCLASS(BlueprintType)
class UVaelKoboldKontoraeltesterData : public UVaelSchleimgaengerData
{
	GENERATED_BODY()

public:

	UVaelKoboldKontoraeltesterData();
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

	/** The thick slime (lumps, the web) and the heart in the ribs: material slots "Seuche" and "Herz" of the model */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Slime;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Heart;

	/** The heart lights the ribs from inside, in the colour of the strain, beating with it */
	UPROPERTY(Transient)
	TObjectPtr<class UPointLightComponent> HeartLight;

	/** Lumps stuck on a body without its own (Kobolde) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Lumps;

	EVaelSeuchenStamm Strain = EVaelSeuchenStamm::Verwesung;
	bool bStrainSet = false;

	/** Dead and waiting to rise; or washed: the film fades */
	bool bWaitingToRise = false;
	bool bWashed = false;
	float FilmShown = 1.0f;
	float HeartBeat = 0.0f;
	FTimerHandle RiseTimer;
};
