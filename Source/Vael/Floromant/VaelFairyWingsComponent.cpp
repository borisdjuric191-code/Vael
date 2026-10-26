// Copyright Epic Games, Inc. All Rights Reserved.

#include "Floromant/VaelFairyWingsComponent.h"
#include "Floromant/VaelFloromantSettings.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "VaelAssets.h"

namespace
{
	/** Grid of each wing sheet: points across and down */
	constexpr int32 WingColumns = 6;
	constexpr int32 WingRows = 8;

	/** Height of the root of the wing in the picture, as share from the top */
	constexpr float WingRootV = 0.43f;

	/** How far the tip of the wing curves back, in cm */
	constexpr float WingCurve = 10.0f;

	/** Degrees the wings stand back from the shoulders at rest, and how far each beat swings them */
	constexpr float WingRestAngle = 18.0f;
	constexpr float WingBeatAngle = 14.0f;

	/** Speed above which the character counts as walking, in cm/s */
	constexpr float WingWalkSpeed = 60.0f;

	/** Material parameters of M_Floromant_Fluegel */
	const FName WingGlowParameter(TEXT("Glow"));
	const FName WingGlowColorParameter(TEXT("GlowColor"));
	const FName WingTintParameter(TEXT("Tint"));
	const FName WingLightThroughParameter(TEXT("LightThrough"));
}

UVaelFairyWingsComponent::UVaelFairyWingsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UVaelFairyWingsComponent::SetWingsShown(bool bInShown)
{
	bShown = bInShown;
	if (bShown && Sheets.IsEmpty())
	{
		BuildWings();
	}

	SetVisibility(bShown, true);
	SetComponentTickEnabled(bShown);
}

void UVaelFairyWingsComponent::SetGlow(const FLinearColor& Color, float Strength)
{
	if (WingMaterial != nullptr)
	{
		WingMaterial->SetVectorParameterValue(WingGlowColorParameter, Color);
		WingMaterial->SetScalarParameterValue(WingGlowParameter, Strength);
	}
}

void UVaelFairyWingsComponent::SetFlight(float InSpeedShare, float InBrake, float InWetness, float InWind)
{
	SpeedShare = InSpeedShare;
	Brake = InBrake;
	Wetness = InWetness;
	Wind = InWind;

	// Soaked, the thin membrane darkens and lets less light through
	if (WingMaterial != nullptr && !FMath::IsNearlyEqual(Wetness, ShownWetness, 0.02f))
	{
		ShownWetness = Wetness;
		const float Shade = FMath::Lerp(1.0f, 0.42f, Wetness);
		WingMaterial->SetVectorParameterValue(WingTintParameter, FLinearColor(Shade, Shade, Shade * 1.05f, 1.0f));
		WingMaterial->SetScalarParameterValue(WingLightThroughParameter, FMath::Lerp(0.8f, 0.35f, Wetness));
	}
}

void UVaelFairyWingsComponent::BuildWings()
{
	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	const FVector2D Size = Settings->WingSize;

	if (UMaterialInterface* Material = VaelAssets::LoadOptional(Settings->WingMaterial))
	{
		WingMaterial = UMaterialInstanceDynamic::Create(Material, this);
	}

	// One sheet in the space of its hinge: it reaches out along Y, up and down along Z, and curves back along -X towards the tip
	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<int32> Triangles;

	for (int32 Row = 0; Row <= WingRows; ++Row)
	{
		const float V = Row / static_cast<float>(WingRows);
		for (int32 Column = 0; Column <= WingColumns; ++Column)
		{
			const float U = Column / static_cast<float>(WingColumns);
			Vertices.Add(FVector(-WingCurve * U * U, U * Size.X, (WingRootV - V) * Size.Y));
			Normals.Add(FVector(-1.0f, 0.0f, 0.0f));
			UVs.Add(FVector2D(U, V));
		}
	}

	for (int32 Row = 0; Row < WingRows; ++Row)
	{
		for (int32 Column = 0; Column < WingColumns; ++Column)
		{
			const int32 A = Row * (WingColumns + 1) + Column;
			const int32 B = A + 1;
			const int32 C = A + WingColumns + 1;
			const int32 D = C + 1;
			Triangles.Append({ A, B, C, B, D, C });
		}
	}

	// Left and right: the left wing is the right one mirrored
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const float Mirror = Side == 0 ? -1.0f : 1.0f;

		USceneComponent* Hinge = NewObject<USceneComponent>(GetOwner());
		Hinge->SetupAttachment(this);
		Hinge->SetRelativeLocation(FVector(Settings->WingRoot.X, Settings->WingRoot.Y * Mirror, Settings->WingRoot.Z));
		Hinge->RegisterComponent();
		Hinges.Add(Hinge);

		UProceduralMeshComponent* Sheet = NewObject<UProceduralMeshComponent>(GetOwner());
		Sheet->SetupAttachment(Hinge);
		Sheet->SetRelativeScale3D(FVector(1.0f, Mirror, 1.0f));
		Sheet->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Sheet->SetCastShadow(true);
		Sheet->bReceivesDecals = false;
		Sheet->RegisterComponent();
		Sheet->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
		if (WingMaterial != nullptr)
		{
			Sheet->SetMaterial(0, WingMaterial);
		}
		Sheets.Add(Sheet);
	}
}

void UVaelFairyWingsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const UVaelFloromantSettings* Settings = UVaelFloromantSettings::Get();
	const AActor* Owner = GetOwner();
	const bool bMoving = Owner != nullptr && Owner->GetVelocity().Size2D() > WingWalkSpeed;
	BurstTimeLeft = FMath::Max(0.0f, BurstTimeLeft - DeltaTime);
	const float Burst = BurstTimeLeft > 0.0f ? 1.0f : 0.0f;
	// Wet wings are heavy: they beat faster to keep her up
	const float BeatRate = (bMoving ? FMath::Lerp(Settings->WingBeatIdle, Settings->WingBeatMoving, FMath::Max(SpeedShare, 0.5f)) : Settings->WingBeatIdle)
		* (1.0f + 3.0f * Burst) * (1.0f + 0.8f * Brake) * (1.0f + 0.35f * Wetness);
	BeatPhase += DeltaTime * 2.0f * UE_PI * BeatRate;

	// Steady fast flight: the wings stop and hold open, and every few seconds two beats keep her up; braking or darting beats again.
	// Soaked wings cannot glide
	const bool bSteady = SpeedShare > 0.75f && Brake < 0.2f && Burst <= 0.0f && Wetness < 0.5f;
	SteadyTime = bSteady ? SteadyTime + DeltaTime : 0.0f;
	PumpTimeLeft = FMath::Max(0.0f, PumpTimeLeft - DeltaTime);
	NextPumpTime -= DeltaTime;
	if (SteadyTime > Settings->GlideDelay && NextPumpTime <= 0.0f)
	{
		PumpTimeLeft = 2.0f / FMath::Max(BeatRate, 0.1f);
		NextPumpTime = PumpTimeLeft + FMath::FRandRange(Settings->GlidePumpInterval.X, Settings->GlidePumpInterval.Y);
	}
	const bool bGliding = SteadyTime > Settings->GlideDelay && PumpTimeLeft <= 0.0f;
	Glide = FMath::FInterpTo(Glide, bGliding ? 1.0f : 0.0f, DeltaTime, bGliding ? 3.0f : 7.0f);

	// The wings open and fold back around their hinges; the beat is quick on the way back and lingers when open, like a moth
	const float Beat = FMath::Sin(BeatPhase);
	// Wet they hang lower and beat shorter; in the wind they shake
	const float Time = GetWorld()->GetTimeSeconds();
	const float Strength = (1.0f + 0.8f * Burst) * (1.0f + 0.6f * Brake) * (1.0f - 0.3f * Wetness);
	const float WindShake = Wind * (2.5f * FMath::Sin(Time * 17.0f) + 1.5f * FMath::Sin(Time * 29.0f + 1.3f));
	const float BeatSwing = WingRestAngle * (1.0f - 0.35f * Wetness) + WingBeatAngle * Strength * (Beat > 0.0f ? Beat : Beat * 0.6f) + WindShake;
	const float BeatFlutter = 3.0f * FMath::Sin(BeatPhase * 2.3f) - 6.0f * Wetness + WindShake * 0.6f;

	// Gliding they are spread wide and tilted up a little, only trembling in the wind of the flight
	const float GlideSwing = WingRestAngle + WingBeatAngle * 0.9f + 1.2f * FMath::Sin(Time * 11.0f) + WindShake;
	const float GlideFlutter = 5.0f + 1.0f * FMath::Sin(Time * 7.3f) + WindShake * 0.6f;
	const float Swing = FMath::Lerp(BeatSwing, GlideSwing, Glide);
	const float Flutter = FMath::Lerp(BeatFlutter, GlideFlutter, Glide);

	for (int32 Side = 0; Side < Hinges.Num(); ++Side)
	{
		const float Mirror = Side == 0 ? -1.0f : 1.0f;
		Hinges[Side]->SetRelativeRotation(FRotator(0.0f, Swing * Mirror, Flutter * Mirror));
	}
}
