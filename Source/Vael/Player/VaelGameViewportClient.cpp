// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/VaelGameViewportClient.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
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
		return false;
	}

	if (CurrentGameInstance->GetNumLocalPlayers() >= AVaelGameMode::MaxLocalPlayers)
	{
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
