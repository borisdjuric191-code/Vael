// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/VaelCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "Animation/AnimInstance.h"
#include "Camera/VaelSharedCamera.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Magic/VaelElementComponent.h"
#include "Magic/VaelMagicSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Player/VaelPlayerController.h"
#include "Vael.h"
#include "VaelGameMode.h"

AVaelCharacter::AVaelCharacter()
{
	// Set size for player capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// The character turns towards the aim direction, which the controller stores as control rotation
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 900.f, 0.f);
	GetCharacterMovement()->bConstrainToPlane = true;
	GetCharacterMovement()->bSnapToPlaneAtStart = true;

	// Placeholder look: the template mannequin, until the classes get their own characters
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -89.f), FRotator(0.f, -90.f, 0.f));

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> PlaceholderMesh(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	if (PlaceholderMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(PlaceholderMesh.Object);
	}

	static ConstructorHelpers::FClassFinder<UAnimInstance> PlaceholderAnimClass(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
	if (PlaceholderAnimClass.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(PlaceholderAnimClass.Class);
	}

	// Create the player marker from engine placeholder assets
	PlayerMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlayerMarker"));

	PlayerMarker->SetupAttachment(RootComponent);
	PlayerMarker->SetRelativeLocation(FVector(0.f, 0.f, -95.f));
	PlayerMarker->SetRelativeScale3D(FVector(1.3f, 1.3f, 0.02f));
	PlayerMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlayerMarker->SetCastShadow(false);
	PlayerMarker->bReceivesDecals = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> MarkerMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (MarkerMesh.Succeeded())
	{
		PlayerMarker->SetStaticMesh(MarkerMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MarkerMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (MarkerMaterial.Succeeded())
	{
		PlayerMarker->SetMaterial(0, MarkerMaterial.Object);
	}

	// Mana regeneration of the browser prototype
	ManaRegenPerSecond = 13.0f;

	// Create the element queue
	ElementComponent = CreateDefaultSubobject<UVaelElementComponent>(TEXT("ElementQueue"));

	// Create the queue orbs: one per possible slot, laid out when the game starts
	QueueOrbRoot = CreateDefaultSubobject<USceneComponent>(TEXT("QueueOrbRoot"));
	QueueOrbRoot->SetupAttachment(RootComponent);
	QueueOrbRoot->SetUsingAbsoluteRotation(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> OrbMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	for (int32 SlotIndex = 0; SlotIndex < VaelElements::MaxQueueSlots; ++SlotIndex)
	{
		UStaticMeshComponent* Orb = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("QueueOrb%d"), SlotIndex));

		Orb->SetupAttachment(QueueOrbRoot);
		Orb->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Orb->SetCastShadow(false);
		Orb->bReceivesDecals = false;
		Orb->SetVisibility(false);

		if (OrbMesh.Succeeded())
		{
			Orb->SetStaticMesh(OrbMesh.Object);
		}

		if (MarkerMaterial.Succeeded())
		{
			Orb->SetMaterial(0, MarkerMaterial.Object);
		}

		QueueOrbs.Add(Orb);
	}

	// Activate ticking in order to drive the dodge roll.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AVaelCharacter::BeginPlay()
{
	Super::BeginPlay();

	DefaultMeshScale = GetMesh()->GetRelativeScale3D();

	// Line the queue orbs up across the screen
	AVaelGameMode* GameMode = GetWorld()->GetAuthGameMode<AVaelGameMode>();
	const AVaelSharedCamera* SharedCamera = GameMode != nullptr ? GameMode->GetSharedCamera() : nullptr;

	QueueOrbRoot->SetRelativeLocation(FVector(0.f, 0.f, QueueOrbHeight));
	QueueOrbRoot->SetWorldRotation(FRotator(0.f, SharedCamera != nullptr ? SharedCamera->GetCameraYaw() : 0.f, 0.f));

	ElementComponent->OnQueueChanged.AddUObject(this, &AVaelCharacter::RefreshQueueOrbs);
	RefreshQueueOrbs();
}

void AVaelCharacter::RefreshQueueOrbs()
{
	const TArray<EVaelElement>& Queue = ElementComponent->GetQueue();
	const int32 NumSlots = ElementComponent->GetNumSlots();
	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();

	for (int32 SlotIndex = 0; SlotIndex < QueueOrbs.Num(); ++SlotIndex)
	{
		UStaticMeshComponent* Orb = QueueOrbs[SlotIndex];

		Orb->SetVisibility(SlotIndex < NumSlots);
		if (SlotIndex >= NumSlots)
		{
			continue;
		}

		// Filled slots are bigger and carry the color of their element
		const bool bFilled = Queue.IsValidIndex(SlotIndex);

		Orb->SetRelativeLocation(FVector(0.f, (SlotIndex - (NumSlots - 1) * 0.5f) * QueueOrbSpacing, 0.f));
		Orb->SetRelativeScale3D(FVector(bFilled ? 0.24f : 0.1f));

		UMaterialInstanceDynamic* OrbMaterial = Cast<UMaterialInstanceDynamic>(Orb->GetMaterial(0));
		if (OrbMaterial == nullptr)
		{
			OrbMaterial = Orb->CreateAndSetMaterialInstanceDynamic(0);
		}

		if (OrbMaterial != nullptr)
		{
			OrbMaterial->SetVectorParameterValue(MarkerColorParameter, bFilled ? MagicSettings->GetElementColor(Queue[SlotIndex]) : EmptySlotColor);
		}
	}
}

void AVaelCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

#if ENABLE_DRAW_DEBUG
	if (bShowStatusText)
	{
		const FString Status = FString::Printf(TEXT("Leben %.0f   Mana %.0f"), GetHealth(), GetMana());
		DrawDebugString(GetWorld(), FVector(0.f, 0.f, QueueOrbHeight + 45.f), Status, this, FColor::White, 0.f, true);
	}
#endif

	if (bIsDodging)
	{
		if (GetWorld()->GetTimeSeconds() >= DodgeEndTime)
		{
			EndDodge();
		}
		else
		{
			// Hold the roll speed, leave falling to the movement component
			UCharacterMovementComponent* Movement = GetCharacterMovement();
			Movement->Velocity = FVector(DodgeDirection.X * DodgeSpeed, DodgeDirection.Y * DodgeSpeed, Movement->Velocity.Z);
		}
	}
}

void AVaelCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	const AVaelPlayerController* VaelController = Cast<AVaelPlayerController>(NewController);
	const AVaelGameMode* GameMode = GetWorld()->GetAuthGameMode<AVaelGameMode>();

	if (VaelController != nullptr && GameMode != nullptr)
	{
		SetPlayerColor(GameMode->GetPlayerColor(VaelController->GetPlayerSlot()));

		UE_LOG(LogVael, Log, TEXT("Player slot %d controls '%s'"), VaelController->GetPlayerSlot(), *GetNameSafe(this));
	}
}

bool AVaelCharacter::StartDodge(const FVector& WorldDirection)
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (bIsDodging || Now < NextDodgeTime || !GetCharacterMovement()->IsMovingOnGround())
	{
		return false;
	}

	DodgeDirection = WorldDirection.GetSafeNormal2D();
	if (DodgeDirection.IsNearlyZero())
	{
		DodgeDirection = GetActorForwardVector().GetSafeNormal2D();
	}

	bIsDodging = true;
	DodgeEndTime = Now + DodgeDuration;
	NextDodgeTime = Now + DodgeCooldown;

	GetMesh()->SetRelativeScale3D(DefaultMeshScale * FVector(1.f, 1.f, DodgeMeshSquash));

	return true;
}

void AVaelCharacter::SetPlayerColor(const FLinearColor& Color)
{
	if (UMaterialInstanceDynamic* MarkerMaterial = PlayerMarker->CreateAndSetMaterialInstanceDynamic(0))
	{
		MarkerMaterial->SetVectorParameterValue(MarkerColorParameter, Color);
	}

	USkeletalMeshComponent* BodyMesh = GetMesh();
	for (int32 MaterialIndex = 0; MaterialIndex < BodyMesh->GetNumMaterials(); ++MaterialIndex)
	{
		if (UMaterialInstanceDynamic* BodyMaterial = BodyMesh->CreateAndSetMaterialInstanceDynamic(MaterialIndex))
		{
			BodyMaterial->SetVectorParameterValue(BodyTintParameter, Color);
		}
	}
}

void AVaelCharacter::EndDodge()
{
	bIsDodging = false;

	GetMesh()->SetRelativeScale3D(DefaultMeshScale);

	// Come out of the roll at no more than walking speed
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	const float ExitSpeed = FMath::Min(DodgeSpeed, Movement->GetMaxSpeed());
	Movement->Velocity = FVector(DodgeDirection.X * ExitSpeed, DodgeDirection.Y * ExitSpeed, Movement->Velocity.Z);
}
