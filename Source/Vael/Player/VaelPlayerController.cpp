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
#include "Creatures/VaelCreature.h"
#include "EngineUtils.h"
#include "Items/VaelInventory.h"
#include "Items/VaelItemSettings.h"
#include "Items/VaelMaterial.h"
#include "Items/VaelMaterialBag.h"
#include "World/VaelSafeZone.h"
#include "Magic/VaelElementComponent.h"
#include "Player/VaelCharacter.h"
#include "Player/VaelHeroSubsystem.h"
#include "Floromant/VaelFloromantComponent.h"
#include "Floromant/VaelFloromantSettings.h"
#include "Magic/VaelFormulaAbility.h"
#include "Player/VaelCheatManager.h"
#include "Nature/VaelGarden.h"
#include "Player/VaelInteractable.h"
#include "GameFramework/InputDeviceSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Magic/VaelFormula.h"
#include "Magic/VaelGrimoireSubsystem.h"
#include "Magic/VaelMagicSettings.h"
#include "Story/VaelQuestSubsystem.h"
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
			bFloromantControls.Reset();
			UpdateClassControls();
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
			EnhancedInputComponent->BindAction(CastAction, ETriggerEvent::Completed, this, &AVaelPlayerController::OnCastReleased);
			EnhancedInputComponent->BindAction(CastAction, ETriggerEvent::Canceled, this, &AVaelPlayerController::OnCastReleased);
			EnhancedInputComponent->BindAction(ClearQueueAction, ETriggerEvent::Started, this, &AVaelPlayerController::OnClearQueue);
			EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &AVaelPlayerController::OnInteract);
			EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Completed, this, &AVaelPlayerController::OnSowHoldEnd);
			EnhancedInputComponent->BindAction(ClearQueueAction, ETriggerEvent::Completed, this, &AVaelPlayerController::OnSowHoldEnd);
			EnhancedInputComponent->BindAction(PotionAction, ETriggerEvent::Started, this, &AVaelPlayerController::OnPotion);

			for (int32 SlotIndex = 0; SlotIndex < QuickSlotActions.Num(); ++SlotIndex)
			{
				EnhancedInputComponent->BindAction(QuickSlotActions[SlotIndex], ETriggerEvent::Started, this, &AVaelPlayerController::OnQuickSlot, SlotIndex);
			}

			EnhancedInputComponent->BindAction(GrimoireAction, ETriggerEvent::Started, this, &AVaelPlayerController::OnToggleGrimoire);
			EnhancedInputComponent->BindAction(MapAction, ETriggerEvent::Started, this, &AVaelPlayerController::ToggleMap);
			EnhancedInputComponent->BindAction(MenuNavigateAction, ETriggerEvent::Triggered, this, &AVaelPlayerController::OnMenuNavigate);
			EnhancedInputComponent->BindAction(MenuNavigateAction, ETriggerEvent::Completed, this, &AVaelPlayerController::OnMenuNavigateReleased);
			EnhancedInputComponent->BindAction(MenuCloseAction, ETriggerEvent::Started, this, &AVaelPlayerController::OnMenuClose);
			EnhancedInputComponent->BindAction(MenuPageAction, ETriggerEvent::Started, this, &AVaelPlayerController::OnMenuPage);
			EnhancedInputComponent->BindAction(MenuConfirmAction, ETriggerEvent::Started, this, &AVaelPlayerController::OnMenuConfirm);
			EnhancedInputComponent->BindAction(InventoryAction, ETriggerEvent::Started, this, &AVaelPlayerController::OnToggleInventory);
			EnhancedInputComponent->BindAction(HeroAction, ETriggerEvent::Started, this, &AVaelPlayerController::OnToggleHero);
			EnhancedInputComponent->BindAction(SpyglassAction, ETriggerEvent::Started, this, &AVaelPlayerController::OnSpyglassRaised);
			EnhancedInputComponent->BindAction(SpyglassAction, ETriggerEvent::Completed, this, &AVaelPlayerController::OnSpyglassLowered);
			EnhancedInputComponent->BindAction(SpyglassAction, ETriggerEvent::Canceled, this, &AVaelPlayerController::OnSpyglassLowered);
			EnhancedInputComponent->BindAction(DialogueContinueAction, ETriggerEvent::Started, this, &AVaelPlayerController::OnDialogueContinue);
			EnhancedInputComponent->BindAction(DialogueSelectAction, ETriggerEvent::Started, this, &AVaelPlayerController::OnDialogueSelect);

			for (int32 SlotIndex = 0; SlotIndex < MenuAssignActions.Num(); ++SlotIndex)
			{
				EnhancedInputComponent->BindAction(MenuAssignActions[SlotIndex], ETriggerEvent::Started, this, &AVaelPlayerController::OnMenuAssign, SlotIndex);
			}

			// Floromant
			for (const bool bLeftHand : { true, false })
			{
				UInputAction* Fire = bLeftHand ? FireLeftAction : FireRightAction;
				EnhancedInputComponent->BindAction(Fire, ETriggerEvent::Started, this, &AVaelPlayerController::OnFireHand, bLeftHand, true);
				EnhancedInputComponent->BindAction(Fire, ETriggerEvent::Completed, this, &AVaelPlayerController::OnFireHand, bLeftHand, false);
				EnhancedInputComponent->BindAction(Fire, ETriggerEvent::Canceled, this, &AVaelPlayerController::OnFireHand, bLeftHand, false);
				EnhancedInputComponent->BindAction(bLeftHand ? TowerLeftAction : TowerRightAction, ETriggerEvent::Started, this, &AVaelPlayerController::OnTower, bLeftHand);
				EnhancedInputComponent->BindAction(bLeftHand ? PutLeftAction : PutRightAction, ETriggerEvent::Started, this, &AVaelPlayerController::OnPutOnHand, bLeftHand);
			}
			EnhancedInputComponent->BindAction(CrossAction, ETriggerEvent::Started, this, &AVaelPlayerController::OnCrossStarted);
			EnhancedInputComponent->BindAction(CrossAction, ETriggerEvent::Completed, this, &AVaelPlayerController::OnCrossEnded);
			EnhancedInputComponent->BindAction(LeapAction, ETriggerEvent::Started, this, &AVaelPlayerController::OnLeap);
			EnhancedInputComponent->BindAction(BeltUpAction, ETriggerEvent::Triggered, this, &AVaelPlayerController::OnPageBelt, -1);
			EnhancedInputComponent->BindAction(BeltDownAction, ETriggerEvent::Triggered, this, &AVaelPlayerController::OnPageBelt, 1);
			EnhancedInputComponent->BindAction(FloromantPotionAction, ETriggerEvent::Triggered, this, &AVaelPlayerController::OnPotion);
			EnhancedInputComponent->BindAction(ReloadAction, ETriggerEvent::Triggered, this, &AVaelPlayerController::OnReload);
			EnhancedInputComponent->BindAction(WhipAction, ETriggerEvent::Started, this, &AVaelPlayerController::OnWhip);
			EnhancedInputComponent->BindAction(HerbariumAction, ETriggerEvent::Started, this, &AVaelPlayerController::OnHerbarium);
			EnhancedInputComponent->BindAction(RiseAction, ETriggerEvent::Started, this, &AVaelPlayerController::OnRise, true);
			EnhancedInputComponent->BindAction(RiseAction, ETriggerEvent::Completed, this, &AVaelPlayerController::OnRise, false);
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

	// Shared by every class: moving, aiming, leaving, map, inventory and the page of class and body
	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_VaelPlayer"));

	// Each class plays differently: the mage has its own layout, the Floromant its own; only one of the two is active
	MageMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Magier"));
	FloromantMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Floromant"));

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
	MageMappingContext->MapKey(DodgeAction, EKeys::Gamepad_LeftTrigger);
	MageMappingContext->MapKey(DodgeAction, EKeys::SpaceBar);

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

		MageMappingContext->MapKey(ElementAction, ElementPadKeys[ElementIndex]);
		MageMappingContext->MapKey(ElementAction, ElementKeyboardKeys[ElementIndex]);
	}

	// Cast and discard the queued elements
	CastAction = CreateAction(TEXT("IA_Cast"), EInputActionValueType::Boolean);
	MageMappingContext->MapKey(CastAction, EKeys::Gamepad_RightTrigger);
	MageMappingContext->MapKey(CastAction, EKeys::LeftMouseButton);

	ClearQueueAction = CreateAction(TEXT("IA_ClearQueue"), EInputActionValueType::Boolean);
	MageMappingContext->MapKey(ClearQueueAction, EKeys::Gamepad_LeftShoulder);
	MageMappingContext->MapKey(ClearQueueAction, EKeys::RightMouseButton);

	// Interact: E; on the gamepad the earth button (Cross / A) interacts instead when something is close and no fight is going on
	InteractAction = CreateAction(TEXT("IA_Interact"), EInputActionValueType::Boolean);
	MageMappingContext->MapKey(InteractAction, EKeys::E);

	// Healing potion: Q; on the gamepad L1 / LB drinks while no element is queued
	PotionAction = CreateAction(TEXT("IA_Potion"), EInputActionValueType::Boolean);
	MageMappingContext->MapKey(PotionAction, EKeys::Q);

	// Quick slots on Z, X, C, V and the d-pad clockwise from up, like the browser prototype
	const FKey QuickSlotPadKeys[] = { EKeys::Gamepad_DPad_Up, EKeys::Gamepad_DPad_Right, EKeys::Gamepad_DPad_Down, EKeys::Gamepad_DPad_Left };
	const FKey QuickSlotKeyboardKeys[] = { EKeys::Z, EKeys::X, EKeys::C, EKeys::V };

	for (int32 SlotIndex = 0; SlotIndex < UE_ARRAY_COUNT(QuickSlotPadKeys); ++SlotIndex)
	{
		UInputAction* QuickSlotAction = CreateAction(*FString::Printf(TEXT("IA_QuickSlot%d"), SlotIndex + 1), EInputActionValueType::Boolean);
		QuickSlotActions.Add(QuickSlotAction);

		MageMappingContext->MapKey(QuickSlotAction, QuickSlotPadKeys[SlotIndex]);
		MageMappingContext->MapKey(QuickSlotAction, QuickSlotKeyboardKeys[SlotIndex]);
	}

	// The grimoire opens and closes while the game is paused; on the controller with Options / Menu, like the menu in Diablo.
	// (Start lets a controller without a player join; once it plays, the same button opens its grimoire.)
	GrimoireAction = CreateAction(TEXT("IA_Grimoire"), EInputActionValueType::Boolean);
	GrimoireAction->bTriggerWhenPaused = true;
	MageMappingContext->MapKey(GrimoireAction, EKeys::Gamepad_Special_Right);
	MageMappingContext->MapKey(GrimoireAction, EKeys::Tab);

	// The big map: View / Create, like the map in Diablo, or M
	MapAction = CreateAction(TEXT("IA_Map"), EInputActionValueType::Boolean);
	MappingContext->MapKey(MapAction, EKeys::Gamepad_Special_Left);
	MappingContext->MapKey(MapAction, EKeys::M);

	// The inventory page of the same menu opens directly with I
	InventoryAction = CreateAction(TEXT("IA_Inventory"), EInputActionValueType::Boolean);
	InventoryAction->bTriggerWhenPaused = true;
	MappingContext->MapKey(InventoryAction, EKeys::I);

	// The page of class and body opens directly with K; on the gamepad it is a page of the menu
	HeroAction = CreateAction(TEXT("IA_Hero"), EInputActionValueType::Boolean);
	HeroAction->bTriggerWhenPaused = true;
	MappingContext->MapKey(HeroAction, EKeys::K);

	// The spyglass is held up while the button is held
	SpyglassAction = CreateAction(TEXT("IA_Spyglass"), EInputActionValueType::Boolean);
	MageMappingContext->MapKey(SpyglassAction, EKeys::Gamepad_LeftThumbstick);
	MageMappingContext->MapKey(SpyglassAction, EKeys::F);

	CreateFloromantInput();

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

	// Answers at the end of a dialogue: up and down
	DialogueSelectAction = CreateAction(TEXT("IA_DialogueSelect"), EInputActionValueType::Axis1D);
	DialogueSelectAction->bTriggerWhenPaused = true;

	UInputModifierDeadZone* SelectDeadZone = NewObject<UInputModifierDeadZone>(DialogueMappingContext);
	SelectDeadZone->LowerThreshold = 0.5f;
	DialogueMappingContext->MapKey(DialogueSelectAction, EKeys::Gamepad_LeftY).Modifiers.Add(SelectDeadZone);
	for (const FKey& UpKey : { EKeys::W, EKeys::Up, EKeys::Gamepad_DPad_Up })
	{
		DialogueMappingContext->MapKey(DialogueSelectAction, UpKey);
	}
	for (const FKey& DownKey : { EKeys::S, EKeys::Down, EKeys::Gamepad_DPad_Down })
	{
		DialogueMappingContext->MapKey(DialogueSelectAction, DownKey).Modifiers.Add(NewObject<UInputModifierNegate>(DialogueMappingContext));
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

void AVaelPlayerController::CreateFloromantInput()
{
	const auto CreateAction = [this](const TCHAR* Name, EInputActionValueType ValueType = EInputActionValueType::Boolean)
	{
		UInputAction* Action = NewObject<UInputAction>(this, Name);
		Action->ValueType = ValueType;
		return Action;
	};
	UInputMappingContext* Context = FloromantMappingContext;
	const float PotionHoldTime = UVaelFloromantSettings::Get()->PotionHoldTime;

	// The hands fire while their button is held
	FireLeftAction = CreateAction(TEXT("IA_Flo_FeuerLinks"));
	Context->MapKey(FireLeftAction, EKeys::Gamepad_LeftShoulder);
	Context->MapKey(FireLeftAction, EKeys::LeftMouseButton);

	FireRightAction = CreateAction(TEXT("IA_Flo_FeuerRechts"));
	Context->MapKey(FireRightAction, EKeys::Gamepad_RightShoulder);
	Context->MapKey(FireRightAction, EKeys::RightMouseButton);

	// Cross and Space: crossing while both hands fire, otherwise the leap (or using what is close by); the handlers decide
	CrossAction = CreateAction(TEXT("IA_Flo_Kreuzen"));
	Context->MapKey(CrossAction, EKeys::Gamepad_FaceButton_Bottom);
	Context->MapKey(CrossAction, EKeys::SpaceBar);

	LeapAction = CreateAction(TEXT("IA_Flo_Wurzelsprung"));
	Context->MapKey(LeapAction, EKeys::Gamepad_FaceButton_Bottom);
	Context->MapKey(LeapAction, EKeys::SpaceBar);

	// Towers on the triggers and Q / E; both at once call the garden (they wait a moment for each other in UpdateTowerChord)
	TowerLeftAction = CreateAction(TEXT("IA_Flo_TurmLinks"));
	Context->MapKey(TowerLeftAction, EKeys::Gamepad_LeftTrigger);
	Context->MapKey(TowerLeftAction, EKeys::Q);

	TowerRightAction = CreateAction(TEXT("IA_Flo_TurmRechts"));
	Context->MapKey(TowerRightAction, EKeys::Gamepad_RightTrigger);
	Context->MapKey(TowerRightAction, EKeys::E);

	// Seed belt: d-pad up / down and the mouse wheel page, d-pad left / right and 1 / 2 lay the plant on a hand.
	// D-pad down only pages when tapped; held, it drinks a healing potion.
	BeltUpAction = CreateAction(TEXT("IA_Flo_GuertelHoch"));
	Context->MapKey(BeltUpAction, EKeys::Gamepad_DPad_Up).Triggers.Add(NewObject<UInputTriggerPressed>(Context));
	Context->MapKey(BeltUpAction, EKeys::MouseScrollUp).Triggers.Add(NewObject<UInputTriggerPressed>(Context));

	BeltDownAction = CreateAction(TEXT("IA_Flo_GuertelRunter"));
	UInputTriggerTap* BeltTap = NewObject<UInputTriggerTap>(Context);
	BeltTap->TapReleaseTimeThreshold = PotionHoldTime;
	Context->MapKey(BeltDownAction, EKeys::Gamepad_DPad_Down).Triggers.Add(BeltTap);
	Context->MapKey(BeltDownAction, EKeys::MouseScrollDown).Triggers.Add(NewObject<UInputTriggerPressed>(Context));

	FloromantPotionAction = CreateAction(TEXT("IA_Flo_Heiltrank"));
	UInputTriggerHold* PotionHold = NewObject<UInputTriggerHold>(Context);
	PotionHold->HoldTimeThreshold = PotionHoldTime;
	PotionHold->bIsOneShot = true;
	Context->MapKey(FloromantPotionAction, EKeys::Gamepad_DPad_Down).Triggers.Add(PotionHold);
	Context->MapKey(FloromantPotionAction, EKeys::H).Triggers.Add(NewObject<UInputTriggerPressed>(Context));

	PutLeftAction = CreateAction(TEXT("IA_Flo_AufLinks"));
	Context->MapKey(PutLeftAction, EKeys::Gamepad_DPad_Left);
	Context->MapKey(PutLeftAction, EKeys::One);

	PutRightAction = CreateAction(TEXT("IA_Flo_AufRechts"));
	Context->MapKey(PutRightAction, EKeys::Gamepad_DPad_Right);
	Context->MapKey(PutRightAction, EKeys::Two);

	// Reloading with Keimpelz: Triangle or R held
	ReloadAction = CreateAction(TEXT("IA_Flo_Nachladen"));
	for (const FKey& ReloadKey : { EKeys::Gamepad_FaceButton_Top, EKeys::R })
	{
		UInputTriggerHold* ReloadHold = NewObject<UInputTriggerHold>(Context);
		ReloadHold->HoldTimeThreshold = 0.35f;
		ReloadHold->bIsOneShot = true;
		Context->MapKey(ReloadAction, ReloadKey).Triggers.Add(ReloadHold);
	}

	// Dodge on Circle and Shift; holding Circle still leaves co-op (shared controls)
	Context->MapKey(DodgeAction, EKeys::Gamepad_FaceButton_Right);
	Context->MapKey(DodgeAction, EKeys::LeftShift);

	WhipAction = CreateAction(TEXT("IA_Flo_Dornenpeitsche"));
	Context->MapKey(WhipAction, EKeys::Gamepad_FaceButton_Left);
	Context->MapKey(WhipAction, EKeys::F);

	// Herbarium and repertoire: the touchpad, Tab on the keyboard. A PS5 controller on the PC (DS4Windows) has no own
	// touchpad button for games; its touchpad click can be set to Tab there.
	HerbariumAction = CreateAction(TEXT("IA_Flo_Herbarium"));
	HerbariumAction->bTriggerWhenPaused = true;
	Context->MapKey(HerbariumAction, EKeys::Tab);

	// Rising high: pressing the right stick (R3), Left Ctrl on the keyboard
	RiseAction = CreateAction(TEXT("IA_Flo_Steigen"));
	Context->MapKey(RiseAction, EKeys::Gamepad_RightThumbstick);
	Context->MapKey(RiseAction, EKeys::LeftControl);

	// The menu on Options / Menu and Escape (the Floromant has no grimoire; its plant book comes later), spyglass on L3 and V,
	// use what is close by with G (on the gamepad Cross does it outside of fights)
	Context->MapKey(GrimoireAction, EKeys::Gamepad_Special_Right);
	Context->MapKey(GrimoireAction, EKeys::Escape);
	Context->MapKey(SpyglassAction, EKeys::Gamepad_LeftThumbstick);
	Context->MapKey(SpyglassAction, EKeys::V);
	Context->MapKey(InteractAction, EKeys::G);
}

void AVaelPlayerController::UpdateClassControls()
{
	const AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>();
	UEnhancedInputLocalPlayerSubsystem* Subsystem = IsLocalPlayerController() ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()) : nullptr;
	if (VaelCharacter == nullptr || Subsystem == nullptr || MageMappingContext == nullptr)
	{
		return;
	}

	const bool bFloromant = VaelCharacter->IsFloromant();
	if (bFloromantControls.IsSet() && bFloromantControls.GetValue() == bFloromant)
	{
		return;
	}
	bFloromantControls = bFloromant;

	Subsystem->RemoveMappingContext(bFloromant ? MageMappingContext : FloromantMappingContext);
	Subsystem->AddMappingContext(bFloromant ? FloromantMappingContext : MageMappingContext, 0);

	// Nothing held carries over into the other class
	bFireHeld[0] = bFireHeld[1] = false;
	CrossStartTime = -1.0f;
	TowerPressTime[0] = TowerPressTime[1] = -1.0f;
}

void AVaelPlayerController::GetStreamingSourceLocationAndRotation(FVector& OutLocation, FRotator& OutRotation) const
{
	if (const APawn* ControlledPawn = GetPawn())
	{
		OutLocation = ControlledPawn->GetActorLocation();
		OutRotation = ControlledPawn->GetActorRotation();
		return;
	}

	Super::GetStreamingSourceLocationAndRotation(OutLocation, OutRotation);
}

void AVaelPlayerController::PlayerTick(float DeltaTime)
{
	UpdateSowHold();
	UpdateClassControls();
	UpdateTowerChord();
	UpdateHeldCast();

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
	AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>();
	if (VaelCharacter == nullptr || VaelCharacter->IsDodging() || VaelCharacter->IsDowned())
	{
		return;
	}

	// On the gamepad the earth button uses what is close by instead, while no fight is going on
	if (Element == EVaelElement::Earth && GetInputGlyphs() != EVaelInputGlyphs::Keyboard)
	{
		if (IVaelInteractable* Interactable = Cast<IVaelInteractable>(GetPadInteractTarget()))
		{
			Interactable->Interact(VaelCharacter);
			return;
		}
	}

	VaelCharacter->GetElementComponent()->AddElement(Element);
}

AActor* AVaelPlayerController::GetPadInteractTarget() const
{
	const AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>();
	AActor* Target = VaelCharacter != nullptr ? UVaelInteractionSubsystem::FindNearest(VaelCharacter) : nullptr;
	if (Target == nullptr || AVaelSafeZone::IsSafe(GetWorld(), VaelCharacter->GetActorLocation()))
	{
		return Target;
	}

	// In a fight the button stays earth, so a chest or a plant nearby doesn't swallow a spell
	const float CombatRange = UVaelItemSettings::Get()->InteractCombatRange;
	for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
	{
		if (!It->IsDead() && !It->IsOnPlayerSide() && FVector::Dist2D(It->GetActorLocation(), VaelCharacter->GetActorLocation()) < CombatRange)
		{
			return nullptr;
		}
	}

	return Target;
}

void AVaelPlayerController::OnPotion()
{
	if (AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>())
	{
		VaelCharacter->DrinkPotion();
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
		bCastHeld = true;
		bHeldJumpStarted = false;
		HeldCastTime = GetWorld()->GetTimeSeconds();
	}
}

void AVaelPlayerController::OnCastReleased()
{
	bCastHeld = false;
	if (const AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>())
	{
		VaelCharacter->GetElementComponent()->ReleaseHeld();
	}
}

void AVaelPlayerController::UpdateHeldCast()
{
	// A held Böe jumps again a moment after the last jump ends, towards where the player aims then
	const AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>();
	UVaelElementComponent* Elements = VaelCharacter != nullptr ? VaelCharacter->GetElementComponent() : nullptr;
	if (!bCastHeld || Elements == nullptr || !Elements->CanCastHeld() || VaelCharacter->IsDowned() || UGameplayStatics::IsGamePaused(this))
	{
		return;
	}

	// Wait for the jump of the last cast to start (after its cast animation) and to end
	const float Now = GetWorld()->GetTimeSeconds();
	if (VaelCharacter->IsDodging())
	{
		bHeldJumpStarted = true;
		NextHeldCastTime = Now + UVaelMagicSettings::Get()->HeldGustPause;
		return;
	}

	if (!bHeldJumpStarted)
	{
		// A jump that never came (blocked, cast cut short) ends the hold
		if (Now - HeldCastTime > 1.5f)
		{
			Elements->ReleaseHeld();
		}
		return;
	}

	if (Now >= NextHeldCastTime)
	{
		UpdateFacing();
		bHeldJumpStarted = false;
		HeldCastTime = Now;
		Elements->CastHeld();
	}
}

FVector AVaelPlayerController::GetFloromantAim() const
{
	const AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>();
	if (VaelCharacter == nullptr)
	{
		return FVector::ZeroVector;
	}

	// Where the mouse points; with the gamepad onto the enemy the aim is locked on, or a few steps ahead
	FVector AimLocation;
	if (!GetMouseAimLocation(AimLocation))
	{
		const AActor* Target = GetAimTarget();
		AimLocation = Target != nullptr ? Target->GetActorLocation()
			: VaelCharacter->GetActorLocation() + UVaelFormulaAbility::GetAimDirection(VaelCharacter) * UVaelFloromantSettings::Get()->ThrowRange * 0.6f;
	}
	return AimLocation;
}

// ---------------------------------------------------------------- Floromant controls

void AVaelPlayerController::OnFireHand(bool bLeft, bool bHeld)
{
	bFireHeld[bLeft ? 0 : 1] = bHeld;

	AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>();
	if (VaelCharacter == nullptr || !VaelCharacter->IsFloromant())
	{
		return;
	}
	if (bHeld)
	{
		UpdateFacing();
	}
	VaelCharacter->GetFloromant()->SetHandFiring(bLeft, bHeld);
}

void AVaelPlayerController::OnCrossStarted()
{
	// Only while both hands fire; alone the button leaps
	CrossStartTime = AreBothHandsHeld() ? GetWorld()->GetTimeSeconds() : -1.0f;
}

void AVaelPlayerController::OnCrossEnded()
{
	const AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>();
	if (CrossStartTime < 0.0f || VaelCharacter == nullptr || !VaelCharacter->IsFloromant())
	{
		CrossStartTime = -1.0f;
		return;
	}

	// A tap is a fast cross shot, holding charges it
	const bool bCharged = GetWorld()->GetTimeSeconds() - CrossStartTime >= 0.3f;
	CrossStartTime = -1.0f;
	VaelCharacter->GetFloromant()->CrossHands(bCharged);
}

void AVaelPlayerController::OnLeap()
{
	// While both hands fire the button crosses them instead
	AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>();
	if (AreBothHandsHeld() || VaelCharacter == nullptr || !VaelCharacter->IsFloromant() || VaelCharacter->IsDodging() || VaelCharacter->IsDowned())
	{
		return;
	}

	// On the gamepad it uses what is close by outside of fights, like the earth button of the mage
	if (GetInputGlyphs() != EVaelInputGlyphs::Keyboard)
	{
		if (IVaelInteractable* Interactable = Cast<IVaelInteractable>(GetPadInteractTarget()))
		{
			Interactable->Interact(VaelCharacter);
			return;
		}
	}

	UpdateFacing();
	VaelCharacter->GetFloromant()->RootLeap(GetFloromantAim());
}

void AVaelPlayerController::OnTower(bool bLeft)
{
	TowerPressTime[bLeft ? 0 : 1] = GetWorld()->GetTimeSeconds();
}

void AVaelPlayerController::UpdateTowerChord()
{
	if (TowerPressTime[0] < 0.0f && TowerPressTime[1] < 0.0f)
	{
		return;
	}

	AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>();
	if (VaelCharacter == nullptr || !VaelCharacter->IsFloromant() || VaelCharacter->IsDowned())
	{
		TowerPressTime[0] = TowerPressTime[1] = -1.0f;
		return;
	}

	// Both buttons within the wait: the Ruf des Gartens, every plant around grows into its giant form
	UVaelFloromantComponent* Floromant = VaelCharacter->GetFloromant();
	if (TowerPressTime[0] >= 0.0f && TowerPressTime[1] >= 0.0f)
	{
		TowerPressTime[0] = TowerPressTime[1] = -1.0f;
		Floromant->Overgrow();
		return;
	}

	// One alone, once the other didn't come: a single tower
	const float Now = GetWorld()->GetTimeSeconds();
	for (int32 Side = 0; Side < 2; ++Side)
	{
		if (TowerPressTime[Side] >= 0.0f && Now - TowerPressTime[Side] >= UVaelFloromantSettings::Get()->TowerChordDelay)
		{
			TowerPressTime[Side] = -1.0f;
			Floromant->RootTower(Side == 0);
		}
	}
}

void AVaelPlayerController::OnPageBelt(int32 Step)
{
	const AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>();
	if (VaelCharacter != nullptr && VaelCharacter->IsFloromant())
	{
		VaelCharacter->GetFloromant()->PageHandBelt(Step);
	}
}

void AVaelPlayerController::OnPutOnHand(bool bLeft)
{
	const AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>();
	if (VaelCharacter != nullptr && VaelCharacter->IsFloromant())
	{
		VaelCharacter->GetFloromant()->PutOnHand(bLeft);
	}
}

void AVaelPlayerController::OnReload()
{
	const AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>();
	if (VaelCharacter != nullptr && VaelCharacter->IsFloromant())
	{
		VaelCharacter->GetFloromant()->ReloadHands();
	}
}

void AVaelPlayerController::OnWhip()
{
	const AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>();
	if (VaelCharacter != nullptr && VaelCharacter->IsFloromant() && !VaelCharacter->IsDodging() && !VaelCharacter->IsDowned())
	{
		UpdateFacing();
		VaelCharacter->GetFloromant()->ThornWhip(GetFloromantAim());
	}
}

void AVaelPlayerController::OnRise(bool bPressed)
{
	const AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>();
	if (VaelCharacter == nullptr || !VaelCharacter->IsFloromant())
	{
		return;
	}

	// A press switches between hovering and up high; or, set so in the project settings, she stays up only while it is held
	UVaelFloromantComponent* Floromant = VaelCharacter->GetFloromant();
	if (UVaelFloromantSettings::Get()->bRiseToggles)
	{
		if (bPressed)
		{
			Floromant->ToggleRise();
		}
	}
	else
	{
		Floromant->SetRiseHeld(bPressed);
	}
}

void AVaelPlayerController::OnHerbarium()
{
	const AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>();
	if (VaelCharacter != nullptr && VaelCharacter->IsFloromant())
	{
		VaelCharacter->GetFloromant()->OpenHerbarium();
	}
}

void AVaelPlayerController::OnClearQueue()
{
	// On the gamepad a player who carries seeds and stands in peace sows by holding L1 / LB; a short tap acts when the button is let go
	const AVaelCharacter* Sower = GetPawn<AVaelCharacter>();
	if (Sower != nullptr && GetInputGlyphs() != EVaelInputGlyphs::Keyboard && Sower->GetElementComponent()->GetQueue().IsEmpty() && CanStartSowing())
	{
		SowHoldStart = GetWorld()->GetTimeSeconds();
		bSowHoldDeferred = true;
		return;
	}

	DiscardOrDrink();
}

void AVaelPlayerController::DiscardOrDrink()
{
	AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>();
	if (VaelCharacter == nullptr)
	{
		return;
	}

	// L1 / LB discards queued elements; with none queued it drinks a healing potion. The right mouse button only discards.
	UVaelElementComponent* Elements = VaelCharacter->GetElementComponent();
	if (Elements->GetQueue().IsEmpty() && GetInputGlyphs() != EVaelInputGlyphs::Keyboard)
	{
		VaelCharacter->DrinkPotion();
		return;
	}

	Elements->ClearQueue();
}

void AVaelPlayerController::OnInteract()
{
	AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>();
	if (IVaelInteractable* Interactable = Cast<IVaelInteractable>(UVaelInteractionSubsystem::FindNearest(VaelCharacter)))
	{
		Interactable->Interact(VaelCharacter);
		return;
	}

	// Nothing to use: held, the button sows a seed
	SowHoldStart = GetWorld()->GetTimeSeconds();
	bSowHoldDeferred = false;
}

bool AVaelPlayerController::CanStartSowing() const
{
	const AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>();
	const UVaelGardenSubsystem* Garden = UVaelGardenSubsystem::Get(this);
	if (VaelCharacter == nullptr || Garden == nullptr || VaelCharacter->IsDowned() || !Garden->CanSow(VaelCharacter))
	{
		return false;
	}

	// Not in a fight: there the button keeps doing what it always does
	const float CombatRange = UVaelItemSettings::Get()->InteractCombatRange;
	for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
	{
		if (!It->IsDead() && !It->IsOnPlayerSide() && FVector::Dist2D(It->GetActorLocation(), VaelCharacter->GetActorLocation()) < CombatRange)
		{
			return false;
		}
	}

	return true;
}

void AVaelPlayerController::UpdateSowHold()
{
	if (SowHoldStart < 0.0f || GetWorld()->GetTimeSeconds() - SowHoldStart < UVaelGardenSettings::Get()->SowHoldSeconds)
	{
		return;
	}

	// Held long enough: the seed goes into the ground, and the tap the button would have meant is dropped
	SowHoldStart = -1.0f;
	bSowHoldDeferred = false;

	if (UVaelGardenSubsystem* Garden = UVaelGardenSubsystem::Get(this))
	{
		Garden->Sow(GetPawn<AVaelCharacter>());
	}
}

void AVaelPlayerController::OnSowHoldEnd()
{
	const bool bWasTap = SowHoldStart >= 0.0f && bSowHoldDeferred;
	SowHoldStart = -1.0f;
	bSowHoldDeferred = false;

	// Let go early: L1 / LB does what a tap always did
	if (bWasTap)
	{
		DiscardOrDrink();
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

void AVaelPlayerController::ToggleMap()
{
	bMapOpen = !bMapOpen && !bGrimoireOpen;
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
	bMapOpen = false;
	MenuPage = Page;
	NextNavigateTime = 0.0;

	// The page of class and body starts on what the player plays
	if (const UVaelHeroSubsystem* Heroes = UVaelHeroSubsystem::Get(this))
	{
		const FVaelHeroChoice Current = Heroes->GetChoice(PlayerSlot);
		HeroSelection = (Current.Class == EVaelHeroClass::Floromant ? 2 : 0) + (Current.bFemale ? 1 : 0);
	}
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
	OpenShop.Reset();
	if (MenuPage == EVaelMenuPage::Shop)
	{
		MenuPage = EVaelMenuPage::Formulas;
	}

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

	if (MenuPage == EVaelMenuPage::Hero)
	{
		HeroSelection = FMath::Clamp(HeroSelection + Step, 0, UVaelHeroSubsystem::NumListedChoices - 1);
		return;
	}

	if (MenuPage == EVaelMenuPage::Shop)
	{
		const int32 NumOffers = OpenShop.IsValid() ? OpenShop->Offers.Num() : 0;
		ShopSelection = FMath::Clamp(ShopSelection + Step, 0, FMath::Max(NumOffers - 1, 0));
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
	if (bGrimoireOpen && MenuPage != EVaelMenuPage::Shop)
	{
		// The pages in a ring: right goes on, left goes back
		const int32 NumPages = static_cast<int32>(EVaelMenuPage::Hero) + 1;
		const int32 Step = Value.Get<float>() < 0.0f ? NumPages - 1 : 1;
		MenuPage = static_cast<EVaelMenuPage>((static_cast<int32>(MenuPage) + Step) % NumPages);
	}
}

void AVaelPlayerController::OnMenuConfirm()
{
	const AVaelCharacter* VaelCharacter = GetPawn<AVaelCharacter>();

	// Buying: from the own purse into the own bag
	if (bGrimoireOpen && MenuPage == EVaelMenuPage::Shop && VaelCharacter != nullptr && OpenShop.IsValid() && OpenShop->Offers.IsValidIndex(ShopSelection))
	{
		const FVaelShopOffer& Offer = OpenShop->Offers[ShopSelection];
		const UVaelMaterial* Goods = Offer.Material.LoadSynchronous();
		const int32 Price = OpenShop->GetPrice(this, ShopSelection);
		if (Goods != nullptr && VaelCharacter->GetInventory()->SpendCoins(Price))
		{
			VaelCharacter->GetMaterialBag()->AddMaterial(Goods, 1);
			UE_LOG(LogVael, Log, TEXT("Player %d buys %s for %d coins"), PlayerSlot + 1, *Goods->DisplayName.ToString(), Price);
		}
		else
		{
			UVaelNoticeSubsystem::Post(this, NSLOCTEXT("VaelItems", "NotEnoughCoins", "Nicht genug Gildenmünzen"), FText::GetEmpty(), FLinearColor(FColor(158, 143, 125)), 2.0f);
		}
		return;
	}

	// Class and body: the choice is kept for the slot and taken on at once
	if (bGrimoireOpen && MenuPage == EVaelMenuPage::Hero && VaelCharacter != nullptr)
	{
		const FVaelHeroChoice Choice = UVaelHeroSubsystem::GetListedChoice(HeroSelection);
		if (UVaelHeroSubsystem* Heroes = UVaelHeroSubsystem::Get(this))
		{
			Heroes->SetChoice(PlayerSlot, Choice);
		}
		GetPawn<AVaelCharacter>()->SetHero(Choice.Class, Choice.bFemale);
		UVaelNoticeSubsystem::Post(this, FText::Format(NSLOCTEXT("VaelHero", "HeroChosen", "Spieler {0} spielt jetzt: {1}"), PlayerSlot + 1, UVaelHeroSubsystem::GetChoiceName(Choice)), FText::GetEmpty(), FLinearColor(FColor(201, 180, 138)), 2.5f);
		return;
	}

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

void AVaelPlayerController::OnToggleHero()
{
	if (!bGrimoireOpen)
	{
		OpenGrimoire(EVaelMenuPage::Hero);
	}
	else if (MenuPage == EVaelMenuPage::Hero)
	{
		CloseGrimoire();
	}
	else if (MenuPage != EVaelMenuPage::Shop)
	{
		MenuPage = EVaelMenuPage::Hero;
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

	// An answer is given: the person replies, or the decision is put off
	if (IsChoosing())
	{
		TArray<FText> Reply;
		UVaelQuestSubsystem* Quests = UVaelQuestSubsystem::Get(this);
		const UVaelQuest* Quest = ChoiceQuest.Get();
		ChoiceQuest.Reset();

		if (Quests != nullptr && Quests->Choose(Quest, ChoiceSelection, Reply) && !Reply.IsEmpty())
		{
			DialogueLines = Reply;
			DialogueIndex = 0;
			DialogueStartTime = FPlatformTime::Seconds();
			return;
		}

		EndDialogue();
		return;
	}

	if (++DialogueIndex < DialogueLines.Num())
	{
		// The answers appear with the last line; a hasty press must not give one
		if (IsChoosing())
		{
			DialogueStartTime = FPlatformTime::Seconds();
		}
		return;
	}

	// The last line was read
	EndDialogue();
}

void AVaelPlayerController::OnDialogueSelect(const FInputActionValue& Value)
{
	if (IsChoosing() && !FMath::IsNearlyZero(Value.Get<float>()))
	{
		const int32 NumAnswers = GetChoiceLabels().Num();
		ChoiceSelection = (ChoiceSelection + (Value.Get<float>() > 0.0f ? NumAnswers - 1 : 1)) % NumAnswers;
	}
}

void AVaelPlayerController::OfferChoiceAfterDialogue(const UVaelQuest* Quest)
{
	if (!IsInDialogue() || Quest == nullptr)
	{
		return;
	}

	ChoiceQuest = Quest;

	// Putting the decision off is selected first, so nothing is decided by accident
	ChoiceSelection = GetChoiceLabels().Num() - 1;
}

bool AVaelPlayerController::IsChoosing() const
{
	return IsInDialogue() && ChoiceQuest.IsValid() && DialogueIndex == DialogueLines.Num() - 1;
}

TArray<FText> AVaelPlayerController::GetChoiceLabels() const
{
	TArray<FText> Labels;

	const UVaelQuestSubsystem* Quests = UVaelQuestSubsystem::Get(this);
	const FVaelQuestStep* Step = Quests != nullptr && ChoiceQuest.IsValid() ? Quests->GetCurrentStep(ChoiceQuest.Get()) : nullptr;
	if (Step != nullptr)
	{
		for (const FVaelQuestChoice& Choice : Step->Choices)
		{
			Labels.Add(Choice.Label);
		}
	}

	Labels.Add(NSLOCTEXT("VaelStory", "ChoiceLater", "Noch nicht entscheiden"));
	return Labels;
}

void AVaelPlayerController::EndDialogue()
{
	if (!IsInDialogue())
	{
		return;
	}

	DialogueLines.Reset();
	DialogueIndex = 0;
	ChoiceQuest.Reset();

	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		InputSubsystem->RemoveMappingContext(DialogueMappingContext);
	}

	UGameplayStatics::SetGamePaused(this, false);

	// A trader opens their goods once the talk is over
	if (const UVaelShop* Shop = PendingShop.Get())
	{
		PendingShop.Reset();
		OpenShopAfterDialogue(Shop);
	}
}

void AVaelPlayerController::OpenShopAfterDialogue(const UVaelShop* Shop)
{
	if (Shop == nullptr)
	{
		return;
	}

	if (IsInDialogue())
	{
		PendingShop = Shop;
		return;
	}

	OpenShop = Shop;
	ShopSelection = 0;
	if (!OpenGrimoire(EVaelMenuPage::Shop))
	{
		OpenShop.Reset();
	}
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
