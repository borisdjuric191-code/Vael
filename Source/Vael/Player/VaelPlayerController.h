// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Magic/VaelElementTypes.h"
#include "UI/VaelUISettings.h"
#include "VaelPlayerController.generated.h"

/** Pages of the menu that pauses the game */
UENUM(BlueprintType)
enum class EVaelMenuPage : uint8
{
	/** The grimoire of the group: formulas and quick slots */
	Formulas,
	/** Equipment and backpack of the player */
	Inventory,
	/** Albrun's compendium of creatures, plants and stones */
	Compendium,
	/** The quests of the group */
	Quests,
	/** Class and body of the player: mage or Floromant, man or woman */
	Hero,
	/** A trader's goods, only reached by talking to the trader; not part of the page ring */
	Shop
};

class AVaelSharedCamera;
class UVaelAutoAimComponent;
class UVaelFormula;
class UVaelShop;
class UVaelQuest;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

/**
 *  Player controller for direct twin-stick style controls.
 *  Left stick or WASD moves, right stick or mouse aims, the character turns towards the aim direction.
 *  Face buttons or 1-4 queue elements, right trigger or left click casts them.
 *  Each class plays differently: the shared controls (IMC_VaelPlayer) carry the layout of the class played (IMC_Magier or IMC_Floromant).
 *  Input actions and their key mappings are created in code, no input assets are needed.
 */
UCLASS()
class AVaelPlayerController : public APlayerController
{
	GENERATED_BODY()

	/** Soft lock on enemies while aiming with a gamepad */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UVaelAutoAimComponent> AutoAim;

protected:

	/** Stick deflection below which gamepad input is ignored */
	UPROPERTY(EditAnywhere, Category="Input", meta = (ClampMin = 0, ClampMax = 0.9))
	float StickDeadZone = 0.25f;

	/** Time the leave button has to be held until a co-op player leaves, in seconds */
	UPROPERTY(EditAnywhere, Category="Input", meta = (ClampMin = 0))
	float LeaveHoldTime = 1.0f;

	/** Mouse movement in pixels that switches aiming from the stick to the mouse */
	UPROPERTY(EditAnywhere, Category="Input", meta = (ClampMin = 0))
	float MouseAimThreshold = 3.0f;

	/** MappingContext */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MappingContext;

	/** Move Input Action for WASD */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveKeyboardAction;

	/** Move Input Action for the left stick */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveGamepadAction;

	/** Aim Input Action for the right stick */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> AimStickAction;

	/** Dodge Input Action */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> DodgeAction;

	/** Leave co-op Input Action */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LeaveAction;

	/** Queue Element Input Actions: fire, water, earth, air, Mark */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> ElementActions;

	/** Cast the queued elements Input Action */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> CastAction;

	/** Discard the queued elements Input Action */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ClearQueueAction;

	/** Interact Input Action: use a chest or another thing close by */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> InteractAction;

	/** Drink a healing potion Input Action: Q; on the gamepad L1 / LB drinks while no element is queued */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PotionAction;

	/** Cast a quick slot Input Actions: Z, X, C, V and the d-pad up, right, down, left */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> QuickSlotActions;

	/** Open and close the grimoire Input Action */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> GrimoireAction;

	/** Open and close the big map Input Action: View / Create button or M */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MapAction;

	/** Mapping context active while the grimoire is open, above the game controls */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MenuMappingContext;

	/** Move the selection in a menu Input Action */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MenuNavigateAction;

	/** Close a menu Input Action */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MenuCloseAction;

	/** Put the selected formula on a quick slot Input Actions, same keys as the quick slots */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> MenuAssignActions;

	/** Switch the menu page Input Action: L1 / R1 (LB / RB), Q / E */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MenuPageAction;

	/** Confirm in the menu Input Action: Cross / A, Enter, Space */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MenuConfirmAction;

	/** Open the page of class and body Input Action: K */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> HeroAction;

	/** Open the inventory Input Action: I */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> InventoryAction;

	/** Spyglass Input Action, held: left stick click or F */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SpyglassAction;

	/** Controls only the mage has (elements, casting, quick slots, potion, interact, dodge, grimoire, spyglass); the classes without own controls use them too */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MageMappingContext;

	/** Controls of the Floromant, a layout of its own: the hands fire, take root as towers and get their plants from the seed belt */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> FloromantMappingContext;

	/** Floromant: fire the left / right hand while held (L1 / R1, left / right mouse button) */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FireLeftAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FireRightAction;

	/** Floromant: cross both hands while both fire (Cross, Space); tapped a fast shot, held a charged one */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> CrossAction;

	/** Floromant: the left / right hand takes root as a tower (L2 / R2, Q / E); both together are the Ruf des Gartens */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> TowerLeftAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> TowerRightAction;

	/** Floromant: page up / down through the seed belt (d-pad up / down tapped, mouse wheel) */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> BeltUpAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> BeltDownAction;

	/** Floromant: lay the picked plant on the left / right hand (d-pad left / right, 1 / 2) */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PutLeftAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PutRightAction;

	/** Floromant: reload with Keimpelz (Triangle held, R held) */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ReloadAction;

	/** Floromant: the plants of both hands change sides (Triangle tapped, R tapped) */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SwapAction;

	/** Floromant: take root or let go (Square, F) */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> RootAction;

	/** Floromant: Herbarium and repertoire (touchpad, Tab) */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> HerbariumAction;

	/** Floromant: rise high or sink back (R3, Left Ctrl) */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> RiseAction;

	/** Floromant: healing potion (d-pad down held, H) */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FloromantPotionAction;

	/** Input Mapping Context while a dialogue is shown */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> DialogueMappingContext;

	/** Next line of a dialogue Input Action: E, Enter, Space, left mouse button, Cross / A, L1 / LB */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> DialogueContinueAction;

	/** Moves through the answers of a dialogue Input Action: W/S, arrow keys, left stick, D-pad */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> DialogueSelectAction;

	/** Seconds between two steps while the selection is held in one direction */
	UPROPERTY(EditAnywhere, Category="Input", meta = (ClampMin = 0.05))
	float MenuRepeatInterval = 0.12f;

	/** Seconds after a dialogue opens before a press shows the next line */
	UPROPERTY(EditAnywhere, Category="Input", meta = (ClampMin = 0))
	float DialogueInputDelay = 0.25f;

	/** Seconds a direction has to be held before the selection starts to repeat */
	UPROPERTY(EditAnywhere, Category="Input", meta = (ClampMin = 0.05))
	float MenuRepeatDelay = 0.3f;

public:

	/** Constructor */
	AVaelPlayerController();

	/** Slot of this player (0-3), decides color and spawn spot. Assigned by the game mode. */
	int32 GetPlayerSlot() const { return PlayerSlot; }
	void SetPlayerSlot(int32 NewPlayerSlot) { PlayerSlot = NewPlayerSlot; }

	/** True for the first local player, who owns keyboard and mouse and can't leave */
	bool IsFirstLocalPlayer() const;

	/** Button symbols for the device this player used last */
	EVaelInputGlyphs GetInputGlyphs() const;

	/** Ground point under the mouse cursor while this player aims with the mouse. Returns false when aiming with a gamepad. */
	bool GetMouseAimLocation(FVector& OutLocation) const;

	/** The enemy the gamepad aim is locked on, null if there is none or this player aims with the mouse */
	AActor* GetAimTarget() const;

	/** True while this player has the grimoire open */
	bool IsGrimoireOpen() const { return bGrimoireOpen; }

	/** True while this player has the big map open; the game goes on under it */
	bool IsMapOpen() const { return bMapOpen; }

	/** Opens or closes the big map; it stays shut while the grimoire is open */
	void ToggleMap();

	/** Index of the selected formula in the grimoire, in the order of all formulas */
	int32 GetGrimoireSelection() const { return GrimoireSelection; }

	/** Page of the open menu */
	EVaelMenuPage GetMenuPage() const { return MenuPage; }

	/** Selected row of the inventory page: the ten equipment places first, then the fields of the backpack */
	int32 GetInventorySelection() const { return InventorySelection; }

	/** Row of the compendium page the player has selected */
	int32 GetCompendiumSelection() const { return CompendiumSelection; }

	/** Row of the page of class and body the player has selected, in the order of UVaelHeroSubsystem::GetListedChoice */
	int32 GetHeroSelection() const { return HeroSelection; }

	/** Row of the shop page the player has selected */
	int32 GetShopSelection() const { return ShopSelection; }

	/** Shop open on the shop page */
	const UVaelShop* GetOpenShop() const { return OpenShop.Get(); }

	/** Opens the shop page for this player right after the current dialogue, or at once without one */
	void OpenShopAfterDialogue(const UVaelShop* Shop);

	/** What the earth button of the gamepad uses instead of queueing earth right now: something close by, outside of fights or in a safe zone; null if nothing */
	AActor* GetPadInteractTarget() const;

	/** Shows lines of a person one after the other and pauses the game, until the player has read them all. Returns false while the game is paused. */
	bool StartDialogue(const FText& Speaker, const TArray<FText>& Lines);

	/** True while this player reads a dialogue */
	bool IsInDialogue() const { return !DialogueLines.IsEmpty(); }

	/** Closes the dialogue at once, as if all lines were read, and lets the game run again */
	void EndDialogue();

	/** Name of the person speaking */
	const FText& GetDialogueSpeaker() const { return DialogueSpeaker; }

	/** The line shown right now */
	FText GetDialogueLine() const { return DialogueLines.IsValidIndex(DialogueIndex) ? DialogueLines[DialogueIndex] : FText::GetEmpty(); }

	/** After the lines of the current dialogue the player picks one of the answers of the quest's current step, or puts the decision off */
	void OfferChoiceAfterDialogue(const UVaelQuest* Quest);

	/** True while the answers of a quest are shown under the line */
	bool IsChoosing() const;

	/** Labels of the answers shown; the last one puts the decision off */
	TArray<FText> GetChoiceLabels() const;

	/** Answer the player has selected */
	int32 GetChoiceSelection() const { return ChoiceSelection; }

	/** Opens the grimoire and pauses the game. Returns false if another player has it open. */
	bool OpenGrimoire(EVaelMenuPage Page = EVaelMenuPage::Formulas);

	/** Closes the grimoire and lets the game go on */
	void CloseGrimoire();

protected:

	/** Initialization */
	virtual void BeginPlay() override;

	/** Keeps the view on the shared camera */
	virtual void OnPossess(APawn* InPawn) override;

	/** Stops listening to the cast results of the old pawn */
	virtual void OnUnPossess() override;

	/** Initialize input bindings */
	virtual void SetupInputComponent() override;

	/** Update */
	virtual void PlayerTick(float DeltaTime) override;

	/** In an open world the land streams in around the player's figure, not around the shared camera high above the group */
	virtual void GetStreamingSourceLocationAndRotation(FVector& OutLocation, FRotator& OutRotation) const override;

	/** Creates the input actions and maps them to keys */
	void CreateInputAssets();

	/** Creates the actions of the Floromant and its own layout; part of CreateInputAssets */
	void CreateFloromantInput();

	/** Input handlers */
	void OnMoveKeyboard(const FInputActionValue& Value);
	void OnMoveGamepad(const FInputActionValue& Value);
	void OnAimStick(const FInputActionValue& Value);
	void OnAimStickReleased();
	void OnDodge();
	void OnLeave();
	void OnElement(EVaelElement Element);
	void OnCast();
	void OnCastReleased();
	void OnClearQueue();

	/** Keeps a held Böe jumping while the cast button stays down */
	void UpdateHeldCast();

	/** True while the cast button is down after a cast; whether the jump of the last cast has started, when it was cast, when the next may go */
	bool bCastHeld = false;
	bool bHeldJumpStarted = false;
	float HeldCastTime = 0.0f;
	float NextHeldCastTime = 0.0f;

	/** Discards queued elements, or drinks a potion on the gamepad when none are queued */
	void DiscardOrDrink();

	/** True if the player carries a seed they can sow and no fight is going on */
	bool CanStartSowing() const;

	/** Sows once the button was held long enough */
	void UpdateSowHold();

	/** The held button was let go */
	void OnSowHoldEnd();

	/** Game time at which the button that sows was pressed, negative while it is not held; and whether its tap still has to act when it is let go */
	float SowHoldStart = -1.0f;
	bool bSowHoldDeferred = false;
	void OnInteract();
	void OnPotion();
	void OnQuickSlot(int32 SlotIndex);
	void OnToggleGrimoire();

	void OnMenuNavigate(const FInputActionValue& Value);
	void OnMenuNavigateReleased();
	void OnMenuClose();
	void OnMenuAssign(int32 SlotIndex);
	void OnMenuPage(const FInputActionValue& Value);
	void OnMenuConfirm();
	void OnToggleInventory();
	void OnToggleHero();

	/** Where a Floromant aims on the ground: the mouse, the enemy the gamepad aim is locked on, or a few steps ahead */
	FVector GetFloromantAim() const;

	/** Puts the controls of the class the pawn plays now in place */
	void UpdateClassControls();

	/** True while the controls of the Floromant are in place, false for those of the mage; unset before the first pawn */
	TOptional<bool> bFloromantControls;

	/** Floromant input handlers */
	void OnFireHand(bool bLeft, bool bHeld);
	void OnCrossStarted();
	void OnCrossEnded();
	void OnTower(bool bLeft);
	void OnPageBelt(int32 Step);
	void OnPutOnHand(bool bLeft);
	void OnReload();
	void OnSwapHands();
	void OnRoot();
	void OnHerbarium();
	void OnRise(bool bPressed);

	/** True while an enemy is close: then Cross is the Kreuzschuss, otherwise it uses what is close by or sows */
	bool IsInFight() const;

	/** Which fire buttons are held (left, right), whether this press of Cross uses or sows instead of crossing, and when each
	 *  tower button was pressed (negative while none waits) */
	bool bFireHeld[2] = { false, false };
	bool bCrossUses = false;
	float TowerPressTime[2] = { -1.0f, -1.0f };

	/** Sets single towers once the wait for the other tower button is over, or calls the garden if both came */
	void UpdateTowerChord();
	void OnSpyglassRaised();
	void OnSpyglassLowered();
	void OnDialogueContinue();
	void OnDialogueSelect(const FInputActionValue& Value);

	/** Tells the players what came of a cast */
	void OnCastFinished(EVaelCastResult Result, const UVaelFormula* Formula);

	/** Warns the players that lightning cast while wet hurt the caster */
	void OnLightningBacklash(float Damage);

	/** Moves the pawn by a camera-relative input */
	void ApplyMoveInput(const FVector2D& Input);

	/** Turns a camera-relative input (X right, Y up on screen) into a world direction on the ground */
	FVector InputToWorldDirection(const FVector2D& Input) const;

	/** Turns the pawn towards mouse, right stick or move direction, in that order */
	void UpdateFacing();

	/** Finds the direction from the pawn to the ground point under the mouse cursor */
	bool GetMouseAimDirection(FVector& OutDirection) const;

	/** Makes the shared camera the view target */
	void UseSharedCamera();

	/** Returns the camera shared by all players */
	AVaelSharedCamera* GetSharedCamera() const;

	/** Slot of this player */
	int32 PlayerSlot = INDEX_NONE;

	/** Right stick deflection of this frame */
	FVector2D StickAim = FVector2D::ZeroVector;

	/** World move direction of this frame and of the last frame */
	FVector MoveDirection = FVector::ZeroVector;
	FVector PreviousMoveDirection = FVector::ZeroVector;

	/** Mouse position of the last frame */
	FVector2D LastMousePosition = FVector2D::ZeroVector;

	/** True once LastMousePosition holds a real position */
	bool bHasMousePosition = false;

	/** True if the mouse was used last, false if the gamepad was */
	bool bUsingMouseAim = false;

	/** True while this player has the grimoire open */
	bool bGrimoireOpen = false;

	/** True while this player has the big map open */
	bool bMapOpen = false;

	/** Selected row of the grimoire */
	int32 GrimoireSelection = 0;

	/** Page of the open menu */
	EVaelMenuPage MenuPage = EVaelMenuPage::Formulas;

	/** Selected row of the inventory page */
	int32 InventorySelection = 0;

	/** Row of the compendium page the player has selected, counting through the books in order */
	int32 CompendiumSelection = 0;

	/** Selected row of the page of class and body */
	int32 HeroSelection = 0;

	/** Selected row of the shop page, and the shop it shows */
	int32 ShopSelection = 0;
	TWeakObjectPtr<const UVaelShop> OpenShop;

	/** Shop that opens once the current dialogue is read */
	TWeakObjectPtr<const UVaelShop> PendingShop;

	/** Dialogue being read: who speaks, the lines and the one shown */
	FText DialogueSpeaker;
	TArray<FText> DialogueLines;
	int32 DialogueIndex = 0;

	/** Real time the dialogue started; the press that opened it must not skip the first line */
	double DialogueStartTime = 0.0;

	/** Quest whose current step asks for an answer at the end of the dialogue, and the answer selected */
	TWeakObjectPtr<const UVaelQuest> ChoiceQuest;
	int32 ChoiceSelection = 0;

	/** Number of rows of the inventory page: equipment places and the items in the backpack */
	int32 GetNumInventoryRows() const;

	/** Real time of the next selection step while a direction is held, 0 when nothing is held */
	double NextNavigateTime = 0.0;

	/** Handle of the cast result delegate of the pawn */
	FDelegateHandle CastFinishedHandle;

	/** Handle of the lightning backlash delegate of the pawn */
	FDelegateHandle BacklashHandle;
};
