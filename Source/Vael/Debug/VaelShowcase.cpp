// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debug/VaelShowcase.h"
#include "Combat/VaelCombatStatics.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Creatures/VaelHornBeetle.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"
#include "Vael.h"

namespace
{
	/** Seconds before the first picture, so the level and the creatures have settled */
	constexpr float ShowcaseWarmUp = 4.0f;
}

AVaelShowcase::AVaelShowcase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

bool AVaelShowcase::IsRequested()
{
	FString Value;
	return FParse::Value(FCommandLine::Get(), TEXT("-VaelShowcase="), Value);
}

void AVaelShowcase::BeginPlay()
{
	Super::BeginPlay();

	FParse::Value(FCommandLine::Get(), TEXT("-VaelShowcase="), NumShots);
	FParse::Value(FCommandLine::Get(), TEXT("-VaelShowcaseInterval="), Interval);
	NumShots = FMath::Max(NumShots, 1);
	Interval = FMath::Max(Interval, 0.2f);

	FParse::Value(FCommandLine::Get(), TEXT("-VaelShowcaseFire="), FireTime);
	Countdown = ShowcaseWarmUp;

	UE_LOG(LogVael, Display, TEXT("Showcase: %d pictures every %.1f s"), NumShots, Interval);
}

void AVaelShowcase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;

	// Optional console command once the player is there, underscores for spaces: -VaelShowcaseCmd=VaelSpawn_Glutkriecher_2
	FString Command;
	if (!bCommandDone && Age > 1.0f && FParse::Value(FCommandLine::Get(), TEXT("-VaelShowcaseCmd="), Command, false))
	{
		bCommandDone = true;
		if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
		{
			PlayerController->ConsoleCommand(Command.TrimQuotes().Replace(TEXT("_"), TEXT(" ")), true);
		}
	}

	// Close camera on the living creature closest to the player, slightly from the front and above
	if (FParse::Param(FCommandLine::Get(), TEXT("VaelShowcaseCloseup")))
	{
		APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
		const APawn* PlayerPawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
		const AVaelCreature* Subject = nullptr;
		float Closest = UE_BIG_NUMBER;
		for (TActorIterator<AVaelCreature> It(GetWorld()); It && PlayerPawn != nullptr; ++It)
		{
			const float Distance = FVector::Dist(It->GetActorLocation(), PlayerPawn->GetActorLocation());
			if (!It->IsDead() && Distance < Closest)
			{
				Subject = *It;
				Closest = Distance;
			}
		}

		if (Subject != nullptr && PlayerController != nullptr)
		{
			if (Closeup == nullptr)
			{
				Closeup = GetWorld()->SpawnActor<ACameraActor>();
				Closeup->GetCameraComponent()->SetFieldOfView(50.0f);
				Closeup->GetCameraComponent()->bConstrainAspectRatio = false;
			}

			const FVector Focus = Subject->GetActorLocation();
			const FVector Eye = Focus + Subject->GetActorRotation().RotateVector(FVector(260.0f, -200.0f, 170.0f));
			Closeup->SetActorLocationAndRotation(Eye, (Focus - Eye).Rotation());
			if (PlayerController->GetViewTarget() != Closeup)
			{
				PlayerController->SetViewTarget(Closeup);
			}
		}
	}

	// Fire on every tensed thread at the given time
	if (FireTime >= 0.0f && Age >= FireTime)
	{
		for (TActorIterator<AVaelHornBeetle> It(GetWorld()); It; ++It)
		{
			if (It->GetBeetleState() == EVaelBeetleState::Tension || It->GetBeetleState() == EVaelBeetleState::Locked)
			{
				UVaelCombatStatics::DealDamage(nullptr, *It, 1.0f, EVaelElement::Fire);
				FireTime = -1.0f;
			}
		}
	}

	Countdown -= DeltaSeconds;
	if (Countdown > 0.0f)
	{
		return;
	}

	if (ShotsTaken >= NumShots)
	{
		UE_LOG(LogVael, Display, TEXT("Showcase: done"));
		FPlatformMisc::RequestExit(false);
		return;
	}

	FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("Showcase_%02d"), ShotsTaken + 1), false, false);
	++ShotsTaken;
	Countdown = Interval;
}
