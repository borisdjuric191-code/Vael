// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class USkeletalMeshComponent;
class UVaelSequenceAnimInstance;

/** Which bones of a creature carry its small movements, and how strong they are */
struct FVaelLifeSetup
{
	FName Pelvis;
	FName Head;
	FName Jaw;
	FName Nose;

	/** Neck bones from the body to the head: they share the look and the stalking stoop */
	TArray<FName> Neck;

	/** Spine bones from the pelvis forward: they swing sideways with the stride (never when it stands: the feet hang on them) */
	TArray<FName> Spine;

	/** Ears: each swivels and flicks by itself */
	TArray<FName> Ears;

	/** Spines or plates along the back: they breathe in a ripple and bristle when it is roused */
	TArray<FName> Crest;

	/** Petals round a head: each trembles on its own beat */
	TArray<FName> Petals;

	/** Chains of bones that sway like tendrils, root first; now and then one lifts and tastes the air */
	TArray<TArray<FName>> Tendrils;

	/** Degrees the head may turn to what it watches, and nod */
	float LookYaw = 50.0f;
	float LookPitch = 25.0f;

	/** Degrees it cocks its head, holding the tilt for a moment (a cat that wonders, a blind beast that listens) */
	float HeadTilt = 12.0f;

	/** Degrees the neck stoops while it stalks, the head lifting against it so the eyes stay on the prey */
	float Stoop = 0.0f;

	/** Degrees the spine swings sideways with each stride */
	float StrideSwing = 0.0f;

	float EarSwivel = 22.0f;
	float CrestBristle = 0.2f;
	float PetalTremble = 3.0f;
	float TendrilSway = 4.0f;

	/** Degrees the jaw drops while it pants after a run, and when it snarls */
	float Pant = 0.0f;
	float Snarl = 0.0f;
};

/**
 *  The small movements laid over a creature's baked animations: it looks at what it hunts, cocks its head, swivels
 *  and flicks its ears, pants and snarls, its nose works, its crest breathes and bristles, its spine swings with the
 *  stride, its tendrils sway. Everything is added through UVaelSequenceAnimInstance::SetBoneTurns; bones that carry
 *  planted limbs are only moved while it walks. Used by the Rankenluchs and the Kelchreisser.
 */
class FVaelLifeLayer
{
public:

	/** Reads the creature's axes off its rest pose (forward = from the pelvis to the head) */
	void Init(const USkeletalMeshComponent* Mesh, const FVaelLifeSetup& InSetup);

	/**
	 *  @param LookAt     what it watches (world), null for nothing
	 *  @param Alert      0 calm .. 1 hunting
	 *  @param Stride     where it is in its stride, 0..1 over one cycle of the gait animation; below 0 while it stands
	 *  @param Effort     0 rested .. 1 out of breath
	 */
	void Update(float DeltaSeconds, float Time, const USkeletalMeshComponent* Mesh, UVaelSequenceAnimInstance* Animator, const FVector* LookAt,
		float Alert, float Stride, float Effort);

	/** Ends every added movement (it died) */
	void Clear(UVaelSequenceAnimInstance* Animator) const;

private:

	FQuat Yaw(float Degrees) const;      // towards its right
	FQuat Nod(float Degrees) const;      // nose down
	FQuat Roll(float Degrees) const;     // right ear down

	FVaelLifeSetup Setup;
	bool bReady = false;
	FVector Forward = FVector::ForwardVector;
	FVector Right = FVector::RightVector;
	FVector HeadPlace = FVector::ZeroVector;
	float YawSign = 1.0f;
	float NodSign = 1.0f;
	float Seed = 0.0f;

	float LookYawNow = 0.0f;
	float LookPitchNow = 0.0f;
	float TiltNow = 0.0f;
	float TiltWanted = 0.0f;
	float TiltUntil = 0.0f;
	float NextTilt = 0.0f;
	float StoopNow = 0.0f;
	float SwingNow = 0.0f;
	float BristleNow = 0.0f;
	float SnarlUntil = 0.0f;
	float NextSnarl = 0.0f;
	float JawNow = 0.0f;
	float SniffUntil = 0.0f;
	float NextSniff = 0.0f;
	TArray<float> EarNow;
	TArray<float> EarWanted;
	TArray<float> EarNext;
	TArray<float> EarFlick;
	int32 TastingTendril = INDEX_NONE;
	float TasteStart = 0.0f;
	float NextTaste = 0.0f;
};
