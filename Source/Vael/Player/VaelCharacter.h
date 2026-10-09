// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelCombatStatics.h"
#include "Magic/VaelElementTypes.h"
#include "VaelCharacter.generated.h"

class UAnimInstance;
class UAnimMontage;
class UNiagaraComponent;
class UPointLightComponent;
class UStaticMeshComponent;
class UVaelElementComponent;
class UVaelElementOrbitComponent;
class UVaelInventory;
class UVaelMaterialBag;

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

	/** Gets a downed player back on their feet with the given health and some seconds of invulnerability */
	UFUNCTION(BlueprintCallable, Category="Combat")
	void Revive(float Health, float InvulnerableSeconds);

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
	//~End AVaelCharacterBase

	/** Tints the character and its marker */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetPlayerColor(const FLinearColor& Color);

	/** Returns the marker component **/
	UStaticMeshComponent* GetPlayerMarker() const { return PlayerMarker.Get(); }

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
