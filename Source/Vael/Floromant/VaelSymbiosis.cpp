// Copyright Epic Games, Inc. All Rights Reserved.

#include "Floromant/VaelSymbiosis.h"
#include "AbilitySystemComponent.h"
#include "Combat/VaelAttributeSet.h"
#include "Combat/VaelCharacterBase.h"
#include "Magic/VaelDebrisBurst.h"
#include "Magic/VaelLightningBolt.h"
#include "Floromant/VaelGlimmDome.h"
#include "Floromant/VaelRoseHeart.h"
#include "GameFramework/PlayerController.h"
#include "Player/VaelCharacter.h"
#include "TimerManager.h"
#include "Combat/VaelCombatStatics.h"
#include "Creatures/VaelCreature.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Floromant/VaelPlantArmData.h"
#include "Floromant/VaelPlantData.h"
#include "GameFramework/Pawn.h"
#include "Magic/VaelElementSource.h"
#include "Magic/VaelSpellEffects.h"
#include "Magic/VaelSpellProjectile.h"
#include "VaelAssets.h"

namespace
{
	/** Id of the plant of an arm, like "Windhalm"; the asset name when the plant data is missing */
	FName SymbiosisPlantId(const UVaelPlantArmData* Arm)
	{
		if (Arm == nullptr)
		{
			return NAME_None;
		}
		if (const UVaelPlantData* Plant = VaelAssets::LoadOptional(Arm->Plant))
		{
			return Plant->PlantId;
		}
		FString Name = Arm->GetName();
		Name.RemoveFromStart(TEXT("DA_Pflanzenarm_"));
		return FName(*Name);
	}

	/** The way turned around the up axis by the degrees */
	FVector SymbiosisTurn(const FVector& Direction, float Degrees)
	{
		return FRotator(0.0f, Degrees, 0.0f).RotateVector(Direction);
	}

	/** Spawns one projectile of a symbiosis; the plant gives speed, range and the water of the shot */
	AVaelSpellProjectile* SymbiosisProjectile(APawn* Instigator, const UVaelPlantArmData* Plant, const FVector& Origin, const FVector& Direction, const FVaelSpellHit& Hit,
		float Radius, int32 Pierce, const FLinearColor& Color, bool bWater)
	{
		UWorld* World = Instigator != nullptr ? Instigator->GetWorld() : nullptr;
		if (World == nullptr || Plant == nullptr)
		{
			return nullptr;
		}
		const FTransform SpawnTransform(Direction.Rotation(), Origin);
		AVaelSpellProjectile* Shot = World->SpawnActorDeferred<AVaelSpellProjectile>(AVaelSpellProjectile::StaticClass(), SpawnTransform, Instigator, Instigator, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Shot == nullptr)
		{
			return nullptr;
		}

		// The plant's own shot (Boris 2026-10-26): the Glutdistel throws its burning seed head, the Windhalm a blade like a
		// dart that pushes a little
		FVaelSpellHit PlantHit = Hit;
		const FName Id = SymbiosisPlantId(Plant);
		if (Id == TEXT("Glutdistel"))
		{
			Shot->SetLook(EVaelProjectileLook::ThistleSeed);
			Shot->SetLookMesh(VaelAssets::LoadOptional(Plant->TipMesh));
		}
		else if (Id == TEXT("Windhalm"))
		{
			Shot->SetLook(EVaelProjectileLook::GrassDart);
			Shot->SetLookMesh(Plant->FingerMeshes.IsEmpty() ? nullptr : VaelAssets::LoadOptional(Plant->FingerMeshes[0]));
			PlantHit.Knockback = FMath::Max(PlantHit.Knockback, 160.0f);
		}
		Shot->InitSpell(PlantHit, Plant->ShotSpeed, Radius, Plant->ShotRange / Plant->ShotSpeed, Pierce, Color);
		if (bWater)
		{
			if (UVaelElementSource* Source = VaelAssets::LoadOptional(Plant->WaterSource))
			{
				Shot->SetLook(EVaelProjectileLook::WaterOrb);
				Shot->SetWaterSource(Source);
				Shot->SetWatersPlants(Plant->WaterRadius, Plant->WaterPerShot);
			}
		}
		Shot->SetEffects(VaelEffects::Load(Hit.Element));
		return Shot;
	}
}

namespace VaelSymbiosis
{
	EVaelSymbiosis Find(const UVaelPlantArmData* A, const UVaelPlantArmData* B)
	{
		const FName First = SymbiosisPlantId(A);
		const FName Second = SymbiosisPlantId(B);
		const auto Is = [&First, &Second](const TCHAR* X, const TCHAR* Y)
		{
			return (First == X && Second == Y) || (First == Y && Second == X);
		};

		if (Is(TEXT("Windhalm"), TEXT("Windhalm")))
		{
			return EVaelSymbiosis::Sturmgras;
		}
		if (Is(TEXT("Glutdistel"), TEXT("Glutdistel")))
		{
			return EVaelSymbiosis::Glutbusch;
		}
		if (Is(TEXT("Taublatt"), TEXT("Taublatt")))
		{
			return EVaelSymbiosis::Quellrose;
		}
		if (Is(TEXT("Windhalm"), TEXT("Glutdistel")))
		{
			return EVaelSymbiosis::Funkenflug;
		}
		if (Is(TEXT("Windhalm"), TEXT("Taublatt")))
		{
			return EVaelSymbiosis::Spruehregen;
		}
		if (Is(TEXT("Glutdistel"), TEXT("Taublatt")))
		{
			return EVaelSymbiosis::Dampfstoss;
		}
		if (Is(TEXT("Glimmklee"), TEXT("Glimmklee")))
		{
			return EVaelSymbiosis::Lichtkuppel;
		}
		if (Is(TEXT("Glimmklee"), TEXT("Glutdistel")))
		{
			return EVaelSymbiosis::Glutglas;
		}
		if (Is(TEXT("Glimmklee"), TEXT("Windhalm")))
		{
			return EVaelSymbiosis::Funkelwind;
		}
		if (Is(TEXT("Glimmklee"), TEXT("Taublatt")))
		{
			return EVaelSymbiosis::Tauglocke;
		}
		if (Is(TEXT("Russrose"), TEXT("Russrose")))
		{
			return EVaelSymbiosis::Rosenherz;
		}
		if (Is(TEXT("Russrose"), TEXT("Windhalm")))
		{
			return EVaelSymbiosis::Bluetensturm;
		}
		if (Is(TEXT("Russrose"), TEXT("Glutdistel")))
		{
			return EVaelSymbiosis::Aschenrose;
		}
		if (Is(TEXT("Russrose"), TEXT("Taublatt")))
		{
			return EVaelSymbiosis::Rosentau;
		}
		if (Is(TEXT("Russrose"), TEXT("Glimmklee")))
		{
			return EVaelSymbiosis::Glashaus;
		}
		return EVaelSymbiosis::None;
	}

	FText GetName(EVaelSymbiosis Kind)
	{
		switch (Kind)
		{
		case EVaelSymbiosis::Sturmgras:		return NSLOCTEXT("VaelSymbiosis", "Sturmgras", "Sturmgras");
		case EVaelSymbiosis::Glutbusch:		return NSLOCTEXT("VaelSymbiosis", "Glutbusch", "Glutbusch");
		case EVaelSymbiosis::Quellrose:		return NSLOCTEXT("VaelSymbiosis", "Quellrose", "Quellrose");
		case EVaelSymbiosis::Funkenflug:	return NSLOCTEXT("VaelSymbiosis", "Funkenflug", "Funkenflug");
		case EVaelSymbiosis::Spruehregen:	return NSLOCTEXT("VaelSymbiosis", "Spruehregen", "Sprühregen");
		case EVaelSymbiosis::Dampfstoss:	return NSLOCTEXT("VaelSymbiosis", "Dampfstoss", "Dampfstoß");
		case EVaelSymbiosis::Lichtkuppel:	return NSLOCTEXT("VaelSymbiosis", "Lichtkuppel", "Lichtkuppel");
		case EVaelSymbiosis::Glutglas:		return NSLOCTEXT("VaelSymbiosis", "Glutglas", "Glutglas");
		case EVaelSymbiosis::Funkelwind:	return NSLOCTEXT("VaelSymbiosis", "Funkelwind", "Funkelwind");
		case EVaelSymbiosis::Tauglocke:		return NSLOCTEXT("VaelSymbiosis", "Tauglocke", "Tauglocke");
		case EVaelSymbiosis::Rosenherz:		return NSLOCTEXT("VaelSymbiosis", "Rosenherz", "Rosenherz");
		case EVaelSymbiosis::Bluetensturm:	return NSLOCTEXT("VaelSymbiosis", "Bluetensturm", "Blütensturm");
		case EVaelSymbiosis::Aschenrose:	return NSLOCTEXT("VaelSymbiosis", "Aschenrose", "Aschenrose");
		case EVaelSymbiosis::Rosentau:		return NSLOCTEXT("VaelSymbiosis", "Rosentau", "Rosentau");
		case EVaelSymbiosis::Glashaus:		return NSLOCTEXT("VaelSymbiosis", "Glashaus", "Glashaus");
		default:							return FText::GetEmpty();
		}
	}

	FLinearColor GetColor(EVaelSymbiosis Kind)
	{
		switch (Kind)
		{
		case EVaelSymbiosis::Sturmgras:		return FLinearColor(0.75f, 0.95f, 0.6f);
		case EVaelSymbiosis::Glutbusch:		return FLinearColor(1.0f, 0.35f, 0.08f);
		case EVaelSymbiosis::Quellrose:		return FLinearColor(0.3f, 0.95f, 0.9f);
		case EVaelSymbiosis::Funkenflug:	return FLinearColor(1.0f, 0.6f, 0.2f);
		case EVaelSymbiosis::Spruehregen:	return FLinearColor(0.55f, 0.9f, 1.0f);
		case EVaelSymbiosis::Dampfstoss:	return FLinearColor(0.9f, 0.9f, 0.95f);
		case EVaelSymbiosis::Lichtkuppel:	return FLinearColor(1.0f, 0.9f, 0.55f);
		case EVaelSymbiosis::Glutglas:		return FLinearColor(1.0f, 0.5f, 0.15f);
		case EVaelSymbiosis::Funkelwind:	return FLinearColor(0.85f, 1.0f, 0.6f);
		case EVaelSymbiosis::Tauglocke:		return FLinearColor(0.4f, 0.95f, 0.9f);
		case EVaelSymbiosis::Rosenherz:		return FLinearColor(0.25f, 1.0f, 0.55f);
		case EVaelSymbiosis::Bluetensturm:	return FLinearColor(0.55f, 0.95f, 0.6f);
		case EVaelSymbiosis::Aschenrose:	return FLinearColor(0.9f, 0.55f, 0.25f);
		case EVaelSymbiosis::Rosentau:		return FLinearColor(0.35f, 0.95f, 0.8f);
		case EVaelSymbiosis::Glashaus:		return FLinearColor(0.6f, 1.0f, 0.55f);
		default:							return FLinearColor::White;
		}
	}

	bool HasDome(EVaelSymbiosis Kind)
	{
		return Kind == EVaelSymbiosis::Lichtkuppel || Kind == EVaelSymbiosis::Glutglas || Kind == EVaelSymbiosis::Funkelwind || Kind == EVaelSymbiosis::Tauglocke
			|| Kind == EVaelSymbiosis::Glashaus;
	}

	bool HasRose(EVaelSymbiosis Kind)
	{
		return Kind == EVaelSymbiosis::Rosenherz || Kind == EVaelSymbiosis::Bluetensturm || Kind == EVaelSymbiosis::Aschenrose || Kind == EVaelSymbiosis::Rosentau
			|| Kind == EVaelSymbiosis::Glashaus;
	}

	void RoseBeat(APawn* Instigator, EVaelSymbiosis Kind, const FVector& Center, float Power, bool bShadow)
	{
		AVaelCharacter* Gardener = Cast<AVaelCharacter>(Instigator);
		if (Gardener == nullptr)
		{
			return;
		}
		// The Blutrose drains instead of healing, the farther the stronger the pair
		if (bShadow)
		{
			VaelRoseCare::DrainAround(Gardener, Center, Kind == EVaelSymbiosis::Rosenherz ? 800.0f : 500.0f, 6.0f * Power, 0.5f);
			return;
		}
		switch (Kind)
		{
		case EVaelSymbiosis::Rosenherz:
		{
			// A strong heartbeat: heals far, and the fallen around get up sooner
			VaelRoseCare::HealAround(Gardener, Center, 800.0f, 0.08f * Power);
			for (FConstPlayerControllerIterator It = Gardener->GetWorld()->GetPlayerControllerIterator(); It; ++It)
			{
				AVaelCharacter* Player = It->Get() != nullptr ? It->Get()->GetPawn<AVaelCharacter>() : nullptr;
				if (Player != nullptr && Player->IsDowned() && FVector::Dist2D(Player->GetActorLocation(), Center) < 800.0f)
				{
					Player->AddReviveProgress(1.0f);
				}
			}
			break;
		}
		case EVaelSymbiosis::Bluetensturm:
			VaelRoseCare::HealAround(Gardener, Center, 1000.0f, 0.05f * Power);
			break;
		case EVaelSymbiosis::Rosentau:
			VaelRoseCare::CleanseAround(Gardener, Center, 600.0f, 15.0f * Power);
			break;
		case EVaelSymbiosis::Glashaus:
			VaelRoseCare::HealAround(Gardener, Center, 300.0f, 0.05f * Power);
			break;
		case EVaelSymbiosis::None:
			// A single Rußrose tower
			VaelRoseCare::HealAround(Gardener, Center, 500.0f, 0.06f * Power);
			break;
		default:
			break;
		}
	}

	float GetTowerInterval(EVaelSymbiosis Kind)
	{
		switch (Kind)
		{
		case EVaelSymbiosis::Glutbusch:		return 2.2f;
		case EVaelSymbiosis::Quellrose:		return 2.0f;
		case EVaelSymbiosis::Dampfstoss:	return 3.0f;
		default:							return 1.4f;
		}
	}

	void Fire(APawn* Instigator, EVaelSymbiosis Kind, const UVaelPlantArmData* A, const UVaelPlantArmData* B, const FVector& Origin, const FVector& Direction, float Power, float Charge, bool bShadow)
	{
		if (Instigator == nullptr || A == nullptr || B == nullptr || Kind == EVaelSymbiosis::None)
		{
			return;
		}

		// The plant of each part of the pair that brings the wind, the fire or the water
		const auto Pick = [A, B](const TCHAR* Id) { return SymbiosisPlantId(A) == Id ? A : B; };
		const FLinearColor Color = GetColor(Kind);
		const float Grown = 1.0f + Charge;
		// Hands fire flat over the ground; towers aim down at their target from the crown
		FVector Ahead = Direction.GetSafeNormal();
		if (Ahead.IsNearlyZero())
		{
			Ahead = Instigator->GetActorForwardVector();
		}

		switch (Kind)
		{
		case EVaelSymbiosis::Sturmgras:
		case EVaelSymbiosis::Funkenflug:
		{
			// A fan of darts; with the Glutdistel they burn
			const UVaelPlantArmData* Wind = Pick(TEXT("Windhalm"));
			const bool bEmbers = Kind == EVaelSymbiosis::Funkenflug;
			const int32 Count = Charge > 0.5f ? 9 : 5;
			const float Spread = Charge > 0.5f ? 50.0f : 30.0f;
			FVaelSpellHit Hit;
			Hit.Damage = Wind->ShotDamage * (bEmbers ? 1.1f : 1.3f) * Power * Grown;
			Hit.Element = bEmbers ? EVaelElement::Fire : Wind->ShotElement;
			if (bEmbers)
			{
				Hit.Status = EVaelStatus::Burning;
				Hit.StatusDuration = 3.0f;
			}
			for (int32 Index = 0; Index < Count; ++Index)
			{
				const float Angle = Count > 1 ? FMath::Lerp(-Spread, Spread, Index / static_cast<float>(Count - 1)) : 0.0f;
				if (AVaelSpellProjectile* Shot = SymbiosisProjectile(Instigator, Wind, Origin, SymbiosisTurn(Ahead, Angle), Hit, Wind->ShotRadius * 1.3f, Charge > 0.5f ? 2 : 1, Color, false))
				{
					Shot->FinishSpawning(FTransform(SymbiosisTurn(Ahead, Angle).Rotation(), Origin));
				}
			}
			break;
		}
		case EVaelSymbiosis::Glutbusch:
		{
			// One big glowing seed, two more beside it when held; it bursts wide and the ground burns
			const UVaelPlantArmData* Fire = Pick(TEXT("Glutdistel"));
			FVaelSpellHit Hit;
			Hit.Damage = Fire->ShotDamage * 2.0f * Power * Grown;
			Hit.Element = EVaelElement::Fire;
			FVaelSpellHit Burst;
			Burst.Damage = FMath::Max(Fire->BurstDamage, 3.0f) * 3.0f * Power * Grown;
			Burst.Element = EVaelElement::Fire;
			Burst.Status = EVaelStatus::Burning;
			Burst.StatusDuration = 3.0f;
			const int32 Count = Charge > 0.5f ? 3 : 1;
			for (int32 Index = 0; Index < Count; ++Index)
			{
				const FVector Way = SymbiosisTurn(Ahead, (Index - (Count - 1) * 0.5f) * 18.0f);
				if (AVaelSpellProjectile* Shot = SymbiosisProjectile(Instigator, Fire, Origin, Way, Hit, Fire->ShotRadius * 1.8f, 0, Color, false))
				{
					Shot->SetExplosion(Burst, FMath::Max(Fire->BurstRadius, 90.0f) * 2.0f * FMath::Sqrt(Grown));
					Shot->SetImpactArea(EVaelElement::Fire, 150.0f * FMath::Sqrt(Grown), 5.0f, 6.0f * Power);
					Shot->FinishSpawning(FTransform(Way.Rotation(), Origin));
				}
			}
			break;
		}
		case EVaelSymbiosis::Quellrose:
		case EVaelSymbiosis::Spruehregen:
		{
			// A big Tauperle, or with the Windhalm a spray of fine drops; all of them soak and water
			const UVaelPlantArmData* Water = Pick(TEXT("Taublatt"));
			const bool bSpray = Kind == EVaelSymbiosis::Spruehregen;
			const int32 Count = bSpray ? (Charge > 0.5f ? 12 : 7) : (Charge > 0.5f ? 3 : 1);
			const float Spread = bSpray ? (Charge > 0.5f ? 55.0f : 35.0f) : 14.0f;
			FVaelSpellHit Hit;
			Hit.Damage = Water->ShotDamage * (bSpray ? 0.6f : 2.2f) * Power * Grown;
			Hit.Element = EVaelElement::Water;
			Hit.Status = EVaelStatus::Wet;
			Hit.StatusDuration = 6.0f;
			FVaelSpellHit Soak;
			Soak.Damage = bSpray ? 0.0f : Water->ShotDamage * Power * Grown;
			Soak.Element = EVaelElement::Water;
			Soak.Status = EVaelStatus::Wet;
			Soak.StatusDuration = 6.0f;
			for (int32 Index = 0; Index < Count; ++Index)
			{
				const float Angle = Count > 1 ? FMath::Lerp(-Spread, Spread, Index / static_cast<float>(Count - 1)) + (bSpray ? FMath::FRandRange(-4.0f, 4.0f) : 0.0f) : 0.0f;
				const FVector Way = SymbiosisTurn(Ahead, Angle);
				if (AVaelSpellProjectile* Shot = SymbiosisProjectile(Instigator, Water, Origin, Way, Hit, Water->ShotRadius * (bSpray ? 0.6f : 2.0f), 0, Color, true))
				{
					Shot->SetExplosion(Soak, bSpray ? 70.0f : 220.0f * FMath::Sqrt(Grown));
					Shot->SetWatersPlants(Water->WaterRadius * (bSpray ? 1.0f : 2.0f), Water->WaterPerShot * (bSpray ? 0.5f : 3.0f));
					Shot->FinishSpawning(FTransform(Way.Rotation(), Origin));
				}
			}
			break;
		}
		case EVaelSymbiosis::Dampfstoss:
		{
			// A ball of water full of glowing seeds: where it ends it boils into scalding steam that blinds
			const UVaelPlantArmData* Water = Pick(TEXT("Taublatt"));
			FVaelSpellHit Hit;
			Hit.Damage = Water->ShotDamage * 1.5f * Power * Grown;
			Hit.Element = EVaelElement::Fire;
			FVaelSpellHit Scald;
			Scald.Damage = 6.0f * Power * Grown;
			Scald.Element = EVaelElement::Fire;
			if (AVaelSpellProjectile* Shot = SymbiosisProjectile(Instigator, Water, Origin, Ahead, Hit, Water->ShotRadius * 1.6f, 0, Color, true))
			{
				Shot->SetExplosion(Scald, 160.0f * FMath::Sqrt(Grown));
				Shot->SetImpactArea(EVaelElement::Water, 230.0f * FMath::Sqrt(Grown), 6.0f, 4.0f * Power, EVaelGroundEffect::Blind);
				Shot->FinishSpawning(FTransform(Ahead.Rotation(), Origin));
			}
			break;
		}
		case EVaelSymbiosis::Lichtkuppel:
		{
			// A flash of light: every enemy around is blinded, and each one gives Saft
			const float Reach = 600.0f * FMath::Sqrt(Grown);
			int32 Blinded = 0;
			for (TActorIterator<AVaelCharacterBase> It(Instigator->GetWorld()); It; ++It)
			{
				if (UVaelCombatStatics::CanDamage(Instigator, *It) && FVector::Dist2D(It->GetActorLocation(), Origin) < Reach)
				{
					It->ApplyBlind(2.0f * Grown);
					++Blinded;
				}
			}
			if (const AVaelCharacterBase* Gardener = Cast<AVaelCharacterBase>(Instigator))
			{
				Gardener->GetAbilitySystemComponent()->ApplyModToAttribute(UVaelAttributeSet::GetManaAttribute(), EGameplayModOp::Additive, 2.0f * Blinded);
			}
			FVaelDebris Light;
			Light.Count = 40;
			Light.Spread = 30.0f;
			Light.Speed = Reach * 1.4f;
			Light.Lift = 30.0f;
			Light.Gravity = -40.0f;
			Light.Drag = 2.0f;
			Light.Lifetime = 0.7f;
			Light.MinSize = 4.0f;
			Light.MaxSize = 9.0f;
			Light.Color = Color;
			Light.Glow = 6.0f;
			Light.bSoft = true;
			Light.bLandOnGround = false;
			AVaelDebrisBurst::Spawn(Instigator, Origin, Light);
			break;
		}
		case EVaelSymbiosis::Glutglas:
		{
			// The clover bundles the light of the glowing heads into a short, burning sun beam
			const UVaelPlantArmData* Fire = Pick(TEXT("Glutdistel"));
			const FVector Flat = Ahead.GetSafeNormal2D();
			const float Reach = 900.0f * FMath::Sqrt(Grown);
			FVaelSpellHit Beam;
			Beam.Damage = Fire->ShotDamage * 4.0f * Power * Grown;
			Beam.Element = EVaelElement::Fire;
			Beam.Status = EVaelStatus::Burning;
			Beam.StatusDuration = 3.0f;
			for (TActorIterator<AVaelCharacterBase> It(Instigator->GetWorld()); It; ++It)
			{
				const FVector To = (It->GetActorLocation() - Origin) * FVector(1.0f, 1.0f, 0.0f);
				const float Along = FVector::DotProduct(To, Flat);
				const float Across = (To - Flat * Along).Size();
				if (Along > 0.0f && Along < Reach && Across < 60.0f && UVaelCombatStatics::CanDamage(Instigator, *It))
				{
					UVaelCombatStatics::ApplySpellHit(Instigator, *It, Beam, Flat);
				}
			}
			AVaelLightningBolt::Spawn(Instigator, Origin, Origin + Flat * Reach, Color, 0.35f, 2.2f, false);
			break;
		}
		case EVaelSymbiosis::Funkelwind:
		{
			// A fan of glittering pollen that blinds everything in front
			const UVaelPlantArmData* Wind = Pick(TEXT("Windhalm"));
			const FVector Flat = Ahead.GetSafeNormal2D();
			const float Reach = 700.0f * FMath::Sqrt(Grown);
			FVaelSpellHit Pollen;
			Pollen.Damage = Wind->ShotDamage * Power * Grown;
			Pollen.Element = Wind->ShotElement;
			for (TActorIterator<AVaelCharacterBase> It(Instigator->GetWorld()); It; ++It)
			{
				const FVector To = (It->GetActorLocation() - Origin) * FVector(1.0f, 1.0f, 0.0f);
				if (To.Size() < Reach && FVector::DotProduct(To.GetSafeNormal(), Flat) > FMath::Cos(FMath::DegreesToRadians(35.0f)) && UVaelCombatStatics::CanDamage(Instigator, *It))
				{
					It->ApplyBlind(2.5f * Grown);
					UVaelCombatStatics::ApplySpellHit(Instigator, *It, Pollen, Flat);
				}
			}
			for (int32 Puff = -2; Puff <= 2; ++Puff)
			{
				FVaelDebris Glitter;
				Glitter.Count = 10;
				Glitter.Spread = 20.0f;
				Glitter.Speed = Reach * 1.2f;
				Glitter.Lift = 20.0f;
				Glitter.Gravity = 20.0f;
				Glitter.Drag = 2.5f;
				Glitter.Lifetime = 0.9f;
				Glitter.MinSize = 2.0f;
				Glitter.MaxSize = 5.0f;
				Glitter.Color = Color;
				Glitter.Glow = 4.0f;
				Glitter.bSoft = true;
				Glitter.bLandOnGround = false;
				AVaelDebrisBurst::Spawn(Instigator, Origin + SymbiosisTurn(Flat, Puff * 14.0f) * 30.0f, Glitter);
			}
			break;
		}
		case EVaelSymbiosis::Tauglocke:
		{
			// A drop of dew that breaks the light: where it bursts a shimmering mist blinds and soaks
			const UVaelPlantArmData* Water = Pick(TEXT("Taublatt"));
			FVaelSpellHit Hit;
			Hit.Damage = Water->ShotDamage * 1.5f * Power * Grown;
			Hit.Element = EVaelElement::Water;
			Hit.Status = EVaelStatus::Wet;
			Hit.StatusDuration = 6.0f;
			if (AVaelSpellProjectile* Shot = SymbiosisProjectile(Instigator, Water, Origin, Ahead, Hit, Water->ShotRadius * 1.6f, 0, Color, true))
			{
				Shot->SetExplosion(Hit, 180.0f * FMath::Sqrt(Grown));
				Shot->SetImpactArea(EVaelElement::Water, 200.0f * FMath::Sqrt(Grown), 3.0f, 0.0f, EVaelGroundEffect::Blind);
				Shot->FinishSpawning(FTransform(Ahead.Rotation(), Origin));
			}
			break;
		}
		case EVaelSymbiosis::Bluetensturm:
		case EVaelSymbiosis::Rosentau:
		{
			// A fan in front: petals that heal allies and cut enemies, or dew that cleanses allies and heals plants
			const bool bPetals = Kind == EVaelSymbiosis::Bluetensturm;
			const FVector Flat = Ahead.GetSafeNormal2D();
			const float Reach = 800.0f * FMath::Sqrt(Grown);
			const UVaelPlantArmData* Wind = Pick(TEXT("Windhalm"));
			for (TActorIterator<AVaelCharacterBase> It(Instigator->GetWorld()); It; ++It)
			{
				const FVector To = (It->GetActorLocation() - Origin) * FVector(1.0f, 1.0f, 0.0f);
				if (To.Size() > Reach || FVector::DotProduct(To.GetSafeNormal(), Flat) < FMath::Cos(FMath::DegreesToRadians(32.0f)))
				{
					continue;
				}
				if (AVaelCharacter* Player = Cast<AVaelCharacter>(*It))
				{
					if (bPetals)
					{
						UVaelCombatStatics::Heal(Player, Player->GetMaxHealth() * 0.15f * Power * Grown);
					}
					else
					{
						VaelRoseCare::CleanseAround(Player, Player->GetActorLocation(), 60.0f, 0.0f);
					}
					VaelRoseCare::Petals(Instigator, Origin, Player->GetActorLocation(), false, 6);
				}
				else if (bPetals && UVaelCombatStatics::CanDamage(Instigator, *It))
				{
					FVaelSpellHit Cut;
					Cut.Damage = Wind->ShotDamage * 2.0f * Power * Grown;
					Cut.Element = Wind->ShotElement;
					UVaelCombatStatics::ApplySpellHit(Instigator, *It, Cut, Flat);
				}
			}
			if (!bPetals)
			{
				VaelRoseCare::CleanseAround(Instigator, Origin + Flat * Reach * 0.5f, Reach * 0.5f, 20.0f * Power * Grown);
			}
			for (int32 Puff = -2; Puff <= 2; ++Puff)
			{
				VaelRoseCare::Petals(Instigator, Origin, Origin + SymbiosisTurn(Flat, Puff * 13.0f) * Reach * 0.4f, false, 6);
			}
			break;
		}
		case EVaelSymbiosis::Aschenrose:
		{
			// Glowing seeds that burn the enemies; where they come down, the allies around heal
			const UVaelPlantArmData* Fire = Pick(TEXT("Glutdistel"));
			const FVector Flat = Ahead.GetSafeNormal2D();
			FVaelSpellHit Hit;
			Hit.Damage = Fire->ShotDamage * 1.4f * Power * Grown;
			Hit.Element = EVaelElement::Fire;
			for (int32 Index = -1; Index <= 1; ++Index)
			{
				const FVector Way = SymbiosisTurn(Flat, Index * 14.0f);
				if (AVaelSpellProjectile* Shot = SymbiosisProjectile(Instigator, Fire, Origin, Way, Hit, Fire->ShotRadius * 1.3f, 0, Color, false))
				{
					Shot->FinishSpawning(FTransform(Way.Rotation(), Origin));
				}
			}
			TWeakObjectPtr<APawn> WeakGardener = Instigator;
			const FVector Landing = Origin + Flat * 600.0f;
			const float HealShare = 0.1f * Power * Grown;
			FTimerHandle Timer;
			Instigator->GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([WeakGardener, Landing, HealShare]()
			{
				if (APawn* Gardener = WeakGardener.Get())
				{
					VaelRoseCare::HealAround(Gardener, Landing, 350.0f, HealShare);
				}
			}), 600.0f / FMath::Max(Fire->ShotSpeed, 100.0f), false);
			break;
		}
		case EVaelSymbiosis::Glashaus:
		{
			// A heart shield around the ally who needs it most, herself without one
			AVaelCharacter* Gardener = Cast<AVaelCharacter>(Instigator);
			AVaelCharacter* Ally = VaelRoseCare::FindHurtAlly(Gardener, Origin, 1200.0f, true, false, Ahead);
			Ally = Ally != nullptr ? Ally : Gardener;
			if (Ally != nullptr)
			{
				Ally->AddHeartShield(Ally->GetMaxHealth() * 0.3f * Power * Grown, 6.0f);
				const UVaelPlantArmData* Glimm = A->bShield ? A : B;
				if (AVaelGlimmDome* Bubble = AVaelGlimmDome::Raise(Gardener, Ally, Glimm, EVaelSymbiosis::Glashaus, 95.0f, bShadow))
				{
					Bubble->SetLifeSpan(6.0f);
				}
				VaelRoseCare::Petals(Instigator, Origin, Ally->GetActorLocation(), false, 10);
			}
			break;
		}
		default:
			break;
		}
	}

	bool FireShot(APawn* Instigator, const UVaelPlantArmData* Plant, const FVector& Muzzle, const FVector& Direction, float Power, bool bShadow, float SizeScale, float DamageScale)
	{
		if (Instigator == nullptr || Plant == nullptr)
		{
			return false;
		}

		FVaelSpellHit Hit;
		Hit.Damage = Plant->ShotDamage * Power * DamageScale;
		Hit.Element = Plant->ShotElement;
		Hit.Status = Plant->ShotStatus;
		Hit.StatusDuration = Plant->ShotStatusDuration;

		AVaelSpellProjectile* Shot = SymbiosisProjectile(Instigator, Plant, Muzzle, Direction, Hit, Plant->ShotRadius * SizeScale, 0, Plant->GetColor(bShadow), true);
		if (Shot == nullptr)
		{
			return false;
		}
		if (Plant->BurstRadius > 0.0f)
		{
			FVaelSpellHit Burst;
			Burst.Damage = Plant->BurstDamage * Power * DamageScale;
			Burst.Element = Plant->ShotElement;
			Shot->SetExplosion(Burst, Plant->BurstRadius * SizeScale);
		}
		Shot->FinishSpawning(FTransform(Direction.Rotation(), Muzzle));
		return true;
	}

	AActor* FindTarget(APawn* Instigator, const FVector& Location, float Range)
	{
		UWorld* World = Instigator != nullptr ? Instigator->GetWorld() : nullptr;
		if (World == nullptr)
		{
			return nullptr;
		}
		AActor* Best = nullptr;
		float BestDistance = FMath::Square(Range);
		for (TActorIterator<AVaelCharacterBase> It(World); It; ++It)
		{
			const AVaelCreature* Creature = Cast<AVaelCreature>(*It);
			if (!UVaelCombatStatics::CanDamage(Instigator, *It) || (Creature != nullptr && Creature->IsHiddenFromAim()))
			{
				continue;
			}
			const float Distance = FVector::DistSquared2D(It->GetActorLocation(), Location);
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				Best = *It;
			}
		}
		return Best;
	}
}
