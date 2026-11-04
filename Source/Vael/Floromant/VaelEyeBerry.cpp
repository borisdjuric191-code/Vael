// Copyright Epic Games, Inc. All Rights Reserved.

#include "Floromant/VaelEyeBerry.h"
#include "Camera/PlayerCameraManager.h"
#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelCombatStatics.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Floromant/VaelPlantArmData.h"
#include "Floromant/VaelSymbiosis.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Vael.h"
#include "VaelAssets.h"
#include "World/VaelGround.h"

namespace
{
	/** Size of the engine plane in cm */
	constexpr float EyeRingPlaneSize = 100.0f;

	/** Seconds the eye takes to open after landing, and to close and sink at the end */
	constexpr float EyeOpenSeconds = 0.4f;
	constexpr float EyeCloseSeconds = 0.6f;

	/** Colours of the gaze: turquoise in the light, red on the shadow path */
	const FLinearColor EyeLightColor(0.25f, 0.95f, 0.9f);
	const FLinearColor EyeShadowColor(1.0f, 0.08f, 0.06f);
}

AVaelEyeBerry::AVaelEyeBerry()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Berry = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Berry"));
	Berry->SetupAttachment(Root);
	Berry->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Berry->SetCanEverAffectNavigation(false);

	Ring = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ring"));
	Ring->SetupAttachment(Root);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		Ring->SetStaticMesh(PlaneMesh.Object);
	}
	Ring->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ring->SetCastShadow(false);
	Ring->bReceivesDecals = false;
	Ring->SetCanEverAffectNavigation(false);
	Ring->SetVisibility(false);

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Root);
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(0.0f);
	Light->SetCastShadows(false);
	Light->SetAttenuationRadius(500.0f);
	Light->SetRelativeLocation(FVector(0.0f, 0.0f, 60.0f));
}

AVaelEyeBerry* AVaelEyeBerry::Throw(APawn* InThrower, const UVaelPlantArmData* InPlant, const FVector& Muzzle, const FVector& Target, bool bInShadow)
{
	UWorld* World = InThrower != nullptr ? InThrower->GetWorld() : nullptr;
	if (World == nullptr || InPlant == nullptr)
	{
		return nullptr;
	}
	FActorSpawnParameters Parameters;
	Parameters.Owner = InThrower;
	Parameters.Instigator = InThrower;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVaelEyeBerry* Eye = World->SpawnActor<AVaelEyeBerry>(AVaelEyeBerry::StaticClass(), FTransform(Muzzle), Parameters);
	if (Eye == nullptr)
	{
		return nullptr;
	}
	Eye->Plant = InPlant;
	Eye->Start = Muzzle;
	Eye->Target = Target;
	Eye->bShadow = bInShadow;

	Eye->Berry->SetStaticMesh(VaelAssets::LoadOptional(InPlant->TipMesh));
	if (UMaterialInterface* Look = VaelAssets::LoadOptional(InPlant->PartMaterial))
	{
		Eye->BerryLook = Eye->Berry->CreateDynamicMaterialInstance(0, Look);
		Eye->BerryLook->SetScalarParameterValue(TEXT("Schatten"), bInShadow ? 1.0f : 0.0f);
		Eye->BerryLook->SetScalarParameterValue(TEXT("Offen"), 0.0f);
	}
	if (UMaterialInterface* Look = VaelAssets::LoadOptional(InPlant->EyeRingMaterial))
	{
		Eye->RingLook = Eye->Ring->CreateDynamicMaterialInstance(0, Look);
		Eye->RingLook->SetVectorParameterValue(TEXT("Farbe"), bInShadow ? EyeShadowColor : EyeLightColor);
	}
	Eye->Light->SetLightColor(bInShadow ? EyeShadowColor : EyeLightColor);
	Eye->Berry->SetRelativeScale3D(FVector(InPlant->TipSize / 10.0f));
	return Eye;
}

void AVaelEyeBerry::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Plant == nullptr)
	{
		Destroy();
		return;
	}
	Age += DeltaSeconds;
	Flash = FMath::Max(0.0f, Flash - DeltaSeconds * 2.0f);

	// In flight: an arc to the target, tumbling, the eye shut
	if (!bLanded)
	{
		const float Share = FMath::Clamp(Age / Plant->EyeFlightSeconds, 0.0f, 1.0f);
		const float Arc = 120.0f + FVector::Dist2D(Start, Target) * 0.15f;
		SetActorLocation(FMath::Lerp(Start, Target, Share) + FVector(0.0f, 0.0f, 4.0f * Arc * Share * (1.0f - Share)));
		Berry->SetRelativeRotation(FRotator(Age * 720.0f, Age * 300.0f, 0.0f));
		if (FMath::Fmod(Age, 0.05f) < GetWorld()->GetDeltaSeconds())
		{
			VaelSymbiosis::MuzzlePuff(this, Plant, GetActorLocation(), bShadow, 0.4f);
		}
		if (Share >= 1.0f)
		{
			bLanded = true;
			VaelSymbiosis::MuzzlePuff(this, Plant, Target + FVector(0.0f, 0.0f, 20.0f), bShadow, 2.0f);
			OpenAge = 0.0f;
			NextLook = EyeOpenSeconds;
			SetActorLocation(Target);
			Ring->SetVisibility(true);
			Ring->SetRelativeLocation(FVector(0.0f, 0.0f, 3.0f));
			Ring->SetRelativeScale3D(FVector(Plant->EyeRadius * 2.0f / EyeRingPlaneSize));
		}
		return;
	}

	OpenAge += DeltaSeconds;
	const float Left = Plant->EyeSeconds - OpenAge;
	if (Left <= -EyeCloseSeconds)
	{
		Destroy();
		return;
	}

	// On the ground the whole berry swells and lies on its side, its eye turned up towards the camera, so berry and eye both
	// read; it opens, blinks now and then, shuts and sinks at the end
	const float Grown = FMath::InterpEaseOut(0.0f, 1.0f, FMath::Clamp(OpenAge / EyeOpenSeconds, 0.0f, 1.0f), 2.0f);
	const float Sink = Left < 0.0f ? -Left / EyeCloseSeconds : 0.0f;
	FVector Axis = FVector::UpVector;
	if (const APlayerController* Controller = GetWorld()->GetFirstPlayerController())
	{
		if (Controller->PlayerCameraManager != nullptr)
		{
			const FVector ToCamera = (Controller->PlayerCameraManager->GetCameraLocation() - GetActorLocation()).GetSafeNormal2D();
			Axis = (ToCamera + FVector::UpVector * 0.5f).GetSafeNormal();
		}
	}
	const float Size = Plant->TipSize / 10.0f * FMath::Lerp(1.0f, Plant->EyeGroundScale, Grown) * (1.0f - 0.6f * Sink);
	Berry->SetRelativeRotation(FRotationMatrix::MakeFromZ(Axis).Rotator());
	// Pivot at the stalk end, 10 cm long and about 4 cm thick: its middle on the ground, resting on its belly
	Berry->SetRelativeLocation(-Axis * Size * 5.0f + FVector(0.0f, 0.0f, Size * 3.5f * (1.0f - Sink)));
	Berry->SetRelativeScale3D(FVector(Size));

	const float Blink = FMath::Fmod(OpenAge + 1.3f, 2.6f) < 0.16f ? 0.1f : 1.0f;
	const float Open = Grown * Blink * (1.0f - Sink);
	const float Glow = Open * (1.0f + 3.0f * Flash);
	if (BerryLook != nullptr)
	{
		BerryLook->SetScalarParameterValue(TEXT("Offen"), Open);
		BerryLook->SetScalarParameterValue(TEXT("Leuchten"), 1.0f + 3.0f * Flash);
	}
	if (RingLook != nullptr)
	{
		RingLook->SetScalarParameterValue(TEXT("Staerke"), Glow * 0.22f);
	}
	Light->SetIntensity(8.0f * Glow);

	if (Left > 0.0f && OpenAge >= NextLook)
	{
		NextLook = OpenAge + Plant->EyePulse;
		LookAround();
	}
}

void AVaelEyeBerry::LookAround()
{
	++Looks;
	Flash = 1.0f;
	APawn* Thrower = GetInstigator();
	const float Share = FMath::FRandRange(Plant->EyeShare.X, Plant->EyeShare.Y);
	AVaelCharacterBase* NearestPlayer = nullptr;
	float NearestDistance = FMath::Square(Plant->EyeRadius);
	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		AVaelCharacterBase* Character = *It;
		const float Distance = FVector::DistSquared2D(Character->GetActorLocation(), GetActorLocation());
		if (Character->IsDefeated() || Character->GetHealth() <= 0.0f || Distance > FMath::Square(Plant->EyeRadius))
		{
			continue;
		}
		if (Character->IsOnPlayerSide())
		{
			if (Character->IsPlayerControlled() && Distance < NearestDistance)
			{
				NearestDistance = Distance;
				NearestPlayer = Character;
			}
			continue;
		}
		// Never all of them: each look catches only its share
		if (!UVaelCombatStatics::CanDamage(Thrower, Character) || FMath::FRand() >= Share)
		{
			continue;
		}
		++Caught;
		if (bShadow)
		{
			Character->ApplyStun(Plant->EyeStareSeconds);
		}
		else
		{
			UVaelCombatStatics::ApplyStatus(Thrower, Character, EVaelStatus::Confused, Plant->EyeConfuseSeconds);
		}
	}

	// The shadow's price: now and then its stare falls on a player
	if (bShadow && NearestPlayer != nullptr && FMath::FRand() < Plant->EyePlayerStareChance)
	{
		UVaelCombatStatics::ApplyStatus(nullptr, NearestPlayer, EVaelStatus::Frozen, Plant->EyePlayerStareSeconds);
		UE_LOG(LogVael, Log, TEXT("Eye berry stares at %s"), *NearestPlayer->GetActorNameOrLabel());
	}
}
