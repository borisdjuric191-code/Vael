// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/VaelCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
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

	// Activate ticking in order to drive the dodge roll.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AVaelCharacter::BeginPlay()
{
	Super::BeginPlay();

	DefaultMeshScale = GetMesh()->GetRelativeScale3D();
}

void AVaelCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

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
