// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Creatures/VaelCreature.h"
#include "Creatures/VaelCreatureData.h"
#include "VaelNightCreature.generated.h"

class UAnimSequence;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UVaelSequenceAnimInstance;

/** One attack of a night creature: a baked animation with the moment it hits */
USTRUCT(BlueprintType)
struct FVaelNightAttack
{
	GENERATED_BODY()

	/** Animation A_<Name>_<Clip> */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack")
	FName Clip;

	/** It starts the attack when its prey is this far away (cm, from capsule to capsule) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack")
	FVector2D Range = FVector2D(0.0f, 300.0f);

	/** Seconds into the animation at which it hits */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack", meta = (ClampMin = 0))
	float HitTime = 0.5f;

	/** The hit lands this far in front of it (cm) and reaches this far around that spot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack", meta = (ClampMin = 0))
	float Reach = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack", meta = (ClampMin = 0))
	float HitRadius = 160.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack", meta = (ClampMin = 0))
	float Damage = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack")
	EVaelElement Element = EVaelElement::Earth;

	/** Seconds whoever is hit can't act (the Moderglocke's sting paralyses) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack", meta = (ClampMin = 0))
	float StunSeconds = 0.0f;

	/** Seconds until this attack is ready again */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack", meta = (ClampMin = 0))
	float Cooldown = 4.0f;

	/** It rushes this far towards its prey until it hits (cm): a dive, a pounce */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack", meta = (ClampMin = 0))
	float Lunge = 0.0f;

	/** Only against prey that can't act right now (the Glockenschlag comes down on the paralysed) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack")
	bool bOnlyStunnedPrey = false;

	/** The reach chain of the data (a stalk with its trap, a tongue's root) is flung out to the prey around the hit */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack")
	bool bReach = false;

	/** A tongue built by the engine shoots from the tongue bone to the prey around the hit */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack")
	bool bTongue = false;
};

/**
 *  Values of the creatures made in Tools/Nachtwesen (Moderglocke, Kahlschwinge, Peitschenunke, Schnappwurz): a
 *  skeletal mesh with baked animations A_<AssetName>_<Clip> in /Game/Vael/Kreaturen/Wurzelforst/<AssetName>/ and a
 *  procedural layer on top (look, breath, ears, glow).
 */
UCLASS(BlueprintType)
class UVaelNightCreatureData : public UVaelCreatureData
{
	GENERATED_BODY()

public:

	UVaelNightCreatureData();

	/** Name of its assets: SK_<AssetName>, A_<AssetName>_<Clip> */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Look")
	FString AssetName;

	/** Size against the model as made in Blender */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Look", meta = (ClampMin = 0.1))
	float BodyScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Look")
	FName IdleClip;

	/** Moving; empty for whatever is rooted */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Look")
	FName MoveClip;

	/** Speed in cm/s at which the move animation plays at its own pace */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Look", meta = (ClampMin = 1))
	float MoveClipSpeed = 150.0f;

	/** Lying in wait until prey comes within WakeRange, and coming out of it; empty: it is awake from the start */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Look")
	FName LurkClip;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Look")
	FName WakeClip;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Look")
	FName HitClip;

	/** Played instead of HitClip now and then (the Moderglocke's cap bursting spores); hits everyone within BurstRadius */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Look")
	FName BurstClip;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Behavior", meta = (ClampMin = 0, ClampMax = 1))
	float BurstChance = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Behavior", meta = (ClampMin = 0))
	float BurstRadius = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Behavior", meta = (ClampMin = 0))
	float BurstDamage = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Look")
	FName DeathClip;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Behavior")
	TArray<FVaelNightAttack> Attacks;

	/** Prey this near wakes it from lying in wait (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Behavior", meta = (ClampMin = 0))
	float WakeRange = 700.0f;

	/** Distance it likes to keep while no attack is ready (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Behavior", meta = (ClampMin = 0))
	float KeepDistance = 250.0f;

	/** Degrees per second it turns towards its prey */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Behavior", meta = (ClampMin = 0))
	float TurnRate = 160.0f;

	/** Rooted: never moves from its place (Schnappwurz) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Behavior")
	bool bRooted = false;

	/** Seconds between two attacks at the least */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Behavior", meta = (ClampMin = 0))
	float AttackPause = 1.2f;

	/** Bone that turns to look at the prey (yaw up to LookMaxYaw degrees, a little pitch) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Procedural")
	FName LookBone;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Procedural", meta = (ClampMin = 0))
	float LookMaxYaw = 50.0f;

	/** Bone that swells with the breath, by this share, every BreathPeriod seconds */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Procedural")
	FName BreathBone;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Procedural", meta = (ClampMin = 0))
	float BreathSwell = 0.03f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Procedural", meta = (ClampMin = 0.1))
	float BreathPeriod = 3.2f;

	/** Bones that twitch by themselves at uneven times (ears), up to this many degrees */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Procedural")
	TArray<FName> TwitchBones;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Procedural", meta = (ClampMin = 0))
	float TwitchAngle = 14.0f;

	/** Bones that sway softly all the time (filament ends, a tail's tip), up to this many degrees */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Procedural")
	TArray<FName> SwayBones;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Procedural", meta = (ClampMin = 0))
	float SwayAngle = 5.0f;

	/** Glow of the model's red parts (GlutStaerke of the creature master material): at rest, how far it pulses, how
	 *  long one pulse takes, and the flare while it attacks */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Procedural", meta = (ClampMin = 0))
	float GlowRest = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Procedural", meta = (ClampMin = 0))
	float GlowPulse = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Procedural", meta = (ClampMin = 0.1))
	float GlowPeriod = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Procedural", meta = (ClampMin = 0))
	float GlowAttack = 0.0f;

	/** Chain of bones flung out to the prey by attacks with bReach, root first, and the jaws on its tip */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Procedural")
	TArray<FName> ReachChain;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Procedural")
	FName ReachUpperJaw;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Procedural")
	FName ReachLowerJaw;

	/** Bone the engine-built tongue shoots from (attacks with bTongue), its thickness in cm and its colour */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Procedural")
	FName TongueBone;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Procedural", meta = (ClampMin = 0))
	float TongueThickness = 9.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Procedural")
	FLinearColor TongueColor = FLinearColor(0.55f, 0.08f, 0.12f);

	/** Path of an asset of this creature: "SK_" + AssetName, or "A_" + AssetName + "_" + Clip */
	FSoftObjectPath GetAssetPath(const FString& Asset) const;
};

/** Drifts silently towards whatever breathes; filaments paralyse, the bell comes down on the paralysed */
UCLASS(BlueprintType)
class UVaelModerglockeData : public UVaelNightCreatureData
{
	GENERATED_BODY()

public:

	UVaelModerglockeData();
};

/** Hunts in pairs from the canopy: dives, cuts with its wing edges, screams, grabs and drops */
UCLASS(BlueprintType)
class UVaelKahlschwingeData : public UVaelNightCreatureData
{
	GENERATED_BODY()

public:

	UVaelKahlschwingeData();
};

/** Sits low and still, then its tongue lashes out; it leaps after what it has struck */
UCLASS(BlueprintType)
class UVaelPeitschenunkeData : public UVaelNightCreatureData
{
	GENERATED_BODY()

public:

	UVaelPeitschenunkeData();
};

/** Rooted: its stalks wind, its traps snap at whatever comes near and reach after what gets away */
UCLASS(BlueprintType)
class UVaelSchnappwurzData : public UVaelNightCreatureData
{
	GENERATED_BODY()

public:

	UVaelSchnappwurzData();
};

/** What a night creature is doing */
UENUM(BlueprintType)
enum class EVaelNightState : uint8
{
	Lurk,
	Wake,
	Fight,
	Attack,
	Flinch
};

/**
 *  A creature that plays the baked animations of Tools/Nachtwesen and adds a procedural layer: it looks at its prey,
 *  breathes, twitches its ears, lets loose ends sway and pulses its glow. Values in UVaelNightCreatureData.
 */
UCLASS()
class AVaelNightCreature : public AVaelCreature
{
	GENERATED_BODY()

public:

	AVaelNightCreature();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool IsHiddenFromAim() const override { return State == EVaelNightState::Lurk; }
	virtual bool IsAmbusher() const override;

	UFUNCTION(BlueprintPure, Category="Night Creature")
	EVaelNightState GetNightState() const { return State; }

protected:

	virtual TSubclassOf<UVaelCreatureData> GetDefaultDataClass() const override;
	virtual void TickBehavior(float DeltaSeconds) override;
	virtual void OnDamageTaken(float Damage, const FGameplayTagContainer& DamageTags) override;
	virtual void OnBodyColorShown(const FLinearColor& Color, bool bHitFlash) override;
	virtual void Die() override;

private:

	UAnimSequence* FindClip(FName Clip);
	void Play(FName Clip, bool bLoop, float BlendTime = 0.25f, float Rate = 1.0f, bool bRestart = false);
	void EnterState(EVaelNightState NewState);
	void TurnTowards(const FVector& Location, float DeltaSeconds);
	bool StartAttack(AActor* Target, float Distance);
	void LandAttack();
	void UpdateProcedural(float DeltaSeconds);
	void UpdateTongue(float Shown, const FVector& TargetLocation);

	/** Debug (-VaelNachtwesenSchau): plays every animation in turn instead of behaving, to look at them in the game */
	void TickShow(float DeltaSeconds);
	bool bShow = false;
	int32 ShowIndex = -1;
	float ShowLeft = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UVaelSequenceAnimInstance> Animator;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UAnimSequence>> Clips;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Tongue;

	TWeakObjectPtr<AActor> Prey;

	/** Players its own sting has paralysed, and until when (world seconds) */
	TMap<TWeakObjectPtr<AActor>, float> PreyHelplessUntil;
	EVaelNightState State = EVaelNightState::Fight;
	float StateTime = 0.0f;
	float StateDuration = 0.0f;
	int32 AttackIndex = INDEX_NONE;
	bool bAttackLanded = false;
	TArray<float> AttackReadyTimes;
	float NextAttackTime = 0.0f;
	FVector LungeDirection = FVector::ForwardVector;
	bool bMoving = false;

	/** Procedural layer */
	float LookYaw = 0.0f;
	float LookPitch = 0.0f;
	TArray<float> TwitchStarts;
	TArray<float> TwitchNext;
	float Glow = 0.0f;
	float FlashGlow = 0.0f;
	float Seed = 0.0f;
};
