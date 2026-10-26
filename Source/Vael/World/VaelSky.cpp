// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/VaelSky.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Vael.h"
#include "World/VaelTimeOfDay.h"
#include "World/VaelTimeSettings.h"

namespace
{
	/** Hours the sun and the moon are up: the sun from the start of dawn to the end of dusk, the moon through the night with some overlap */
	constexpr float SunRiseHour = 5.0f;
	constexpr float SunSetHour = 21.0f;
	constexpr float MoonRiseHour = 19.0f;
	constexpr float MoonSetHour = 7.0f;

	/** Degrees the sun and moon wander across the sky from rising to setting */
	constexpr float SkySweep = 120.0f;

	/** Share of the way from rising to setting, outside 0 to 1 while it is down */
	float ShareUp(float Hour, float Rise, float Set)
	{
		const float Span = FMath::Fmod(Set - Rise + 24.0f, 24.0f);
		return FMath::Fmod(Hour - Rise + 24.0f, 24.0f) / Span;
	}
}

AVaelSky::AVaelSky()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AVaelSky::EnsureSky(UWorld* World)
{
	if (World == nullptr || TActorIterator<AVaelSky>(World))
	{
		return;
	}

	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	World->SpawnActor<AVaelSky>(AVaelSky::StaticClass(), FTransform::Identity, Parameters);
}

void AVaelSky::BeginPlay()
{
	Super::BeginPlay();
	Capture();
}

void AVaelSky::Capture()
{
	UWorld* World = GetWorld();

	// The sun of the level: the first one still shining (the Wurzelforst hides doubled lights)
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		UDirectionalLightComponent* LevelSun = Cast<UDirectionalLightComponent>(It->GetLightComponent());
		if (LevelSun == nullptr || It->IsHidden() || !LevelSun->IsVisible())
		{
			continue;
		}
		BaseSunIntensity = LevelSun->Intensity;
		BaseSunColor = LevelSun->GetLightColor();
		BaseYaw = LevelSun->GetComponentRotation().Yaw;
		LevelSun->SetVisibility(false);
		break;
	}

	const auto MakeLight = [this](int32 AtmosphereIndex)
	{
		UDirectionalLightComponent* Light = NewObject<UDirectionalLightComponent>(this);
		Light->SetupAttachment(RootComponent);
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetUsingAbsoluteRotation(true);
		Light->SetAtmosphereSunLight(true);
		Light->SetAtmosphereSunLightIndex(AtmosphereIndex);
		Light->RegisterComponent();
		return Light;
	};
	Sun = MakeLight(0);
	Sun->SetIntensity(BaseSunIntensity);
	Sun->SetLightColor(BaseSunColor);
	Moon = MakeLight(1);
	Moon->SetLightColor(UVaelTimeSettings::Get()->MoonColor);
	Moon->SetLightSourceAngle(0.3f);

	for (TActorIterator<ASkyLight> It(World); It; ++It)
	{
		if (!It->IsHidden() && It->GetLightComponent() != nullptr && It->GetLightComponent()->IsVisible())
		{
			SkyLight = It->GetLightComponent();
			BaseSkyIntensity = SkyLight->Intensity;
			break;
		}
	}

	for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
	{
		if (!It->IsHidden() && It->GetComponent() != nullptr && It->GetComponent()->IsVisible())
		{
			Fog = It->GetComponent();
			BaseFogDensity = Fog->FogDensity;
			BaseFogColor = Fog->FogInscatteringLuminance;
			break;
		}
	}

	// The exposure the level chose, so the night darkens from there
	for (TActorIterator<APostProcessVolume> It(World); It; ++It)
	{
		if (It->bUnbound && It->Settings.bOverride_AutoExposureBias)
		{
			BaseExposureBias = It->Settings.AutoExposureBias;
			break;
		}
	}

	Picture = NewObject<UPostProcessComponent>(this);
	Picture->SetupAttachment(RootComponent);
	Picture->bUnbound = true;
	Picture->Priority = 100.0f;
	Picture->RegisterComponent();
	Picture->Settings.bOverride_AutoExposureBias = true;
	Picture->Settings.bOverride_ColorSaturation = true;
	Picture->Settings.bOverride_SceneColorTint = true;

	UE_LOG(LogVael, Log, TEXT("Sky takes over the sun (%.2f) and follows the clock"), BaseSunIntensity);
}

void AVaelSky::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const UVaelTimeOfDay* Clock = UVaelTimeOfDay::Get(this);
	if (Clock == nullptr || Sun == nullptr)
	{
		return;
	}

	const UVaelTimeSettings* Settings = UVaelTimeSettings::Get();
	const float Hour = Clock->GetHour();
	const float Night = Clock->GetNightFactor();

	// The sun climbs from the horizon at dawn to its highest at noon and sinks into the dusk, wandering across the sky
	const float SunShare = ShareUp(Hour, SunRiseHour, SunSetHour);
	const bool bSunUp = SunShare >= 0.0f && SunShare <= 1.0f;
	const float SunElevation = bSunUp ? FMath::Sin(SunShare * UE_PI) * Settings->SunHighest : -10.0f;
	Sun->SetWorldRotation(FRotator(-FMath::Max(SunElevation, 2.0f), BaseYaw + (FMath::Clamp(SunShare, 0.0f, 1.0f) - 0.5f) * SkySweep, 0.0f));
	Sun->SetIntensity(BaseSunIntensity * FMath::SmoothStep(0.0f, 12.0f, SunElevation));
	Sun->SetLightColor(FMath::Lerp(Settings->LowSunColor, BaseSunColor, FMath::SmoothStep(4.0f, 28.0f, SunElevation)));

	// The moon: cold, faint, from the other side of the sky
	const float MoonShare = ShareUp(Hour, MoonRiseHour, MoonSetHour);
	const bool bMoonUp = MoonShare >= 0.0f && MoonShare <= 1.0f;
	const float MoonElevation = bMoonUp ? FMath::Sin(MoonShare * UE_PI) * Settings->MoonHighest : -10.0f;
	Moon->SetWorldRotation(FRotator(-FMath::Max(MoonElevation, 2.0f), BaseYaw + 180.0f + (FMath::Clamp(MoonShare, 0.0f, 1.0f) - 0.5f) * SkySweep, 0.0f));
	Moon->SetIntensity(BaseSunIntensity * Settings->MoonShare * FMath::SmoothStep(0.0f, 10.0f, MoonElevation) * Night);

	if (SkyLight != nullptr)
	{
		SkyLight->SetIntensity(BaseSkyIntensity * FMath::Lerp(1.0f, Settings->NightSkyShare, Night));
	}
	if (Fog != nullptr)
	{
		Fog->SetFogDensity(BaseFogDensity * FMath::Lerp(1.0f, Settings->NightFogDensity, Night));
		Fog->SetFogInscatteringColor(FMath::Lerp(BaseFogColor, Settings->NightFogColor, Night));
	}

	// The picture darkens and cools, color drains out of it
	if (Picture != nullptr)
	{
		const float Saturation = FMath::Lerp(1.0f, Settings->NightSaturation, Night);
		Picture->Settings.AutoExposureBias = BaseExposureBias - Settings->NightExposureDrop * Night;
		Picture->Settings.ColorSaturation = FVector4(Saturation, Saturation, Saturation, 1.0f);
		Picture->Settings.SceneColorTint = FMath::Lerp(FLinearColor::White, Settings->NightTint, Night);
	}

	PlayerLightTime -= DeltaSeconds;
	if (PlayerLightTime <= 0.0f)
	{
		PlayerLightTime = 0.25f;
		UpdatePlayerLights(Night);
	}
}

void AVaelSky::UpdatePlayerLights(float Night)
{
	const UVaelTimeSettings* Settings = UVaelTimeSettings::Get();

	// Players who left take their light with them
	for (auto It = PlayerLights.CreateIterator(); It; ++It)
	{
		if (It.Key() == nullptr || It.Value() == nullptr)
		{
			It.RemoveCurrent();
		}
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APawn* Pawn = It->Get() != nullptr ? It->Get()->GetPawn() : nullptr;
		if (Pawn == nullptr)
		{
			continue;
		}

		TObjectPtr<UPointLightComponent>& Light = PlayerLights.FindOrAdd(Pawn);
		if (Light == nullptr)
		{
			// A soft glow above the head that lights the ground around the player, no shadows
			Light = NewObject<UPointLightComponent>(Pawn);
			Light->SetupAttachment(Pawn->GetRootComponent());
			Light->SetRelativeLocation(FVector(0.0f, 0.0f, 220.0f));
			Light->SetMobility(EComponentMobility::Movable);
			Light->SetIntensityUnits(ELightUnits::Candelas);
			Light->SetCastShadows(false);
			Light->SetLightColor(Settings->PlayerLightColor);
			Light->RegisterComponent();
		}
		Light->SetAttenuationRadius(Settings->PlayerLightRadius);
		Light->SetIntensity(Settings->PlayerLightIntensity * Night);
		Light->SetVisibility(Night > 0.01f);
	}
}
