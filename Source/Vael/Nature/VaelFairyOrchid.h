// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Nature/VaelFairyFlight.h"
#include "VaelFairyOrchid.generated.h"

class APawn;
class UCapsuleComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** One blossom on the orchid mesh, measured by Tools/Kreaturenfabrik/blender/vael_feenragwurz_takt.py */
USTRUCT(BlueprintType)
struct FVaelOrchidBlossom
{
	GENERATED_BODY()

	/** Middle of the blossom in mesh space, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fairy Orchid")
	FVector Location = FVector::ZeroVector;

	/** Direction the blossom opens to in mesh space */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fairy Orchid")
	FVector Direction = FVector(0.0f, 1.0f, 0.0f);
};

/**
 *  A Feenragwurz (Herbarium, Wurzelforst): a tall orchid whose blossoms nod on their stalks (material, UV map "Takt"),
 *  with a few fairies around her. A fairy hovers in front of a blossom for a while, then darts over to another one,
 *  leaning into the flight with her legs trailing behind. A player nearby is watched.
 *  The fairies have no skeleton: wings and legs are bent by the material (UV map "Takt"), this actor only sets the
 *  angles in the custom data of their instances. Stich and lure abilities come later.
 *  Placed by Scripts/BuildFeenragwurz.py.
 */
UCLASS()
class VAEL_API AVaelFairyOrchid : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Plant;

	/** Blocks players and creatures at the moss mound only; stem and blossoms let them pass */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCapsuleComponent> Trunk;

	/** One instance per fairy; custom data 0/1 = upper/lower wing angle, 2 = legs trailing, 3 = legs swinging, in radians */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInstancedStaticMeshComponent> Fairies;

protected:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fairy Orchid|Look")
	TObjectPtr<UStaticMesh> PlantMesh;

	/** Material with the blossom nodding from the UV map "Takt" */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fairy Orchid|Look")
	TObjectPtr<UMaterialInterface> PlantMaterial;

	/** Fairy looking along +Y, feet at the origin */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fairy Orchid|Look")
	TObjectPtr<UStaticMesh> FairyMesh;

	/** Material with wings and legs from the custom data */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fairy Orchid|Look")
	TObjectPtr<UMaterialInterface> FairyMaterial;

	/** The blossoms of the mesh */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fairy Orchid|Look")
	TArray<FVaelOrchidBlossom> Blossoms;

	/** Radius and half height of the blocking moss mound in cm (mesh scale) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fairy Orchid|Look")
	FVector2D TrunkSize = FVector2D(34.0f, 45.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fairy Orchid|Fairies", meta = (ClampMin = 0, ClampMax = 6))
	int32 FairyCount = 2;

	/** Size of a fairy against the mesh */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fairy Orchid|Fairies", meta = (ClampMin = 0.1))
	float FairyScale = 1.0f;

	/** How far in front of a blossom a fairy hovers, in cm (mesh scale) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fairy Orchid|Fairies", meta = (ClampMin = 0))
	float HoverGap = 34.0f;

	/** Seconds a fairy stays at a blossom, least and most */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fairy Orchid|Fairies")
	FVector2D HoverTime = FVector2D(2.5f, 7.0f);

	/** Top flying speed in cm/s (mesh scale) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fairy Orchid|Fairies", meta = (ClampMin = 1))
	float FlySpeed = 320.0f;

	/** Wing beats per second while hovering and at top speed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fairy Orchid|Fairies", meta = (ClampMin = 0))
	FVector2D BeatRate = FVector2D(5.0f, 9.0f);

	/** A player within this distance (cm) is watched */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fairy Orchid|Fairies", meta = (ClampMin = 0))
	float WatchRange = 700.0f;

	/** Beyond this distance to every player (cm) the fairies rest and the clock runs once a second */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fairy Orchid|Fairies", meta = (ClampMin = 0))
	float AwakeRange = 3500.0f;

public:

	AVaelFairyOrchid();

	virtual void OnConstruction(const FTransform& Transform) override;

	/** Sets meshes and materials and puts the fairies at their first blossoms (for placement scripts) */
	UFUNCTION(BlueprintCallable, Category="Vael|Fairy Orchid")
	void BuildOrchid();

	virtual void BeginPlay() override;

	/** Flies the fairies and beats their wings */
	virtual void Tick(float DeltaSeconds) override;

private:

	/** One fairy on her way, all in the space of the actor */
	struct FFairy : VaelFairyFlight::FFairy
	{
		FVector Target = FVector::ZeroVector;

		/** Where she looks while she hovers */
		FVector Facing = FVector::ForwardVector;
		int32 Blossom = 0;
		float Seconds = 0.0f;
		bool bFlying = false;
	};

	/** Nearest player pawn and its distance, or null */
	APawn* FindNearestPlayer(float& OutDistance) const;

	/** The place in front of a blossom */
	FVector HoverPoint(int32 Index) const;

	/** A free blossom other than the one she leaves */
	int32 PickBlossom(int32 Except) const;

	void MoveFairy(FFairy& Fairy, float DeltaSeconds, const FVector* WatchedPlace);

	/** Writes one fairy into her instance */
	void ShowFairy(int32 Index, bool bLast);

	VaelFairyFlight::FSettings GetFlight() const;

	TArray<FFairy> Flock;

	float Clock = 0.0f;
};
