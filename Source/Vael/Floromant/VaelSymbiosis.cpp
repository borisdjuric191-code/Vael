// Copyright Epic Games, Inc. All Rights Reserved.

#include "Floromant/VaelSymbiosis.h"
#include "Combat/VaelCharacterBase.h"
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
		Shot->InitSpell(Hit, Plant->ShotSpeed, Radius, Plant->ShotRange / Plant->ShotSpeed, Pierce, Color);
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
		default:							return FLinearColor::White;
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
