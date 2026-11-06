// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Floromant/VaelSymbiosis.h"
#include "VaelPlantTower.generated.h"

class AVaelCharacter;
class UMaterialInstanceDynamic;
class USplineMeshComponent;
class UStaticMeshComponent;
class UVaelPlantArmData;
class UVaelSymbiosisKnotComponent;

/**
 *  A tower of the Floromant (L2 / R2, Boris 2026-10-26): she sticks the plant of a hand into the ground and it grows into a
 *  column of its twisted vines with the plant's crown on top (finger stalks, heads or the open rose), which fires the shot of
 *  its plant at the closest enemy, slower and a little bigger than the hand.
 *  Two towers whose feet stand closer than the symbiosis radius grow together: they lean in and twist into one trunk with a mixed
 *  blossom of both crowns on top, and from then on the pair acts as one (VaelSymbiosis), standing a while longer.
 */
UCLASS()
class AVaelPlantTower : public AActor
{
	GENERATED_BODY()

public:

	AVaelPlantTower();

	/** Plants a tower of the plant on the ground at the location for the Floromant; fuses it with a tower close by */
	static AVaelPlantTower* PlantTower(AVaelCharacter* Gardener, UVaelPlantArmData* InPlant, const FVector& GroundLocation, bool bInShadow);

	virtual void Tick(float DeltaSeconds) override;

	/** True until it starts to wither */
	bool IsStanding() const { return !bWithering; }

	/** Lets it wither and vanish */
	void Wither();

	/** Turmruf: grows into its giant form for the seconds, taller and firing faster */
	void CallUp(float Seconds);

	/** Fires at once, stronger, like when the wave of the root net reaches it */
	void FireNow(float Power);

	/** The plant of the tower */
	UVaelPlantArmData* GetPlant() const { return Plant; }

	/** Top of the column, where the crown sits */
	FVector GetCrown() const;

	/** The Floromant who planted it */
	AVaelCharacter* GetGardener() const;

private:

	/** Builds the column and the crown */
	void Build();

	/** Moves column and crown as far as the tower has grown, the crown turned a little towards its target */
	void UpdateLook(float DeltaSeconds);

	/** Joins the tower with another into a symbiosis; this one leads and acts for both */
	void FuseWith(AVaelPlantTower* Other);

	/** Fires its own shot, or the pair's, at the target */
	void Fire(AActor* Target, float Power);

	USplineMeshComponent* MakeStalk(UStaticMesh* Mesh);

	UPROPERTY(Transient)
	TObjectPtr<UVaelPlantArmData> Plant;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> Column;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> CrownStalks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> CrownPieces;

	/** The core in the crown (Harzkolben): the shots leave from its tip */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> CrownCore;

	/** The fairies of a Feenragwurz tower */
	UPROPERTY(Transient)
	TObjectPtr<class AVaelFairyEscort> Escort;

	/** Knot of the symbiosis, only on the tower that leads it */
	UPROPERTY(Transient)
	TObjectPtr<UVaelSymbiosisKnotComponent> Knot;

	/** The other tower of a symbiosis, and whether this one leads it */
	TWeakObjectPtr<AVaelPlantTower> Partner;
	bool bLeads = false;
	EVaelSymbiosis Symbiosis = EVaelSymbiosis::None;
	float FuseGrow = 0.0f;

	bool bShadow = false;
	bool bWithering = false;
	float Grow = 0.0f;
	float LifeLeft = 0.0f;
	float FireCountdown = 0.5f;
	float RoseBeatCountdown = 1.0f;

	/** Glass dome of a Glimmklee tower, or of a pair with a Glimmklee */
	TWeakObjectPtr<class AVaelGlimmDome> Dome;

	/** Direction of the crown on top of the bent column */
	FVector GetCrownAxis() const;

	/** Lets the column bend like a spring towards where it should lean (aim, sway), kicked by its shots */
	void UpdateBend(float DeltaSeconds);

	/** True for the plants that bend their column towards the target and shoot where the crown looks (like a Hydra) */
	bool AimsLikeHydra() const;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 *  The shared trunk of a symbiosis (Boris 2026-10-26): both towers lean in, their columns meet in the middle and twist round
	 *  each other into one trunk twice as thick, which carries both crowns up as one mixed blossom. False while not fused.
	 *  Center is the foot of the trunk, Bend the leader's, Fuse how far it has grown together (0 to 1).
	 */
	bool GetTrunk(FVector& OutCenter, FVector& OutBend, float& OutLength, float& OutFuse) const;

	/** Crown and its direction of the tower standing alone */
	FVector GetOwnCrown() const;
	FVector GetOwnCrownAxis() const;

	/**
	 *  The crown stalks fire in turn like the fingers of the hand (Boris 2026-10-26): aiming they draw together into a bundle, the
	 *  stalk whose turn it is glows up in a ball and draws back, its head flies off as the shot and grows again
	 */
	TArray<float> StalkCharges;
	TArray<float> StalkRegrow;
	TArray<float> StalkKicks;
	TArray<FVector> StalkTips;
	TArray<FVector> StalkDirections;
	int32 NextStalk = 0;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> ChargeOrbs;

	/** Charging, regrowing and the glow balls of the crown stalks */
	void UpdateStalkShots(float DeltaSeconds);

	/**
	 *  The mixed blossom charges before the pair acts (only on the leader): the parts of both plants light up, sparks of light
	 *  run from them into the heart, the heart swells and fires
	 */
	float PairCharge = 0.0f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> StreamOrbs;

	void UpdatePairCharge(float DeltaSeconds);

	/** Twisted pieces of each column vine up the shared trunk, made when the tower fuses */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> Braid;

	// --- Question-mark towers (Boris 2026-11-06): the plants that shoot single straight shots (Glutdistel, Taublatt, Windhalm)
	// rise, curl over at the top like a "?" and hang their crown down in front, looking ahead; they shoot flat and straight like
	// her arms

	/** True for a plant listed in UVaelFloromantSettings::HookTowerPlants */
	bool IsHookTower() const;

	/** Top of the curl, where the column turns over into the hanging neck */
	FVector GetHookApex() const;

	/** The hanging neck of each column vine, from the top of the curl down into the crown */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> HookNeck;

	/** Cached once in Build */
	bool bHook = false;

	/** Middle line of the snake (foot, curl, dip, head) and its tangents, kept from the last look update */
	FVector HookPoints[4];
	FVector HookTangents[4];

	/** A point on the middle line, T from 0 (foot) to 3 (head) */
	FVector GetHookCurve(float T) const;

	// --- Plant dragon (Boris 2026-11-06): a second Glutdistel planted at a Glutdistel snake does not stand beside it; it sinks
	// into the ground, its glow runs through the snake up to the head, and the head grows into a dragon head woven of vines,
	// horned and bushy with thistles. When it fires it opens its jaws and the ring of Glutdisteln in its throat fires round
	// and round as a drumfire

	/** The snake that carries the dragon head, and the tower that sank into it */
	bool bDragon = false;
	bool bAbsorbed = false;

	/** Growth of the dragon head (0 to 1), its jaw (0 shut to 1 wide open) and the turn of the thistle ring in the throat */
	float DragonGrow = 0.0f;
	float Jaw = 0.0f;
	float DrumAngle = 0.0f;

	/** Where the sunk tower stood: the glow runs from there into the foot of the snake */
	FVector FeedFrom = FVector::ZeroVector;

	/** Shots of the drumfire still to come, the time to the next one, which head of the ring fires next */
	int32 BurstLeft = 0;
	float BurstTimer = 0.0f;
	float BurstPower = 1.0f;
	int32 NextDrum = 0;
	TWeakObjectPtr<AActor> BurstTarget;
	TArray<float> DrumRegrow;

	/** Frame of the head on the crown of the snake (scaled with the tower), and of the lower jaw at its hinge; the vines, teeth
	 *  and root fibres of the head are laid into these once and move with them */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> HeadFrame;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> JawFrame;

	/** The vines, thorn teeth and root fibres of skull and jaw (laid once) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> DragonVines;

	/** Shaggy root tufts along the lifted neck (follow the snake every frame) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> DragonManeStalks;

	/** Earth sprays once per drumfire, when the jaws tear open */
	bool bSprayed = false;

	/** Thistle heads: first the ring in the throat, then the ones in the tangle of the skull, then those of the neck */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> DragonHeads;

	/** Glow balls: two eyes, the throat, then the sparks that run up the snake while it grows */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> DragonGlows;

	void BuildDragon();
	void UpdateDragon(float DeltaSeconds);

	/** Where the crown looks, eased towards the target */
	FVector LookDirection = FVector::ForwardVector;
	TWeakObjectPtr<AActor> Target;

	/** Bend of the column: flat direction times the angle in radians, and its speed (Boris 2026-10-26) */
	FVector Bend = FVector::ZeroVector;
	FVector BendVelocity = FVector::ZeroVector;

	/** 0 to 1 while the tower aims at a target; the bud of a Mohnmörser opening at a throw */
	float AimBlend = 0.0f;

	/** Seconds of the Turmruf left, and how far it has grown into its giant form (0 to 1) */
	float CallLeft = 0.0f;
	float Giant = 0.0f;

	/** Turmrufe in a row while it stands giant: each grows the veil of a Mondtraufe pair a little more (Boris 2026-11-04) */
	int32 CallStacks = 0;

	/** Size of the veil of a Mondtraufe pair against the hands: +20 % fused, +12 % per Turmruf, at most +50 % */
	float GetVeilScale() const;
	float BudOpen = 0.0f;

	// --- Schlotwinde tower (Boris 2026-11-06): no crown; the vines rise and their tips stop short of each other, and each vine
	// acts alone: an enemy close by is lashed, one further out is caught, held and pulled to the foot of the tower

	bool IsBindweedTower() const;
	void BuildBindweedTower();
	void UpdateBindweedTower(float DeltaSeconds, bool bActs);

	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> WindTendrils;

	/** Leaves at the tips and golden trumpets along the vines */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> WindTips;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> WindTrumpets;

	/** Per vine: seconds into its lash (negative: none), seconds until it may act again, the way it lashes, what it lashes, the
	 *  lasso it threw, whether the lash has hit */
	TArray<float> WindWhipTime;
	TArray<float> WindCooldown;
	TArray<FVector> WindWhipDir;
	TArray<float> WindWhipReach;
	TArray<TWeakObjectPtr<AActor>> WindWhipTarget;
	TArray<TWeakObjectPtr<class AVaelBindweedLasso>> WindLassos;
	TArray<bool> WindHit;
	/** Whether the enemy a vine pulled in has been tied to the Rankenbrücke yet */
	TArray<bool> WindBound;

	/** Rankenbrücke (two Schlotwinde towers, Boris 2026-11-06): the living fence spanned between them */
	TWeakObjectPtr<class AVaelVineWall> Bridge;
};
