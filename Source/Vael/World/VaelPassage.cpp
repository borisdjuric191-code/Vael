// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelPassage.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "Player/VaelCharacter.h"
#include "UI/VaelNoticeSubsystem.h"
#include "World/VaelTravel.h"

namespace
{
	/** Seconds after going through in which no passage takes the same player again (no ping-pong between the mouths) */
	constexpr float PassageRest = 3.0f;
}

AVaelPassage::AVaelPassage()
{
	Mouth = CreateDefaultSubobject<USphereComponent>(TEXT("Mouth"));
	Mouth->SetSphereRadius(260.0f);
	Mouth->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Mouth->SetGenerateOverlapEvents(true);
	RootComponent = Mouth;
}

void AVaelPassage::BeginPlay()
{
	Super::BeginPlay();
	Mouth->OnComponentBeginOverlap.AddDynamic(this, &AVaelPassage::OnEnter);
}

void AVaelPassage::OnEnter(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	AVaelCharacter* Player = Cast<AVaelCharacter>(OtherActor);
	if (Player == nullptr || Player->IsDowned() || !Player->IsPlayerControlled())
	{
		return;
	}

	// The same player may not bounce straight back through the mouth it came out of
	static TMap<TWeakObjectPtr<AActor>, double> LastPassage;
	const double Now = GetWorld()->GetTimeSeconds();
	if (const double* Last = LastPassage.Find(Player); Last != nullptr && Now - *Last < PassageRest)
	{
		return;
	}
	LastPassage.Add(Player, Now);

	VaelTravel::Land(Player, Destination);
	if (!Message.IsEmpty() && Player->IsLocallyControlled())
	{
		UVaelNoticeSubsystem::Post(this, Message, FText::GetEmpty(), FLinearColor(FColor(232, 214, 186)), 3.0f);
	}
}
