// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelCombatStatics.h"
#include "Floromant/VaelFlightPose.h"
#include "Floromant/VaelFloromantSettings.h"
#include "Magic/VaelElementTypes.h"
#include "VaelCharacter.generated.h"

class UAnimInstance;
class UAnimMontage;
class UNiagaraComponent;
class UPointLightComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UVaelElementComponent;
class UVaelElementOrbitComponent;
class UVaelFairyWingsComponent;
class UVaelFloromantComponent;
class UVaelInventory;
class UVaelMaterialBag;
class UDecalComponent;

/**
 *  A directly controlled player character seen from the shared isometric camera.
 *  Turns towards the aim direction of its controller, can dodge roll and combines elements into formulas.
 */
UCLASS()
class AVaelCharacter : public AVaelCharacterBase
{
	GENERATED_BODY()

private:

	/** Placeholder disc at the character's feet showing the player color */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> PlayerMarker;

	/** Head of the base look: its own mesh that follows the bones of the body */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> HeadMesh;

	/** Hands of the base look with real fingers (the mannequin's, vael_mensch_haende.py), following the bones of the body */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> HandMesh;

	/** Element queue and formula casting */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UVaelElementComponent> ElementComponent;

	/** Equipment and backpack of this player */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UVaelInventory> Inventory;

	/** Ores, plants and parts of creatures of this player */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UVaelMaterialBag> MaterialBag;

	/** The queued elements, circling above the head */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UVaelElementOrbitComponent> ElementOrbit;

	/** Seeds and plants while the player plays a Floromant */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UVaelFloromantComponent> Floromant;

	/** Moth wings of the Floromant, hidden while the player plays a mage */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UVaelFairyWingsComponent> Wings;

	/** Light at the casting hand in the color of the newest queued element */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPointLightComponent> HandLight;

protected:

	/** Height of the circle of element orbs above the centre of the character */
	UPROPERTY(EditAnywhere, Category="Appearance")
	float QueueOrbHeight = 135.0f;

	/** Brightness of the light at the hand for one queued element, in candela; more elements and elements from the environment shine stronger */
	UPROPERTY(EditAnywhere, Category="Appearance", meta = (ClampMin = 0))
	float HandLightIntensity = 12.0f;


	/** Debug help: shows health and mana as text above the character */
	UPROPERTY(EditAnywhere, Category="Appearance")
	bool bShowStatusText = false;

	/** Speed during a dodge roll */
	UPROPERTY(EditAnywhere, Category="Dodge", meta = (ClampMin = 0))
	float DodgeSpeed = 1400.0f;

	/** Length of a dodge roll in seconds */
	UPROPERTY(EditAnywhere, Category="Dodge", meta = (ClampMin = 0))
	float DodgeDuration = 0.4f;

	/** Time from the start of a dodge roll until the next one is allowed, in seconds */
	UPROPERTY(EditAnywhere, Category="Dodge", meta = (ClampMin = 0))
	float DodgeCooldown = 0.9f;

	/** Placeholder for the roll animation while no dodge montage exists: height scale of the mesh while dodging */
	UPROPERTY(EditAnywhere, Category="Dodge", meta = (ClampMin = 0.1, ClampMax = 1))
	float DodgeMeshSquash = 0.6f;

	/** Montage of the dodge roll, stretched to the length of the roll. Used once the asset exists; until then the mesh is squashed. */
	UPROPERTY(EditAnywhere, Category="Dodge")
	TSoftObjectPtr<UAnimMontage> DodgeMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Vael/Characters/Mage/AM_Mage_Dodge.AM_Mage_Dodge")));

	/** Montage of spell dashes like the Boee, stretched to their length. Without it the dodge montage is used. */
	UPROPERTY(EditAnywhere, Category="Dodge")
	TSoftObjectPtr<UAnimMontage> SpellDashMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Vael/Characters/Mage/AM_Mage_SpellDash.AM_Mage_SpellDash")));

	/** Animation blueprint of the mage, based on UVaelAnimInstance. Used once the asset exists; until then the template animations play. */
	UPROPERTY(EditAnywhere, Category="Appearance")
	TSoftClassPtr<UAnimInstance> MageAnimClass = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(TEXT("/Game/Vael/Characters/Mage/ABP_Mage.ABP_Mage_C")));

	/** The assembled MetaHuman of the mage (vael_metahuman_build.py) */
	UPROPERTY(EditAnywhere, Category="Appearance")
	TSoftClassPtr<AActor> MetaHumanClass = TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Game/Vael/Characters/Menschen/MH_Magier_Build/MH_Magier/BP_MH_Magier.BP_MH_Magier_C")));

	/** What the MetaHuman wears, on its skeleton: the starting robe (vael_kleidung_anpassen.py) and the boots (vael_stiefel_bauen.py) */
	UPROPERTY(EditAnywhere, Category="Appearance")
	TArray<TSoftObjectPtr<USkeletalMesh>> MetaHumanClothes = {
		TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Vael/Characters/Menschen/MH_Kleidung/SK_MagierMann_Robe_MH.SK_MagierMann_Robe_MH"))),
		TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Vael/Characters/Menschen/MH_Kleidung/SK_MagierMann_Stiefel_MH.SK_MagierMann_Stiefel_MH")))
	};

	/** The MetaHuman's body without what its clothes cover, so no skin pokes through (vael_koerper_verdecken.py) */
	UPROPERTY(EditAnywhere, Category="Appearance")
	TSoftObjectPtr<USkeletalMesh> MetaHumanBodyUnderRobe = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Vael/Characters/Menschen/MH_Kleidung/SK_MH_Magier_Koerper_Robe_Stiefel.SK_MH_Magier_Koerper_Robe_Stiefel")));

	/** Body of the two base looks of the mage in the starting robe (creature factory, humans), male and female. Empty or missing: the template mannequin stays. */
	UPROPERTY(EditAnywhere, Category="Appearance")
	TSoftObjectPtr<USkeletalMesh> BodyMeshes[2] = {
		TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Vael/Characters/Menschen/MagierMann/SK_MagierMann_Koerper.SK_MagierMann_Koerper"))),
		TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Vael/Characters/Menschen/MagierFrau/SK_MagierFrau_Koerper.SK_MagierFrau_Koerper"))) };

	/** Head of each base look: its own part on the same skeleton, so faces can be swapped later */
	UPROPERTY(EditAnywhere, Category="Appearance")
	TSoftObjectPtr<USkeletalMesh> HeadMeshes[2] = {
		TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Vael/Characters/Menschen/MagierMann/SK_MagierMann_Kopf.SK_MagierMann_Kopf"))),
		TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Vael/Characters/Menschen/MagierFrau/SK_MagierFrau_Kopf.SK_MagierFrau_Kopf"))) };

	/** Hands of each base look, a part of its own; missing: the body keeps whatever hands it has */
	UPROPERTY(EditAnywhere, Category="Appearance")
	TSoftObjectPtr<USkeletalMesh> HandMeshes[2] = {
		TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Vael/Characters/Menschen/MagierMann/SK_MagierMann_Haende.SK_MagierMann_Haende"))),
		TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Vael/Characters/Menschen/MagierFrau/SK_MagierFrau_Haende.SK_MagierFrau_Haende"))) };

	/** Seconds nothing can hurt the player after a hit */
	UPROPERTY(EditAnywhere, Category="Combat", meta = (ClampMin = 0))
	float HitInvulnerability = 0.35f;

	/** A teammate this close helps a downed player up, in cm */
	UPROPERTY(EditAnywhere, Category="Combat", meta = (ClampMin = 0))
	float ReviveDistance = 224.0f;

	/** Seconds a teammate has to stay close to help a downed player up */
	UPROPERTY(EditAnywhere, Category="Combat", meta = (ClampMin = 0))
	float ReviveDuration = 2.4f;

	/** Health after being helped up */
	UPROPERTY(EditAnywhere, Category="Combat", meta = (ClampMin = 1))
	float ReviveHealth = 45.0f;

	/** Seconds nothing can hurt the player after being helped up */
	UPROPERTY(EditAnywhere, Category="Combat", meta = (ClampMin = 0))
	float ReviveInvulnerability = 1.5f;

	/** Vector parameter that tints the character materials in the player color */
	UPROPERTY(EditAnywhere, Category="Appearance")
	FName BodyTintParameter = TEXT("Paint Tint");

	/** Vector parameter that colors the marker disc */
	UPROPERTY(EditAnywhere, Category="Appearance")
	FName MarkerColorParameter = TEXT("Color");

public:

	/** Constructor */
	AVaelCharacter();

	/** Initialization */
	virtual void BeginPlay() override;

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

	/** Applies the color of the possessing player */
	virtual void PossessedBy(AController* NewController) override;

	/** Starts a dodge roll in the given world direction, or in the facing direction if it is zero. Returns false while on cooldown. */
	UFUNCTION(BlueprintCallable, Category="Dodge")
	bool StartDodge(const FVector& WorldDirection);

	/** True while a dodge roll is in progress */
	UFUNCTION(BlueprintPure, Category="Dodge")
	bool IsDodging() const { return bIsDodging; }

	/** Gives the character one of the base looks of its class: 0 the man, 1 the woman. Body and head are separate parts. */
	void SetBaseLook(int32 LookIndex);

	/** Shows the MetaHuman body instead of the base look. The base look stays hidden underneath and keeps animating: the body follows it. */
	void SetMetaHumanShown(bool bShown);

	/** Performance test: more MetaHumans standing around the player and moving with it, the cost of a co-op group */
	void SpawnMetaHumanCopies(int32 Count);

	/** Turns the character into a mage or a Floromant, man or woman: body, wings, Saft or mana and what the buttons do */
	void SetHero(EVaelHeroClass InClass, bool bFemale);

	/** Level of the player and the share of the way to the next one (placeholder progression: experience from defeated creatures) */
	int32 GetLevel() const { return Level; }
	float GetExperienceFraction() const;

	/** Gives experience; enough of it raises the level */
	void AddExperience(float Amount);

	/** The way the player has sworn to, none before the oath */
	EVaelPath GetPath() const { return Path; }
	void SetPath(EVaelPath InPath) { Path = InPath; }

	/** Share of the dodge cooldown still to wait, 0 when a roll is possible */
	float GetDodgeCooldownFraction() const;

	/** Class the player plays */
	EVaelHeroClass GetHeroClass() const { return HeroClass; }
	bool IsFloromant() const { return HeroClass == EVaelHeroClass::Floromant; }

	/** The base look it wears, -1 while it still shows the template mannequin */
	int32 GetBaseLook() const { return BaseLook; }

	/** Weighs the player down, like a creature clinging to the back: walks at the share of the speed for the seconds. Call again to keep it up. */
	void SetBurden(float SpeedMultiplier, float Duration);

	/**
	 *  Rushes on the wind like a dodge roll, just faster and regardless of the dodge cooldown.
	 *  When the dash ends the burst hits everyone around within the radius. Returns false while down.
	 */
	bool StartSpellDash(const FVector& WorldDirection, float Speed, float Duration, const FVaelSpellHit& InDashBurst, float BurstRadius);

	/** Number of the player for messages, starting at 1 */
	int32 GetPlayerNumber() const;

	/** Share of the help a downed player has received, 1 means back on their feet */
	float GetReviveFraction() const { return ReviveDuration > 0.0f ? FMath::Clamp(ReviveProgress / ReviveDuration, 0.0f, 1.0f) : 0.0f; }

	/** True while the player is down and waits for help */
	UFUNCTION(BlueprintPure, Category="Combat")
	bool IsDowned() const { return bDowned; }

	/** Drinks a healing potion from the bag if health is not full and the last one has worn off. Returns true if one was drunk. */
	bool DrinkPotion();

	/** Healing potions in the bag */
	int32 GetPotionCount() const;

	/** Gets a downed player back on their feet with the given health and some seconds of invulnerability */
	UFUNCTION(BlueprintCallable, Category="Combat")
	void Revive(float Health, float InvulnerableSeconds);

	/** Helps a fallen player up by the seconds, like a teammate standing next to them (the heartbeat of a Rosenherz) */
	void AddReviveProgress(float Seconds);

	/** A heart shield of the Rußrose: swallows this much damage within the seconds (Glashaus) */
	void AddHeartShield(float Amount, float Seconds);

	/** Takes damage off the heart shield first; returns what is left for the health */
	float AbsorbIncoming(float Damage);

	/** Nothing can hurt the player for the given seconds */
	UFUNCTION(BlueprintCallable, Category="Combat")
	void SetInvulnerableFor(float Seconds);

	/** Raises or lowers the spyglass: while raised the player stands still, watches what is inside the focus circle and goes unnoticed by creatures */
	void SetObserving(bool bInObserving);

	/** True while the spyglass is raised */
	bool IsObserving() const { return bObserving; }

	/** Middle of the circle the spyglass is aimed at, on the ground */
	FVector GetFocusLocation() const { return FocusLocation; }

	//~Begin AVaelCharacterBase
	virtual bool IsInvulnerable() const override;
	virtual bool IsDefeated() const override { return bDowned; }
	virtual float GetIncomingDamageMultiplier(const FGameplayTagContainer& DamageTags) const override;
	virtual bool IsAboveGround() const override;
	virtual void ApplyKnockback(const FVector& Direction, float Speed) override;
	virtual void ApplyPull(const FVector& Location, float Speed, float DeltaSeconds) override;
	//~End AVaelCharacterBase

	/** Tints the character and its marker */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetPlayerColor(const FLinearColor& Color);

	/** Returns the marker component **/
	UStaticMeshComponent* GetPlayerMarker() const { return PlayerMarker.Get(); }

	/** Returns the seeds and plants of the Floromant **/
	UVaelFloromantComponent* GetFloromant() const { return Floromant.Get(); }

	/** Returns how the body of a flying Floromant hangs in the air; weight 0 for a mage */
	const FVaelFlightPose& GetFlightPose() const { return FlightPose; }

	/** Starts a cast gesture posed in code (VaelAnimInstance) */
	void PlayCastGesture(EVaelCastGesture Gesture);

	/** The cast gesture playing and the seconds since it began; none once it is over */
	EVaelCastGesture GetCastGesture(float& OutSeconds) const;

	/** Where a spell leaves the hand during a gesture: just before the tips of index and middle finger. False without those bones. */
	bool GetGestureFingertips(FVector& OutLocation) const;

	/** Where breath leaves the face (the mouth of the base looks, from the head bone); the top of the capsule without a head bone */
	FVector GetMouthLocation() const;

	/** Returns the element queue **/
	UVaelElementComponent* GetElementComponent() const { return ElementComponent.Get(); }

	/** Returns the material bag **/
	UVaelMaterialBag* GetMaterialBag() const { return MaterialBag.Get(); }

	/** Returns the equipment and backpack **/
	UVaelInventory* GetInventory() const { return Inventory.Get(); }

protected:

	/** Brings the player down at zero health and starts the short invulnerability after a hit */
	virtual void OnHealthChanged(float OldValue, float NewValue) override;

private:

	int32 BaseLook = -1;

	/** Lets the body of an assembled MetaHuman follow the mesh of this character */
	void FollowMesh(AActor* Human);

	/** The MetaHuman while it is shown, attached to the mesh */
	UPROPERTY(Transient)
	TObjectPtr<AActor> MetaHuman;

	/** Class the player plays, and whether it was set yet */
	EVaelHeroClass HeroClass = EVaelHeroClass::Mage;
	bool bHeroSet = false;

	/** Level, experience gathered towards the next one and the sworn way */
	int32 Level = 1;
	float Experience = 0.0f;
	EVaelPath Path = EVaelPath::None;

	/** Experience the next level needs */
	float GetExperienceForNextLevel() const { return 100.0f + 60.0f * (Level - 1); }

	/** Mana regained per second as a mage; the Floromant has its own Saft */
	float MageManaRegen = 0.0f;

	/** Colour of the player, applied again when the body changes */
	FLinearColor PlayerColor = FLinearColor::White;
	bool bHasPlayerColor = false;

	/** Gives the character what the save game holds for its player slot, once */
	void RestoreFromSave();

	bool bRestoredFromSave = false;

	/** Lets the element orbs show the queued elements */
	void RefreshQueueOrbs();

	/** Shows the new queue and plays the hand gesture for a chosen element */
	void OnElementQueueChanged();

	/** Lets the hand glow in the color of the newest queued element, or switches the glow off for an empty queue */
	void RefreshHandEffect();

	/** Lets the light at the hand flicker */
	void FlickerHandLight();

	/** Brightness of the light at the hand before its flicker, 0 while no element is queued */
	float CurrentHandLightIntensity = 0.0f;

	/** Glow at the hand while elements are queued, null while the effect doesn't exist */
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> HandEffect;

	/** Number of elements in the queue at the last change */
	int32 NumQueuedElements = 0;

	/** Hand gesture for a chosen element, null while the asset doesn't exist */
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> LoadedElementSelectMontage;

	/** Puts the player down until a teammate helps them up */
	void GoDown();

	/** Advances the help of a teammate standing close to a downed player */
	void TickRevive(float DeltaSeconds);

	/** True while the player is down */
	bool bDowned = false;

	/** World time from which the next potion can be drunk */
	float NextPotionTime = 0.0f;

	/** Health the regeneration after a potion gives back per second, and the world time it lasts until */
	float PotionRegenPerSecond = 0.0f;
	float PotionRegenEndTime = 0.0f;
	FTimerHandle PotionRegenTimer;

	/** Gives back a second's worth of the potion's regeneration */
	void TickPotionRegen();

	/** Seconds a teammate has helped so far */
	float ReviveProgress = 0.0f;

	/** World time until which nothing can hurt the player */
	float InvulnerableEndTime = 0.0f;

	/** Ends the dodge roll and returns to normal movement */
	void EndDodge();

	/** Turns the body into the roll direction and plays the montage stretched over the roll, or squashes the mesh without one */
	void BeginRollLook(UAnimMontage* Montage, float Duration);

	/** Montages loaded at the start, null while their assets don't exist */
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> LoadedDodgeMontage;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> LoadedSpellDashMontage;

	/** Montage playing for the current roll or dash */
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveRollMontage;

	/** Mesh scale outside of a dodge roll */
	FVector DefaultMeshScale = FVector::OneVector;

	/** Place of the mesh on the ground; the Floromant hovers above it */
	FVector DefaultMeshLocation = FVector::ZeroVector;

	/** Lets a Floromant hover with a gentle bob and fly with lean, dangling legs and a shadow; a mage stands on the ground */
	void UpdateHover(float DeltaSeconds);

	/** Places the soft shadow under a flying Floromant, made the first time it is needed */
	void UpdateFlightShadow(float Height, float Landed);

	/** Soft shadow under a flying Floromant */
	UPROPERTY(Transient)
	TObjectPtr<UDecalComponent> FlightShadow;

	UPROPERTY(Transient)
	TObjectPtr<class UMaterialInstanceDynamic> FlightShadowMaterial;

	/** Damage the heart shield still swallows, and until when */
	float HeartShieldLeft = 0.0f;
	float HeartShieldEndTime = 0.0f;

	/** Cast gesture playing, and when it began in game seconds */
	EVaelCastGesture CastGesture = EVaelCastGesture::None;
	float CastGestureStartTime = 0.0f;

	/** Flight state of the Floromant, smoothed over the frames; vectors in the world */
	FVaelFlightPose FlightPose;
	FVector FlightLastVelocity = FVector::ZeroVector;
	FVector FlightAccel = FVector::ZeroVector;
	FVector FlightLean = FVector::ZeroVector;
	FVector FlightLegLag = FVector::ZeroVector;
	FVector FlightLegLagSpeed = FVector::ZeroVector;
	float FlightSpeedShare = 0.0f;

	/** Walking speed while not channeling a spell */
	float DefaultWalkSpeed = 0.0f;

	/** Normalized direction of the current dodge roll */
	FVector DodgeDirection = FVector::ZeroVector;

	/** World time at which the current dodge roll ends */
	float DodgeEndTime = 0.0f;

	/** World time from which the next dodge roll is allowed */
	float NextDodgeTime = 0.0f;

	/** True while a dodge roll is in progress */
	bool bIsDodging = false;

	/** Share of the walk speed while something weighs the player down, and the world time it lasts until */
	float BurdenMultiplier = 1.0f;
	float BurdenEndTime = 0.0f;

	/** Speed of the current roll or dash in cm/s */
	float CurrentDodgeSpeed = 0.0f;

	/** What the end of the current spell dash does, only used while the burst radius is above 0 */
	FVaelSpellHit DashBurst;

	/** Radius of the burst at the end of the current spell dash, 0 for a plain dodge roll */
	float DashBurstRadius = 0.0f;

	/** Where the current spell dash began, for the look of its burst */
	FVector DashStartLocation = FVector::ZeroVector;

	/** Shows the burst at the end of a spell dash: lightning or a ring of wind, the outline as placeholder for everything else */
	void ShowDashBurst();

	/** True while the spyglass is raised, and where it is aimed */
	bool bObserving = false;
	FVector FocusLocation = FVector::ZeroVector;

	/** Moves the focus circle of the spyglass with the aim */
	void UpdateFocus();
};
