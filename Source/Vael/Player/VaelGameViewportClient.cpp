// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/VaelGameViewportClient.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "InputKeyEventArgs.h"
#include "Vael.h"
#include "VaelGameMode.h"

bool UVaelGameViewportClient::InputKey(const FInputKeyEventArgs& EventArgs)
{
	if (EventArgs.Event == IE_Pressed && EventArgs.Key == EKeys::Gamepad_Special_Right && TryJoinLocalPlayer(EventArgs.GetPlatformUser()))
	{
		return true;
	}

	return Super::InputKey(EventArgs);
}

void UVaelGameViewportClient::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// A controller that has no player yet belongs to its own Slate user, who has no focus: its presses would never reach the game,
	// and Start could not let it join. While the game window has focus, every controller points at the game.
	FocusCountdown -= DeltaTime;
	if (FocusCountdown <= 0.0f && Viewport != nullptr && Viewport->HasFocus() && FSlateApplication::IsInitialized())
	{
		FocusCountdown = 0.5f;
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}
}

bool UVaelGameViewportClient::TryJoinLocalPlayer(FPlatformUserId PlatformUser)
{
	UGameInstance* CurrentGameInstance = GetGameInstance();
	const UWorld* CurrentWorld = GetWorld();

	// Joining is only possible while a Vael game mode is running
	if (CurrentGameInstance == nullptr || CurrentWorld == nullptr || CurrentWorld->GetAuthGameMode<AVaelGameMode>() == nullptr)
	{
		return false;
	}

	// Every device belongs to one platform user and every platform user gets at most one player
	if (!PlatformUser.IsValid() || CurrentGameInstance->FindLocalPlayerFromPlatformUserId(PlatformUser) != nullptr)
	{
		UE_LOG(LogVael, Log, TEXT("Start pressed by platform user %d: %s"), PlatformUser.GetInternalId(), PlatformUser.IsValid() ? TEXT("already plays") : TEXT("no valid user"));
		return false;
	}

	if (CurrentGameInstance->GetNumLocalPlayers() >= AVaelGameMode::MaxLocalPlayers)
	{
		UE_LOG(LogVael, Log, TEXT("Start pressed by platform user %d: the game is full"), PlatformUser.GetInternalId());
		return false;
	}

	FString Error;
	if (CurrentGameInstance->CreateLocalPlayer(PlatformUser, Error, true) == nullptr)
	{
		UE_LOG(LogVael, Warning, TEXT("Local player could not join: %s"), *Error);
		return false;
	}

	return true;
}
