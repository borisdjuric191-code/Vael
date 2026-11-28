// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelPositionGuard.h"

#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Vael.h"

UVaelPositionGuard::UVaelPositionGuard()
{
	PrimaryComponentTick.bCanEverTick = true;
	// After the movement of the frame, so the spot read is where the character ended up
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UVaelPositionGuard::BeginPlay()
{
	Super::BeginPlay();
	Character = Cast<ACharacter>(GetOwner());
}

void UVaelPositionGuard::NoteTeleport(const FVector& To)
{
	bTeleported = true;
	FallingTime = 0.0f;
	// where a teleport puts the player is the new safe spot until real ground is found there: a rescue never brings someone back
	// to where they came from (out of a cave they just went into)
	LastSafe = To;
	bHasSafe = true;
	SafeClock = 0.0f;
}

void UVaelPositionGuard::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ACharacter* Owner = Character.Get();
	UCharacterMovementComponent* Movement = Owner != nullptr ? Owner->GetCharacterMovement() : nullptr;
	if (Movement == nullptr)
	{
		return;
	}
	const FVector Here = Owner->GetActorLocation();

	// A jump no teleport explains: into the log with everything that tells where it came from
	if (bHasLast && !bTeleported && FVector::Dist(Here, LastLocation) > JumpReport)
	{
		const FFindFloorResult& Floor = Movement->CurrentFloor;
		UE_LOG(LogVael, Warning, TEXT("VaelGuard: '%s' jumped %.1f m in one frame from %s to %s (mode %d, speed %.0f, floor '%s', base '%s')"),
			*Owner->GetName(), FVector::Dist(Here, LastLocation) / 100.0f, *LastLocation.ToCompactString(), *Here.ToCompactString(),
			static_cast<int32>(Movement->MovementMode.GetValue()), Movement->Velocity.Size(), *GetNameSafe(Floor.HitResult.GetActor()),
			*GetNameSafe(Owner->GetMovementBase() != nullptr ? Owner->GetMovementBase()->GetOwner() : nullptr));
	}

	// Flung: a speed far above anything the game gives, mostly the push out of a mesh the capsule ended up in
	if (Movement->MovementMode != MOVE_None && Movement->Velocity.Size2D() > MaxSpeed)
	{
		UE_LOG(LogVael, Warning, TEXT("VaelGuard: '%s' flung at %.0f cm/s at %s (mode %d), slowed"), *Owner->GetName(), Movement->Velocity.Size2D(),
			*Here.ToCompactString(), static_cast<int32>(Movement->MovementMode.GetValue()));
		const FVector Flat = FVector(Movement->Velocity.X, Movement->Velocity.Y, 0.0f).GetClampedToMaxSize(MaxSpeed * 0.25f);
		Movement->Velocity = FVector(Flat.X, Flat.Y, FMath::Min(Movement->Velocity.Z, 600.0f));
	}
	if (Movement->Velocity.Z > MaxSpeed)
	{
		Movement->Velocity.Z = 600.0f;
	}

	// The last safe spot: standing on walkable ground (not on something moving)
	SafeClock += DeltaTime;
	if (Movement->IsMovingOnGround() && Movement->CurrentFloor.IsWalkableFloor() && SafeClock >= 0.3f)
	{
		const UPrimitiveComponent* Base = Owner->GetMovementBase();
		if (Base == nullptr || Base->Mobility != EComponentMobility::Movable)
		{
			LastSafe = Here;
			bHasSafe = true;
			SafeClock = 0.0f;
		}
	}

	// Falling through the floor (a tile not streamed in yet, a gap): back to the last safe spot instead of out of the world
	FallingTime = Movement->IsFalling() ? FallingTime + DeltaTime : 0.0f;
	const AWorldSettings* Settings = GetWorld()->GetWorldSettings();
	const float KillZ = Settings != nullptr ? Settings->KillZ : -UE_BIG_NUMBER;
	const bool bDeep = bHasSafe && Here.Z < LastSafe.Z - RescueDepth;
	if (bHasSafe && ((FallingTime > RescueAfterFalling && bDeep) || Here.Z < KillZ + 2000.0f))
	{
		++Rescues;
		UE_LOG(LogVael, Warning, TEXT("VaelGuard: '%s' fell through the floor at %s (%.1f s falling), back to %s (rescue %d)"), *Owner->GetName(),
			*Here.ToCompactString(), FallingTime, *LastSafe.ToCompactString(), Rescues);
		bTeleported = true;
		Owner->TeleportTo(LastSafe + FVector(0.0f, 0.0f, 30.0f), Owner->GetActorRotation(), false, true);
		Movement->Velocity = FVector::ZeroVector;
		Movement->SetMovementMode(MOVE_Falling);
		FallingTime = 0.0f;
	}

	LastLocation = Owner->GetActorLocation();
	bHasLast = true;
	bTeleported = false;
}
