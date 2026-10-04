// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "VaelPlayerController.generated.h"

class AVaelSharedCamera;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

/**
 *  Player controller for direct twin-stick style controls.
 *  Left stick or WASD moves, right stick or mouse aims, the character turns towards the aim direction.
 *  Input actions and their key mappings are created in code, no input assets are needed.
 */
UCLASS(abstract)
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

public:

	/** Constructor */
	AVaelPlayerController();

	/** Slot of this player (0-3), decides color and spawn spot. Assigned by the game mode. */
	int32 GetPlayerSlot() const { return PlayerSlot; }
	void SetPlayerSlot(int32 NewPlayerSlot) { PlayerSlot = NewPlayerSlot; }

	/** True for the first local player, who owns keyboard and mouse and can't leave */
	bool IsFirstLocalPlayer() const;

protected:

	/** Initialization */
	virtual void BeginPlay() override;

	/** Keeps the view on the shared camera */
	virtual void OnPossess(APawn* InPawn) override;

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
};
