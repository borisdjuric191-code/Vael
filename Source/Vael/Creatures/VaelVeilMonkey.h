// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Creatures/VaelCreature.h"
#include "VaelVeilMonkey.generated.h"

class AVaelCharacter;
class USceneComponent;
class UStaticMeshComponent;
class UVaelMaterial;

/** What a veil monkey is doing */
UENUM(BlueprintType)
enum class EVaelMonkeyState : uint8
{
	/** Stays out of sight altogether: the land is too healthy for this one to show itself */
	Dormant,
	/** Keeps something between itself and the players, watches and points */
	Hide,
	/** Leaps at a player */
	Pounce,
	/** Clings to the back of a player, slows and robs them */
	Cling,
	/** Fire or lightning blinded its big eyes; it cowers and takes more damage */
	Blinded
};

/**
 *  Schleieraffe (working title): a shy monkey with huge glowing eyes that hides behind rocks, trunks and walls and points at
 *  the player with the least life. In healthy land it only watches and flees; the more corrupted the region, the more of them
 *  come out and the wilder they get, until they leap onto players' backs, cling on, slow them and steal their materials.
 *  A dodge roll throws it off, fire and lightning blind it, and whoever kills or blinds it gets the loot back.
 *  Looks: built in code from engine shapes until the creature factory has a model for it.
 */
UCLASS()
class AVaelVeilMonkey : public AVaelCreature
{
	GENERATED_BODY()

public:

	/** Constructor */
	AVaelVeilMonkey();

	/** Builds the look */
	virtual void BeginPlay() override;

	/** Moves body, eyes and the pointing arm */
	virtual void Tick(float DeltaSeconds) override;

	/** Blinded monkeys take more damage */
	virtual float GetIncomingDamageMultiplier(const FGameplayTagContainer& DamageTags) const override;

	/** What the monkey is doing */
	UFUNCTION(BlueprintPure, Category="Creature")
	EVaelMonkeyState GetMonkeyState() const { return State; }

	/** How wild the corruption of its region makes it, 0 a watcher to 1 as wild as it gets */
	UFUNCTION(BlueprintPure, Category="Creature")
	float GetAggression() const;

	/** Materials it carries right now, and thefts so far, for tests */
	int32 GetNumStolen() const;
	int32 GetTheftCount() const { return TheftCount; }

	/** Leaps at the player at once, whatever the corruption, for tests */
	void StartPounce(AVaelCharacter* Victim);

protected:

	virtual TSubclassOf<UVaelCreatureData> GetDefaultDataClass() const override;
	virtual void TickBehavior(float DeltaSeconds) override;
	virtual void OnDamageTaken(float Damage, const FGameplayTagContainer& DamageTags) override;
	virtual void OnBodyColorShown(const FLinearColor& Color, bool bHitFlash) override;
	virtual void Die() override;
	virtual float GetStatusTextHeight() const override { return 90.0f; }

private:

	/** Something it took from a player */
	struct FVaelStolen
	{
		TWeakObjectPtr<AVaelCharacter> Owner;
		TWeakObjectPtr<const UVaelMaterial> Material;
		int32 Count = 0;
	};

	/** Switches to a new state */
	void EnterState(EVaelMonkeyState NewState);

	/** Shows or hides the whole monkey; a dormant one can't be touched */
	void SetDormant(bool bDormant);

	/** The player with the least life in sight: the one it points at */
	AVaelCharacter* FindPointedPlayer() const;

	/** The closest player who isn't down */
	AVaelCharacter* FindNearestPlayer(float& OutDistance) const;

	/** A place behind something solid, seen from the player; false if nothing fits */
	bool FindCover(const AVaelCharacter* From, float MinDistanceToPlayer, FVector& OutHidePoint) const;

	/** True if a lantern or another fire light burns close to the place: it stays out of their light */
	bool IsNearLight(const FVector& Location) const;

	/** Lets go of the player it clings to and stands beside them again */
	void LetGo();

	/** Takes a few of a random material out of the bag of the player it clings to */
	void Steal();

	/** Gives everything it carries back to its owners */
	void ReturnLoot();

	/** Builds body, hood, eyes, arm and tail */
	void BuildRig();

	EVaelMonkeyState State = EVaelMonkeyState::Hide;

	/** Seconds in the current state */
	float StateTime = 0.0f;

	/** How shy this one is, 0 to 1: the shy ones only come out in corrupted land */
	float Shyness = 0.0f;

	/** Seconds until it looks for cover again, checks whether to show itself, may leap again, steals again */
	float CoverCountdown = 0.0f;
	float PresenceCountdown = 0.0f;
	float PounceCooldown = 0.0f;
	float StealCountdown = 0.0f;

	/** Where it wants to sit, and whether that is behind something */
	FVector HidePoint = FVector::ZeroVector;
	bool bHasCover = false;

	/** The player it leaps at or clings to, and the one it points at */
	TWeakObjectPtr<AVaelCharacter> Victim;
	TWeakObjectPtr<AVaelCharacter> Pointed;

	/** True while a player looks its way: it shuts its eyes and is gone in the dark */
	bool bWatched = false;

	/** Fire lights of the level, looked up now and then */
	TArray<TWeakObjectPtr<AActor>> Lights;
	float LightCountdown = 0.0f;

	TArray<FVaelStolen> Loot;
	int32 TheftCount = 0;

	/** How far the eyes are open, 0 to 1, and seconds until the next blink */
	float EyesOpen = 1.0f;
	float BlinkCountdown = 2.0f;

	/** Progress of the hops of a factory mesh, grows with the way it runs */
	float HopPhase = 0.0f;

	FLinearColor FurColor = FLinearColor::Black;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Rig;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Torso;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Hood;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Eyes;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Arm;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Tail;
};
