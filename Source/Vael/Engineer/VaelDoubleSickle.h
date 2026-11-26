// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelDoubleSickle.generated.h"

class UStaticMeshComponent;
class UVaelEngineerComponent;

/**
 *  The Doppelsichel of the Aschenjaeger (Boris 25.11., Docs/Ingenieur_Aschenmark_Materialien.md 6d): two sickle blades riveted
 *  in an S at a hub, thrown spinning flat. It takes the place of the sling while the whole set is worn and always comes back.
 *    tapped   it flies straight to the aim point and back
 *    held     it swings out in a wide boomerang loop that passes through the aim point and comes home on the other side
 *  It cuts every enemy it passes once on the way out and once on the way back. Coming home it flies to his hand; caught in time
 *  (the attack pressed just as it arrives) the next throw goes out faster and harder.
 */
UCLASS()
class AVaelDoubleSickle : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Body;

public:

	AVaelDoubleSickle();

	virtual void Tick(float DeltaSeconds) override;

	/** Throws it from the hand towards the aim point; bCurve swings round the aim point instead of flying straight there.
	 *  SpeedScale and Damage carry a caught throw's bonus. */
	static AVaelDoubleSickle* Throw(UVaelEngineerComponent* Engineer, const FVector& From, const FVector& Aim, bool bCurve, float Damage, float SpeedScale, FName Coat = NAME_None);

	/** What the blades carry (26.11.): a charge formula he brushed on before the throw, or what they took up in flight - fire
	 *  (Glutladung), the acid of a Säurefass pool (Säureladung), smoke (Rauchladung); none for bare blades */
	FName GetCoat() const { return Coat; }

	/** Mines and Säurefässer it set off on this throw */
	int32 GetDevicesStruck() const { return DevicesStruck; }

	/** On its way back to the hand */
	bool IsReturning() const { return Phase == EPhase::Return; }

	/** Seconds until it reaches the hand, while it comes back (a great number before) */
	float GetSecondsToHand() const;

	/** How many enemies it cut on this throw */
	int32 GetHits() const { return Hits; }

	/** Furthest it went from the hand on this throw, cm */
	float GetFurthest() const { return Furthest; }

	/** It reached the hand */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnCaught, AVaelDoubleSickle*);
	FOnCaught OnCaught;

private:

	enum class EPhase : uint8 { Out, Loop, Return };

	TWeakObjectPtr<UVaelEngineerComponent> Engineer;
	EPhase Phase = EPhase::Out;
	FVector Target = FVector::ZeroVector;
	/** The held throw: a boomerang loop from where it left, out past one side, through the aim point and home past the other;
	 *  LoopAhead reaches the aim point, LoopSide the widest swing, LoopT runs 0..1 along it */
	FVector LoopFrom = FVector::ZeroVector;
	FVector LoopAhead = FVector::ZeroVector;
	FVector LoopSide = FVector::ZeroVector;
	float LoopT = 0.0f;
	float Speed = 1400.0f;
	float Damage = 0.0f;
	float Age = 0.0f;
	float Furthest = 0.0f;
	int32 Hits = 0;
	FVector LastLocation = FVector::ZeroVector;
	FVector ThrownFrom = FVector::ZeroVector;
	bool bCurve = false;
	FName Coat;
	int32 DevicesStruck = 0;
	float NextTrail = 0.0f;
	TSet<TWeakObjectPtr<AActor>> StruckDevices;

	/** Fire, acid or smoke on its way: the bare blades take it up */
	void TakeUpFromWorld(const FVector& Location);
	/** A mine or Säurefass the blades brush goes off */
	void StrikeDevices(const FVector& From, const FVector& To);
	void SetCoat(FName NewCoat, const FText& Shout);

	/** Who it has cut in the current phase (out, or back): each enemy at most once per way */
	TSet<TWeakObjectPtr<AActor>> CutThisWay;

	FVector GetHand() const;
	void CutAlong(const FVector& From, const FVector& To);
};
