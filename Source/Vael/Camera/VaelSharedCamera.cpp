// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/VaelSharedCamera.h"
#include "Camera/CameraComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UnrealClient.h"

AVaelSharedCamera::AVaelSharedCamera()
{
	// Create the camera component
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->bConstrainAspectRatio = false;
	Camera->SetFieldOfView(FieldOfView);
	RootComponent = Camera;

	// Update after the pawns have moved so the camera never lags a frame behind
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
}

void AVaelSharedCamera::BeginPlay()
{
	Super::BeginPlay();

	Camera->SetFieldOfView(FieldOfView);
	CurrentDistance = MinDistance;
}

void AVaelSharedCamera::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Collect the pawns of all players
	TArray<FVector, TInlineAllocator<4>> Locations;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		if (PlayerController != nullptr && PlayerController->GetPawn() != nullptr)
		{
			Locations.Add(PlayerController->GetPawn()->GetActorLocation());
		}
	}

	NumTrackedPlayers = Locations.Num();
	if (NumTrackedPlayers == 0)
	{
		return;
	}

	// Find the centre of the group and how far the outermost player is from it
	GroupCenter = FBox(Locations.GetData(), Locations.Num()).GetCenter();

	float Radius = 0.0f;
	for (const FVector& Location : Locations)
	{
		Radius = FMath::Max(Radius, FVector::Dist2D(Location, GroupCenter));
	}

	// Zoom out far enough to frame the whole group
	const float FitTangent = GetFitTangent();
	const float TargetDistance = FMath::Clamp((Radius + FramePadding) / FitTangent, MinDistance, FMath::Max(MinDistance, MaxDistance));
	LeashRadius = FMath::Max(MaxDistance * FitTangent - FramePadding, 0.0f);

	if (bSnapNextUpdate)
	{
		FocusPoint = GroupCenter;
		CurrentDistance = TargetDistance;
		bSnapNextUpdate = false;
	}
	else
	{
		FocusPoint = FMath::VInterpTo(FocusPoint, GroupCenter, DeltaSeconds, FollowSpeed);
		CurrentDistance = FMath::FInterpTo(CurrentDistance, TargetDistance, DeltaSeconds, ZoomSpeed);
	}

	const FRotator Rotation(CameraPitch, CameraYaw, 0.0f);
	SetActorLocationAndRotation(FocusPoint - Rotation.Vector() * CurrentDistance, Rotation);
}

FVector AVaelSharedCamera::ConstrainMoveDirection(const FVector& PawnLocation, const FVector& Direction) const
{
	if (NumTrackedPlayers < 2)
	{
		return Direction;
	}

	FVector FromCenter = PawnLocation - GroupCenter;
	FromCenter.Z = 0.0f;

	const float Distance = FromCenter.Size();
	if (Distance < LeashRadius || Distance <= UE_KINDA_SMALL_NUMBER)
	{
		return Direction;
	}

	// At the edge: keep sideways and inward movement, drop the outward part
	const FVector Outward = FromCenter / Distance;
	const float OutwardAmount = FVector::DotProduct(Direction, Outward);

	return OutwardAmount > 0.0f ? Direction - Outward * OutwardAmount : Direction;
}

float AVaelSharedCamera::GetFitTangent() const
{
	float AspectRatio = 16.0f / 9.0f;

	if (const UGameViewportClient* ViewportClient = GetWorld()->GetGameViewport())
	{
		if (ViewportClient->Viewport != nullptr)
		{
			const FIntPoint Size = ViewportClient->Viewport->GetSizeXY();
			if (Size.X > 0 && Size.Y > 0)
			{
				AspectRatio = static_cast<float>(Size.X) / static_cast<float>(Size.Y);
			}
		}
	}

	// The narrower of the two view angles decides how much fits on screen
	const float HorizontalTangent = FMath::Tan(FMath::DegreesToRadians(FieldOfView * 0.5f));
	const float VerticalTangent = HorizontalTangent / AspectRatio;

	return FMath::Max(FMath::Min(HorizontalTangent, VerticalTangent), UE_KINDA_SMALL_NUMBER);
}
