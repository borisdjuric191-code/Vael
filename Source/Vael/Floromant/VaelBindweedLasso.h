// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Magic/VaelElementTypes.h"
#include "VaelBindweedLasso.generated.h"

class UMaterialInstanceDynamic;
class USplineMeshComponent;
class UStaticMeshComponent;
class UVaelPlantArmData;

/** What a lasso of the Schlotwinde does when it lets go */
UENUM()
enum class EVaelLassoEnd : uint8
{
	/** It simply lets go and draws back */
	Release,
	/** Small enemies are yanked to the owner and stunned; to a big one the owner flies herself */
	Yank,
	/** Würgwinde: the enemy is slammed down, hurt and stunned */
	Slam,
	/** Wirbelwinde: the enemy is flung away from where the lasso came from */
	Fling,
	/** Haftmohn: a capsule of the Funkenmohn blooms on the wrapped enemy and bursts */
	Bloom
};

/** How a lasso holds what it caught */
USTRUCT()
struct FVaelLassoSpec
{
	GENERATED_BODY()

	/** Seconds it holds by itself before it ends with OnEnd; 0 holds until it is told to end (the hand lets go) */
	float HoldSeconds = 0.0f;
	EVaelLassoEnd OnEnd = EVaelLassoEnd::Release;

	/** Damage every second while it holds, and how much that grows every second (the Würgwinde) */
	float DamagePerSecond = 0.0f;
	float DamageGrowth = 0.0f;

	/** A status it gives the held enemy again and again (burning, wet), and seconds of blindness when it catches */
	EVaelStatus Status = EVaelStatus::None;
	float BlindSeconds = 0.0f;

	/** Crawls over the ground from its start (the tower's snare) instead of flying through the air */
	bool bOverGround = false;

	/** Power of the pair or rooted hand on its damage */
	float Power = 1.0f;

	/** Colour of the glow along it and of the pollen when it catches */
	FLinearColor Color = FLinearColor(1.0f, 0.78f, 0.2f);

	/** Haftmohn: the Funkenmohn whose capsule blooms on the wrapped enemy */
	UPROPERTY()
	TObjectPtr<const UVaelPlantArmData> BloomPlant;
};

/**
 *  A lasso of the Schlotwinde (Boris 2026-10-27): a bindweed tendril with golden trumpets shoots from its start (the fingers of
 *  the hand, the foot of a tower, the blossom of a pair) to an enemy, coils round it and holds it fast. It holds until its time
 *  is up or the hand lets go, and then does what its end says: yank, slam, fling, bloom or just let go.
 *  The hand moves its start every frame (SetStart); the lasso also works on allies (Rankenrettung pulls one to her).
 */
UCLASS()
class AVaelBindweedLasso : public AActor
{
	GENERATED_BODY()

public:

	AVaelBindweedLasso();

	/** Shoots a lasso of the plant from the start at the target; returns null if it can't */
	static AVaelBindweedLasso* Throw(APawn* InOwner, const UVaelPlantArmData* InPlant, const FVector& Start, AActor* InTarget, const FVaelLassoSpec& InSpec, bool bInShadow);

	/** Up to Count living enemies of the owner within the range ahead of the start, nearest first (any direction if HalfAngle >= 180) */
	static TArray<AActor*> FindTargets(APawn* InOwner, const FVector& Start, const FVector& Direction, float Range, float HalfAngle, int32 Count);

	/** True for a creature too heavy to yank */
	static bool IsHeavy(const AActor* Actor);

	virtual void Tick(float DeltaSeconds) override;

	/** Where the lasso comes from, moved by the hand every frame */
	void SetStart(const FVector& InStart) { Start = InStart; }

	/** Lets go: does what its end says (or the given end) and draws back */
	void Finish();
	void Finish(EVaelLassoEnd End);

	/** True while it holds its target */
	bool IsHolding() const { return Phase == EPhase::Holding; }

	/** True once it is done and drawing back or gone */
	bool IsDone() const { return Phase == EPhase::Retracting; }

	AActor* GetTarget() const { return Target.Get(); }

protected:

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	enum class EPhase : uint8 { Flying, Holding, Ending, Retracting };

	void Build();
	void UpdateLook(float DeltaSeconds);
	void Catch();
	void Hold(float DeltaSeconds);
	void End(float DeltaSeconds);
	USplineMeshComponent* MakeStrand(UStaticMesh* Mesh);

	/** The point the lasso has reached, on its target or on the way */
	FVector GetTip() const;

	UPROPERTY(Transient)
	TObjectPtr<const UVaelPlantArmData> Plant;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;

	/** The tendril from the start to the tip, the coils round the target, the trumpets along it */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> Strand;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> Coils;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Trumpets;

	TWeakObjectPtr<AActor> Target;
	FVaelLassoSpec Spec;
	EVaelLassoEnd Ending = EVaelLassoEnd::Release;
	EPhase Phase = EPhase::Flying;
	bool bShadow = false;

	FVector Start = FVector::ZeroVector;
	/** How far along the way to the target the tip has come (0 to 1), and the share of the vine still out while it draws back */
	float Reach = 0.0f;
	float Out = 1.0f;
	float HeldTime = 0.0f;
	float EndTime = 0.0f;
	float Seed = 0.0f;
	/** Where the target was when it was let go (the bloom bursts there, the yank throws it from there) */
	FVector YankFrom = FVector::ZeroVector;
	/** Where the target was when it was let go, for the bloom */
	FVector LastTargetLocation = FVector::ZeroVector;
};
