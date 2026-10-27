// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaelWhisperCaps.generated.h"

class APawn;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class USceneComponent;
class UStaticMesh;

/**
 *  A ring of Flüsterkappen (Herbarium): pale hooded mushrooms with faces, leaning in as if whispering to each other.
 *  The ring is laid out from a seed in the editor (each cap traced onto the ground or deadwood below, turned with its
 *  face to the middle, leaning in, sunk a little, with a moss cushion at its foot). In play the caps breathe (material),
 *  one after another whispers (its mouth and eyes flare, custom data 0 of the instance), a player nearby makes them
 *  whisper faster and turn their faces after him, a player very close makes them fall silent and stare. They glow by
 *  day too and much stronger at night (material parameter "Nacht" from the clock), with a soft blue light in the middle.
 *  Placed by Scripts/BuildFluesterkappen.py. Pure scenery for now: no collision, harvesting comes with the Herbarium.
 */
UCLASS()
class VAEL_API AVaelWhisperCaps : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> Root;

	/** The mushrooms; custom data 0 = whisper (0..1), 1 = random phase for the breathing */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInstancedStaticMeshComponent> Caps;

	/** Moss cushions at the feet, so the root feet grow out of the ground instead of standing on it */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInstancedStaticMeshComponent> Moss;

	/** Soft blue glow in the middle of the ring */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPointLightComponent> Glow;

protected:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Whisper Caps|Look")
	TObjectPtr<UStaticMesh> CapMesh;

	/** Material with the scalar parameter "Nacht" and per instance custom data 0 (whisper) and 1 (phase) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Whisper Caps|Look")
	TObjectPtr<UMaterialInterface> CapMaterial;

	/** Moss meshes picked at random for the cushions; none = no moss */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Whisper Caps|Look")
	TArray<TObjectPtr<UStaticMesh>> MossMeshes;

	/** Height of the cap mesh as imported, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Whisper Caps|Look", meta = (ClampMin = 1))
	float MeshHeight = 160.0f;

	/** Yaw (degrees) of the direction the face of the mesh looks, measured by vael_gesicht_vermessen.py */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Whisper Caps|Look")
	float FaceYaw = 90.0f;

	/** Changes the whole layout */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Whisper Caps|Ring")
	int32 Seed = 1;

	/** How many caps stand in the ring */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Whisper Caps|Ring", meta = (ClampMin = 1, ClampMax = 16))
	int32 Count = 7;

	/** Radius of the ring in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Whisper Caps|Ring", meta = (ClampMin = 10))
	float RingRadius = 95.0f;

	/** Height of each cap in cm, picked in this range (Boris: 1.2 to 1.8 m, so the faces read from the camera) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Whisper Caps|Ring")
	FVector2D HeightRange = FVector2D(120.0f, 180.0f);

	/** How far the caps lean into the ring, in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Whisper Caps|Ring", meta = (ClampMin = 0, ClampMax = 30))
	float LeanIn = 9.0f;

	/** Random turn away from facing the middle exactly, in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Whisper Caps|Ring", meta = (ClampMin = 0, ClampMax = 90))
	float TurnJitter = 22.0f;

	/** How far the foot sinks into the ground, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Whisper Caps|Ring", meta = (ClampMin = 0))
	float Sink = 5.0f;

	/** Seconds one cap whispers (calm, nobody near) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Whisper Caps|Whisper", meta = (ClampMin = 0.2))
	float WhisperTime = 1.6f;

	/** Seconds of quiet between two whispers (calm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Whisper Caps|Whisper", meta = (ClampMin = 0))
	FVector2D PauseRange = FVector2D(0.4f, 2.2f);

	/** A player within this distance (cm) makes them whisper faster and turn their faces after him */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Whisper Caps|Whisper", meta = (ClampMin = 0))
	float NoticeRange = 1400.0f;

	/** A player within this distance (cm) makes them fall silent and stare */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Whisper Caps|Whisper", meta = (ClampMin = 0))
	float SilenceRange = 380.0f;

	/** How far a cap turns after a player, at most, in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Whisper Caps|Whisper", meta = (ClampMin = 0, ClampMax = 90))
	float MaxTurn = 60.0f;

	/** Degrees per second the caps turn */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Whisper Caps|Whisper", meta = (ClampMin = 1))
	float TurnSpeed = 30.0f;

	/** Brightness of the light in full day and in deep night */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Whisper Caps|Light")
	FVector2D LightIntensity = FVector2D(1.5f, 9.0f);

public:

	AVaelWhisperCaps();

	/** Lays out the ring, so the editor shows it */
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Lays out the ring again from the current settings (for placement scripts) */
	UFUNCTION(BlueprintCallable, Category="Vael|Whisper Caps")
	void BuildRing();

	virtual void BeginPlay() override;

	/** Whispers, turns and glows */
	virtual void Tick(float DeltaSeconds) override;

private:

	/** Nearest player pawn and its distance, or null */
	APawn* FindNearestPlayer(float& OutDistance) const;

	/** Starts the next whisper with a cap other than the last one */
	void NextSpeaker(float Hurry);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CapMaterialInstance;

	/** Each cap as laid out (relative to the actor) and how far it is turned after a player right now */
	TArray<FTransform> BaseTransforms;
	TArray<float> Turn;
	TArray<float> Whisper;

	int32 Speaker = INDEX_NONE;
	float SpeakTime = 0.0f;
	float Pause = 0.0f;
	float Silence = 0.0f;
	float LastNight = -1.0f;
};
