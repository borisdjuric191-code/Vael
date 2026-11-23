// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelCave.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Components/SphereComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Player/VaelCharacter.h"
#include "Player/VaelPlayerController.h"
#include "Save/VaelSaveSubsystem.h"
#include "UI/VaelNoticeSubsystem.h"
#include "Vael.h"
#include "World/VaelTravel.h"

namespace
{
	/** Seconds after a level starts in which no mouth takes anyone (while the group is still being put down); whoever stands in
	 *  the mouth when it is over is taken then */
	constexpr float MouthRest = 1.0f;
}

AVaelCaveEntrance::AVaelCaveEntrance()
{
	Mouth = CreateDefaultSubobject<USphereComponent>(TEXT("Mouth"));
	Mouth->SetSphereRadius(220.0f);
	Mouth->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Mouth->SetGenerateOverlapEvents(true);
	RootComponent = Mouth;
}

void AVaelCaveEntrance::BeginPlay()
{
	Super::BeginPlay();
	Mouth->SetSphereRadius(MouthRadius);
	Mouth->OnComponentBeginOverlap.AddDynamic(this, &AVaelCaveEntrance::OnEnter);
	if (HintRadius > MouthRadius && !Hint.IsEmpty())
	{
		HintArea = NewObject<USphereComponent>(this);
		HintArea->SetupAttachment(RootComponent);
		HintArea->SetSphereRadius(HintRadius);
		HintArea->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
		HintArea->SetGenerateOverlapEvents(true);
		HintArea->RegisterComponent();
		HintArea->OnComponentBeginOverlap.AddDynamic(this, &AVaelCaveEntrance::OnNear);
	}
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		TArray<AActor*> Inside;
		Mouth->GetOverlappingActors(Inside, AVaelCharacter::StaticClass());
		for (AActor* Each : Inside)
		{
			TryTake(Cast<AVaelCharacter>(Each));
		}
	}), MouthRest + 0.1f, false);
}

void AVaelCaveEntrance::OnEnter(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	TryTake(Cast<AVaelCharacter>(OtherActor));
}

void AVaelCaveEntrance::OnNear(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	const AVaelCharacter* Player = Cast<AVaelCharacter>(OtherActor);
	const float Now = GetWorld()->GetTimeSeconds();
	if (Player == nullptr || !Player->IsPlayerControlled() || Now - LastHint < 8.0f)
	{
		return;
	}
	LastHint = Now;
	UVaelNoticeSubsystem::Post(this, Hint, Message, FLinearColor(FColor(232, 214, 186)), 3.0f);
}

void AVaelCaveEntrance::TryTake(AVaelCharacter* Player)
{
	UVaelCaveTravel* Travel = UVaelCaveTravel::Get(this);
	if (Player == nullptr || Player->IsDowned() || !Player->IsPlayerControlled() || Travel == nullptr || Travel->IsTravelling()
		|| (TargetLevel.IsNone() && !bInLevel) || GetWorld()->GetTimeSeconds() < MouthRest)
	{
		return;
	}
	if (bInLevel)
	{
		Travel->Hop(this, Destination, Message);
		return;
	}
	Travel->Travel(this, TargetLevel, ReturnPlace, Message);
}

// ---------------------------------------------------------------- travel

UVaelCaveTravel* UVaelCaveTravel::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	return GameInstance != nullptr ? GameInstance->GetSubsystem<UVaelCaveTravel>() : nullptr;
}

void UVaelCaveTravel::Travel(const UObject* WorldContext, FName Level, const FVector2D& Place, const FText& Message)
{
	UWorld* World = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
	if (World == nullptr || bTravelling)
	{
		return;
	}
	// What everyone carries goes along: the next level reads it back when its players are set up
	if (UVaelSaveSubsystem* Save = UVaelSaveSubsystem::Get(WorldContext))
	{
		Save->CaptureForTravel();
	}
	PendingLevel = Level;
	PendingPlace = Place;
	PendingMessage = Message;
	Landed.Reset();
	bTravelling = true;
	UE_LOG(LogVael, Log, TEXT("Cave travel: %s -> %s, landing at %s"), *UWorld::RemovePIEPrefix(World->GetMapName()), *Level.ToString(),
		Place.IsZero() ? TEXT("the PlayerStart") : *Place.ToString());
	UGameplayStatics::OpenLevel(World, Level);
}

void UVaelCaveTravel::Hop(const UObject* WorldContext, const FVector& Place, const FText& Message)
{
	UWorld* World = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
	if (World == nullptr || bHopping)
	{
		return;
	}
	// A short rest so the mouth at the other end doesn't take the group straight back
	bHopping = true;
	FTimerHandle Rest;
	World->GetTimerManager().SetTimer(Rest, FTimerDelegate::CreateWeakLambda(this, [this]() { bHopping = false; }), 2.5f, false);
	UE_LOG(LogVael, Log, TEXT("Cave hop in %s to %s"), *UWorld::RemovePIEPrefix(World->GetMapName()), *Place.ToCompactString());
	int32 Slot = 0;
	for (TActorIterator<AVaelCharacter> It(World); It; ++It)
	{
		if (!It->IsPlayerControlled())
		{
			continue;
		}
		const float Angle = Slot++ * HALF_PI + 0.4f;
		VaelTravel::Land(*It, FVector2D(Place) + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * 180.0f, Place.Z + 220.0f, Place.Z - 150.0f);
	}
	if (!Message.IsEmpty())
	{
		UVaelNoticeSubsystem::Post(WorldContext, Message, FText::GetEmpty(), FLinearColor(FColor(232, 214, 186)), 3.5f);
	}
}

void UVaelCaveTravel::OnPlayerReady(AVaelCharacter* Player)
{
	if (Player == nullptr || PendingLevel.IsNone() || UWorld::RemovePIEPrefix(Player->GetWorld()->GetMapName()) != PendingLevel.ToString())
	{
		return;
	}
	bTravelling = false;
	const AVaelPlayerController* Controller = Player->GetController<AVaelPlayerController>();
	const int32 Slot = Controller != nullptr ? Controller->GetPlayerSlot() : 0;
	if (Landed.Contains(Slot))
	{
		return;
	}
	Landed.Add(Slot);
	if (!PendingPlace.IsZero())
	{
		// side by side in front of the mouth
		const float Angle = Slot * HALF_PI + 0.4f;
		VaelTravel::Land(Player, PendingPlace + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * 180.0f);
	}
	if (!PendingMessage.IsEmpty() && Slot == 0)
	{
		UVaelNoticeSubsystem::Post(Player, PendingMessage, FText::GetEmpty(), FLinearColor(FColor(232, 214, 186)), 3.5f);
	}
}
