// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/VaelPlayerController.h"
#include "Camera/VaelSharedCamera.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Pawn.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputTriggers.h"
#include "Magic/VaelElementComponent.h"
#include "Player/VaelCharacter.h"
#include "Player/VaelCheatManager.h"
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
	const auto MapStick = [this](const UInputAction* Action, const FKey& StickKey)
	{
		UInputModifierDeadZone* DeadZone = NewObject<UInputModifierDeadZone>(MappingContext);
		DeadZone->LowerThreshold = StickDeadZone;

		MappingContext->MapKey(Action, StickKey).Modifiers.Add(DeadZone);
	};

	MapStick(MoveGamepadAction, EKeys::Gamepad_Left2D);
	MapStick(AimStickAction, EKeys::Gamepad_Right2D);

	// Dodge roll
	MappingContext->MapKey(DodgeAction, EKeys::Gamepad_LeftTrigger);
	MappingContext->MapKey(DodgeAction, EKeys::SpaceBar);

	// Leave co-op by holding Circle / B
	UInputTriggerHold* HoldTrigger = NewObject<UInputTriggerHold>(MappingContext);
	HoldTrigger->HoldTimeThreshold = LeaveHoldTime;
	HoldTrigger->bIsOneShot = true;

	MappingContext->MapKey(LeaveAction, EKeys::Gamepad_FaceButton_Right).Triggers.Add(HoldTrigger);

	// Elements, laid out like the browser prototype: fire, water, earth, air on Circle, Square, Cross, Triangle (B, X, A, Y) and on 1-4.
	// Circle / B is shared with leaving co-op: a tap queues fire, holding it leaves.
	const FKey ElementPadKeys[] = { EKeys::Gamepad_FaceButton_Right, EKeys::Gamepad_FaceButton_Left, EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_FaceButton_Top };
	const FKey ElementKeyboardKeys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four };
	const TCHAR* ElementActionNames[] = { TEXT("IA_Element_Fire"), TEXT("IA_Element_Water"), TEXT("IA_Element_Earth"), TEXT("IA_Element_Air") };

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

	UpdateFacing();
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
	if (VaelCharacter != nullptr && !VaelCharacter->IsDodging())
	{
		VaelCharacter->GetElementComponent()->AddElement(Element);
	}
}

void AVaelPlayerController::OnCast()
{
	const AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>();
	if (VaelCharacter != nullptr && !VaelCharacter->IsDodging())
	{
		// Bring the aim up to date so the spell flies where the player points right now
		UpdateFacing();
		VaelCharacter->GetElementComponent()->CastQueue();
	}
}

void AVaelPlayerController::OnClearQueue()
{
	if (const AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>())
	{
		VaelCharacter->GetElementComponent()->ClearQueue();
	}
}

void AVaelPlayerController::ApplyMoveInput(const FVector2D& Input)
{
	AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>();
	if (VaelCharacter == nullptr || VaelCharacter->IsDodging())
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

	if (bUsingMouseAim)
	{
		GetMouseAimDirection(Facing);
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
