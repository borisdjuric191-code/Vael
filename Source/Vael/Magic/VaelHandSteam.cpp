// Copyright Epic Games, Inc. All Rights Reserved.

#include "Magic/VaelHandSteam.h"
#include "GameFramework/Actor.h"
#include "Magic/VaelCloudCards.h"
#include "Magic/VaelMagicSettings.h"
#include "Player/VaelCharacter.h"
#include "VaelAssets.h"

namespace
{
	/** Puffs, how long one lives, how fast it rises and drifts in cm/s, its size in cm at birth and at the end, and how dense it starts */
	constexpr int32 SteamPuffs = 9;
	constexpr float SteamPuffLife = 0.85f;
	constexpr float SteamRise = 62.0f;
	constexpr float SteamDrift = 16.0f;
	constexpr float SteamBirthSize = 10.0f;
	constexpr float SteamEndSize = 46.0f;
	constexpr float SteamSolid = 0.5f;
}

UVaelHandSteamComponent::UVaelHandSteamComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// The steam stays in the air where it rose, the mage does not carry it along
	SetUsingAbsoluteLocation(true);
	SetUsingAbsoluteRotation(true);
	SetUsingAbsoluteScale(true);
}

bool UVaelHandSteamComponent::Start(float InSeconds)
{
	UMaterialInterface* Material = VaelAssets::LoadOptional(UVaelMagicSettings::Get()->CloudCardMaterial);
	if (Material == nullptr)
	{
		return false;
	}

	Seconds = FMath::Max(InSeconds, 0.1f);

	Cards = NewObject<UVaelCloudCardsComponent>(GetOwner());
	Cards->SetupAttachment(this);
	Cards->RegisterComponent();
	Cards->Init(Material);
	for (int32 Puff = 0; Puff < SteamPuffs; ++Puff)
	{
		Cards->AddCard(FVector::ZeroVector, SteamBirthSize, 0.0f);
	}

	SetWorldLocation(GetOwner()->GetActorLocation());
	Births.Init(FVector::ZeroVector, SteamPuffs);
	SetComponentTickEnabled(true);
	return true;
}

void UVaelHandSteamComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	Age += DeltaTime;
	if (Age >= Seconds + SteamPuffLife)
	{
		Cards->DestroyComponent();
		DestroyComponent();
		return;
	}

	const AVaelCharacter* Mage = Cast<AVaelCharacter>(GetOwner());
	FVector Palm = GetOwner()->GetActorLocation();
	if (Mage != nullptr)
	{
		Mage->GetGesturePalm(Palm);
	}

	const FVector Middle = GetComponentLocation();
	for (int32 Puff = 0; Puff < SteamPuffs; ++Puff)
	{
		// A puff is born where the hand is at its turn and then rises by itself
		const float Birth = Puff * Seconds / SteamPuffs;
		const float Life = (Age - Birth) / SteamPuffLife;
		if (Life < 0.0f || Life > 1.0f)
		{
			Cards->SetCard(Puff, FVector::ZeroVector, 1.0f, 0.0f);
			continue;
		}
		if (Births[Puff].IsZero())
		{
			Births[Puff] = Palm;
		}

		const float Sway = FMath::Sin(Life * 5.0f + Puff * 2.1f);
		const FVector Place = Births[Puff] - Middle + FVector(Sway * SteamDrift, FMath::Cos(Puff * 1.3f) * SteamDrift, SteamRise) * Life * SteamPuffLife;
		const float Fade = FMath::Min(Life * 6.0f, 1.0f) * (1.0f - Life);
		Cards->SetCard(Puff, Place, FMath::Lerp(SteamBirthSize, SteamEndSize, Life), SteamSolid * Fade);
	}
	Cards->Commit();
}
