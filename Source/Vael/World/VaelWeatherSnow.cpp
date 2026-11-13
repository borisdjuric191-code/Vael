// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelWeatherSnow.h"
#include "Components/LocalFogVolumeComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Magic/VaelFlakes.h"
#include "World/VaelGround.h"
#include "World/VaelRegion.h"
#include "World/VaelWorldSettings.h"

namespace
{
	/** Flakes start this high above the ground and anywhere in this radius around the players (cm) */
	constexpr float SnowHeight = 1300.0f;
	constexpr float SnowRadius = 2600.0f;
	/** Size of the white haze of a blizzard around the players (cm) */
	constexpr float WhiteoutRadius = 4500.0f;
}

AVaelWeatherSnow::AVaelWeatherSnow()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Flakes = CreateDefaultSubobject<UVaelFlakesComponent>(TEXT("Flakes"));
	Flakes->SetupAttachment(RootComponent);
	// The flakes keep their world places while the look follows the players
	Flakes->SetUsingAbsoluteLocation(true);
	Flakes->SetUsingAbsoluteRotation(true);

	Whiteout = CreateDefaultSubobject<ULocalFogVolumeComponent>(TEXT("Whiteout"));
	Whiteout->SetupAttachment(RootComponent);
	Whiteout->SetRelativeScale3D(FVector(WhiteoutRadius / 500.0f));
	Whiteout->SetFogAlbedo(FLinearColor(0.85f, 0.9f, 1.0f));
	Whiteout->SetRadialFogExtinction(0.0f);
	Whiteout->SetHeightFogExtinction(0.0f);
}

AVaelWeatherSnow* AVaelWeatherSnow::Spawn(AVaelRegion* InRegion)
{
	UWorld* World = InRegion != nullptr ? InRegion->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}

	FActorSpawnParameters Parameters;
	Parameters.Owner = InRegion;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVaelWeatherSnow* Snow = World->SpawnActor<AVaelWeatherSnow>(AVaelWeatherSnow::StaticClass(), FTransform::Identity, Parameters);
	if (Snow != nullptr)
	{
		Snow->Region = InRegion;
	}
	return Snow;
}

void AVaelWeatherSnow::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Region == nullptr)
	{
		return;
	}

	// The middle of the players in the region
	FVector Middle = FVector::ZeroVector;
	int32 Count = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		const APawn* Pawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
		if (Pawn != nullptr && Region->Governs(Pawn->GetActorLocation()))
		{
			Middle += Pawn->GetActorLocation();
			++Count;
		}
	}
	if (Count == 0)
	{
		return;
	}
	Middle /= Count;
	SetActorLocation(Middle);

	const UVaelWorldSettings* Settings = UVaelWorldSettings::Get();
	const float Strength = Region->GetSnowStrength();
	const FVector Wind = Region->GetWind();

	// New flakes, shifted against the wind so the drifting snow still covers the picture
	Credit += Settings->SnowFlakesPerSecond * Strength * DeltaSeconds;
	const FVector Upwind = -Wind * 1.4f;
	while (Credit >= 1.0f)
	{
		Credit -= 1.0f;
		const float Angle = FMath::FRandRange(0.0f, UE_TWO_PI);
		const float Out = SnowRadius * FMath::Sqrt(FMath::FRand());
		const FVector Spot = Middle + Upwind + FVector(FMath::Cos(Angle) * Out, FMath::Sin(Angle) * Out, 0.0f);

		FHitResult GroundHit;
		const float GroundZ = VaelGround::TraceGround(GetWorld(), Spot + FVector(0.0f, 0.0f, 4000.0f), Spot - FVector(0.0f, 0.0f, 6000.0f), GroundHit)
			? GroundHit.Location.Z : Middle.Z - 100.0f;
		const FVector Start(Spot.X, Spot.Y, GroundZ + SnowHeight * FMath::FRandRange(0.4f, 1.0f));
		// A blizzard drives them nearly sideways; plain snow sinks slowly
		const float Fall = Strength > 1.5f ? FMath::FRandRange(160.0f, 260.0f) : FMath::FRandRange(70.0f, 120.0f);
		Flakes->Add(EVaelFlake::Snow, Start, Wind * 0.6f - FVector(0.0f, 0.0f, Fall), FMath::FRandRange(3.0f, 6.5f), GroundZ);
	}

	// The white haze of a blizzard rises and thins with it
	const float TargetWhiteout = FMath::Clamp(Strength - 1.0f, 0.0f, 1.6f) / 1.6f * Settings->BlizzardWhiteout;
	ShownWhiteout = FMath::FInterpTo(ShownWhiteout, TargetWhiteout, DeltaSeconds, 0.5f);
	Whiteout->SetRadialFogExtinction(ShownWhiteout);
	Whiteout->SetHeightFogExtinction(ShownWhiteout * 0.5f);
}
