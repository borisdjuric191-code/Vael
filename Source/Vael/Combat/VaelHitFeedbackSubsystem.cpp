// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/VaelHitFeedbackSubsystem.h"
#include "Camera/VaelSharedCamera.h"
#include "Components/MeshComponent.h"
#include "Creatures/VaelCreature.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameplayTagContainer.h"
#include "Kismet/GameplayStatics.h"
#include "Magic/VaelDebrisBurst.h"
#include "Magic/VaelGameplayTags.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UI/VaelUISettings.h"
#include "VaelAssets.h"
#include "VaelGameMode.h"

namespace
{
	/** The flash over a hit body (Scripts/CreateDeathMaterials.py), params "Farbe" and "Staerke" */
	const FSoftObjectPath FlashMaterialPath(TEXT("/Game/Vael/VFX/Tod/M_Treffer_Blitz.M_Treffer_Blitz"));

	/** Seconds a hit flash lasts */
	constexpr float FlashTime = 0.13f;

	/** Most flashes at once (every one is a material instance) */
	constexpr int32 MaxFlashes = 64;

	FLinearColor ElementColor(const FGameplayTagContainer* Tags, float& OutGlow)
	{
		OutGlow = 5.0f;
		if (Tags == nullptr)
		{
			return FLinearColor(1.0f, 0.85f, 0.6f);
		}
		if (Tags->HasTag(VaelTags::GetElementTag(EVaelElement::Fire)))
		{
			return FLinearColor(1.0f, 0.45f, 0.1f);
		}
		if (Tags->HasTag(VaelTags::GetElementTag(EVaelElement::Water)))
		{
			return FLinearColor(0.45f, 0.75f, 1.0f);
		}
		if (Tags->HasTag(VaelTags::GetElementTag(EVaelElement::Air)))
		{
			OutGlow = 8.0f;
			return FLinearColor(0.7f, 0.85f, 1.0f);
		}
		if (Tags->HasTag(VaelTags::GetElementTag(EVaelElement::Mark)))
		{
			return FLinearColor(0.6f, 0.2f, 0.95f);
		}
		if (Tags->HasTag(VaelTags::GetElementTag(EVaelElement::Earth)))
		{
			OutGlow = 0.0f;
			return FLinearColor(0.42f, 0.34f, 0.25f);
		}
		return FLinearColor(1.0f, 0.85f, 0.6f);
	}
}

void UVaelHitFeedbackSubsystem::OnDamageTaken(AActor* Target, float Damage, float EffectMultiplier, bool bTargetIsPlayer, const FGameplayTagContainer* DamageTags,
	bool bOverTime, float HealthBefore)
{
	UWorld* World = Target != nullptr ? Target->GetWorld() : nullptr;
	UVaelHitFeedbackSubsystem* Subsystem = World != nullptr ? World->GetSubsystem<UVaelHitFeedbackSubsystem>() : nullptr;
	if (Subsystem == nullptr)
	{
		return;
	}

	const UVaelUISettings* Settings = UVaelUISettings::Get();
	const bool bHeavy = !bOverTime && (Damage >= Settings->HeavyHitDamage || (!bTargetIsPlayer && EffectMultiplier >= Settings->WeaknessThreshold));
	const bool bKill = !bTargetIsPlayer && HealthBefore > 0.0f && Damage >= HealthBefore;

	if (bTargetIsPlayer)
	{
		// The harder the blow, the more the screen shakes; the hero flashes red (Boris 2026-11-27: getting hit must be felt)
		if (!bOverTime)
		{
			Shake(Target, Settings->PlayerHurtShake * (bHeavy ? 2.0f : 1.0f));
			Subsystem->Flash(Target, FLinearColor(1.0f, 0.08f, 0.04f), bHeavy ? 0.9f : 0.6f);
		}
	}
	else
	{
		if (bHeavy || bKill)
		{
			Shake(Target, Settings->HeavyHitShake * (bKill ? 0.8f : 1.0f));
		}

		AVaelCreature* Creature = Cast<AVaelCreature>(Target);
		if (Creature != nullptr && !bOverTime && Damage >= Settings->StaggerMinDamage)
		{
			Creature->Stagger(bHeavy ? Settings->HeavyStaggerDuration : Settings->StaggerDuration);
		}

		// What a hit looks like on any body: a flash over all its meshes and sparks of the element where it landed
		float Glow = 5.0f;
		const FLinearColor Color = ElementColor(DamageTags, Glow);
		if (!bOverTime)
		{
			Subsystem->Flash(Target, FLinearColor::LerpUsingHSV(FLinearColor(1.0f, 0.95f, 0.85f), Color, 0.35f), bHeavy || bKill ? 1.0f : 0.75f);
			// a flier hit in the air is knocked back in its flight and throws a bigger shower (Boris 2026-11-27: hits on fast fliers
			// need more satisfaction)
			const UCharacterMovementComponent* Movement = Creature != nullptr ? Creature->GetCharacterMovement() : nullptr;
			const bool bFlier = Movement != nullptr && (Movement->IsFlying() || Movement->IsFalling());
			Subsystem->Sparks(Target, Color, Glow, bKill || bFlier);
			if (bFlier && !bKill && Creature != nullptr)
			{
				const FVector From = Creature->GetLastBlow().From;
				if (!From.IsNearlyZero())
				{
					Creature->ApplyKnockback(Creature->GetActorLocation() - From, 320.0f);
				}
				Shake(Target, Settings->HeavyHitShake * 0.35f);
			}
		}
		else
		{
			Subsystem->Flash(Target, Color, 0.3f);
		}
	}

	if (bHeavy)
	{
		Subsystem->StartHitStop(Settings->HitStopDuration);
	}
	else if (bKill)
	{
		// the killing blow stops the world for a breath, even a light one
		Subsystem->StartHitStop(Settings->HitStopDuration * 0.7f);
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

void UVaelHitFeedbackSubsystem::Flash(AActor* Target, const FLinearColor& Color, float Strength)
{
	UMaterialInterface* Material = VaelAssets::LoadOptional<UMaterialInterface>(TSoftObjectPtr<UMaterialInterface>(FlashMaterialPath));
	if (Material == nullptr || Target == nullptr)
	{
		return;
	}
	FFlash* Entry = Flashes.FindByPredicate([Target](const FFlash& Each) { return Each.Target.Get() == Target; });
	if (Entry == nullptr)
	{
		if (Flashes.Num() >= MaxFlashes)
		{
			// the bodies gone since are dropped first
			Flashes.RemoveAll([](const FFlash& Each) { return !Each.Target.IsValid(); });
			if (Flashes.Num() >= MaxFlashes)
			{
				return;
			}
		}
		// a material of a body that is gone is used again
		TBitArray<> Used(false, FlashMaterials.Num());
		for (const FFlash& Each : Flashes)
		{
			Used[Each.Slot] = true;
		}
		int32 Slot = Used.Find(false);
		if (Slot == INDEX_NONE)
		{
			Slot = FlashMaterials.Add(UMaterialInstanceDynamic::Create(Material, this));
		}
		FlashMaterials[Slot]->SetScalarParameterValue(TEXT("Staerke"), 0.0f);
		FFlash& Added = Flashes.AddDefaulted_GetRef();
		Added.Target = Target;
		Added.Slot = Slot;
		// the body and what hangs on it (the MetaHuman of a hero is an actor of its own, attached to the mesh)
		TArray<AActor*> Parts;
		Target->GetAttachedActors(Parts, true, true);
		Parts.Add(Target);
		TArray<UMeshComponent*> Meshes;
		for (AActor* Part : Parts)
		{
			TInlineComponentArray<UMeshComponent*> Found(Part);
			for (UMeshComponent* Mesh : Found)
			{
				// a mesh that already wears an overlay (a corpse) keeps it
				if (Mesh->IsVisible() && Mesh->GetOverlayMaterial() == nullptr)
				{
					Meshes.Add(Mesh);
				}
			}
		}
		// only the big parts, once: some bodies carry hundreds of small pieces (the crystals of the Erztochter)
		Meshes.Sort([](const UMeshComponent& A, const UMeshComponent& B) { return A.Bounds.SphereRadius > B.Bounds.SphereRadius; });
		const float Biggest = Meshes.IsEmpty() ? 0.0f : Meshes[0]->Bounds.SphereRadius;
		for (int32 Index = 0; Index < Meshes.Num() && Index < 6; ++Index)
		{
			if (Meshes[Index]->Bounds.SphereRadius < Biggest * 0.3f)
			{
				break;
			}
			Meshes[Index]->SetOverlayMaterial(FlashMaterials[Slot]);
		}
		Entry = &Added;
	}
	Entry->Start = FPlatformTime::Seconds();
	Entry->Strength = FMath::Max(Strength, 0.1f);
	Entry->bRunning = true;
	if (UMaterialInstanceDynamic* Instance = FlashMaterials[Entry->Slot])
	{
		Instance->SetVectorParameterValue(TEXT("Farbe"), Color);
		Instance->SetScalarParameterValue(TEXT("Staerke"), Entry->Strength);
	}
}

void UVaelHitFeedbackSubsystem::Sparks(AActor* Target, const FLinearColor& Color, float Glow, bool bKill) const
{
	AVaelCreature* Creature = Cast<AVaelCreature>(Target);
	if (Creature == nullptr)
	{
		return;
	}
	// fast weapons: not more than a few bursts a second on the same body
	const double Now = FPlatformTime::Seconds();
	UVaelHitFeedbackSubsystem* Self = const_cast<UVaelHitFeedbackSubsystem*>(this);
	if (const double* Last = Self->LastSparks.Find(Target); Last != nullptr && Now - *Last < 0.09 && !bKill)
	{
		return;
	}
	Self->LastSparks.Add(Target, Now);
	if (Self->LastSparks.Num() > 64)
	{
		for (auto It = Self->LastSparks.CreateIterator(); It; ++It)
		{
			if (!It.Key().IsValid() || Now - It.Value() > 1.0)
			{
				It.RemoveCurrent();
			}
		}
	}

	// on the side the blow came from
	const FVector Centre = Creature->GetActorLocation();
	const FVector From = Creature->GetLastBlow().From;
	const FVector Toward = From.IsNearlyZero() ? FVector::ZeroVector : (From - Centre).GetSafeNormal2D();
	const FVector Where = Centre + Toward * Creature->GetSimpleCollisionRadius() * 0.8f;
	FVaelDebris Burst;
	Burst.Count = bKill ? 12 : 6;
	Burst.Spread = 10.0f;
	Burst.Speed = bKill ? 620.0f : 460.0f;
	Burst.Lift = 260.0f;
	Burst.Gravity = 1100.0f;
	Burst.Drag = 3.0f;
	Burst.Lifetime = bKill ? 0.4f : 0.26f;
	Burst.MinSize = 2.0f;
	Burst.MaxSize = bKill ? 6.0f : 4.5f;
	Burst.Color = Color;
	Burst.Glow = Glow;
	Burst.bLandOnGround = false;
	Burst.bSoft = Glow > 0.0f;
	AVaelDebrisBurst::Spawn(Target, Where, Burst);
}

void UVaelHitFeedbackSubsystem::StartHitStop(float Duration)
{
	const UVaelUISettings* Settings = UVaelUISettings::Get();
	const double Now = FPlatformTime::Seconds();

	if (Duration <= 0.0f || Now < NextHitStopTime || HitStopEndTime > 0.0)
	{
		return;
	}

	HitStopEndTime = Now + Duration;
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
	const double Now = FPlatformTime::Seconds();
	if (HitStopEndTime > 0.0 && Now >= HitStopEndTime)
	{
		EndHitStop();
	}

	// The flashes fade quickly; the material stays on the body, at nothing
	for (int32 Index = Flashes.Num() - 1; Index >= 0; --Index)
	{
		FFlash& Each = Flashes[Index];
		if (!Each.Target.IsValid())
		{
			Flashes.RemoveAtSwap(Index);
			continue;
		}
		if (!Each.bRunning)
		{
			continue;
		}
		UMaterialInstanceDynamic* Instance = FlashMaterials.IsValidIndex(Each.Slot) ? FlashMaterials[Each.Slot].Get() : nullptr;
		const float Left = 1.0f - static_cast<float>((Now - Each.Start) / FlashTime);
		if (Instance != nullptr)
		{
			Instance->SetScalarParameterValue(TEXT("Staerke"), Left > 0.0f ? Each.Strength * Left * Left : 0.0f);
		}
		Each.bRunning = Left > 0.0f;
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
