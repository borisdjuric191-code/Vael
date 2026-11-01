// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Magic/VaelElementTypes.h"
#include "VaelFloatingGrimoireComponent.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class UProceduralMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** One page turning over, built anew every frame */
struct FVaelGrimoireLeaf
{
	/** 0 lying on the right, 1 lying on the left; below 0 it still waits for its turn */
	float Progress = -1.0f;
	bool bActive = false;
};

/**
 *  The grimoire of the mage: leaves the hip when a fight starts or the menu opens, floats open before the chest and goes back
 *  when things are calm again. Its pages glow in the colour of the newest queued element, turn when an element is chosen and riffle at a cast.
 *  Keeps moving while the game is paused, because the menu pauses it right when the book opens.
 *  Parts by Tools/Grimoire/vael_grimoire_bauen.py, imported by Scripts/CreateGrimoireKit.py; without them nothing shows.
 */
UCLASS()
class UVaelFloatingGrimoireComponent : public USceneComponent
{
	GENERATED_BODY()

public:

	/** Constructor */
	UVaelFloatingGrimoireComponent();

	//~Begin UActorComponent
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~End UActorComponent

	/** Whether this character carries the book at all: mages do, a Floromant doesn't */
	void SetAllowed(bool bInAllowed) { bAllowed = bInAllowed; }

	/** Holds the book open while the menu is open */
	void SetMenuOpen(bool bInMenuOpen) { bMenuOpen = bInMenuOpen; }

	/** Test help: keeps the book out regardless of the fight */
	void SetForced(bool bInForced) { bForced = bInForced; }

	/** Something is happening: the book stays out for the linger time from now on */
	void NoteCombat();

	/** A creature has noticed the player at this distance; only counts when it is close enough to matter */
	void NoteDanger(float Distance);

	/** Seconds until the book lies fully open, 0 for a character without a book */
	float GetSecondsUntilOpen() const;

	/** Middle of the book in the world */
	FVector GetBookCentre() const;

protected:

	/** Where the book floats, in the frame of the character: ahead, right, up from the middle of the body, in cm */
	UPROPERTY(EditAnywhere, Category="Grimoire")
	FVector HoverOffset = FVector(75.0f, -20.0f, 34.0f);

	/** Where it floats while both arms of the mage are before the body */
	UPROPERTY(EditAnywhere, Category="Grimoire")
	FVector AsideOffset = FVector(35.0f, -115.0f, 40.0f);

	/** Where it comes from and goes back to: the left hip */
	UPROPERTY(EditAnywhere, Category="Grimoire")
	FVector HipOffset = FVector(5.0f, -30.0f, -15.0f);

	/** Size of the book; 1 is a tome of 30 x 40 cm per page, which still reads under the far camera */
	UPROPERTY(EditAnywhere, Category="Grimoire", meta = (ClampMin = 0.1))
	float BookScale = 1.0f;

	/** Degrees the far edge of the book is raised towards the mage */
	UPROPERTY(EditAnywhere, Category="Grimoire")
	float TiltDegrees = 22.0f;

	/** Degrees each half of the open book stands up from flat */
	UPROPERTY(EditAnywhere, Category="Grimoire")
	float OpenBendDegrees = 7.0f;

	/** Seconds it takes to fly from the hip to its place */
	UPROPERTY(EditAnywhere, Category="Grimoire", meta = (ClampMin = 0.01))
	float FlyTime = 0.3f;

	/** Seconds it takes to open */
	UPROPERTY(EditAnywhere, Category="Grimoire", meta = (ClampMin = 0.01))
	float OpenTime = 0.42f;

	/** Seconds a page takes to turn */
	UPROPERTY(EditAnywhere, Category="Grimoire", meta = (ClampMin = 0.01))
	float PageTurnTime = 0.55f;

	/** Seconds the book stays out after the last sign of a fight */
	UPROPERTY(EditAnywhere, Category="Grimoire", meta = (ClampMin = 0))
	float LingerTime = 7.0f;

	/** A creature noticing the player only brings the book out within this distance, in cm */
	UPROPERTY(EditAnywhere, Category="Grimoire", meta = (ClampMin = 0))
	float DangerDistance = 1500.0f;

	/** How fast the book catches up with the mage, and turns with it */
	UPROPERTY(EditAnywhere, Category="Grimoire", meta = (ClampMin = 0))
	float FollowSpeed = 9.0f;

	UPROPERTY(EditAnywhere, Category="Grimoire", meta = (ClampMin = 0))
	float TurnSpeed = 7.0f;

	/** Glow of the pages at rest and in the moment of a cast */
	UPROPERTY(EditAnywhere, Category="Grimoire", meta = (ClampMin = 0))
	float RestGlow = 1.6f;

	UPROPERTY(EditAnywhere, Category="Grimoire", meta = (ClampMin = 0))
	float CastGlow = 9.0f;

	/** Light of the open pages on the mage, in candela, at rest */
	UPROPERTY(EditAnywhere, Category="Grimoire", meta = (ClampMin = 0))
	float LightIntensity = 9.0f;

	/** Colour of the pages while no element is queued */
	UPROPERTY(EditAnywhere, Category="Grimoire")
	FLinearColor IdleColor = FLinearColor(0.2f, 0.55f, 1.0f);

	UPROPERTY(EditAnywhere, Category="Grimoire")
	TSoftObjectPtr<UStaticMesh> FrontCoverMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Vael/Characters/Grimoire/SM_Grimoire_Vorderdeckel.SM_Grimoire_Vorderdeckel")));

	UPROPERTY(EditAnywhere, Category="Grimoire")
	TSoftObjectPtr<UStaticMesh> BackCoverMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Vael/Characters/Grimoire/SM_Grimoire_Rueckdeckel.SM_Grimoire_Rueckdeckel")));

	UPROPERTY(EditAnywhere, Category="Grimoire")
	TSoftObjectPtr<UStaticMesh> SpineMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Vael/Characters/Grimoire/SM_Grimoire_Ruecken.SM_Grimoire_Ruecken")));

	UPROPERTY(EditAnywhere, Category="Grimoire")
	TSoftObjectPtr<UStaticMesh> PagesMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Vael/Characters/Grimoire/SM_Grimoire_Block.SM_Grimoire_Block")));

	UPROPERTY(EditAnywhere, Category="Grimoire")
	TSoftObjectPtr<UMaterialInterface> PageMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Vael/Characters/Grimoire/MI_Grimoire_Seite.MI_Grimoire_Seite")));

private:

	/** Makes the parts the first time the book is needed. False while its assets don't exist. */
	bool BuildBook();

	/** Adds a mesh of the book */
	UStaticMeshComponent* AddPart(USceneComponent* Parent, UStaticMesh* Mesh);

	/** True while something wants the book out */
	bool IsWanted() const;

	/** Lets the book fly to where it belongs right now */
	void FollowMage(float DeltaSeconds, float Time);

	/** Lays cover, spine and pages out for how far the book is open */
	void PoseBook(float Openness);

	/** Starts a page turning over, after the delay in seconds; does nothing while all leaves are in the air */
	void TurnPage(float Delay = 0.0f);

	/** Advances the pages in the air and builds their meshes */
	void AnimateLeaves(float DeltaSeconds);

	/** A new element or a cleared queue */
	void OnQueueChanged();

	/** A cast has ended */
	void OnCastFinished(EVaelCastResult Result, const class UVaelFormula* Formula);

	/** Body of the book, which the scale and the offset of the closed book act on */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Body;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Spine;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> LeftCover;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> RightCover;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> LeftPages;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> RightPages;

	/** Pages in the air, one section each */
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> LeafMesh;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> PageLight;

	/** Shared by the pages, for their glow */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PageGlow;

	bool bAllowed = true;
	bool bMenuOpen = false;
	bool bForced = false;
	bool bBuilt = false;

	/** Game time of the last sign of a fight */
	float LastCombatTime = -1000.0f;

	/** Time that goes on during the pause of the menu, and its value in the last frame */
	double LastRealTime = 0.0;
	float RealTime = 0.0f;

	/** 0 at the hip, 1 at its place */
	float Fly = 0.0f;

	/** 0 closed, 1 open */
	float Open = 0.0f;

	/** Where the book is on its way after the mage, and which way it looks */
	FVector FollowLocation = FVector::ZeroVector;
	float FollowYaw = 0.0f;

	/** How far it has stepped aside, 0 to 1 */
	float Aside = 0.0f;

	/** Lean into the movement of the mage, in degrees: pitch and roll */
	FVector2D Lean = FVector2D::ZeroVector;

	/** Colour of the glow, the colour it moves to, and the flash of a cast from 1 down to 0 */
	FLinearColor GlowColor = FLinearColor::Black;
	FLinearColor GlowTarget = FLinearColor::Black;
	float Flash = 0.0f;

	/** Size of the queue at the last change, to tell a new element from a cleared queue */
	int32 LastQueueSize = 0;

	/** Seconds until a page turns by itself while the book lies open */
	float IdleTurnCountdown = 3.0f;

	TArray<FVaelGrimoireLeaf> Leaves;
};
