// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engineer/VaelEngineerGear.h"
#include "Magic/VaelElementTypes.h"
#include "VaelEngineerComponent.generated.h"

class AVaelCharacter;
class AVaelCharacterBase;
class AVaelEngineerDevice;
class UStaticMeshComponent;
class UPoseableMeshComponent;
struct FVaelSavedPlayer;

/**
 *  What a player does as engineer (Lore-Bibel, Ingenieur; Boris 2026-11-07): a class like a hunter of Monster Hunter who builds
 *  everything himself. He finds only materials and blueprints; gear that falls for him he takes apart. He starts with the sling
 *  and endless stones; every other weapon, every special shot, every device and every piece of armour he builds at the workbench
 *  of the refugium (shots and devices also at the field box by a campfire). What he carries is limited, so he prepares before
 *  he sets out: the rest waits in his stock and fills his pouches up again at the workbench or the field box.
 *
 *  Controls (IMC_Ingenieur, AVaelPlayerController): R2 / left mouse fires while held, L2 / right mouse lays the picked device
 *  where he aims, R1 / mouse wheel turns the shot, d-pad left / right and Z / C the device, d-pad up and R the weapon,
 *  Square and X ignite every mine, L1 and Q the potion belt.
 */
UCLASS()
class UVaelEngineerComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UVaelEngineerComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// --- Building

	/** Why a piece can't be built right now; Ok if it can */
	enum class EBuildBlock : uint8 { Ok, Built, NeedsFirst, NeedsWorkbench, Materials, Full, NeedsPlan, Unknown, NeedsIdea };

	// --- Ideas (goal 16.11.): what he meets gives him ideas; an idea unlocks fixed recipes of weapons, armour and tools

	bool HasIdea(FName Id) const { return Ideas.Contains(Id); }
	int32 GetNumIdeas() const { return Ideas.Num(); }

	/** Whether the idea or the blueprint of a recipe is his (true for recipes that need neither) */
	bool KnowsRecipe(const FVaelGear& Gear) const;

	/** Takes the ideas the compendium of the group has earned by now; bTell shows the moment of the idea */
	void CheckIdeas(bool bTell);
	void LearnIdea(FName Id, bool bTell);
	void LearnAllIdeas();

	/** The last idea he had and how long ago, for the card on the HUD */
	FName GetLastIdea() const { return LastIdea; }
	float GetSecondsSinceIdea() const { return LastIdea.IsNone() ? 1000.0f : Now() - LastIdeaTime; }

	/** The last piece he built and when (the forge throws sparks) */
	FName GetLastBuilt() const { return LastBuilt; }
	float GetSecondsSinceBuilt() const { return LastBuilt.IsNone() ? 1000.0f : Now() - LastBuiltTime; }

	// --- Formulas of shots (goal 14.11.): found by mixing at the workbench or the field box, then built like any recipe

	bool IsFormulaKnown(FName Id) const { return Formulas.Contains(Id); }
	int32 GetNumKnownFormulas() const { return Formulas.Num(); }
	void LearnAllFormulas();

	/** Mixes these materials: if they make a formula he learns it (if new) and builds one batch from the bag; if not, nothing is
	 *  lost and OutHint says why. OutFormula is the formula found. */
	EVaelMixResult Mix(const TArray<FName>& MixParts, FText& OutHint, FName& OutFormula);

	/** Whether he tried exactly these parts before without result */
	bool WasTried(const TArray<FName>& MixParts) const { return Tried.Contains(VaelEngineerGear::GetMixKey(MixParts)); }

	// --- Pockets of the Werkrolle (goal 14.11.: like the arm slots of the Floromant): 4 kinds of shots with the leather
	// pouches, 6 with the armour pouches, 8 with the silk pouches. Only shots in a pocket can be loaded in a fight; the rest
	// waits in the backpack. Which kinds sit in the pockets he changes only at the workbench or a campfire.

	int32 GetPocketCount() const { return 4 + 2 * GetPouchTier(); }
	const TArray<FName>& GetPocketKinds() const { return Pockets; }
	bool IsInPocket(FName Id) const { return Pockets.Contains(Id); }
	bool HasFreePocket() const { return Pockets.Num() < GetPocketCount(); }

	/** Workbench and field box: a kind of shot gets a pocket of its own (if one is free) and fills it from the backpack */
	bool PutInPocket(FName Id);

	/** Workbench and field box: a kind of shot goes from its pocket back into the backpack and frees the pocket */
	int32 EmptyPocket(FName Id);

	/** Blueprints he has found (a recipe that needs one can't be built without it) */
	bool HasPlan(FName Id) const { return Plans.Contains(Id); }

	/** Finds a blueprint he doesn't know yet with this chance; returns the recipe or None */
	FName FindPlan(float Chance);
	void LearnAllPlans();

	/** The branch of a weapon (its tier-1 upgrade), None for the plain weapon */
	FName GetBranch(FName WeaponId) const;

	/** Test: takes up a weapon with a branch already built */
	void ForceBranch(FName WeaponId, FName BranchId) { WeaponBranches.Add(WeaponId, BranchId); Weapon = WeaponId; ShownWeapon = NAME_None; }
	EBuildBlock GetBuildBlock(const FVaelGear& Gear, bool bAtWorkbench) const;

	/** Builds a piece from the bag: a weapon or armour is his for good (armour is worn at once if its place is free), shots and
	 *  devices go into the pouches up to what he can carry, the rest into the stock */
	bool Build(FName Id, bool bAtWorkbench);

	/** Workbench and field box: the pouches fill up from the stock */
	void Restock();

	/** The Werkrolle: moves shots or devices of one kind from the backpack into the pockets, up to what they hold;
	 *  returns how many moved */
	int32 ReadyFromStock(FName Id);

	/** The Werkrolle: lays a kind of shot he carries into the weapon */
	bool SetAmmo(FName Id);

	bool HasBuilt(FName Id) const;

	/** How many shots or devices of a kind are in the pouches and in the stock */
	int32 GetCarried(FName Id) const;
	int32 GetStock(FName Id) const;
	int32 GetCarryLimit(const FVaelGear& Gear) const;

	// --- Weapon and shot

	const FVaelGear* GetWeapon() const;

	/** The next weapon he has built (d-pad up, R) */
	void CycleWeapon();

	/** The shot in the weapon: Standard (endless stones, arrows, bolts) or a special one */
	FName GetAmmo() const { return Ammo; }
	FText GetAmmoName() const;
	FLinearColor GetAmmoColor() const;

	/** The next shot he has in the pouches (R1, mouse wheel) */
	void CycleAmmo(int32 Step);

	/** Reaches for one kind of shot straight away (keyboard 1 to 9); nothing if he has none of it */
	void PickAmmo(FName Id);

	/** Fires once towards the aim point if the weapon is ready; holding the button fires again and again.
	 *  DrawScale: how far the bow was drawn (1.0, 1.35, 1.8); bDrawPierce: a full draw goes through */
	bool Fire(const FVector& AimLocation, float DrawScale = 1.0f, bool bDrawPierce = false);

	/** The bow is drawn while the button is held and shoots on release: 0.4 s x1.0, 0.8 s x1.2, 1.2 s x1.5 and through */
	bool UsesDraw() const;
	void StartDraw();
	bool ReleaseDraw(const FVector& AimLocation);
	bool IsDrawing() const { return DrawStart >= 0.0f; }

	/** The slingshot: how much it is held up (eases in while drawing, out after the shot) and how far the band is pulled, 0..1 */
	float GetSlingAim() const { return SlingAim; }
	float GetSlingDraw() const { return SlingDrawShown; }

	/** Whether the Zwille is in his left hand right now (the fist stays closed round it) */
	bool IsHoldingSling() const;

	/** The release (goal 9.11.): 1 the moment the stone leaves, down to 0 over a third of a second (kick and follow-through) */
	float GetSlingKick() const;

	/** Trembling of the arms when he holds the band at full draw: 0 to 1 over a second */
	float GetSlingTremble() const;

	/** How far the band was pulled at the last release (0..1): a full draw snaps harder */
	float GetReleasePull() const { return ReleasePull; }

	/** Where the pouch is: 1 drawn, 0 at rest, below 0 swinging out ahead of the fork after the release */
	float GetPouchSwing() const { return PouchSwing; }

	/** Seconds to pull the slingshot band all the way */
	static constexpr float SlingFullDraw = 1.0f;
	float GetDrawSeconds() const;

	/** The crossbow holds two bolts, then reloads for 1.4 s */
	int32 GetMagazine() const { return Magazine; }

	/** Reaching for a shot: from the pouches at the belt 0.35 s, from the backpack 1.2 s, and a hit stops a reach into the
	 *  backpack. While he reaches he doesn't shoot. */
	bool IsGrabbing() const { return !GrabAmmo.IsNone(); }
	FName GetGrabAmmo() const { return GrabAmmo; }
	bool IsGrabFromPack() const { return bGrabFromPack; }
	float GetGrabFraction() const;
	float GetSecondsSinceCycle() const { return Now() - LastCycle; }

	/** The shots he can reach for: the endless one, then every kind in the pouches or the backpack */
	TArray<FName> GetAmmoChoices() const;

	/** He was hit: a reach into the backpack is lost */
	void OnHurt();

	/** Pouches at the belt: 0 leather (20 shots, 2 devices), 1 armour (30, 3), 2 silk (40, 4) */
	int32 GetPouchTier() const;

	/** Shots of the sling in a row (the fourth is the Hagel: five stones at 40 % each) */
	int32 GetHagelCount() const { return HagelCount; }

	/** Tier of a weapon (its upgrades, 1.18^tier more damage) */
	int32 GetWeaponTier(FName Id) const;
	void SetWeaponTier(FName Id, int32 Tier);
	float GetTierScale() const;

	/** Seconds until the weapon can fire again, and the whole interval */
	float GetReloadLeft() const;
	float GetInterval() const;

	// --- Devices

	FName GetDevice() const { return Device; }
	void CycleDevice(int32 Step);

	/** Lays the picked device at the aim point (in reach) */
	bool PlaceDevice(const FVector& AimLocation);

	/** Square / X: every mine he laid goes off */
	int32 IgniteAll();

	int32 GetNumPlaced() const;

	/** Seconds until a kept device (Lockpfeife, Wurfhaken) can be used again, 0 when ready */
	float GetDeviceReadyIn(FName Id) const;

	/** Lockpfeife: enemies round him follow the whistle to this spot (heavy ones shortly, bosses not); returns how many came */
	int32 Whistle(const FVector& Spot);

	// --- Armour

	/** The piece worn on a place, None if none */
	FName GetWorn(EVaelArmorPlace Place) const;

	/** Wears a built piece on its place */
	bool Wear(FName Id);

	/** Takes the piece off its place; it stays built, in the toolbox */
	bool TakeOff(EVaelArmorPlace Place);

	/** For trying the armour out (Boris 17.11.): every piece of the Schwarzglut set built and lying in the toolbox, none worn
	 *  by this; the toolbox calls it when the engineer opens it. Off with bTestArmorInToolbox in the .cpp */
	void PutTestArmorInToolbox();

	/** The pieces of a set (without the shield): it is built piece by piece but worn only whole (Boris 25.11.) */
	static TArray<FName> GetSetPieces(FName Set);
	int32 CountSetBuilt(FName Set) const;

	/** How many worn pieces belong to a set (the shield doesn't count towards two or four pieces) */
	int32 CountSet(FName Set) const;

	/** The sum of armour of the worn pieces (each point is a percent less damage) */
	float GetArmorTotal() const;

	/** Damage a hit on him really does, after armour, set bonuses and his shield; From is the attacker, if known.
	 *  A perfect block of the Schwarzglut set with its shield throws the attacker back and sets it on fire. */
	float ScaleIncoming(float Damage, AActor* From = nullptr, bool bFire = false);

	/** He hit something for this much: the set of the Blutschuppen heals */
	void OnDamageDealt(float Damage);

	/** He dodged: four pieces of the Rollpanzer or the Schwarzglut set curl him up for a moment */
	void OnDodged();

	/** The dodge button is held or let go. In the whole Schwarzglut set he rolls up into the Panzerkugel to dodge (Boris
	 *  17.11.: the heavy set makes him slow, the roll is how he gets into position), and rolls on while it is held */
	void SetRollHeld(bool bHeld) { bRollHeld = bHeld; }

	/** True while he is rolled up into the Panzerkugel */
	bool IsBall() const { return bBallShown; }

	/** True while the Panzerkugel rolls on because the dodge is held (the dodge doesn't end then, even after a long frame) */
	bool WantsRollOn() const;

	/** Test only: shows the Panzerkugel standing still, for pictures */
	void SetBallPreview(bool bPreview) { bBallPreview = bPreview; }

	// --- The folding shield (goal 14.11.): L2 / right mouse held

	/** Whether he wears a shield */
	bool HasShield() const;

	/** Holds the shield up or lets it fold (nothing without a shield) */
	void SetShieldRaised(bool bRaised);
	bool IsShieldRaised() const { return bShieldRaised; }

	/** 0 folded on the forearm .. 1 fanned open */
	float GetShieldOpen() const { return ShieldOpen; }

	/** Test only: holds the shield still at this much open (0..1) for pictures; below 0 it moves by itself again */
	void SetShieldPreview(float Open);

	/** Test only: holds the aim of the sling at this much (0..1) for pictures; below 0 it follows the drawing again */
	void SetSlingAimPreview(float Aim) { SlingAimPreview = Aim; }

	/** How a hit from this attacker is blocked: 0 not at all, 1 by the shield (70 % less), 2 a perfect block (nothing gets through) */
	int32 GetBlock(const AActor* From) const;

	/** How fast he walks with his set (Rußfeder faster, the heavy Schwarzglut slower) and behind the raised shield */
	float GetMoveSpeedScale() const;

	// --- Shots that land (from AVaelEngineerShot and the turrets)

	/** What a shot does where it lands on an enemy, or on a device of his */
	void ApplyHit(AActor* Target, const FVector& Location, float Damage, FName ShotAmmo, const FVector& From = FVector::ZeroVector, FName WeaponId = NAME_None);

	/** The factors of the last hit, for the test and the HUD: zone (1.5 / 1.3 / 1.0 / 0.8), distance, crit */
	float LastZone = 1.0f;
	float LastDistance = 1.0f;
	bool bLastCrit = false;
	void ApplyBurst(const FVector& Location, FName ShotAmmo, float Damage, const FVector& From = FVector::ZeroVector);

	/** A blast of fire round a spot: damage, knockback, burning if asked, and his mines nearby go off */
	void Explode(const FVector& Location, float Radius, float Damage, bool bBurn);

	/** A device was hit by one of his shots */
	void OnDeviceShot(AVaelEngineerDevice* Placed, FName ShotAmmo);

	/** The damage scale of his sets (Sichelklinge) and whether every shot pierces */
	float GetDamageScale() const;
	bool PiercesAll() const;

	// --- Gear that falls for him

	/** Takes a piece of gear apart into materials instead of carrying it; returns false if nothing came of it */
	static bool Dismantle(AVaelCharacter* Player, int32 Rarity, int32 ItemLevel);

	// --- Save

	void WriteSave(FVaelSavedPlayer& Saved) const;
	void ReadSave(const FVaelSavedPlayer& Saved);

	/** Back to how he starts: only the sling */
	void ResetToStart();

private:

	AVaelCharacter* GetPlayer() const;
	void RestoreTickRate();
	bool AttachQuartzGoggles();
	void DropIncompleteSets();
	float Now() const;

	/** The weapon in his hand: shown while he plays the engineer, the mesh of the weapon he holds */
	void UpdateWeaponMesh();

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	/** The Lockpfeife in his right hand for a moment while he whistles */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> WhistleMesh;
	float WhistleShownUntil = 0.0f;
	/** The Fernglas (25.11.) in his hand while the spyglass is raised; it marks what is inside its focus circle */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> SpyglassMesh;
	int32 SpyglassMarked = 0;
	void UpdateSpyglass();
public:
	/** How often the Fernglas marked a creature, for the tests */
	int32 GetSpyglassMarked() const { return SpyglassMarked; }
private:

	/** The slingshot band (two strands from the prong tips to the pouch) and the leather pouch; stretched every frame */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> BandMesh[2];

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> PouchShell;

	/** The shot lying in the pouch while he draws, in the look of the loaded ammo */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> PouchLoad;
	FName PouchLoadAmmo;
	bool bPouchLoadFull = false;

	/** Makes sure the Zwille follows the hand every frame: the component itself ticks only four times a second, so the
	 *  Zwille hangs on the body's "pose is final" callback (goal 9.11.: it lagged behind and moved in steps) */
	void UpdateSling(float DeltaTime);

	/** Every frame, right after the body's final pose: places the Zwille in the left hand and stretches its band */
	void OnBodyPoseFinal();
	void UpdateSlingFrame(float DeltaTime);
	TWeakObjectPtr<USkeletalMeshComponent> SlingBody;
	FDelegateHandle SlingBodyHandle;
	double SlingFrameTime = -1.0;

	/** World positions of the two prong tips of the Zwille in the hand */
	bool SlingTips(FVector& OutLeft, FVector& OutRight) const;

	/** The Panzerschleuder (Boris 16.11.): with the Glutpanzer's arms the sling is mounted on the left forearm instead of held in
	 *  the fist, so his hands stay free. A bracer along the forearm and a fork riding on top of it that slides forward over
	 *  the fist while he aims (Tools/Ingenieur/vael_armschleuder.py) */
	bool UsesArmSling() const;
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> ArmSlingBracer;
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> ArmSlingFork;
	bool bShownArmSling = false;
	/** Places bracer and fork on the forearm by the aim; returns the frame of the forearm (X ahead, Z the thumb side) */
	FTransform PlaceArmSling();

	/** The Panzerkugel (Tools/Ingenieur/vael_panzerkugel.py): the curled Aschenroller covered with the Glutpanzer's back
	 *  shell. Shown instead of the whole hero while he dodges in the full Schwarzglut set; held, the dodge rolls on, steered
	 *  by the stick, faster downhill; it throws enemies aside and ends dazed against a wall */
	bool UsesArmorRoll() const;
	void UpdateArmorRoll(float DeltaTime);
	void ShowBall(bool bShown);
	void HideHeroForBall();
	bool MakeBall();
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> RollBall;
	TArray<TWeakObjectPtr<UPrimitiveComponent>> HiddenForBall;
	TArray<TWeakObjectPtr<AActor>> RolledOver;
	bool bRollHeld = false;
	bool bBallPreview = false;
	bool bBallShown = false;
	float BallSince = 0.0f;
	float BallSpin = 0.0f;
	float BallSpunTotal = 0.0f;
	int32 BallFrames = 0;
	float NextBallDust = 0.0f;
	FVector LastBallLocation = FVector::ZeroVector;

	float SlingAim = 0.0f;
	float SlingAimPreview = -1.0f;
	float SlingDrawShown = 0.0f;

	/** The pouch after the release (goal 16.11.): a spring that snaps it forward through the fork and lets it swing out; 1 drawn,
	 *  0 at rest behind the fork, below 0 ahead of the fork */
	float PouchSwing = 0.0f;
	float PouchSwingSpeed = 0.0f;
	bool bPouchSnapping = false;
	/** The pull at the moment of the release, for the snap and the follow-through */
	float ReleasePull = 0.0f;
	float SlingLastShot = -100.0f;

	/** The reach of the next shot, set by how far the slingshot was drawn (cm); 0 = the reach of the weapon */
	float ShotReach = 0.0f;

	FName ShownWeapon;
	void ApplyHealthBonus();

	UPROPERTY()
	TSet<FName> Built;

	/** Pouches and stock of shots and devices */
	UPROPERTY()
	TMap<FName, int32> Carried;

	UPROPERTY()
	TMap<FName, int32> Stock;

	/** Worn pieces by place */
	UPROPERTY()
	TArray<FName> Worn;

	FName Weapon;
	FName Ammo;
	float DrawStart = -1.0f;
	FName GrabAmmo;
	float GrabStart = 0.0f;
	float GrabEnd = 0.0f;
	bool bGrabFromPack = false;
	float LastCycle = -100.0f;
	void BeginGrab(FName Id);

	/** What he carries on the body: the backpack, the pouches of their tier, the goggles once built */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> PackMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> PouchMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> GogglesMesh;

	int32 ShownPouchTier = -1;
	bool bShownGoggles = false;
	bool bShownPack = false;
	bool bShownBackpack = false;
	bool bShownPouchBelt = false;
	void UpdateCarriedMeshes();

	/** The worn armour on the body: helmet, chest plate, two bracers, two greaves of the set of each piece */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> ArmorMeshes;

	/** The Glutpanzer as its own body (goal 16.11.): Kopf, Brust, Arme, Beine, each following the base body */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USkeletalMeshComponent>> GlutpanzerBodies;

	FString ShownArmor;
	void UpdateArmorMeshes();

	/** The tower shield: a mount on the left forearm and the five rows of bands that slide out of each other */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> ShieldHub;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> ShieldBands;

	/** The Glutpanzer's back shell (SKM_Ing_GP_Schild, goal 16.11.): on the back of the chest while it is worn; with the
	 *  Klappschild worn too it is the shield, which curls into a ball like a rolling Aschenroller, rolls over the left shoulder
	 *  and unrolls before him into a tower shield */
	UPROPERTY(Transient)
	TObjectPtr<UPoseableMeshComponent> ShellShield;

	/** Its rest pose in its own space: the chain Schild_0..4, the joint at the end of the chain, its helpers */
	TArray<FTransform> ShellBones;
	FVector ShellEnd = FVector::ZeroVector;
	FVector ShellDown = FVector::DownVector;
	FVector ShellIn = FVector::ForwardVector;
	/** Where it sits on the back, relative to spine_05 of the chest */
	FTransform ShellOnBack;
	void UpdateShellShield();
	/** Moves the shell between back and front by ShieldOpen; false once it rests on the back again */
	bool TickShellShield();
	/** Whether the back shell is his shield now (Glutpanzer chest and a shield worn) */
	bool IsShellShield() const;

	/** Where the folded shield lies on the forearm, relative to the bone */
	FTransform ShieldFolded;
	bool bShieldRaised = false;
	float ShieldOpen = 0.0f;
	float ShieldRaisedAt = -100.0f;
	float LastShieldTick = 0.0f;
	bool bShieldMoving = false;
	float ShieldPreview = -1.0f;
	void UpdateShieldMeshes();
	void TickShield();
	UStaticMeshComponent* AttachLimbPiece(int32 Slot, const FString& Piece, FName From, FName To, float Along);
	UStaticMeshComponent* AttachPiece(UStaticMeshComponent* Existing, const TCHAR* Name, const TCHAR* Piece, FName Bone, const FTransform& InActorSpace);
	int32 Magazine = 2;
	float ReloadEnd = 0.0f;
	int32 HagelCount = 0;

	UPROPERTY()
	TMap<FName, int32> WeaponTiers;

	UPROPERTY()
	TMap<FName, FName> WeaponBranches;

	UPROPERTY()
	TSet<FName> Plans;

	/** Formulas of shots he has found, and mixes that made nothing */
	UPROPERTY()
	TSet<FName> Formulas;

	UPROPERTY()
	TSet<FString> Tried;

	/** Ideas he has had (goal 16.11.) */
	UPROPERTY()
	TSet<FName> Ideas;
	FName LastIdea;
	float LastIdeaTime = -100.0f;
	bool bIdeasPrimed = false;
	FName LastBuilt;
	float LastBuiltTime = -100.0f;

	/** The kinds of shots that have a pocket in the Werkrolle (also when the pocket is empty right now) */
	UPROPERTY()
	TArray<FName> Pockets;

	/** What a shot does to one enemy besides its damage (the effect of its formula) */
	void ApplyEffect(AVaelCharacterBase* Enemy, const FVaelGear* AmmoGear, EVaelAmmoEffect Effect, const FVector& Location, const FVector& From, float Damage);

	/** Enemies of his within a radius of a spot */
	TArray<AVaelCharacterBase*> EnemiesAround(const FVector& Location, float Radius) const;

	/** Glutnadel: blasts that go off later */
	struct FDelayedBlast
	{
		FVector Location = FVector::ZeroVector;
		TWeakObjectPtr<AActor> Stuck;
		float At = 0.0f;
		float Radius = 300.0f;
		float Damage = 0.0f;
		bool bBurn = false;
	};
	TArray<FDelayedBlast> DelayedBlasts;

	/** Brandsatz and Dampfwolke: fields on the ground that work every half second; Heiltau heals the same way */
	struct FField
	{
		FVector Location = FVector::ZeroVector;
		float End = 0.0f;
		float Radius = 300.0f;
		float Damage = 0.0f;
		EVaelAmmoEffect Effect = EVaelAmmoEffect::FireField;
	};
	TArray<FField> Fields;
	float NextFieldTick = 0.0f;
	void TickEffects();

	FName Device;

	/** When each kept device can be used again (world seconds) */
	TMap<FName, float> DeviceReadyAt;
	float LastShot = -100.0f;
	float CurledUntil = 0.0f;
	float AppliedHealth = 0.0f;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AVaelEngineerDevice>> Placed;

	/** Part breaking (like Monster Hunter): damage on the head side and on the armoured flanks of each enemy; at a quarter of
	 *  its life (three sixteenths with the Sternmilch lens) the part breaks: the head makes the creature hit 15 % weaker, the
	 *  flank leaves it open (marked for a minute), and a Bruchstück drops with two of its own materials */
	struct FParts
	{
		float Head = 0.0f;
		float Flank = 0.0f;
		bool bHeadBroken = false;
		bool bFlankBroken = false;
	};
	TMap<TWeakObjectPtr<AActor>, FParts> Parts;
	void HitPart(AVaelCharacterBase* Enemy, float Damage, float Zone);

public:
	/** How many parts this engineer has broken (for the test) */
	int32 BrokenParts = 0;

private:
	/** Poisons ticking on enemies */
	struct FPoison
	{
		TWeakObjectPtr<AVaelCharacterBase> Target;
		float DamagePerSecond = 0.0f;
		float End = 0.0f;
		/** Bleeding and Schwarzfeuer bite with their own element; poison is earth */
		EVaelElement Element = EVaelElement::Earth;
	};
	TArray<FPoison> Poisons;
	float NextPoisonTick = 0.0f;
};
