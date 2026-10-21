// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelLurkingEyes.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** The engine sphere is 100 cm across */
	constexpr float SphereMeshSize = 100.0f;

	/** Seconds one blink takes */
	constexpr float BlinkDuration = 0.14f;
}

AVaelLurkingEyes::AVaelLurkingEyes()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	for (TObjectPtr<UStaticMeshComponent>* Eye : { &LeftEye, &RightEye })
	{
		*Eye = CreateDefaultSubobject<UStaticMeshComponent>(Eye == &LeftEye ? TEXT("LeftEye") : TEXT("RightEye"));
		(*Eye)->SetupAttachment(Root);
		(*Eye)->SetStaticMesh(Sphere.Object);
		(*Eye)->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		(*Eye)->SetCastShadow(false);
		(*Eye)->SetGenerateOverlapEvents(false);
	}
}

void AVaelLurkingEyes::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (EyeMaterial != nullptr)
	{
		LeftEye->SetMaterial(0, EyeMaterial);
		RightEye->SetMaterial(0, EyeMaterial);
	}
	ApplyLook(1.0f, 0.0f);
}

void AVaelLurkingEyes::BeginPlay()
{
	Super::BeginPlay();

	for (UStaticMeshComponent* Eye : { LeftEye.Get(), RightEye.Get() })
	{
		if (UMaterialInstanceDynamic* Material = Eye->CreateDynamicMaterialInstance(0))
		{
			EyeMaterials.Add(Material);
		}
	}
	Phase = EPhase::Sunk;
	ApplyLook(0.0f, 0.0f);
	// The first look comes early, so a player passing by has a chance to see it
	NextAppear = FMath::FRandRange(2.0f, AppearDelay.X);
}

void AVaelLurkingEyes::ScheduleNext()
{
	NextAppear = FMath::FRandRange(AppearDelay.X, FMath::Max(AppearDelay.X, AppearDelay.Y));
}

APawn* AVaelLurkingEyes::FindNearestPlayer(float& OutDistance) const
{
	APawn* Nearest = nullptr;
	OutDistance = TNumericLimits<float>::Max();
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APawn* Pawn = It->Get() != nullptr ? It->Get()->GetPawn() : nullptr;
		if (Pawn == nullptr)
		{
			continue;
		}
		const float Distance = FVector::Dist2D(Pawn->GetActorLocation(), GetActorLocation());
		if (Distance < OutDistance)
		{
			OutDistance = Distance;
			Nearest = Pawn;
		}
	}
	return Nearest;
}

void AVaelLurkingEyes::ApplyLook(float Up, float Blink)
{
	const float Size = EyeSize / SphereMeshSize;
	// Flattened a little like eyes, squeezed shut during a blink
	const FVector Scale(Size, Size, Size * 0.7f * FMath::Lerp(1.0f, 0.12f, Blink));
	const float Height = FMath::Lerp(-SunkDepth, EyeSize * 0.15f, Up);
	LeftEye->SetRelativeLocation(FVector(0.0f, -EyeSpacing * 0.5f, Height));
	RightEye->SetRelativeLocation(FVector(0.0f, EyeSpacing * 0.5f, Height));
	LeftEye->SetRelativeScale3D(Scale);
	RightEye->SetRelativeScale3D(Scale);

	for (UMaterialInstanceDynamic* Material : EyeMaterials)
	{
		Material->SetScalarParameterValue(TEXT("Glow"), Up);
	}
	const bool bVisible = Up > 0.01f;
	LeftEye->SetVisibility(bVisible);
	RightEye->SetVisibility(bVisible);
}

void AVaelLurkingEyes::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	float Distance = 0.0f;
	const APawn* Player = FindNearestPlayer(Distance);
	const bool bTooClose = Player != nullptr && Distance < ShyRange;
	PhaseTime += DeltaSeconds;

	switch (Phase)
	{
	case EPhase::Sunk:
		NextAppear -= DeltaSeconds;
		if (NextAppear <= 0.0f)
		{
			if (Player != nullptr && Distance <= WatchRange && !bTooClose)
			{
				Phase = EPhase::Rising;
				PhaseTime = 0.0f;
				BlinkAt = FMath::FRandRange(0.6f, FMath::Max(0.6f, WatchTime - 0.5f));
				// Smooth motion while up
				SetActorTickInterval(0.0f);
			}
			else
			{
				NextAppear = 1.0f;
			}
		}
		return;

	case EPhase::Rising:
		if (bTooClose)
		{
			Phase = EPhase::Sinking;
			PhaseTime = (1.0f - FMath::Clamp(PhaseTime / FadeTime, 0.0f, 1.0f)) * FadeTime;
		}
		else if (PhaseTime >= FadeTime)
		{
			Phase = EPhase::Watching;
			PhaseTime = 0.0f;
		}
		break;

	case EPhase::Watching:
		if (bTooClose || PhaseTime >= WatchTime)
		{
			Phase = EPhase::Sinking;
			// Startled eyes vanish quicker
			PhaseTime = bTooClose ? FadeTime * 0.6f : 0.0f;
		}
		break;

	case EPhase::Sinking:
		if (PhaseTime >= FadeTime)
		{
			Phase = EPhase::Sunk;
			PhaseTime = 0.0f;
			ApplyLook(0.0f, 0.0f);
			ScheduleNext();
			SetActorTickInterval(0.25f);
			return;
		}
		break;
	}

	// Turn towards the nearest player, slowly, like something that watches
	if (Player != nullptr)
	{
		const FVector ToPlayer = Player->GetActorLocation() - GetActorLocation();
		const float TargetYaw = FMath::RadiansToDegrees(FMath::Atan2(ToPlayer.Y, ToPlayer.X));
		const FRotator Current = GetActorRotation();
		SetActorRotation(FRotator(0.0f, FMath::FixedTurn(Current.Yaw, TargetYaw, 40.0f * DeltaSeconds), 0.0f));
	}

	float Up = 1.0f;
	if (Phase == EPhase::Rising)
	{
		Up = FMath::SmoothStep(0.0f, 1.0f, PhaseTime / FadeTime);
	}
	else if (Phase == EPhase::Sinking)
	{
		Up = 1.0f - FMath::SmoothStep(0.0f, 1.0f, PhaseTime / FadeTime);
	}
	const float SinceBlink = PhaseTime - BlinkAt;
	const float Blink = Phase == EPhase::Watching && SinceBlink >= 0.0f && SinceBlink < BlinkDuration
		? FMath::Sin(SinceBlink / BlinkDuration * UE_PI) : 0.0f;
	ApplyLook(Up, Blink);
}
