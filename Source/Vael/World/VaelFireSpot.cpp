// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelFireSpot.h"
#include "Magic/VaelGroundArea.h"
#include "Magic/VaelMagicSettings.h"
#include "World/VaelRegion.h"
#include "World/VaelWorldSettings.h"
#include "Engine/World.h"
#include "Vael.h"
#include "Camera/PlayerCameraManager.h"
#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelCombatStatics.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "World/VaelSafeZone.h"
#include "World/VaelTimeOfDay.h"
#include "EngineUtils.h"

AVaelFireSpot::AVaelFireSpot()
{
	PrimaryActorTick.bCanEverTick = true;

	Fire = CreateDefaultSubobject<UVaelFireComponent>(TEXT("Fire"));
	RootComponent = Fire;

	// A campfire: a broad bed of flames, sparks and light, burning calmly
	Shape.Radius = 45.0f;
	Shape.Height = 110.0f;
	Shape.bScorch = false;
	Shape.bCampfire = true;
}

AVaelFireSpot* AVaelFireSpot::Spawn(UWorld* World, const FVector& Location, const FVaelFireShape& InShape, float InBurnTime)
{
	if (World == nullptr)
	{
		return nullptr;
	}
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Parameters.bDeferConstruction = true;
	AVaelFireSpot* Spot = World->SpawnActor<AVaelFireSpot>(AVaelFireSpot::StaticClass(), FTransform(Location), Parameters);
	if (Spot != nullptr)
	{
		Spot->Shape = InShape;
		Spot->BurnTime = InBurnTime;
		Spot->FinishSpawning(FTransform(Location));
	}
	return Spot;
}

AVaelFireSpot* AVaelFireSpot::SpawnBurst(UWorld* World, const FVector& Location, float Radius)
{
	FVaelFireShape Shape;
	Shape.Radius = Radius * 0.55f;
	Shape.Height = Radius * 1.3f;
	Shape.Heat = 1.5f;
	Shape.Smoke = 0.9f;
	Shape.Speed = 2.4f;
	Shape.Tongues = 18;
	Shape.bFloating = true;
	Shape.bBurst = true;
	AVaelFireSpot* Spot = Spawn(World, Location, Shape, 0.6f);
	if (Spot != nullptr)
	{
		Spot->bBurst = true;
		Spot->Fire->SetSizeScale(0.3f);
		UE_LOG(LogVael, Verbose, TEXT("Fire burst of %.0f cm at %s, strength %.2f"), Radius, *Location.ToCompactString(), Spot->Fire->GetStrength());
	}
	return Spot;
}

void AVaelFireSpot::BeginPlay()
{
	Super::BeginPlay();
	BaseRadius = Shape.Radius;
	TargetRadius = Shape.Radius;
	TargetLocation = GetActorLocation();
	Fire->Ignite(Shape, 0.35f);

	// A placed campfire can be rested at
	if (BurnTime <= 0.0f && Shape.WallNormal.IsNearlyZero())
	{
		UVaelInteractionSubsystem::Register(this);
	}
}

void AVaelFireSpot::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UVaelInteractionSubsystem::Unregister(this);
	Super::EndPlay(EndPlayReason);
}

bool AVaelFireSpot::CanInteract(const AVaelCharacter* Player) const
{
	// Only in a safe camp, only once the day is ending
	const UVaelTimeOfDay* Clock = UVaelTimeOfDay::Get(this);
	const bool bEvening = Clock != nullptr && (Clock->GetPhase() == EVaelDayPhase::Dusk || Clock->GetPhase() == EVaelDayPhase::Night);
	return BurnTime <= 0.0f && Users == 0 && CanGiveFire() && bEvening && AVaelSafeZone::IsSafe(GetWorld(), GetActorLocation(), 300.0f);
}

FText AVaelFireSpot::GetInteractPrompt() const
{
	return NSLOCTEXT("VaelWorld", "RestAtFire", "Bis zum Morgen rasten");
}

void AVaelFireSpot::Interact(AVaelCharacter* Player)
{
	if (!CanInteract(Player))
	{
		return;
	}

	// The picture fades to black, the night passes, everybody wakes rested at dawn
	UWorld* World = GetWorld();
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (It->Get() != nullptr && It->Get()->PlayerCameraManager != nullptr)
		{
			It->Get()->PlayerCameraManager->StartCameraFade(0.0f, 1.0f, 1.0f, FLinearColor::Black, false, true);
		}
	}

	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(World, [World]()
	{
		if (UVaelTimeOfDay* Clock = UVaelTimeOfDay::Get(World))
		{
			Clock->SkipToDawn();
		}
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PlayerController = It->Get();
			if (AVaelCharacterBase* Rested = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacterBase>() : nullptr)
			{
				UVaelCombatStatics::Heal(Rested, Rested->GetMaxHealth());
			}
			if (PlayerController != nullptr && PlayerController->PlayerCameraManager != nullptr)
			{
				PlayerController->PlayerCameraManager->StartCameraFade(1.0f, 0.0f, 1.5f, FLinearColor::Black, false, false);
			}
		}
	}), 1.3f, false);

	UE_LOG(LogVael, Log, TEXT("The group rests at the fire until the morning"));
}

void AVaelFireSpot::Feed(const FVector& Location, float Radius)
{
	if (bBurst || bPutOut)
	{
		return;
	}
	// It grows by part of the area of the fire it takes in, and its middle moves a little towards it
	const UVaelFireSettings* Settings = UVaelFireSettings::Get();
	const float Before = FMath::Square(TargetRadius);
	const float Added = Settings->FeedShare * FMath::Square(Radius);
	TargetRadius = FMath::Min(Settings->MaxHerdRadius, FMath::Sqrt(Before + Added));
	// A placed fire (a campfire) and a fire on a wall stay where they are
	const FVector Towards(Location.X, Location.Y, TargetLocation.Z);
	if (BurnTime > 0.0f && Shape.WallNormal.IsNearlyZero())
	{
		TargetLocation = FMath::Lerp(TargetLocation, Towards, 0.4f * Added / FMath::Max(Before + Added, 1.0f));
	}
	SinceFed = 0.0f;
	Fire->Feed();
}

void AVaelFireSpot::GiveFire(const FVector& Towards)
{
	if (!CanGiveFire())
	{
		return;
	}

	const UVaelMagicSettings* MagicSettings = UVaelMagicSettings::Get();
	TargetRadius *= 1.0f - MagicSettings->FireDrawShare;
	SinceFed = 0.0f;

	// The flames are pulled towards the mage while their tongues tear off
	Fire->PullTowards(Towards - GetActorLocation(), MagicSettings->DrawFlightTime * 1.6f);

	// A campfire keeps a small fire, a Brandherd drawn too low dies
	if (BurnTime <= 0.0f)
	{
		TargetRadius = FMath::Max(TargetRadius, BaseRadius * MagicSettings->FireDrawPlacedFloor);
	}
	else if (TargetRadius < BaseRadius * MagicSettings->FireDrawPutOutShare)
	{
		PutOut(1.0f);
	}
}

void AVaelFireSpot::PutOut(float FadeOut)
{
	if (!bPutOut)
	{
		bPutOut = true;
		Fire->Extinguish(FadeOut, false);
	}
}

void AVaelFireSpot::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;
	SinceFed += DeltaSeconds;

	// A blast blooms out fast and keeps growing a little while it dies
	if (bBurst)
	{
		Fire->SetSizeScale(0.3f + 1.1f * FMath::Sqrt(FMath::Clamp(Age / 0.6f, 0.0f, 1.0f)));
	}
	else if (!bPutOut)
	{
		// A Brandherd grows towards what it was fed; left alone it shrinks slowly and burns down calmly. A placed fire
		// shrinks back to its own size, a spawned one further, before it goes out
		const UVaelFireSettings* Settings = UVaelFireSettings::Get();
		if (SinceFed > Settings->HerdHoldSeconds)
		{
			const float Smallest = BaseRadius * (BurnTime > 0.0f ? Settings->HerdShrinkTo : 1.0f);
			if (TargetRadius > Smallest)
			{
				TargetRadius = FMath::Max(Smallest, TargetRadius - Settings->HerdShrinkSpeed * DeltaSeconds);
			}
			else if (BurnTime <= 0.0f)
			{
				// A campfire mages drew from grows back to its own size
				TargetRadius = FMath::Min(BaseRadius, TargetRadius + UVaelMagicSettings::Get()->FireDrawRegrowSpeed * DeltaSeconds);
			}
		}
		// A Brandherd creeps downwind; campfires and fires on walls stay
		if (BurnTime > 0.0f && Shape.WallNormal.IsNearlyZero())
		{
			TargetLocation += AVaelRegion::GetWindAt(GetWorld(), TargetLocation) * UVaelWorldSettings::Get()->WindFireDrift * DeltaSeconds;
		}
		const float Radius = FMath::FInterpConstantTo(Fire->GetShape().Radius, TargetRadius, DeltaSeconds, Settings->HerdGrowSpeed);
		Fire->SetRadius(Radius);
		if (!GetActorLocation().Equals(TargetLocation, 1.0f))
		{
			SetActorLocation(FMath::VInterpConstantTo(GetActorLocation(), TargetLocation, DeltaSeconds, Settings->HerdGrowSpeed * 0.5f));
		}
	}
	if (BurnTime > 0.0f && Age >= BurnTime && Users == 0)
	{
		PutOut(2.5f);
	}

	// Gone once the flames are out and the scorch mark has cooled
	if (bPutOut && !Fire->IsBurning() && Age > BurnTime + UVaelFireSettings::Get()->ScorchLinger + 1.0f)
	{
		Destroy();
	}
}

AVaelFireSpot* AVaelFireSpot::SpawnOrJoin(UWorld* World, const FVector& Location, const FVaelFireShape& InShape, float InBurnTime, bool bUser)
{
	if (World == nullptr)
	{
		return nullptr;
	}

	// Fire in mist boils the mist into steam instead of burning (a fire area handing over its look is already past that)
	if (!bUser && AVaelGroundArea::QuenchInMist(World, Location))
	{
		return nullptr;
	}

	// A fire close by of the same kind (ground or wall) takes it over
	const bool bWall = !InShape.WallNormal.IsNearlyZero();
	for (TActorIterator<AVaelFireSpot> It(World); It; ++It)
	{
		AVaelFireSpot* Spot = *It;
		if (Spot->bBurst || Spot->bPutOut || Spot->Shape.WallNormal.IsNearlyZero() == bWall
				|| (bWall ? FVector::Dist(Spot->GetActorLocation(), Location) : FVector::Dist2D(Spot->GetActorLocation(), Location))
				> Spot->Fire->GetShape().Radius + InShape.Radius * 0.6f)
		{
			continue;
		}
		if (Spot->BurnTime > 0.0f)
		{
			Spot->BurnTime = InBurnTime > 0.0f ? FMath::Max(Spot->BurnTime, Spot->Age + InBurnTime) : 0.0f;
		}
		Spot->Feed(Location, InShape.Radius);
		if (bUser)
		{
			++Spot->Users;
		}
		return Spot;
	}

	AVaelFireSpot* Spot = Spawn(World, Location, InShape, InBurnTime);
	if (Spot != nullptr && bUser)
	{
		Spot->Users = 1;
	}
	return Spot;
}

void AVaelFireSpot::ReleaseUser()
{
	Users = FMath::Max(0, Users - 1);
	if (Users == 0 && (BurnTime <= 0.0f || Age >= BurnTime))
	{
		PutOut(2.0f);
	}
}
