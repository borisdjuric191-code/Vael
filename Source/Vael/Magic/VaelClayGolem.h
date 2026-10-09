// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelCombatStatics.h"
#include "VaelClayGolem.generated.h"

class UStaticMeshComponent;

/**
 *  A golem of clay a mage calls for some seconds, like with the Lehmgolem. Stands where it was called and fights on the side of the players:
 *  every enemy near it goes for the golem instead of the players, it stamps on enemies next to it,
 *  and when it is beaten or its time is up it falls apart into a patch of mud that slows enemies.
 *  Placeholder look: clay-colored spheres that rise out of the ground.
 */
UCLASS()
class AVaelClayGolem : public AVaelCharacterBase
{
	GENERATED_BODY()

protected:

	/** Enemies within this distance in cm go for the golem instead of the players */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Golem", meta = (ClampMin = 0))
	float LureRadius = 980.0f;

	/** Enemies within this distance in cm are hit when the golem stamps */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Golem", meta = (ClampMin = 0))
	float StampRadius = 200.0f;

	/** Seconds between two stamps, and the share of it the golem spends lifting its arms */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Golem", meta = (ClampMin = 0.1))
	float StampInterval = 1.4f;

	/** Patch of mud the golem leaves when it falls apart: radius in cm and seconds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Golem", meta = (ClampMin = 0))
	float MudRadius = 224.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Golem", meta = (ClampMin = 0))
	float MudLifetime = 4.0f;

public:

	/** Constructor */
	AVaelClayGolem();

	/** Calls a golem of a class onto the ground at a location; the caster's hit is what each stamp does. Returns null if it can't be spawned. */
	static AVaelClayGolem* Summon(TSubclassOf<AVaelClayGolem> GolemClass, APawn* Caster, const FVector& GroundLocation, const FVaelSpellHit& StampHit, float Lifetime);

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

	/** Falls apart when its time is up */
	virtual void LifeSpanExpired() override;

	/** Distance within which enemies go for the golem */
	float GetLureRadius() const { return LureRadius; }

	/** The golem fights for the players */
	virtual bool IsOnPlayerSide() const override { return true; }

	/** True once it has fallen apart */
	virtual bool IsDefeated() const override { return bCrumbled; }

	/** Clay is too heavy to be thrown or pulled */
	virtual void ApplyKnockback(const FVector& Direction, float Speed) override {}
	virtual void ApplyPull(const FVector& Location, float Speed, float DeltaSeconds) override {}

protected:

	/** Builds the look */
	virtual void BeginPlay() override;

	/** Falls apart when its health runs out */
	virtual void OnHealthChanged(float OldValue, float NewValue) override;

private:

	/** Hits every enemy next to it and throws up dust */
	void Stamp();

	/** Leaves the mud and goes */
	void Crumble();

	/** What each stamp does */
	FVaelSpellHit StampHit;

	/** Seconds since it was called and until the next stamp */
	float Age = 0.0f;
	float StampCountdown = 0.0f;

	/** True once it has fallen apart */
	bool bCrumbled = false;

	/** Body, head, arms and legs of the placeholder look, and where each sits */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> BodyParts;

	TArray<FVector> PartPlaces;
};
