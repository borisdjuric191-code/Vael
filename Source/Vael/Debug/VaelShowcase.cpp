// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debug/VaelShowcase.h"
#include "Combat/VaelCombatStatics.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Creatures/VaelCreatureData.h"
#include "Creatures/VaelHornBeetle.h"
#include "Magic/VaelWaterBolt.h"
#include "Magic/VaelWaterPuddle.h"
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

	// Optional console command once the player is there, underscores for spaces: -VaelShowcaseCmd=VaelSpawn_Glutkriecher_2;
	// -VaelShowcaseCmdEvery=<seconds> repeats it, like a spell cast again and again
	FString Command;
	float CommandTime = 1.0f;
	float CommandEvery = 0.0f;
	FParse::Value(FCommandLine::Get(), TEXT("-VaelShowcaseCmdAt="), CommandTime);
	FParse::Value(FCommandLine::Get(), TEXT("-VaelShowcaseCmdEvery="), CommandEvery);
	const bool bCommandDue = !bCommandDone ? Age > CommandTime : CommandEvery > 0.0f && Age > NextCommandAge;
	if (bCommandDue && FParse::Value(FCommandLine::Get(), TEXT("-VaelShowcaseCmd="), Command, false))
	{
		bCommandDone = true;
		NextCommandAge = Age + CommandEvery;
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

		// -VaelShowcaseKind=Glutkriecher looks only at creatures of that kind
		FString OnlyKind;
		FParse::Value(FCommandLine::Get(), TEXT("-VaelShowcaseKind="), OnlyKind);
		for (TActorIterator<AVaelCreature> It(GetWorld()); It && PlayerPawn != nullptr; ++It)
		{
			const float Distance = FVector::Dist(It->GetActorLocation(), PlayerPawn->GetActorLocation());
			if (!It->IsDead() && !It->IsHidden() && Distance < Closest && (OnlyKind.IsEmpty() || It->GetCreatureData()->KindId.ToString().Contains(OnlyKind)))
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
				Closeup->GetCameraComponent()->PostProcessSettings.bOverride_MotionBlurAmount = true;
				Closeup->GetCameraComponent()->PostProcessSettings.MotionBlurAmount = 0.0f;
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

	// -VaelShowcaseWater: close camera on a jet of water in flight, from its side; without one on the biggest puddle, from above
	// -VaelShowcaseWater=Puddle looks only at the puddle
	FString WaterView;
	const bool bWaterView = FParse::Param(FCommandLine::Get(), TEXT("VaelShowcaseWater")) || FParse::Value(FCommandLine::Get(), TEXT("-VaelShowcaseWater="), WaterView);
	if (bWaterView)
	{
		APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();

		FVector Eye = FVector::ZeroVector;
		FVector Focus = FVector::ZeroVector;
		bool bFound = false;
		for (TActorIterator<AVaelWaterBolt> It(GetWorld()); It && WaterView != TEXT("Puddle"); ++It)
		{
			Focus = It->GetActorLocation();
			const FVector Side = FVector::CrossProduct(It->GetActorForwardVector(), FVector::UpVector).GetSafeNormal();
			Eye = Focus + Side * 220.0f + FVector(0.0f, 0.0f, 140.0f) - It->GetActorForwardVector() * 60.0f;
			bFound = true;
		}

		if (!bFound)
		{
			float Most = 0.0f;
			for (TActorIterator<AVaelWaterPuddle> It(GetWorld()); It; ++It)
			{
				if (It->GetWater() > Most)
				{
					Most = It->GetWater();
					Focus = It->GetActorLocation();
					Eye = Focus + FVector(-330.0f, -150.0f, 420.0f);
					bFound = true;
				}
			}
		}

		if (bFound && PlayerController != nullptr)
		{
			if (Closeup == nullptr)
			{
				Closeup = GetWorld()->SpawnActor<ACameraActor>();
				Closeup->GetCameraComponent()->SetFieldOfView(50.0f);
				Closeup->GetCameraComponent()->bConstrainAspectRatio = false;
				Closeup->GetCameraComponent()->PostProcessSettings.bOverride_MotionBlurAmount = true;
				Closeup->GetCameraComponent()->PostProcessSettings.MotionBlurAmount = 0.0f;
			}

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
