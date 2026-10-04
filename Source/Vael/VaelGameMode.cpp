// Copyright Epic Games, Inc. All Rights Reserved.

#include "VaelGameMode.h"
#include "Camera/VaelSharedCamera.h"
#include "Creatures/VaelTrainingDummy.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Player/VaelCharacter.h"
#include "Player/VaelPlayerController.h"

AVaelGameMode::AVaelGameMode()
{
	DefaultPawnClass = AVaelCharacter::StaticClass();
	PlayerControllerClass = AVaelPlayerController::StaticClass();
	SharedCameraClass = AVaelSharedCamera::StaticClass();
	TrainingDummyClass = AVaelTrainingDummy::StaticClass();

	// blue, green, orange, beige
	PlayerColors.Add(FLinearColor(0.05f, 0.25f, 1.0f));
	PlayerColors.Add(FLinearColor(0.1f, 0.7f, 0.15f));
	PlayerColors.Add(FLinearColor(1.0f, 0.4f, 0.02f));
	PlayerColors.Add(FLinearColor(0.73f, 0.56f, 0.33f));
}

void AVaelGameMode::PostLogin(APlayerController* NewPlayer)
{
	// The slot has to be known before the pawn is spawned, which happens in Super
	if (AVaelPlayerController* VaelController = Cast<AVaelPlayerController>(NewPlayer))
	{
		VaelController->SetPlayerSlot(FindFreePlayerSlot(VaelController));
	}

	Super::PostLogin(NewPlayer);
}

void AVaelGameMode::RestartPlayer(AController* NewPlayer)
{
	if (NewPlayer != nullptr)
	{
		// Look for a player who is already in the level
		const APawn* Anchor = nullptr;
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* Other = It->Get();
			if (Other != nullptr && Other != NewPlayer && Other->GetPawn() != nullptr)
			{
				Anchor = Other->GetPawn();
				break;
			}
		}

		if (Anchor != nullptr)
		{
			const AVaelPlayerController* VaelController = Cast<AVaelPlayerController>(NewPlayer);
			const int32 PlayerSlot = VaelController != nullptr ? FMath::Max(VaelController->GetPlayerSlot(), 0) : 0;

			// Each slot gets its own spot around the anchor
			const float Angle = FMath::DegreesToRadians(90.0f * PlayerSlot);
			const FVector Offset = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * JoinSpawnRadius;

			RestartPlayerAtTransform(NewPlayer, FTransform(Anchor->GetActorRotation(), Anchor->GetActorLocation() + Offset));
			return;
		}
	}

	Super::RestartPlayer(NewPlayer);

	if (!bTrainingDummiesSpawned && NewPlayer != nullptr && NewPlayer->GetPawn() != nullptr)
	{
		bTrainingDummiesSpawned = true;
		SpawnTrainingDummies(NewPlayer->GetPawn()->GetActorLocation());
	}
}

void AVaelGameMode::SpawnTrainingDummies(const FVector& Center)
{
	if (TrainingDummyClass == nullptr)
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	for (int32 DummyIndex = 0; DummyIndex < TrainingDummyCount; ++DummyIndex)
	{
		const float Angle = UE_TWO_PI * DummyIndex / TrainingDummyCount;
		const FVector Location = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * TrainingDummyDistance;

		GetWorld()->SpawnActor<AVaelTrainingDummy>(TrainingDummyClass, Location, FRotator::ZeroRotator, SpawnParameters);
	}
}

AVaelSharedCamera* AVaelGameMode::GetSharedCamera()
{
	if (SharedCamera == nullptr)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		UClass* CameraClass = SharedCameraClass != nullptr ? SharedCameraClass.Get() : AVaelSharedCamera::StaticClass();
		SharedCamera = GetWorld()->SpawnActor<AVaelSharedCamera>(CameraClass, FTransform::Identity, SpawnParameters);
	}

	return SharedCamera;
}

FLinearColor AVaelGameMode::GetPlayerColor(int32 PlayerSlot) const
{
	return PlayerColors.IsValidIndex(PlayerSlot) ? PlayerColors[PlayerSlot] : FLinearColor::White;
}

int32 AVaelGameMode::FindFreePlayerSlot(const APlayerController* ForPlayer) const
{
	TBitArray<> UsedSlots(false, MaxLocalPlayers);

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const AVaelPlayerController* Other = Cast<AVaelPlayerController>(It->Get());
		if (Other != nullptr && Other != ForPlayer && UsedSlots.IsValidIndex(Other->GetPlayerSlot()))
		{
			UsedSlots[Other->GetPlayerSlot()] = true;
		}
	}

	const int32 FreeSlot = UsedSlots.Find(false);
	return FreeSlot != INDEX_NONE ? FreeSlot : 0;
}
