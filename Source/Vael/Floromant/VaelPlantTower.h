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

	/** The shared root wood of every dragon: its material (the plants colour its veins), the skull and the lower jaw */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> WoodMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> WoodSkull;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> WoodJaw;

	/** The neck tufts are thin roots of the root wood (laid thicker than plant stalks) */
	bool bWoodTufts = false;

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

	// --- Mouth organ (Boris 2026-11-19): the plant that sank into the snake builds an organ in the dragon's jaws and decides its
	// breath. Dampfdrache (Glutdistel + Taublatt): four Taublatt petals clamp a glowing Glutdistel in the throat like the rose
	// pistol; they squeeze, and a jet of steam streams out that leaves clouds of scalding, blinding steam

	/** The plant that sank in (the Glutdistel of a Glutbusch, the Taublatt of a Dampfdrache) */
	UPROPERTY(Transient)
	TObjectPtr<UVaelPlantArmData> DragonPartner;

	/** The material of the sunk tower, for the partner's pieces in the head */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> PartnerMaterial;

	/** True for the steam dragon */
	bool bSteamDragon = false;

	/** Donnerdrache (Windhalm + Glutdistel, Boris 2026-11-19): like the mage's wind and fire make the Kettenblitz, the dragon
	 *  breathes lightning that leaps from enemy to enemy; wet ones and those in steam carry it further. Windhalm blades stand
	 *  out of its cheeks like gills and sweep back from its skull like horns, crackling with arcs while it charges, and a ring
	 *  of blades round the glowing core in its throat */
	bool bThunderDragon = false;

	/** Seconds of the breath left, to the next strike, strikes left, its power, seconds to the next crackle between the blades */
	float ThunderLeft = 0.0f;
	float ThunderTimer = 0.0f;
	int32 ThunderStrikes = 0;
	float ThunderPower = 1.0f;
	float CrackleTimer = 0.0f;

	/** Tips of the gill and horn blades in the frame of the head (arcs leap between them) */
	TArray<FVector> BladeTips;

	/** One lightning strike out of the jaws: to the target, then from enemy to enemy */
	void StrikeThunder(const FVector& Mouth, float Power, float Height);

	/** Glutglasdrache (Glimmklee + Glutdistel, 2026-11-19): leaves of the sunk Glimmklee frame a burning glass in its jaws; light
	 *  gathers in it, and a thin focused sun beam of its own (Boris: not the mage's beam) sweeps out of it and leaves a trail of
	 *  burning ground */
	bool bSunDragon = false;

	/** Seconds of the beam left, whether it has been lit in this breath, its power; seconds to its next burn and next fire */
	float SunLeft = 0.0f;
	bool bSunLit = false;
	float SunPower = 1.0f;
	float SunBurnTimer = 0.0f;
	float SunFireTimer = 0.0f;

	/** Where the last fire of the beam was laid (it only burns where it strikes) */
	FVector SunFirePlace = FVector(1.0e7f);

	/** The look of the beam: a thin white-hot core, a golden halo round it, a glow where it ends, motes of light running along */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> SunCore;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> SunHalo;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> SunEnd;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> SunMotes;

	/** How far the beam is lit (0 to 1, it shoots out and fades) */
	float SunShown = 0.0f;

	/** Burns along the beam, lays the trail of fire and moves its look */
	void UpdateSunBeam(float DeltaSeconds, float Height);

	/** Sturmdrache (Windhalm + Windhalm, Boris 2026-11-19): a slim dragon of grass with wings of Windhalm blades growing out of
	 *  its neck behind the curl. The wings fire the fan of darts: they rise and draw back, sweep forward and their blades fly
	 *  off as darts and grow again; the head aims, roars and bites */
	bool bWingDragon = false;

	/** Root strands of both wings (left first): per wing two arm pieces, then the ribs */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> WingBones;

	/** Blades spanned between the ribs of both wings (left first), how far each has grown again (0 to 1), and their tips */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> WingBlades;

	TArray<float> FeatherGrow;
	TArray<FVector> FeatherTips;

	/** Seconds into the wing strike (negative: none), whether it has fired, its power; the pose: spread, lift, forward sweep */
	float WingStrike = -1.0f;
	bool bWingFired = false;
	float WingPower = 1.0f;
	float WingSpread = 0.0f;
	float WingLift = 0.0f;
	float WingSweep = 0.0f;

	/** Moves the wings and fires their darts */
	void UpdateWings(float DeltaSeconds, float Height);

	/** Flutdrache (Taublatt + Taublatt, 2026-11-19): a Taublatt dragon with a frill of Taublatt petals behind its head that
	 *  folds flat at rest and fans wide open when it charges; it breathes a water jet out of its jaws that soaks and pushes back */
	bool bFloodDragon = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> FrillPetals;

	/** How far the frill is open (0 to 1); the water jet itself runs on the beam of the sun dragon (SunLeft, UpdateSunBeam) */
	float FrillOpen = 0.0f;

	/** Where the beam leaves the glass this frame */
	FVector SunMouth = FVector::ZeroVector;

	/** The burning glass and the clover leaves round its rim */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> LensGlass;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LensMaterial;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> LensLeaves;

	/** The four petals of the steam gland, and the partner's leaves grown into the skull */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> GlandPetals;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> HeadLeaves;

	/** Roots of the sunk partner in its own colour, winding up round the neck from the foot to the head (pieces per root in turn) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> PartnerVines;

	/** The steam jet in the look of the mage's steam (Boris 2026-11-19): soft cloud cards born at the gland, flying out along
	 *  the jet, swelling and fading; per card where it is, how fast it flies, its age in seconds (negative: free) */
	UPROPERTY(Transient)
	TObjectPtr<class UVaelCloudCardsComponent> JetCards;

	TArray<FVector> JetCardPlaces;
	TArray<FVector> JetCardVelocities;
	TArray<float> JetCardAges;
	float JetSpawnDebt = 0.0f;

	/** Births, moves and fades the cards of the steam jet */
	void UpdateJetCards(float DeltaSeconds, const FVector& Mouth, const FVector& Way, float Height);

	/** Seconds of the steam jet left, to its next puff, its power, puffs so far; how tight the petals press (0 open to 1 shut) */
	float JetLeft = 0.0f;
	float JetTimer = 0.0f;
	float JetPower = 1.0f;
	int32 JetPuffs = 0;
	float Squeeze = 0.0f;

	/** True while the dragon breathes: drumfire or steam jet */
	bool IsBreathing() const { return BurstLeft > 0 || JetLeft > 0.0f || ThunderLeft > 0.0f || SunLeft > 0.0f || WingStrike >= 0.0f; }

	/** Seconds the breath lasts (the sweep of the head runs over it) */
	float GetBreathSeconds() const;

	/** One puff of the steam jet: scalds along the jet and leaves a cloud where it ends */
	void PuffSteam(const FVector& Mouth, const FVector& Way, float Height);

	// --- Living dragon neck (Boris 2026-11-19): the long neck from the ground to the head is a chain of springy joints rooted
	// at the foot; the head leads and the curl and the dip follow late, so it sways, sniffs, follows its target, draws back like
	// a cobra before the drumfire, kicks back with every shot, sweeps the fire over a fan and lunges out to bite close enemies

	/** Joints of the neck in world space and their speeds: the top of the curl, the dip and the head; the head's facing */
	FVector NeckApex = FVector::ZeroVector;
	FVector NeckDip = FVector::ZeroVector;
	FVector NeckHead = FVector::ZeroVector;
	FVector NeckApexVelocity = FVector::ZeroVector;
	FVector NeckDipVelocity = FVector::ZeroVector;
	FVector NeckHeadVelocity = FVector::ZeroVector;
	FVector HeadFacing = FVector::ForwardVector;
	bool bNeckPlaced = false;

	/** The free spine (Boris 2026-11-19): rooted fast in the ground, the whole body from the roots to the nape is a chain of joints
	 *  of fixed length that snakes after the head, and the head roams freely round its roots; the last joint is NeckHead */
	TArray<FVector> Spine;
	TArray<FVector> SpineVelocity;

	/** Time of the roaming head; it stands still while the dragon breathes */
	float RoamTime = 0.0f;

	/** Seconds since the dragon began to burst out of the earth (negative: done or never), and the next rumble of the ground */
	float EmergeTime = -1.0f;
	float EmergeRumble = 0.0f;

	/** The braided strands laid along the spine (per vine one piece per segment, then a thick core that fills the gaps), and the
	 *  roots anchoring the foot in the ground */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> SpinePieces;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> GroundRoots;

	/** A point on the spine, U from 0 at the roots to 1 at the head */
	FVector GetSpinePoint(float U) const;

	/** Lays the strands of the spine and the ground roots */
	void LaySpine(float Height, float NeckSlim);

	/** Drawn back before the drumfire (0 to 1), turn of the fan sweep in radians, seconds into the drumfire */
	float Coil = 0.0f;
	float Sweep = 0.0f;
	float BurstAge = 0.0f;

	/** The bite: seconds into it (negative: none), seconds until the next one, whom it bites, where it strikes, whether it hit */
	float BiteTime = -1.0f;
	float BiteCooldown = 0.0f;
	TWeakObjectPtr<AActor> BiteTarget;
	FVector BitePoint = FVector::ZeroVector;
	bool bBitten = false;

	/** Moves the joints of the dragon neck towards their pose of the moment */
	void UpdateDragonNeck(float DeltaSeconds, float Height);

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
