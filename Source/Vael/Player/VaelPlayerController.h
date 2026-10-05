// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Magic/VaelElementTypes.h"
#include "UI/VaelUISettings.h"
#include "VaelPlayerController.generated.h"

class AVaelSharedCamera;
class UVaelFormula;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

/**
 *  Player controller for direct twin-stick style controls.
 *  Left stick or WASD moves, right stick or mouse aims, the character turns towards the aim direction.
 *  Face buttons or 1-4 queue elements, right trigger or left click casts them.
 *  Input actions and their key mappings are created in code, no input assets are needed.
 */
UCLASS()
class AVaelPlayerController : public APlayerController
{
	GENERATED_BODY()

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

	/** Queue Element Input Actions: fire, water, earth, air */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> ElementActions;

	/** Cast the queued elements Input Action */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> CastAction;

	/** Discard the queued elements Input Action */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ClearQueueAction;

	/** Cast a quick slot Input Actions: Z, X, C, V and the d-pad up, right, down, left */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> QuickSlotActions;

	/** Open and close the grimoire Input Action */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> GrimoireAction;

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

	/** Seconds between two steps while the selection is held in one direction */
	UPROPERTY(EditAnywhere, Category="Input", meta = (ClampMin = 0.05))
	float MenuRepeatInterval = 0.12f;

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

	/** True while this player has the grimoire open */
	bool IsGrimoireOpen() const { return bGrimoireOpen; }

	/** Index of the selected formula in the grimoire, in the order of all formulas */
	int32 GetGrimoireSelection() const { return GrimoireSelection; }

	/** Opens the grimoire and pauses the game. Returns false if another player has it open. */
	bool OpenGrimoire();

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

	/** Creates the input actions and maps them to keys */
	void CreateInputAssets();

	/** Input handlers */
	void OnMoveKeyboard(const FInputActionValue& Value);
	void OnMoveGamepad(const FInputActionValue& Value);
	void OnAimStick(const FInputActionValue& Value);
	void OnAimStickReleased();
	void OnDodge();
	void OnLeave();
	void OnElement(EVaelElement Element);
	void OnCast();
	void OnClearQueue();
	void OnQuickSlot(int32 SlotIndex);
	void OnToggleGrimoire();
	void OnMenuNavigate(const FInputActionValue& Value);
	void OnMenuNavigateReleased();
	void OnMenuClose();
	void OnMenuAssign(int32 SlotIndex);

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

	/** Selected row of the grimoire */
	int32 GrimoireSelection = 0;

	/** Real time of the next selection step while a direction is held, 0 when nothing is held */
	double NextNavigateTime = 0.0;

	/** Handle of the cast result delegate of the pawn */
	FDelegateHandle CastFinishedHandle;

	/** Handle of the lightning backlash delegate of the pawn */
	FDelegateHandle BacklashHandle;
};
