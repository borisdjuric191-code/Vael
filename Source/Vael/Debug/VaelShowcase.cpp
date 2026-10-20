// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debug/VaelShowcase.h"
#include "Combat/VaelCombatStatics.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Creatures/VaelCreatureData.h"
#include "Creatures/VaelHornBeetle.h"
#include "Magic/VaelWaterBolt.h"
#include "Magic/VaelWaterGeyser.h"
#include "Magic/VaelWaterWave.h"
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

	// Pictures of the menus are taken while the game is paused
	SetTickableWhenPaused(true);

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
			// Steps joined by ~ run one per repeat, the last one again and again, like VaelCast_W_-15~VaelCast_W_-15~VaelCast_WWW_-15
			TArray<FString> Steps;
			Command.TrimQuotes().ParseIntoArray(Steps, TEXT("~"));
			const FString Step = Steps.IsEmpty() ? FString() : Steps[FMath::Min(NumCommandsRun++, Steps.Num() - 1)];

			// Several commands of one step joined by +, like VaelLearnAll+VaelCast_WWW_-15
			TArray<FString> Commands;
			Step.ParseIntoArray(Commands, TEXT("+"));
			for (const FString& Single : Commands)
			{
				PlayerController->ConsoleCommand(Single.Replace(TEXT("_"), TEXT(" ")), true);
			}
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

		// -VaelShowcaseWater=Wave follows a breaking wave from the side and a little ahead
		for (TActorIterator<AVaelWaterWave> It(GetWorld()); It && WaterView == TEXT("Wave"); ++It)
		{
			Focus = It->GetActorLocation() + FVector(0.0f, 0.0f, 90.0f);
			Eye = Focus + It->GetActorRightVector() * 650.0f + It->GetActorForwardVector() * 420.0f + FVector(0.0f, 0.0f, 330.0f);
			bFound = true;
		}

		// -VaelShowcaseWater=Geyser looks at a geyser from the side, a little above its foot
		for (TActorIterator<AVaelWaterGeyser> It(GetWorld()); It && WaterView == TEXT("Geyser"); ++It)
		{
			Focus = It->GetActorLocation() + FVector(0.0f, 0.0f, 230.0f);
			Eye = Focus + FVector(-900.0f, -520.0f, 380.0f);
			bFound = true;
		}

		for (TActorIterator<AVaelWaterBolt> It(GetWorld()); It && WaterView != TEXT("Puddle") && WaterView != TEXT("Wave") && WaterView != TEXT("Geyser"); ++It)
		{
			Focus = It->GetActorLocation();
			const FVector Side = FVector::CrossProduct(It->GetActorForwardVector(), FVector::UpVector).GetSafeNormal();
			Eye = Focus + Side * 220.0f + FVector(0.0f, 0.0f, 140.0f) - It->GetActorForwardVector() * 60.0f;
			bFound = true;
		}

		if (!bFound && WaterView != TEXT("Wave") && WaterView != TEXT("Geyser"))
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

	// -VaelShowcaseEye=X,Y,Z -VaelShowcaseFocus=X,Y,Z: fixed camera at a world place in cm, for things the game camera can't see well
	FString EyeText;
	FString FocusText;
	if (FParse::Value(FCommandLine::Get(), TEXT("-VaelShowcaseEye="), EyeText, false) && FParse::Value(FCommandLine::Get(), TEXT("-VaelShowcaseFocus="), FocusText, false))
	{
		APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
		TArray<FString> EyeParts;
		TArray<FString> FocusParts;
		EyeText.ParseIntoArray(EyeParts, TEXT(","));
		FocusText.ParseIntoArray(FocusParts, TEXT(","));

		if (PlayerController != nullptr && EyeParts.Num() == 3 && FocusParts.Num() == 3)
		{
			if (Closeup == nullptr)
			{
				Closeup = GetWorld()->SpawnActor<ACameraActor>();
				Closeup->GetCameraComponent()->SetFieldOfView(50.0f);
				Closeup->GetCameraComponent()->bConstrainAspectRatio = false;
				Closeup->GetCameraComponent()->PostProcessSettings.bOverride_MotionBlurAmount = true;
				Closeup->GetCameraComponent()->PostProcessSettings.MotionBlurAmount = 0.0f;
			}

			const FVector Eye(FCString::Atof(*EyeParts[0]), FCString::Atof(*EyeParts[1]), FCString::Atof(*EyeParts[2]));
			const FVector Focus(FCString::Atof(*FocusParts[0]), FCString::Atof(*FocusParts[1]), FCString::Atof(*FocusParts[2]));
			Closeup->SetActorLocationAndRotation(Eye, (Focus - Eye).Rotation());
			if (PlayerController->GetViewTarget() != Closeup)
			{
				PlayerController->SetViewTarget(Closeup);
			}
		}
	}

	// -VaelShowcaseBehind: camera low behind the player, looking where they aim, for spells that roll over the ground
	if (FParse::Param(FCommandLine::Get(), TEXT("VaelShowcaseBehind")))
	{
		APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
		const APawn* PlayerPawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
		if (PlayerPawn != nullptr)
		{
			if (Closeup == nullptr)
			{
				Closeup = GetWorld()->SpawnActor<ACameraActor>();
				Closeup->GetCameraComponent()->SetFieldOfView(70.0f);
				Closeup->GetCameraComponent()->bConstrainAspectRatio = false;
				Closeup->GetCameraComponent()->PostProcessSettings.bOverride_MotionBlurAmount = true;
				Closeup->GetCameraComponent()->PostProcessSettings.MotionBlurAmount = 0.0f;
			}

			const FVector Aim = FRotator(0.0f, PlayerController->GetControlRotation().Yaw, 0.0f).Vector();
			const FVector Eye = PlayerPawn->GetActorLocation() - Aim * 380.0f + FVector(0.0f, 0.0f, 260.0f);
			const FVector Focus = PlayerPawn->GetActorLocation() + Aim * 350.0f;
			Closeup->SetActorLocationAndRotation(Eye, (Focus - Eye).Rotation());
			if (PlayerController->GetViewTarget() != Closeup)
			{
				PlayerController->SetViewTarget(Closeup);
			}
		}
	}

	// -VaelShowcaseHero: close camera on the first player from the front, for body and wings; =Back from behind, =Far a step back for the plants around them
	FString HeroView;
	if (FParse::Param(FCommandLine::Get(), TEXT("VaelShowcaseHero")) || FParse::Value(FCommandLine::Get(), TEXT("-VaelShowcaseHero="), HeroView))
	{
		APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
		const APawn* PlayerPawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
		if (PlayerPawn != nullptr)
		{
			if (Closeup == nullptr)
			{
				Closeup = GetWorld()->SpawnActor<ACameraActor>();
				Closeup->GetCameraComponent()->SetFieldOfView(50.0f);
				Closeup->GetCameraComponent()->bConstrainAspectRatio = false;
				Closeup->GetCameraComponent()->PostProcessSettings.bOverride_MotionBlurAmount = true;
				Closeup->GetCameraComponent()->PostProcessSettings.MotionBlurAmount = 0.0f;
			}

			const FVector Offset = HeroView == TEXT("Back") ? FVector(-300.0f, 140.0f, 120.0f) : HeroView == TEXT("Far") ? FVector(700.0f, -520.0f, 520.0f) : FVector(300.0f, -140.0f, 90.0f);
			const FVector Focus = PlayerPawn->GetActorLocation() + (HeroView == TEXT("Far") ? PlayerPawn->GetActorForwardVector() * 250.0f : FVector(0.0f, 0.0f, 20.0f));
			const FVector Eye = PlayerPawn->GetActorLocation() + FRotator(0.0f, PlayerPawn->GetActorRotation().Yaw, 0.0f).RotateVector(Offset);
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

	// The panels of the players are widgets on top of the game picture: they only show in the picture with the UI
	FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("Showcase_%02d"), ShotsTaken + 1), true, false);
	++ShotsTaken;
	Countdown = Interval;
}
