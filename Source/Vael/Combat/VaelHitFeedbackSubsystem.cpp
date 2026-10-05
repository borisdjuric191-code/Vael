// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VaelHitFeedbackSubsystem.h"
#include "Camera/VaelSharedCamera.h"
#include "Creatures/VaelCreature.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "UI/VaelUISettings.h"
#include "VaelGameMode.h"

void UVaelHitFeedbackSubsystem::OnDamageTaken(AActor* Target, float Damage, float EffectMultiplier, bool bTargetIsPlayer)
{
	UWorld* World = Target != nullptr ? Target->GetWorld() : nullptr;
	UVaelHitFeedbackSubsystem* Subsystem = World != nullptr ? World->GetSubsystem<UVaelHitFeedbackSubsystem>() : nullptr;
	if (Subsystem == nullptr)
	{
		return;
	}

	const UVaelUISettings* Settings = UVaelUISettings::Get();
	const bool bHeavy = Damage >= Settings->HeavyHitDamage || (!bTargetIsPlayer && EffectMultiplier >= Settings->WeaknessThreshold);

	if (bTargetIsPlayer)
	{
		// The harder the blow, the more the screen shakes
		Shake(Target, Settings->PlayerHurtShake * (bHeavy ? 2.0f : 1.0f));
	}
	else
	{
		if (bHeavy)
		{
			Shake(Target, Settings->HeavyHitShake);
		}

		if (AVaelCreature* Creature = Cast<AVaelCreature>(Target); Creature != nullptr && Damage >= Settings->StaggerMinDamage)
		{
			Creature->Stagger(bHeavy ? Settings->HeavyStaggerDuration : Settings->StaggerDuration);
		}
	}

	if (bHeavy)
	{
		Subsystem->StartHitStop();
	}
}

void UVaelHitFeedbackSubsystem::Shake(const UObject* WorldContext, float Strength)
{
	const UWorld* World = GEngine != nullptr ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	AVaelGameMode* GameMode = World != nullptr ? World->GetAuthGameMode<AVaelGameMode>() : nullptr;

	if (GameMode != nullptr && Strength > 0.0f)
	{
		if (AVaelSharedCamera* SharedCamera = GameMode->GetSharedCamera())
		{
			SharedCamera->AddShake(Strength);
		}
	}
}

void UVaelHitFeedbackSubsystem::StartHitStop()
{
	const UVaelUISettings* Settings = UVaelUISettings::Get();
	const double Now = FPlatformTime::Seconds();

	if (Settings->HitStopDuration <= 0.0f || Now < NextHitStopTime || HitStopEndTime > 0.0)
	{
		return;
	}

	HitStopEndTime = Now + Settings->HitStopDuration;
	NextHitStopTime = HitStopEndTime + Settings->HitStopCooldown;

	UGameplayStatics::SetGlobalTimeDilation(GetWorld(), Settings->HitStopTimeDilation);
}

void UVaelHitFeedbackSubsystem::EndHitStop()
{
	HitStopEndTime = 0.0;
	UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 1.0f);
}

void UVaelHitFeedbackSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Measured in real time, the game itself runs slowed down
	if (HitStopEndTime > 0.0 && FPlatformTime::Seconds() >= HitStopEndTime)
	{
		EndHitStop();
	}
}

TStatId UVaelHitFeedbackSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UVaelHitFeedbackSubsystem, STATGROUP_Tickables);
}

void UVaelHitFeedbackSubsystem::Deinitialize()
{
	if (HitStopEndTime > 0.0)
	{
		EndHitStop();
	}

	Super::Deinitialize();
}
