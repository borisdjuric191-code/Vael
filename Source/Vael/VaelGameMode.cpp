// Copyright Epic Games, Inc. All Rights Reserved.

#include "VaelGameMode.h"
#include "Camera/VaelSharedCamera.h"
#include "AbilitySystemComponent.h"
#include "Combat/VaelAttributeSet.h"
#include "Combat/VaelCombatStatics.h"
#include "Creatures/VaelCreature.h"
#include "Creatures/VaelCreatureData.h"
#include "Creatures/VaelTrainingDummy.h"
#include "GameFramework/PlayerStart.h"
#include "TimerManager.h"
#include "UI/VaelNoticeSubsystem.h"
#include "UI/VaelHUD.h"
#include "Vael.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Magic/VaelGroundArea.h"
#include "Player/VaelCharacter.h"
#include "Player/VaelPlayerController.h"
#include "Creatures/VaelCreatureSpawner.h"
#include "World/VaelGround.h"

AVaelGameMode::AVaelGameMode()
{
	DefaultPawnClass = AVaelCharacter::StaticClass();
	PlayerControllerClass = AVaelPlayerController::StaticClass();
	HUDClass = AVaelHUD::StaticClass();
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

	// Real levels bring their own creatures; the test setup is only for the test level
	if (!bTrainingDummiesSpawned && NewPlayer != nullptr && NewPlayer->GetPawn() != nullptr && AVaelCreatureSpawner::LevelHasSpawners(GetWorld()))
	{
		bTrainingDummiesSpawned = true;
	}

	if (!bTrainingDummiesSpawned && NewPlayer != nullptr && NewPlayer->GetPawn() != nullptr)
	{
		bTrainingDummiesSpawned = true;
		SpawnTrainingDummies(NewPlayer->GetPawn()->GetActorLocation());

		if (bSpawnTestAreas)
		{
			SpawnTestAreas(NewPlayer->GetPawn()->GetActorLocation());
		}

		if (bSpawnTestCreatures)
		{
			SpawnTestCreatures(NewPlayer->GetPawn()->GetActorLocation());
		}
	}
}

void AVaelGameMode::SpawnTestAreas(const FVector& Center)
{
	// Between the training dummies: the camp fire at 60 degrees, the puddle at 300 degrees
	const auto SpawnOnGround = [this, &Center](float AngleDegrees, EVaelElement Element, float Radius, bool bExtinguishable)
	{
		const float Angle = FMath::DegreesToRadians(AngleDegrees);
		FVector Location = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * TestAreaDistance;

		// The player start may float above the floor
		FHitResult GroundHit;
		if (VaelGround::TraceGround(GetWorld(), Location + FVector(0.0f, 0.0f, 500.0f), Location - FVector(0.0f, 0.0f, 2000.0f), GroundHit))
		{
			Location = GroundHit.Location + FVector(0.0f, 0.0f, 2.0f);
		}

		AVaelGroundArea::SpawnArea(GetWorld(), Location, Element, Radius, 0.0f, 0.0f, nullptr, bExtinguishable);
	};

	SpawnOnGround(60.0f, EVaelElement::Fire, 120.0f, false);
	SpawnOnGround(300.0f, EVaelElement::Water, 250.0f, true);
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

void AVaelGameMode::SpawnTestCreatures(const FVector& Center)
{
	// Between the test areas and away from the training dummies: crawlers ahead, harpies and a preacher to the sides
	const auto SpawnAtAngle = [this, &Center](UVaelCreatureData* Data, float AngleDegrees, int32 Count)
	{
		const float Angle = FMath::DegreesToRadians(AngleDegrees);
		SpawnCreatureGroup(Data, Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * TestCreatureDistance, Count, 150.0f);
	};

	SpawnAtAngle(GetMutableDefault<UVaelEmberCrawlerData>(), 0.0f, 3);
	SpawnAtAngle(GetMutableDefault<UVaelAshHarpyData>(), 120.0f, 3);
	SpawnAtAngle(GetMutableDefault<UVaelPreacherData>(), 240.0f, 1);
}

int32 AVaelGameMode::SpawnCreatureGroup(UVaelCreatureData* Data, const FVector& Center, int32 Count, float Spread)
{
	int32 NumSpawned = 0;

	for (int32 CreatureIndex = 0; CreatureIndex < Count; ++CreatureIndex)
	{
		// The first one in the center, the others in a ring around it
		const float Angle = UE_TWO_PI * CreatureIndex / FMath::Max(Count - 1, 1);
		const FVector Offset = CreatureIndex == 0 ? FVector::ZeroVector : FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Spread;

		FVector Ground;
		if (AVaelCreature::FindGround(GetWorld(), Center + Offset, Ground) && AVaelCreature::SpawnCreature(GetWorld(), Data, Ground, FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f)) != nullptr)
		{
			++NumSpawned;
		}
	}

	if (NumSpawned < Count)
	{
		UE_LOG(LogVael, Warning, TEXT("Only %d of %d creatures of '%s' found ground near %s"), NumSpawned, Count, *GetNameSafe(Data), *Center.ToCompactString());
	}

	return NumSpawned;
}

void AVaelGameMode::OnPlayerDowned(AVaelCharacter* Player)
{
	bool bAllDown = true;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		const AVaelCharacter* Other = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;

		if (Other != nullptr && !Other->IsDowned())
		{
			bAllDown = false;
			break;
		}
	}

	if (!bAllDown)
	{
		UVaelNoticeSubsystem::Post(this, FText::Format(NSLOCTEXT("VaelPlayers", "PlayerDown", "Spieler {0} ist gefallen"), Player != nullptr ? Player->GetPlayerNumber() : 1), NSLOCTEXT("VaelPlayers", "PlayerDownDetail", "Stell dich daneben, um wiederzubeleben."), FLinearColor(FColor(255, 122, 106)));
		return;
	}

	UVaelNoticeSubsystem::Post(this, NSLOCTEXT("VaelPlayers", "AllDown", "Alle sind gefallen"), FText::GetEmpty(), FLinearColor(FColor(255, 122, 106)));
	GetWorldTimerManager().SetTimer(RespawnTimer, this, &AVaelGameMode::RespawnGroup, FMath::Max(AllDownRespawnDelay, 0.01f));
}

void AVaelGameMode::RespawnGroup()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PlayerController = It->Get();
		AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
		if (Player == nullptr)
		{
			continue;
		}

		Player->Revive(Player->GetMaxHealth(), RespawnInvulnerability);
		Player->GetAbilitySystemComponent()->SetNumericAttributeBase(UVaelAttributeSet::GetManaAttribute(), Player->GetMaxMana());

		// Each slot gets its own spot around the start
		if (const AActor* Start = FindPlayerStart(PlayerController))
		{
			const AVaelPlayerController* VaelController = Cast<AVaelPlayerController>(PlayerController);
			const int32 PlayerSlot = VaelController != nullptr ? FMath::Max(VaelController->GetPlayerSlot(), 0) : 0;
			const float Angle = FMath::DegreesToRadians(90.0f * PlayerSlot);
			const FVector Offset = PlayerSlot == 0 ? FVector::ZeroVector : FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * JoinSpawnRadius;

			Player->TeleportTo(Start->GetActorLocation() + Offset, Player->GetActorRotation());
		}
	}

	UVaelNoticeSubsystem::Post(this, NSLOCTEXT("VaelPlayers", "Respawned", "Ihr erwacht am Lagerfeuer"), NSLOCTEXT("VaelPlayers", "RespawnedDetail", "Edda hat euch aus der Asche gezogen."), FLinearColor(FColor(232, 176, 122)));
}
