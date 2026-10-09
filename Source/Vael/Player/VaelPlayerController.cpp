// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/VaelPlayerController.h"
#include "Camera/VaelSharedCamera.h"
#include "Player/VaelAutoAimComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Pawn.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Compendium/VaelCompendiumSubsystem.h"
#include "InputModifiers.h"
#include "InputTriggers.h"
#include "Items/VaelInventory.h"
#include "Magic/VaelElementComponent.h"
#include "Player/VaelCharacter.h"
#include "Player/VaelCheatManager.h"
#include "Player/VaelInteractable.h"
#include "GameFramework/InputDeviceSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Magic/VaelFormula.h"
#include "Magic/VaelGrimoireSubsystem.h"
#include "Magic/VaelMagicSettings.h"
#include "UI/VaelNoticeSubsystem.h"
#include "TimerManager.h"
#include "Vael.h"
#include "VaelGameMode.h"

AVaelPlayerController::AVaelPlayerController()
{
	// configure the controller
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Crosshairs;

	// all players look through the shared camera, never through their own pawn
	bAutoManageActiveCameraTarget = false;

	AutoAim = CreateDefaultSubobject<UVaelAutoAimComponent>(TEXT("AutoAim"));

	CheatClass = UVaelCheatManager::StaticClass();
}

bool AVaelPlayerController::IsFirstLocalPlayer() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const UGameInstance* GameInstance = GetGameInstance();

	return LocalPlayer != nullptr && GameInstance != nullptr && GameInstance->GetFirstGamePlayer() == LocalPlayer;
}

void AVaelPlayerController::BeginPlay()
{
	Super::BeginPlay();

	UseSharedCamera();
}

void AVaelPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	UseSharedCamera();

	if (const AVaelCharacter* VaelCharacter = Cast<AVaelCharacter>(InPawn))
	{
		CastFinishedHandle = VaelCharacter->GetElementComponent()->OnCastFinished.AddUObject(this, &AVaelPlayerController::OnCastFinished);
		BacklashHandle = VaelCharacter->GetElementComponent()->OnLightningBacklash.AddUObject(this, &AVaelPlayerController::OnLightningBacklash);
	}
}

void AVaelPlayerController::OnUnPossess()
{
	if (const AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>())
	{
		VaelCharacter->GetElementComponent()->OnCastFinished.Remove(CastFinishedHandle);
		VaelCharacter->GetElementComponent()->OnLightningBacklash.Remove(BacklashHandle);
	}

	CloseGrimoire();

	Super::OnUnPossess();
}

void AVaelPlayerController::SetupInputComponent()
{
	// set up gameplay key bindings
	Super::SetupInputComponent();

	// Only set up input on local player controllers
	if (IsLocalPlayerController())
	{
		CreateInputAssets();

		// Add Input Mapping Context
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(MappingContext, 0);
		}

		// Set up action bindings
		if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
		{
			EnhancedInputComponent->BindAction(MoveKeyboardAction, ETriggerEvent::Triggered, this, &AVaelPlayerController::OnMoveKeyboard);
			EnhancedInputComponent->BindAction(MoveGamepadAction, ETriggerEvent::Triggered, this, &AVaelPlayerController::OnMoveGamepad);

			EnhancedInputComponent->BindAction(AimStickAction, ETriggerEvent::Triggered, this, &AVaelPlayerController::OnAimStick);
			EnhancedInputComponent->BindAction(AimStickAction, ETriggerEvent::Completed, this, &AVaelPlayerController::OnAimStickReleased);
			EnhancedInputComponent->BindAction(AimStickAction, ETriggerEvent::Canceled, this, &AVaelPlayerController::OnAimStickReleased);

			EnhancedInputComponent->BindAction(DodgeAction, ETriggerEvent::Started, this, &AVaelPlayerController::OnDodge);
			EnhancedInputComponent->BindAction(LeaveAction, ETriggerEvent::Triggered, this, &AVaelPlayerController::OnLeave);

			// The element actions are stored in the order of the element enum
			for (int32 ElementIndex = 0; ElementIndex < ElementActions.Num(); ++ElementIndex)
			{
				EnhancedInputComponent->BindAction(ElementActions[ElementIndex], ETriggerEvent::Started, this, &AVaelPlayerController::OnElement, static_cast<EVaelElement>(ElementIndex));
			}

			EnhancedInputComponent->BindAction(CastAction, ETriggerEvent::Started, this, &AVaelPlayerController::OnCast);
			EnhancedInputComponent->BindAction(ClearQueueAction, ETriggerEvent::Started, this, &AVaelPlayerController::OnClearQueue);
			EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &AVaelPlayerController::OnInteract);

			for (int32 SlotIndex = 0; SlotIndex < QuickSlotActions.Num(); ++SlotIndex)
			{
				EnhancedInputComponent->BindAction(QuickSlotActions[SlotIndex], ETriggerEvent::Started, this, &AVaelPlayerController::OnQuickSlot, SlotIndex);
			}

			EnhancedInputComponent->BindAction(GrimoireAction, ETriggerEvent::Started, this, &AVaelPlayerController::OnToggleGrimoire);
			EnhancedInputComponent->BindAction(MenuNavigateAction, ETriggerEvent::Triggered, this, &AVaelPlayerController::OnMenuNavigate);
			EnhancedInputComponent->BindAction(MenuNavigateAction, ETriggerEvent::Completed, this, &AVaelPlayerController::OnMenuNavigateReleased);
			EnhancedInputComponent->BindAction(MenuCloseAction, ETriggerEvent::Started, this, &AVaelPlayerController::OnMenuClose);
			EnhancedInputComponent->BindAction(MenuPageAction, ETriggerEvent::Started, this, &AVaelPlayerController::OnMenuPage);
			EnhancedInputComponent->BindAction(MenuConfirmAction, ETriggerEvent::Started, this, &AVaelPlayerController::OnMenuConfirm);
			EnhancedInputComponent->BindAction(InventoryAction, ETriggerEvent::Started, this, &AVaelPlayerController::OnToggleInventory);
			EnhancedInputComponent->BindAction(SpyglassAction, ETriggerEvent::Started, this, &AVaelPlayerController::OnSpyglassRaised);
			EnhancedInputComponent->BindAction(SpyglassAction, ETriggerEvent::Completed, this, &AVaelPlayerController::OnSpyglassLowered);
			EnhancedInputComponent->BindAction(SpyglassAction, ETriggerEvent::Canceled, this, &AVaelPlayerController::OnSpyglassLowered);
			EnhancedInputComponent->BindAction(DialogueContinueAction, ETriggerEvent::Started, this, &AVaelPlayerController::OnDialogueContinue);

			for (int32 SlotIndex = 0; SlotIndex < MenuAssignActions.Num(); ++SlotIndex)
			{
				EnhancedInputComponent->BindAction(MenuAssignActions[SlotIndex], ETriggerEvent::Started, this, &AVaelPlayerController::OnMenuAssign, SlotIndex);
			}
		}
		else
		{
			UE_LOG(LogVael, Error, TEXT("'%s' Failed to find an Enhanced Input Component! Vael is built to use the Enhanced Input system."), *GetNameSafe(this));
		}
	}
}

void AVaelPlayerController::CreateInputAssets()
{
	if (MappingContext != nullptr)
	{
		return;
	}

	const auto CreateAction = [this](const TCHAR* Name, EInputActionValueType ValueType)
	{
		UInputAction* Action = NewObject<UInputAction>(this, Name);
		Action->ValueType = ValueType;
		return Action;
	};

	MoveKeyboardAction = CreateAction(TEXT("IA_MoveKeyboard"), EInputActionValueType::Axis2D);
	MoveGamepadAction = CreateAction(TEXT("IA_MoveGamepad"), EInputActionValueType::Axis2D);
	AimStickAction = CreateAction(TEXT("IA_AimStick"), EInputActionValueType::Axis2D);
	DodgeAction = CreateAction(TEXT("IA_Dodge"), EInputActionValueType::Boolean);
	LeaveAction = CreateAction(TEXT("IA_Leave"), EInputActionValueType::Boolean);

	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_VaelPlayer"));

	// WASD: a key press arrives on the X axis, swizzle moves it to Y, negate flips the sign
	MappingContext->MapKey(MoveKeyboardAction, EKeys::D);
	MappingContext->MapKey(MoveKeyboardAction, EKeys::A).Modifiers.Add(NewObject<UInputModifierNegate>(MappingContext));
	MappingContext->MapKey(MoveKeyboardAction, EKeys::W).Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(MappingContext));

	FEnhancedActionKeyMapping& BackMapping = MappingContext->MapKey(MoveKeyboardAction, EKeys::S);
	BackMapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(MappingContext));
	BackMapping.Modifiers.Add(NewObject<UInputModifierNegate>(MappingContext));

	// Sticks
	const auto MapStick = [this](const UInputAction* Action, const FKey& StickKey, bool bInvertY)
	{
		UInputModifierDeadZone* DeadZone = NewObject<UInputModifierDeadZone>(MappingContext);
		DeadZone->LowerThreshold = StickDeadZone;

		FEnhancedActionKeyMapping& StickMapping = MappingContext->MapKey(Action, StickKey);
		StickMapping.Modifiers.Add(DeadZone);

		if (bInvertY)
		{
			UInputModifierNegate* InvertY = NewObject<UInputModifierNegate>(MappingContext);
			InvertY->bX = false;
			InvertY->bY = true;
			InvertY->bZ = false;

			StickMapping.Modifiers.Add(InvertY);
		}
	};

	MapStick(MoveGamepadAction, EKeys::Gamepad_Left2D, false);

	// The right stick reports up as negative Y, unlike the left one
	MapStick(AimStickAction, EKeys::Gamepad_Right2D, true);

	// Dodge roll
	MappingContext->MapKey(DodgeAction, EKeys::Gamepad_LeftTrigger);
	MappingContext->MapKey(DodgeAction, EKeys::SpaceBar);

	// Leave co-op by holding Circle / B
	UInputTriggerHold* HoldTrigger = NewObject<UInputTriggerHold>(MappingContext);
	HoldTrigger->HoldTimeThreshold = LeaveHoldTime;
	HoldTrigger->bIsOneShot = true;

	MappingContext->MapKey(LeaveAction, EKeys::Gamepad_FaceButton_Right).Triggers.Add(HoldTrigger);

	// Elements, laid out like the browser prototype: fire, water, earth, air on Circle, Square, Cross, Triangle (B, X, A, Y) and on 1-4, Mark on R1 (RB) and 5.
	// Circle / B is shared with leaving co-op: a tap queues fire, holding it leaves.
	const FKey ElementPadKeys[] = { EKeys::Gamepad_FaceButton_Right, EKeys::Gamepad_FaceButton_Left, EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_FaceButton_Top, EKeys::Gamepad_RightShoulder };
	const FKey ElementKeyboardKeys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five };
	const TCHAR* ElementActionNames[] = { TEXT("IA_Element_Fire"), TEXT("IA_Element_Water"), TEXT("IA_Element_Earth"), TEXT("IA_Element_Air"), TEXT("IA_Element_Mark") };

	const int32 NumElementActions = UE_ARRAY_COUNT(ElementActionNames);
	for (int32 ElementIndex = 0; ElementIndex < NumElementActions; ++ElementIndex)
	{
		UInputAction* ElementAction = CreateAction(ElementActionNames[ElementIndex], EInputActionValueType::Boolean);
		ElementActions.Add(ElementAction);

		MappingContext->MapKey(ElementAction, ElementPadKeys[ElementIndex]);
		MappingContext->MapKey(ElementAction, ElementKeyboardKeys[ElementIndex]);
	}

	// Cast and discard the queued elements
	CastAction = CreateAction(TEXT("IA_Cast"), EInputActionValueType::Boolean);
	MappingContext->MapKey(CastAction, EKeys::Gamepad_RightTrigger);
	MappingContext->MapKey(CastAction, EKeys::LeftMouseButton);

	ClearQueueAction = CreateAction(TEXT("IA_ClearQueue"), EInputActionValueType::Boolean);
	MappingContext->MapKey(ClearQueueAction, EKeys::Gamepad_LeftShoulder);
	MappingContext->MapKey(ClearQueueAction, EKeys::RightMouseButton);

	// Interact: E; on the gamepad L1 / LB, which interacts when something is close and discards the combo otherwise, like the prototype
	InteractAction = CreateAction(TEXT("IA_Interact"), EInputActionValueType::Boolean);
	MappingContext->MapKey(InteractAction, EKeys::E);

	// Quick slots on Z, X, C, V and the d-pad clockwise from up, like the browser prototype
	const FKey QuickSlotPadKeys[] = { EKeys::Gamepad_DPad_Up, EKeys::Gamepad_DPad_Right, EKeys::Gamepad_DPad_Down, EKeys::Gamepad_DPad_Left };
	const FKey QuickSlotKeyboardKeys[] = { EKeys::Z, EKeys::X, EKeys::C, EKeys::V };

	for (int32 SlotIndex = 0; SlotIndex < UE_ARRAY_COUNT(QuickSlotPadKeys); ++SlotIndex)
	{
		UInputAction* QuickSlotAction = CreateAction(*FString::Printf(TEXT("IA_QuickSlot%d"), SlotIndex + 1), EInputActionValueType::Boolean);
		QuickSlotActions.Add(QuickSlotAction);

		MappingContext->MapKey(QuickSlotAction, QuickSlotPadKeys[SlotIndex]);
		MappingContext->MapKey(QuickSlotAction, QuickSlotKeyboardKeys[SlotIndex]);
	}

	// The grimoire opens and closes while the game is paused
	GrimoireAction = CreateAction(TEXT("IA_Grimoire"), EInputActionValueType::Boolean);
	GrimoireAction->bTriggerWhenPaused = true;
	MappingContext->MapKey(GrimoireAction, EKeys::Gamepad_Special_Left);
	MappingContext->MapKey(GrimoireAction, EKeys::Tab);

	// The inventory page of the same menu opens directly with I
	InventoryAction = CreateAction(TEXT("IA_Inventory"), EInputActionValueType::Boolean);
	InventoryAction->bTriggerWhenPaused = true;
	MappingContext->MapKey(InventoryAction, EKeys::I);

	// The spyglass is held up while the button is held
	SpyglassAction = CreateAction(TEXT("IA_Spyglass"), EInputActionValueType::Boolean);
	MappingContext->MapKey(SpyglassAction, EKeys::Gamepad_LeftThumbstick);
	MappingContext->MapKey(SpyglassAction, EKeys::F);

	// Menu controls, only active while the grimoire is open
	MenuMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_VaelMenu"));

	MenuNavigateAction = CreateAction(TEXT("IA_MenuNavigate"), EInputActionValueType::Axis1D);
	MenuNavigateAction->bTriggerWhenPaused = true;

	UInputModifierDeadZone* NavigateDeadZone = NewObject<UInputModifierDeadZone>(MenuMappingContext);
	NavigateDeadZone->LowerThreshold = 0.5f;
	MenuMappingContext->MapKey(MenuNavigateAction, EKeys::Gamepad_LeftY).Modifiers.Add(NavigateDeadZone);
	MenuMappingContext->MapKey(MenuNavigateAction, EKeys::W);
	MenuMappingContext->MapKey(MenuNavigateAction, EKeys::Up);
	MenuMappingContext->MapKey(MenuNavigateAction, EKeys::S).Modifiers.Add(NewObject<UInputModifierNegate>(MenuMappingContext));
	MenuMappingContext->MapKey(MenuNavigateAction, EKeys::Down).Modifiers.Add(NewObject<UInputModifierNegate>(MenuMappingContext));

	MenuCloseAction = CreateAction(TEXT("IA_MenuClose"), EInputActionValueType::Boolean);
	MenuCloseAction->bTriggerWhenPaused = true;
	MenuMappingContext->MapKey(MenuCloseAction, EKeys::Gamepad_FaceButton_Right);
	MenuMappingContext->MapKey(MenuCloseAction, EKeys::Escape);

	// Pages: L1 / R1 (LB / RB) and Q / E, left is negative
	MenuPageAction = CreateAction(TEXT("IA_MenuPage"), EInputActionValueType::Axis1D);
	MenuPageAction->bTriggerWhenPaused = true;
	MenuMappingContext->MapKey(MenuPageAction, EKeys::Gamepad_RightShoulder);
	MenuMappingContext->MapKey(MenuPageAction, EKeys::E);
	MenuMappingContext->MapKey(MenuPageAction, EKeys::Gamepad_LeftShoulder).Modifiers.Add(NewObject<UInputModifierNegate>(MenuMappingContext));
	MenuMappingContext->MapKey(MenuPageAction, EKeys::Q).Modifiers.Add(NewObject<UInputModifierNegate>(MenuMappingContext));

	// Confirm: put on or take off the selected item
	MenuConfirmAction = CreateAction(TEXT("IA_MenuConfirm"), EInputActionValueType::Boolean);
	MenuConfirmAction->bTriggerWhenPaused = true;
	MenuMappingContext->MapKey(MenuConfirmAction, EKeys::Gamepad_FaceButton_Bottom);
	MenuMappingContext->MapKey(MenuConfirmAction, EKeys::Enter);
	MenuMappingContext->MapKey(MenuConfirmAction, EKeys::SpaceBar);

	// Dialogues: every button that talks or confirms shows the next line
	DialogueMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_VaelDialogue"));
	DialogueContinueAction = CreateAction(TEXT("IA_DialogueContinue"), EInputActionValueType::Boolean);
	DialogueContinueAction->bTriggerWhenPaused = true;

	for (const FKey& ContinueKey : { EKeys::E, EKeys::Enter, EKeys::SpaceBar, EKeys::LeftMouseButton, EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_LeftShoulder })
	{
		DialogueMappingContext->MapKey(DialogueContinueAction, ContinueKey);
	}

	for (int32 SlotIndex = 0; SlotIndex < UE_ARRAY_COUNT(QuickSlotPadKeys); ++SlotIndex)
	{
		UInputAction* AssignAction = CreateAction(*FString::Printf(TEXT("IA_MenuAssign%d"), SlotIndex + 1), EInputActionValueType::Boolean);
		AssignAction->bTriggerWhenPaused = true;
		MenuAssignActions.Add(AssignAction);

		MenuMappingContext->MapKey(AssignAction, QuickSlotPadKeys[SlotIndex]);
		MenuMappingContext->MapKey(AssignAction, QuickSlotKeyboardKeys[SlotIndex]);
	}
}

void AVaelPlayerController::PlayerTick(float DeltaTime)
{
	PreviousMoveDirection = MoveDirection;
	MoveDirection = FVector::ZeroVector;

	// Input handlers run in here
	Super::PlayerTick(DeltaTime);

	// Moving the mouse switches aiming over to the cursor
	float MouseX = 0.f;
	float MouseY = 0.f;
	if (IsFirstLocalPlayer() && GetMousePosition(MouseX, MouseY))
	{
		const FVector2D MousePosition(MouseX, MouseY);
		if (!bHasMousePosition)
		{
			LastMousePosition = MousePosition;
			bHasMousePosition = true;
		}
		else if (!MousePosition.Equals(LastMousePosition, MouseAimThreshold))
		{
			LastMousePosition = MousePosition;
			bUsingMouseAim = true;
		}
	}

	// Gamepad players lock on to enemies; mouse players aim exactly where they point
	const bool bStickAims = StickAim.Size() > 0.5f;
	AutoAim->UpdateTarget(DeltaTime, GetPawn(), bStickAims ? InputToWorldDirection(StickAim) : FVector::ZeroVector, !bUsingMouseAim);

	UpdateFacing();
}

AActor* AVaelPlayerController::GetAimTarget() const
{
	return AutoAim != nullptr ? AutoAim->GetTarget() : nullptr;
}

void AVaelPlayerController::OnMoveKeyboard(const FInputActionValue& Value)
{
	bUsingMouseAim = IsFirstLocalPlayer();
	ApplyMoveInput(Value.Get<FVector2D>());
}

void AVaelPlayerController::OnMoveGamepad(const FInputActionValue& Value)
{
	bUsingMouseAim = false;
	ApplyMoveInput(Value.Get<FVector2D>());
}

void AVaelPlayerController::OnAimStick(const FInputActionValue& Value)
{
	bUsingMouseAim = false;
	StickAim = Value.Get<FVector2D>();
}

void AVaelPlayerController::OnAimStickReleased()
{
	StickAim = FVector2D::ZeroVector;
}

void AVaelPlayerController::OnDodge()
{
	if (AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>())
	{
		// The move input of this frame may not have been handled yet
		VaelCharacter->StartDodge(MoveDirection.IsNearlyZero() ? PreviousMoveDirection : MoveDirection);
	}
}

void AVaelPlayerController::OnLeave()
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UGameInstance* GameInstance = GetGameInstance();

	if (LocalPlayer == nullptr || GameInstance == nullptr || IsFirstLocalPlayer())
	{
		return;
	}

	// Removing the player destroys this controller, so wait until input handling is over
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(GameInstance,
		[GameInstance, WeakLocalPlayer = TWeakObjectPtr<ULocalPlayer>(LocalPlayer)]()
		{
			if (ULocalPlayer* LeavingPlayer = WeakLocalPlayer.Get())
			{
				GameInstance->RemoveLocalPlayer(LeavingPlayer);
			}
		}));
}

void AVaelPlayerController::OnElement(EVaelElement Element)
{
	const AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>();
	if (VaelCharacter != nullptr && !VaelCharacter->IsDodging() && !VaelCharacter->IsDowned())
	{
		VaelCharacter->GetElementComponent()->AddElement(Element);
	}
}

void AVaelPlayerController::OnCast()
{
	const AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>();
	if (VaelCharacter != nullptr && !VaelCharacter->IsDodging() && !VaelCharacter->IsDowned())
	{
		// Bring the aim up to date so the spell flies where the player points right now
		UpdateFacing();
		VaelCharacter->GetElementComponent()->CastQueue();
	}
}

void AVaelPlayerController::OnClearQueue()
{
	AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>();
	if (VaelCharacter == nullptr)
	{
		return;
	}

	// L1 / LB uses what is close by, otherwise it discards the combo; the right mouse button only discards
	IVaelInteractable* Interactable = Cast<IVaelInteractable>(UVaelInteractionSubsystem::FindNearest(VaelCharacter));
	if (Interactable != nullptr && GetInputGlyphs() != EVaelInputGlyphs::Keyboard)
	{
		Interactable->Interact(VaelCharacter);
		return;
	}

	VaelCharacter->GetElementComponent()->ClearQueue();
}

void AVaelPlayerController::OnInteract()
{
	AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>();
	if (IVaelInteractable* Interactable = Cast<IVaelInteractable>(UVaelInteractionSubsystem::FindNearest(VaelCharacter)))
	{
		Interactable->Interact(VaelCharacter);
	}
}

void AVaelPlayerController::ApplyMoveInput(const FVector2D& Input)
{
	AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>();
	if (VaelCharacter == nullptr || VaelCharacter->IsDodging() || VaelCharacter->IsDowned())
	{
		return;
	}

	FVector Direction = InputToWorldDirection(Input).GetClampedToMaxSize(1.0f);

	// Don't walk out of the area the shared camera can show
	if (const AVaelSharedCamera* SharedCamera = GetSharedCamera())
	{
		Direction = SharedCamera->ConstrainMoveDirection(VaelCharacter->GetActorLocation(), Direction);
	}

	VaelCharacter->AddMovementInput(Direction, 1.0f, false);
	MoveDirection += Direction;

	UE_LOG(LogVael, VeryVerbose, TEXT("Player slot %d: move input %s, pawn at %s"), PlayerSlot, *Input.ToString(), *VaelCharacter->GetActorLocation().ToCompactString());
}

FVector AVaelPlayerController::InputToWorldDirection(const FVector2D& Input) const
{
	const AVaelSharedCamera* SharedCamera = GetSharedCamera();
	const FRotationMatrix YawMatrix(FRotator(0.f, SharedCamera != nullptr ? SharedCamera->GetCameraYaw() : 0.f, 0.f));

	return YawMatrix.GetUnitAxis(EAxis::X) * Input.Y + YawMatrix.GetUnitAxis(EAxis::Y) * Input.X;
}

void AVaelPlayerController::UpdateFacing()
{
	const AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>();
	if (VaelCharacter == nullptr)
	{
		return;
	}

	FVector Facing = FVector::ZeroVector;

	const AActor* AimTarget = GetAimTarget();

	if (bUsingMouseAim)
	{
		GetMouseAimDirection(Facing);
	}
	else if (AimTarget != nullptr)
	{
		Facing = AimTarget->GetActorLocation() - VaelCharacter->GetActorLocation();
	}
	else if (!StickAim.IsNearlyZero())
	{
		Facing = InputToWorldDirection(StickAim);
	}
	else
	{
		Facing = MoveDirection;
	}

	Facing = Facing.GetSafeNormal2D();
	if (!Facing.IsNearlyZero())
	{
		// The character movement component turns the pawn towards the control rotation
		SetControlRotation(FRotator(0.f, Facing.Rotation().Yaw, 0.f));
	}
}

bool AVaelPlayerController::GetMouseAimDirection(FVector& OutDirection) const
{
	const APawn* ControlledPawn = GetPawn();

	FVector RayOrigin;
	FVector RayDirection;
	if (ControlledPawn == nullptr || !DeprojectMousePositionToWorld(RayOrigin, RayDirection) || FMath::IsNearlyZero(RayDirection.Z))
	{
		return false;
	}

	// Intersect the cursor ray with the ground plane at the pawn's feet
	const FVector PawnLocation = ControlledPawn->GetActorLocation();
	const float GroundHeight = PawnLocation.Z - ControlledPawn->GetSimpleCollisionHalfHeight();
	const float RayLength = (GroundHeight - RayOrigin.Z) / RayDirection.Z;
	if (RayLength <= 0.f)
	{
		return false;
	}

	OutDirection = RayOrigin + RayDirection * RayLength - PawnLocation;
	OutDirection.Z = 0.f;

	return !OutDirection.IsNearlyZero();
}

bool AVaelPlayerController::GetMouseAimLocation(FVector& OutLocation) const
{
	FVector Direction;
	if (!bUsingMouseAim || !GetMouseAimDirection(Direction))
	{
		return false;
	}

	OutLocation = GetPawn()->GetActorLocation() + Direction;
	return true;
}

void AVaelPlayerController::UseSharedCamera()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (AVaelSharedCamera* SharedCamera = GetSharedCamera())
	{
		SetViewTarget(SharedCamera);
	}
}

AVaelSharedCamera* AVaelPlayerController::GetSharedCamera() const
{
	AVaelGameMode* GameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<AVaelGameMode>() : nullptr;

	return GameMode != nullptr ? GameMode->GetSharedCamera() : nullptr;
}

EVaelInputGlyphs AVaelPlayerController::GetInputGlyphs() const
{
	// Without information the first player uses keyboard and mouse, everybody else a controller
	bool bUsesGamepad = !IsFirstLocalPlayer();
	FString DeviceName;

	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const UInputDeviceSubsystem* InputDevices = UInputDeviceSubsystem::Get();
	if (LocalPlayer != nullptr && InputDevices != nullptr)
	{
		const FHardwareDeviceIdentifier Device = InputDevices->GetMostRecentlyUsedHardwareDevice(LocalPlayer->GetPlatformUserId());
		if (Device.PrimaryDeviceType == EHardwareDevicePrimaryType::Gamepad)
		{
			bUsesGamepad = true;
			DeviceName = Device.HardwareDeviceIdentifier.ToString() + TEXT(" ") + Device.InputClassName.ToString();
		}
		else if (Device.PrimaryDeviceType == EHardwareDevicePrimaryType::KeyboardAndMouse)
		{
			bUsesGamepad = false;
		}
	}

	if (!bUsesGamepad)
	{
		return EVaelInputGlyphs::Keyboard;
	}

	switch (UVaelUISettings::Get()->GamepadGlyphs)
	{
	case EVaelGamepadGlyphPreference::Xbox:
		return EVaelInputGlyphs::Xbox;
	case EVaelGamepadGlyphPreference::PlayStation:
		return EVaelInputGlyphs::PlayStation;
	default:
		break;
	}

	const bool bPlayStation = DeviceName.Contains(TEXT("Dual")) || DeviceName.Contains(TEXT("PlayStation")) || DeviceName.Contains(TEXT("PS4")) || DeviceName.Contains(TEXT("PS5"));
	return bPlayStation ? EVaelInputGlyphs::PlayStation : EVaelInputGlyphs::Xbox;
}

void AVaelPlayerController::OnQuickSlot(int32 SlotIndex)
{
	const AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>();
	if (VaelCharacter != nullptr && !VaelCharacter->IsDodging() && !VaelCharacter->IsDowned() && !UGameplayStatics::IsGamePaused(this))
	{
		// Bring the aim up to date so the spell flies where the player points right now
		UpdateFacing();
		VaelCharacter->GetElementComponent()->CastQuickSlot(SlotIndex);
	}
}

void AVaelPlayerController::OnToggleGrimoire()
{
	if (bGrimoireOpen)
	{
		CloseGrimoire();
	}
	else
	{
		OpenGrimoire();
	}
}

bool AVaelPlayerController::OpenGrimoire(EVaelMenuPage Page)
{
	if (bGrimoireOpen || GetPawn<AVaelCharacter>() == nullptr)
	{
		return false;
	}

	// One grimoire for the group: nobody else may open it while the game is paused
	if (UGameplayStatics::IsGamePaused(this))
	{
		return false;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (InputSubsystem == nullptr || MenuMappingContext == nullptr)
	{
		return false;
	}

	bGrimoireOpen = true;
	MenuPage = Page;
	NextNavigateTime = 0.0;
	InputSubsystem->AddMappingContext(MenuMappingContext, 1);
	UGameplayStatics::SetGamePaused(this, true);

	return true;
}

void AVaelPlayerController::CloseGrimoire()
{
	if (!bGrimoireOpen)
	{
		return;
	}

	bGrimoireOpen = false;

	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		InputSubsystem->RemoveMappingContext(MenuMappingContext);
	}

	UGameplayStatics::SetGamePaused(this, false);
}

void AVaelPlayerController::OnMenuNavigate(const FInputActionValue& Value)
{
	const float Direction = Value.Get<float>();
	const UGameInstance* GameInstance = GetGameInstance();
	const UVaelGrimoireSubsystem* Grimoire = GameInstance != nullptr ? GameInstance->GetSubsystem<UVaelGrimoireSubsystem>() : nullptr;

	if (!bGrimoireOpen || Grimoire == nullptr || FMath::Abs(Direction) < 0.5f)
	{
		return;
	}

	// One step right away, then repeating while held
	const double Now = FPlatformTime::Seconds();
	if (NextNavigateTime > 0.0 && Now < NextNavigateTime)
	{
		return;
	}

	NextNavigateTime = Now + (NextNavigateTime > 0.0 ? MenuRepeatInterval : MenuRepeatDelay);

	// Up on the stick or key is positive and moves the selection up the list
	const int32 Step = Direction > 0.0f ? -1 : 1;

	if (MenuPage == EVaelMenuPage::Inventory)
	{
		InventorySelection = FMath::Clamp(InventorySelection + Step, 0, FMath::Max(GetNumInventoryRows() - 1, 0));
		return;
	}

	if (MenuPage == EVaelMenuPage::Compendium)
	{
		const UVaelCompendiumSubsystem* Compendium = UVaelCompendiumSubsystem::Get(this);
		int32 NumEntries = 0;
		for (const EVaelCompendiumBook Book : UVaelCompendiumSubsystem::GetBooks())
		{
			NumEntries += Compendium != nullptr ? Compendium->GetEntries(Book).Num() : 0;
		}

		CompendiumSelection = FMath::Clamp(CompendiumSelection + Step, 0, FMath::Max(NumEntries - 1, 0));
		return;
	}

	const int32 NumFormulas = Grimoire->GetAllFormulas().Num();
	GrimoireSelection = FMath::Clamp(GrimoireSelection + Step, 0, FMath::Max(NumFormulas - 1, 0));
}

void AVaelPlayerController::OnMenuPage(const FInputActionValue& Value)
{
	if (bGrimoireOpen)
	{
		// The pages in a ring: right goes on, left goes back
		const int32 NumPages = static_cast<int32>(EVaelMenuPage::Compendium) + 1;
		const int32 Step = Value.Get<float>() < 0.0f ? NumPages - 1 : 1;
		MenuPage = static_cast<EVaelMenuPage>((static_cast<int32>(MenuPage) + Step) % NumPages);
	}
}

void AVaelPlayerController::OnMenuConfirm()
{
	const AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>();
	if (!bGrimoireOpen || MenuPage != EVaelMenuPage::Inventory || VaelCharacter == nullptr)
	{
		return;
	}

	// Equipment rows take the item off, backpack rows put it on
	UVaelInventory* Inventory = VaelCharacter->GetInventory();
	const int32 NumEquipRows = static_cast<int32>(EVaelEquipSlot::Count);

	if (InventorySelection < NumEquipRows)
	{
		if (!Inventory->Unequip(static_cast<EVaelEquipSlot>(InventorySelection)) && Inventory->GetEquipped(static_cast<EVaelEquipSlot>(InventorySelection)).IsValid())
		{
			UVaelNoticeSubsystem::Post(this, NSLOCTEXT("VaelItems", "BackpackFull", "Rucksack voll"), FText::GetEmpty(), FLinearColor(FColor(158, 143, 125)), 2.0f);
		}
	}
	else
	{
		Inventory->EquipFromBackpack(InventorySelection - NumEquipRows);
	}

	InventorySelection = FMath::Clamp(InventorySelection, 0, FMath::Max(GetNumInventoryRows() - 1, 0));
}

void AVaelPlayerController::OnToggleInventory()
{
	if (!bGrimoireOpen)
	{
		OpenGrimoire(EVaelMenuPage::Inventory);
	}
	else if (MenuPage == EVaelMenuPage::Inventory)
	{
		CloseGrimoire();
	}
	else
	{
		MenuPage = EVaelMenuPage::Inventory;
	}
}

int32 AVaelPlayerController::GetNumInventoryRows() const
{
	const AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>();
	const int32 NumBackpack = VaelCharacter != nullptr ? VaelCharacter->GetInventory()->GetBackpack().Num() : 0;
	return static_cast<int32>(EVaelEquipSlot::Count) + NumBackpack;
}

bool AVaelPlayerController::StartDialogue(const FText& Speaker, const TArray<FText>& Lines)
{
	// One thing at a time: no dialogue while the game is paused for a menu or another dialogue
	if (Lines.IsEmpty() || IsInDialogue() || UGameplayStatics::IsGamePaused(this))
	{
		return false;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (InputSubsystem == nullptr || DialogueMappingContext == nullptr)
	{
		return false;
	}

	DialogueSpeaker = Speaker;
	DialogueLines = Lines;
	DialogueIndex = 0;
	DialogueStartTime = FPlatformTime::Seconds();

	InputSubsystem->AddMappingContext(DialogueMappingContext, 2);
	UGameplayStatics::SetGamePaused(this, true);

	return true;
}

void AVaelPlayerController::OnDialogueContinue()
{
	if (!IsInDialogue() || FPlatformTime::Seconds() - DialogueStartTime < DialogueInputDelay)
	{
		return;
	}

	if (++DialogueIndex < DialogueLines.Num())
	{
		return;
	}

	// The last line was read
	DialogueLines.Reset();
	DialogueIndex = 0;

	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		InputSubsystem->RemoveMappingContext(DialogueMappingContext);
	}

	UGameplayStatics::SetGamePaused(this, false);
}

void AVaelPlayerController::OnMenuNavigateReleased()
{
	NextNavigateTime = 0.0;
}

void AVaelPlayerController::OnMenuClose()
{
	CloseGrimoire();
}

void AVaelPlayerController::OnMenuAssign(int32 SlotIndex)
{
	const AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>();
	const UGameInstance* GameInstance = GetGameInstance();
	const UVaelGrimoireSubsystem* Grimoire = GameInstance != nullptr ? GameInstance->GetSubsystem<UVaelGrimoireSubsystem>() : nullptr;

	if (!bGrimoireOpen || MenuPage != EVaelMenuPage::Formulas || VaelCharacter == nullptr || Grimoire == nullptr || !Grimoire->GetAllFormulas().IsValidIndex(GrimoireSelection))
	{
		return;
	}

	UVaelFormula* Formula = Grimoire->GetAllFormulas()[GrimoireSelection];
	if (!Grimoire->IsFormulaKnown(Formula))
	{
		return;
	}

	// Pressing the slot of the formula again takes it off
	UVaelElementComponent* ElementComponent = VaelCharacter->GetElementComponent();
	ElementComponent->AssignQuickSlot(SlotIndex, ElementComponent->GetQuickSlotFormula(SlotIndex) == Formula ? nullptr : Formula);
}

void AVaelPlayerController::OnCastFinished(EVaelCastResult Result, const UVaelFormula* Formula)
{
	switch (Result)
	{
	case EVaelCastResult::NoFormula:
		UVaelNoticeSubsystem::Post(this, NSLOCTEXT("VaelMagic", "NoFormulaEcho", "Echo: Diese Formel ist versiegelt"),
			NSLOCTEXT("VaelMagic", "NoFormulaEchoDetail", "Ihr Fragment liegt jenseits der Aschenmark."), FLinearColor(FColor(158, 143, 125)));
		break;

	case EVaelCastResult::UnknownFormula:
		UVaelNoticeSubsystem::Post(this, NSLOCTEXT("VaelMagic", "SealedEcho", "Echo einer versiegelten Formel"),
			Formula != nullptr ? Formula->Hint : FText::GetEmpty(), FLinearColor(FColor(201, 180, 138)), 6.0f);
		break;

	case EVaelCastResult::UnstableDischarge:
		UVaelNoticeSubsystem::Post(this, NSLOCTEXT("VaelMagic", "Unstable", "Instabile Entladung"),
			NSLOCTEXT("VaelMagic", "UnstableDetail", "Die Formel entgleitet dir. Versuch es noch einmal."), FLinearColor(FColor(216, 200, 255)));
		break;

	case EVaelCastResult::NotEnoughMana:
		UVaelNoticeSubsystem::Post(this, NSLOCTEXT("VaelMagic", "NoMana", "Zu wenig Mana"), FText::GetEmpty(), FLinearColor(FColor(127, 176, 255)), 1.5f);
		break;

	case EVaelCastResult::MarkAsleep:
		UVaelNoticeSubsystem::Post(this, NSLOCTEXT("VaelMagic", "MarkStirs", "Etwas in dir regt sich"),
			NSLOCTEXT("VaelMagic", "MarkAsleep", "Das Mark schläft noch in dir."), UVaelMagicSettings::Get()->GetElementColor(EVaelElement::Mark), 2.5f);
		break;

	case EVaelCastResult::EmptyQueue:
		UVaelNoticeSubsystem::Post(this, NSLOCTEXT("VaelMagic", "EmptyQuickSlot", "Schnellplatz leer"),
			NSLOCTEXT("VaelMagic", "EmptyQuickSlotDetail", "Im Grimoire belegen (Tab / Ansicht)."), FLinearColor(FColor(158, 143, 125)), 2.5f);
		break;

	default:
		break;
	}
}

void AVaelPlayerController::OnLightningBacklash(float Damage)
{
	UVaelNoticeSubsystem::Post(this, NSLOCTEXT("VaelMagic", "Backlash", "Der Blitz schl\u00E4gt zur\u00FCck"),
		NSLOCTEXT("VaelMagic", "BacklashDetail", "Wer nass einen Blitz wirkt, wird selbst getroffen."), FLinearColor(FColor(214, 236, 255)), 3.0f);
}

void AVaelPlayerController::OnSpyglassRaised()
{
	if (AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>())
	{
		VaelCharacter->SetObserving(true);
	}
}

void AVaelPlayerController::OnSpyglassLowered()
{
	if (AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>())
	{
		VaelCharacter->SetObserving(false);
	}
}
