// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelWatchBerry.generated.h"

class APawn;
class UCapsuleComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** One eye on the shrub mesh, measured by Tools/Kreaturenfabrik/blender/vael_augen_vermessen.py */
USTRUCT(BlueprintType)
struct FVaelWatchEye
{
	GENERATED_BODY()

	/** Middle of the eye in mesh space, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Watch Berry")
	FVector Location = FVector::ZeroVector;

	/** Direction the eye looks in mesh space */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Watch Berry")
	FVector Direction = FVector(0.0f, 0.0f, -1.0f);

	/** Blink phase 0..1, the same as in the mesh's UV map "Takt" */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Watch Berry")
	float Phase = 0.0f;
};

/**
 *  A Wachbeere (Herbarium, Wurzelforst): a tall weeping shrub with long thorny canes and hanging raspberry-like
 *  berries covered with small eyes. The eyes open and close one after another in their own rhythm (material, UV map
 *  "Takt"), and every open eye throws a faint light beam; the beams blink with their eyes (same phase, custom data 0)
 *  and slowly sweep around. A player nearby is watched: the beams swing over to him.
 *  Whoever stands in the beam of an open eye may be confused (a wild shrub spares nobody, players included): only a
 *  random share of 30-70 % is caught, creatures stagger about without attacking, a player's steps drift off course.
 *  The Bannblick (shadow form) and the Floromant's Wachbeere tower come later.
 *  Placed by Scripts/BuildWachbeeren.py.
 */
UCLASS()
class VAEL_API AVaelWatchBerry : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Shrub;

	/** Blocks players and creatures at the root bulb only; the canes and berries let them pass */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCapsuleComponent> Trunk;

	/** One light beam per eye; custom data 0 = blink phase */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInstancedStaticMeshComponent> Beams;

protected:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watch Berry|Look")
	TObjectPtr<UStaticMesh> ShrubMesh;

	/** Material with the UV map "Takt" blink and the scalar parameters "Versatz" (phase shift) and "Nacht" */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watch Berry|Look")
	TObjectPtr<UMaterialInterface> ShrubMaterial;

	/** Cone along +Z with its tip at BeamTipZ, BeamMeshLength long and BeamMeshRadius wide at its base */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watch Berry|Beams")
	TObjectPtr<UStaticMesh> BeamMesh;

	/** Additive beam material with custom data 0 (phase) and the scalar parameter "Nacht" */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watch Berry|Beams")
	TObjectPtr<UMaterialInterface> BeamMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watch Berry|Beams")
	float BeamTipZ = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watch Berry|Beams", meta = (ClampMin = 1))
	float BeamMeshLength = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watch Berry|Beams", meta = (ClampMin = 1))
	float BeamMeshRadius = 50.0f;

	/** The eyes of the mesh */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watch Berry|Beams")
	TArray<FVaelWatchEye> Eyes;

	/** Length of a beam in cm (mesh scale, grows with the shrub) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watch Berry|Beams", meta = (ClampMin = 10))
	float BeamLength = 240.0f;

	/** Half opening angle of a beam in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watch Berry|Beams", meta = (ClampMin = 1, ClampMax = 45))
	float BeamHalfAngle = 9.0f;

	/** How much the beams bend down towards the ground (0 = straight out of the eye, 1 = straight down) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watch Berry|Beams", meta = (ClampMin = 0, ClampMax = 1))
	float BeamDroop = 0.45f;

	/** How far a beam sweeps around while nobody is near, in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watch Berry|Beams", meta = (ClampMin = 0, ClampMax = 60))
	float SweepAngle = 14.0f;

	/** Shifts the blink rhythm of this shrub against the others (0..1) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watch Berry|Beams", meta = (ClampMin = 0, ClampMax = 1))
	float PhaseShift = 0.0f;

	/** A player within this distance (cm) is watched: the beams swing over to him */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watch Berry|Watch", meta = (ClampMin = 0))
	float WatchRange = 1600.0f;

	/** How far a beam turns from its eye's direction towards a watched player, at most, in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watch Berry|Watch", meta = (ClampMin = 0, ClampMax = 90))
	float MaxTurn = 55.0f;

	/** How fast the beams follow, 1/s */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watch Berry|Watch", meta = (ClampMin = 0.1))
	float FollowSpeed = 2.5f;

	/** Seconds for one open-and-close of an eye and the share of it the eye stays open: the same as BlinzelDauer
	 *  and OffenAnteil in the materials, so a beam only confuses while its eye is open */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watch Berry|Confusion", meta = (ClampMin = 0.5))
	float BlinkPeriod = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watch Berry|Confusion", meta = (ClampMin = 0, ClampMax = 1))
	float OpenShare = 0.62f;

	/** Share of those caught in a beam that get confused, picked anew in this range for every roll (Boris: never all) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watch Berry|Confusion")
	FVector2D ConfuseShare = FVector2D(0.3f, 0.7f);

	/** How long a creature and a player stay confused, in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watch Berry|Confusion", meta = (ClampMin = 0))
	float CreatureConfuseTime = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watch Berry|Confusion", meta = (ClampMin = 0))
	float PlayerConfuseTime = 3.0f;

	/** After a roll (caught or spared) a target is safe from this shrub for this many seconds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watch Berry|Confusion", meta = (ClampMin = 0))
	float ConfuseCooldown = 6.0f;

	/** Added to the beam's radius when testing whether a character stands in it, in cm (world) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watch Berry|Confusion", meta = (ClampMin = 0))
	float BodyRadius = 35.0f;

	/** Radius and half height of the blocking root bulb in cm (mesh scale) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watch Berry|Look")
	FVector2D TrunkSize = FVector2D(30.0f, 90.0f);

public:

	AVaelWatchBerry();

	virtual void OnConstruction(const FTransform& Transform) override;

	/** Sets mesh, materials and beams from the current settings (for placement scripts) */
	UFUNCTION(BlueprintCallable, Category="Vael|Watch Berry")
	void BuildShrub();

	virtual void BeginPlay() override;

	/** Night glow, sweeping and watching */
	virtual void Tick(float DeltaSeconds) override;

private:

	/** Nearest player pawn and its distance, or null */
	APawn* FindNearestPlayer(float& OutDistance) const;

	/** Where a beam starts and looks (relative to the actor) for a direction in mesh space */
	FTransform BeamTransform(int32 Index, const FVector& Direction) const;

	/** True while the eye is open (same rhythm as the materials) */
	bool IsEyeOpen(int32 Index) const;

	/** Confuses some of the characters standing in the beam of an open eye */
	void ConfuseInBeams();

	/** Each eye's resting direction (with the droop) and where its beam looks right now, in mesh space */
	TArray<FVector> RestDirections;
	TArray<FVector> Directions;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ShrubMaterialInstance;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BeamMaterialInstance;

	/** When each character may be rolled again */
	TMap<TWeakObjectPtr<AActor>, float> NextRoll;
	float NextConfuseCheck = 0.0f;

	float LastNight = -1.0f;
	float Watch = 0.0f;
	float Clock = 0.0f;
};
