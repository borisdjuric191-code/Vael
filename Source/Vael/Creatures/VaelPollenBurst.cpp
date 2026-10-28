// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/VaelPollenBurst.h"
#include "Engine/World.h"
#include "Magic/VaelCloudCards.h"
#include "Magic/VaelMagicSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "VaelAssets.h"

namespace
{
	/** Puffs of the main colour and of the accent colour */
	constexpr int32 MainPuffs = 16;
	constexpr int32 AccentPuffs = 6;

	/** Share of the lifetime the cloud takes to swell out; it fades in the rest */
	constexpr float SwellShare = 0.3f;
}

AVaelPollenBurst::AVaelPollenBurst()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Main = CreateDefaultSubobject<UVaelCloudCardsComponent>(TEXT("Main"));
	Main->SetupAttachment(RootComponent);
	Accent = CreateDefaultSubobject<UVaelCloudCardsComponent>(TEXT("Accent"));
	Accent->SetupAttachment(RootComponent);
}

AVaelPollenBurst* AVaelPollenBurst::Spawn(UWorld* World, const FVector& Location, float InRadius, const FLinearColor& Color, const FLinearColor& AccentColor, float InLifetime)
{
	if (World == nullptr)
	{
		return nullptr;
	}
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVaelPollenBurst* Burst = World->SpawnActor<AVaelPollenBurst>(Location, FRotator::ZeroRotator, Parameters);
	if (Burst != nullptr)
	{
		Burst->Setup(InRadius, Color, AccentColor, InLifetime);
	}
	return Burst;
}

void AVaelPollenBurst::Setup(float InRadius, const FLinearColor& Color, const FLinearColor& AccentColor, float InLifetime)
{
	Radius = InRadius;
	Lifetime = InLifetime;
	UMaterialInterface* CloudMaterial = VaelAssets::LoadOptional(UVaelMagicSettings::Get()->CloudCardMaterial);
	if (CloudMaterial == nullptr)
	{
		Destroy();
		return;
	}
	for (const auto& [Cards, Tint] : { TPair<UVaelCloudCardsComponent*, FLinearColor>(Main, Color), TPair<UVaelCloudCardsComponent*, FLinearColor>(Accent, AccentColor) })
	{
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(CloudMaterial, this);
		Material->SetVectorParameterValue(TEXT("Farbe"), Tint);
		Cards->Init(Material);
	}

	// Mostly outwards over the ground and upwards, as if blown out of the thistle
	for (int32 Index = 0; Index < MainPuffs + AccentPuffs; ++Index)
	{
		FPuff Puff;
		Puff.bAccent = Index >= MainPuffs;
		const float Yaw = FMath::FRandRange(0.0f, UE_TWO_PI);
		const float Up = FMath::FRandRange(0.15f, Puff.bAccent ? 0.6f : 1.1f);
		Puff.Direction = FVector(FMath::Cos(Yaw), FMath::Sin(Yaw), Up).GetSafeNormal();
		Puff.Reach = Radius * FMath::FRandRange(0.35f, 1.0f);
		Puff.Size = Radius * FMath::FRandRange(0.45f, 0.8f) * (Puff.bAccent ? 0.7f : 1.0f);
		Puff.Delay = FMath::FRandRange(0.0f, 0.12f);
		Puff.Card = (Puff.bAccent ? Accent : Main)->AddCard(FVector::ZeroVector, 1.0f, 0.0f);
		Puffs.Add(Puff);
	}
	Main->Commit();
	Accent->Commit();
}

void AVaelPollenBurst::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;
	if (Age >= Lifetime)
	{
		Destroy();
		return;
	}
	for (const FPuff& Puff : Puffs)
	{
		const float Life = FMath::Clamp((Age - Puff.Delay) / FMath::Max(Lifetime - Puff.Delay, 0.01f), 0.0f, 1.0f);
		// Swells out fast and eases, then drifts a little further and up while it fades
		const float Out = 1.0f - FMath::Pow(1.0f - FMath::Clamp(Life / SwellShare, 0.0f, 1.0f), 3.0f);
		const FVector Place = Puff.Direction * Puff.Reach * (Out + 0.25f * Life) + FVector(0.0f, 0.0f, 40.0f * Life);
		const float Size = Puff.Size * (0.35f + 0.65f * Out + 0.3f * Life);
		const float Solid = Life <= 0.0f ? 0.0f : FMath::Clamp(Life / 0.08f, 0.0f, 1.0f) * (1.0f - FMath::SmoothStep(SwellShare, 1.0f, Life)) * 0.9f;
		(Puff.bAccent ? Accent : Main)->SetCard(Puff.Card, Place, Size, Solid);
	}
	Main->Commit();
	Accent->Commit();
}
