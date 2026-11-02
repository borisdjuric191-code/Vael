// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelCombatStatics.h"
#include "VaelClayGolem.generated.h"

class UStaticMeshComponent;

/**
 *  A golem of clay a mage calls for some seconds, like with the Lehmgolem, built like a thing out of the swamp: a hunched heap of mud
 *  with long arms that hang to the knees and a head sunk between the shoulders. It is never still: mud runs down it in streaks
 *  (its material), drops slide off, fall and splash on the ground, and new mud creeps up its legs out of the ground.
 *  It fights on the side of the players: every enemy near it goes for the golem instead of the players, it lumbers towards the
 *  nearest enemy without going far from where it was called, stamps on enemies next to it and leaves a trail of slowing mud;
 *  when it is beaten or its time is up it falls apart into a patch of mud that slows enemies.
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

	/** Walking speed in cm/s, and how far in cm it goes from where it was called */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Golem", meta = (ClampMin = 0))
	float WalkSpeed = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Golem", meta = (ClampMin = 0))
	float Leash = 800.0f;

	/** Trail of mud while it walks: one small patch every so many cm, its radius in cm and seconds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Golem", meta = (ClampMin = 10))
	float TrailSpacing = 110.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Golem", meta = (ClampMin = 0))
	float TrailRadius = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Golem", meta = (ClampMin = 0))
	float TrailLifetime = 2.5f;

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

	/** Where a lump of the body belongs: it moves with the trunk, a leg or an arm */
	enum class EPartRole : uint8 { Trunk, Head, LegLeft, LegRight, ArmLeft, ArmRight };

	/** A lump of the body: where it sits, its size in cm, how low on its limb (1 = foot or hand), how fast it churns */
	struct FBodyPart
	{
		FVector Place;
		FVector Size = FVector(50.0f);
		EPartRole Role = EPartRole::Trunk;
		float Low = 0.0f;
		FRotator Spin;
	};

	/** A drop of mud: slides down the body, falls off, splashes on the ground and sinks in; or creeps up a leg out of the ground */
	struct FDrop
	{
		enum class EState : uint8 { Slide, Fall, Splash, Rise };
		EState State = EState::Slide;
		FVector Place;
		FVector Velocity = FVector::ZeroVector;
		float Speed = 0.0f;
		float Time = 0.0f;
		float Size = 10.0f;
		/** Height in cm where a sliding drop lets go of the body */
		float DropOff = 0.0f;
	};

	/** Turns towards the nearest enemy and walks there, back home when it went too far */
	void Walk(float DeltaSeconds);

	/** Moves the lumps: rising out of the ground, lumbering gait, breathing, arms lifted for a stamp */
	void Pose(float Windup, float DeltaSeconds);

	/** Moves the drops of mud */
	void Flow(float DeltaSeconds);

	/** Starts a drop sliding from a random place on the upper body, or creeping up from the ground at a foot */
	void StartDrop(int32 Index, bool bRise);

	/** Hits every enemy next to it and throws up dust */
	void Stamp();

	/** Leaves the mud and goes */
	void Crumble();

	/** What each stamp does */
	FVaelSpellHit StampHit;

	/** Seconds since it was called and until the next stamp */
	float Age = 0.0f;
	float StampCountdown = 0.0f;

	/** Walking cycle, 0..2 pi per two steps, and how much it walks right now (0 standing, 1 full stride) */
	float Gait = 0.0f;
	float Stride = 0.0f;

	/** Where it was called, and where it last left mud on the ground */
	FVector Home = FVector::ZeroVector;
	FVector LastTrail = FVector::ZeroVector;

	/** True once it has fallen apart */
	bool bCrumbled = false;

	/** The lumps of the body and the drops, each with its mesh */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> BodyParts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> DropMeshes;

	TArray<FBodyPart> Parts;
	TArray<FDrop> Drops;
};
