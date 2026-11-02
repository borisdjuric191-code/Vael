// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/VaelRechargeGlow.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Magic/VaelSpellEffects.h"
#include "Player/VaelCharacter.h"

namespace
{
	/** Seconds until the glow is gone */
	constexpr float GlowLife = 1.5f;

	/** Health: rising dots and strokes, sizes against the 100 cm look sphere; a stroke is thin and tall */
	constexpr int32 HealthBitCount = 24;
	constexpr float HealthDotSize = 0.13f;
	constexpr float HealthStrokeSize = 0.08f;
	constexpr float HealthStrokeStretch = 5.0f;

	/** Mana: beads of the rising ring and sparks winding up in two strands */
	constexpr int32 RingBeadCount = 16;
	constexpr float RingBeadSize = 0.1f;
	constexpr float RingStartRadius = 60.0f;
	constexpr float RingEndRadius = 22.0f;
	constexpr float RingRise = 160.0f;
	constexpr float RingLife = 0.9f;
	constexpr int32 SparkCount = 14;
	constexpr float SparkSize = 0.12f;
	constexpr float SparkTurns = 1.5f;

	/** Glow of the bits and the light at its brightest: more turns the colour white */
	constexpr float BitGlow = 4.5f;
	constexpr float LightIntensity = 2500.0f;
	constexpr float LightRadius = 300.0f;

	const FLinearColor HealthGlowColor = FLinearColor(FColor(0x30, 0xf0, 0x48));
	const FLinearColor ManaGlowColor = FLinearColor(FColor(0x28, 0x6a, 0xff));

	/** Size over a bit's life: pops in quickly, holds, fades out over the last part */
	float BitFade(float Along)
	{
		return FMath::Min(FMath::Clamp(Along / 0.15f, 0.0f, 1.0f), FMath::Clamp((1.0f - Along) / 0.45f, 0.0f, 1.0f));
	}
}

AVaelRechargeGlow::AVaelRechargeGlow()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(RootComponent);
	Light->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));
	Light->SetCastShadows(false);
	Light->SetIntensity(0.0f);
	Light->SetAttenuationRadius(LightRadius);

	PrimaryActorTick.bCanEverTick = true;
}

AVaelRechargeGlow* AVaelRechargeGlow::Play(AVaelCharacter* InPlayer, EVaelOrbKind InKind)
{
	UWorld* World = InPlayer != nullptr ? InPlayer->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}

	const FTransform SpawnTransform(InPlayer->GetActorLocation() - FVector(0.0f, 0.0f, InPlayer->GetSimpleCollisionHalfHeight()));

	AVaelRechargeGlow* Glow = World->SpawnActorDeferred<AVaelRechargeGlow>(AVaelRechargeGlow::StaticClass(), SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Glow != nullptr)
	{
		Glow->Player = InPlayer;
		Glow->Kind = InKind;
		Glow->FinishSpawning(SpawnTransform);
	}

	return Glow;
}

void AVaelRechargeGlow::BeginPlay()
{
	Super::BeginPlay();

	Light->SetLightColor(Kind == EVaelOrbKind::Health ? HealthGlowColor : ManaGlowColor);
	BuildBits();
}

void AVaelRechargeGlow::BuildBits()
{
	const bool bHealth = Kind == EVaelOrbKind::Health;
	const FLinearColor Color = bHealth ? HealthGlowColor : ManaGlowColor;

	UMaterialInterface* CoreMaterial = nullptr;
	UMaterialInterface* GlowMaterial = nullptr;
	VaelEffects::LoadLookMaterials(CoreMaterial, GlowMaterial);

	const int32 Count = bHealth ? HealthBitCount : SparkCount;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FGlowBit Bit;
		if (bHealth)
		{
			// Every other bit is a stroke, spread evenly around the body with some jitter
			const bool bStroke = Index % 2 == 1;
			Bit.Delay = FMath::FRandRange(0.0f, 0.55f);
			Bit.Life = FMath::FRandRange(0.7f, 0.9f);
			Bit.Radius = FMath::FRandRange(18.0f, 42.0f);
			Bit.Angle = (Index + FMath::FRandRange(-0.3f, 0.3f)) * 2.0f * UE_PI / Count;
			Bit.StartHeight = FMath::FRandRange(10.0f, 80.0f);
			Bit.Rise = FMath::FRandRange(110.0f, 160.0f);
			Bit.Size = bStroke ? HealthStrokeSize : HealthDotSize;
			Bit.Stretch = bStroke ? HealthStrokeStretch : 1.0f;
		}
		else
		{
			// Two strands half a turn apart, each spark a little behind the one before
			Bit.Delay = (Index / 2) * 0.06f;
			Bit.Life = 0.8f;
			Bit.Angle = (Index % 2) * UE_PI;
			Bit.Radius = 45.0f;
			Bit.StartHeight = 5.0f;
			Bit.Rise = 185.0f;
			Bit.Size = SparkSize;
		}
		Bits.Add(Bit);

		UStaticMeshComponent* Shape = VaelEffects::AddLookShape(this, GlowMaterial, Color, BitGlow);
		Shape->SetVisibility(false);
		Shapes.Add(Shape);
	}

	if (!bHealth)
	{
		for (int32 Index = 0; Index < RingBeadCount; ++Index)
		{
			UStaticMeshComponent* Bead = VaelEffects::AddLookShape(this, GlowMaterial, Color, BitGlow);
			Bead->SetVisibility(true);
			RingBeads.Add(Bead);
		}
	}
}

void AVaelRechargeGlow::PlaceHealthBit(int32 Index, float Along) const
{
	const FGlowBit& Bit = Bits[Index];
	const float Height = Bit.StartHeight + Bit.Rise * FMath::InterpEaseOut(0.0f, 1.0f, Along, 2.0f);
	const float Fade = BitFade(Along);

	UStaticMeshComponent* Shape = Shapes[Index];
	Shape->SetRelativeLocation(FVector(FMath::Cos(Bit.Angle) * Bit.Radius, FMath::Sin(Bit.Angle) * Bit.Radius, Height));
	Shape->SetRelativeScale3D(FVector(Bit.Size, Bit.Size, Bit.Size * Bit.Stretch) * FMath::Max(Fade, 0.01f));
}

void AVaelRechargeGlow::PlaceManaBit(int32 Index, float Along) const
{
	const FGlowBit& Bit = Bits[Index];
	const float Climb = FMath::InterpEaseInOut(0.0f, 1.0f, Along, 1.6f);
	const float Angle = Bit.Angle + Climb * SparkTurns * 2.0f * UE_PI;
	const float Radius = FMath::Lerp(Bit.Radius, Bit.Radius * 0.45f, Climb);
	const float Fade = BitFade(Along);

	UStaticMeshComponent* Shape = Shapes[Index];
	Shape->SetRelativeLocation(FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, Bit.StartHeight + Bit.Rise * Climb));
	Shape->SetRelativeScale3D(FVector(Bit.Size * FMath::Max(Fade, 0.01f)));
}

void AVaelRechargeGlow::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Clock += DeltaSeconds;
	if (Clock >= GlowLife)
	{
		Destroy();
		return;
	}

	// Stays at the feet of the player while they walk on, without turning with them
	if (const AVaelCharacter* Target = Player.Get())
	{
		SetActorLocationAndRotation(Target->GetActorLocation() - FVector(0.0f, 0.0f, Target->GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator);
	}

	Light->SetIntensity(LightIntensity * BitFade(Clock / GlowLife));

	for (int32 Index = 0; Index < Bits.Num(); ++Index)
	{
		const float Along = (Clock - Bits[Index].Delay) / Bits[Index].Life;
		const bool bShown = Along > 0.0f && Along < 1.0f;
		Shapes[Index]->SetVisibility(bShown);
		if (!bShown)
		{
			continue;
		}

		if (Kind == EVaelOrbKind::Health)
		{
			PlaceHealthBit(Index, Along);
		}
		else
		{
			PlaceManaBit(Index, Along);
		}
	}

	// The mana ring rises from the feet and closes around the chest
	const float RingAlong = FMath::Clamp(Clock / RingLife, 0.0f, 1.0f);
	const float RingClimb = FMath::InterpEaseOut(0.0f, 1.0f, RingAlong, 2.0f);
	const float RingRadius = FMath::Lerp(RingStartRadius, RingEndRadius, RingClimb);
	const float RingFade = BitFade(RingAlong);
	for (int32 Index = 0; Index < RingBeads.Num(); ++Index)
	{
		const float Angle = Index * 2.0f * UE_PI / RingBeads.Num() + Clock * 2.0f;
		RingBeads[Index]->SetVisibility(RingAlong < 1.0f);
		RingBeads[Index]->SetRelativeLocation(FVector(FMath::Cos(Angle) * RingRadius, FMath::Sin(Angle) * RingRadius, 10.0f + RingRise * RingClimb));
		RingBeads[Index]->SetRelativeScale3D(FVector(RingBeadSize * FMath::Max(RingFade, 0.01f)));
	}
}
