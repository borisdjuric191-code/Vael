// Copyright Epic Games, Inc. All Rights Reserved.

#include "Floromant/VaelSymbiosis.h"
#include "AbilitySystemComponent.h"
#include "Combat/VaelAttributeSet.h"
#include "Combat/VaelCharacterBase.h"
#include "Magic/VaelDebrisBurst.h"
#include "Magic/VaelLightningBolt.h"
#include "Floromant/VaelBindweedLasso.h"
#include "Floromant/VaelGlimmDome.h"
#include "Floromant/VaelPlant.h"
#include "World/VaelGround.h"
#include "Floromant/VaelRoseHeart.h"
#include "Floromant/VaelPoppyCapsule.h"
#include "Floromant/VaelMoonVeil.h"
#include "Floromant/VaelFairyEscort.h"
#include "Floromant/VaelEyeBerry.h"
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
		// The shot is felt: halo, light, sparks and a clear burst in the plant colour (AVaelSpellProjectile::SetPlantFeel)
		Shot->SetPlantFeel(Color, 1.0f);
		Shot->SetEffects(VaelEffects::Load(Hit.Element));
		return Shot;
	}

	/** Where a pair acts on the ground: the tower's target, or ahead of the hands at the enemy nearest there */
	FVector SymbiosisGroundAhead(APawn* Instigator, const FVector& Origin, const FVector& Ahead, float Reach, float HandReach)
	{
		const FVector Flat = Ahead.GetSafeNormal2D().IsNearlyZero() ? Instigator->GetActorForwardVector() : Ahead.GetSafeNormal2D();
		FVector Place = Origin + Flat * (Reach > 0.0f ? Reach : HandReach);
		if (Reach <= 0.0f)
		{
			if (const AActor* Enemy = VaelSymbiosis::FindTarget(Instigator, Place, 450.0f))
			{
				Place = Enemy->GetActorLocation();
			}
		}
		FHitResult Ground;
		if (VaelGround::TraceGround(Instigator->GetWorld(), Place + FVector(0.0f, 0.0f, 400.0f), Place - FVector(0.0f, 0.0f, 1200.0f), Ground, Instigator))
		{
			return Ground.ImpactPoint;
		}
		return Place;
	}

	/** The fairies of a pair: a flight of them leaves for the place and lures there, strewing Feenstaub; a moment later the
	 *  pair's own touch falls on every enemy around the place */
	void SymbiosisFairies(APawn* Instigator, const UVaelPlantArmData* Fairy, const FVector& Origin, const FVector& Place, float Radius, float Seconds, bool bShadow,
		TFunction<void(APawn*, AVaelCreature*)> Touch = nullptr)
	{
		UWorld* World = Instigator->GetWorld();
		FActorSpawnParameters Parameters;
		Parameters.Owner = Instigator;
		Parameters.Instigator = Instigator;
		if (AVaelFairyEscort* Flight = World->SpawnActor<AVaelFairyEscort>(AVaelFairyEscort::StaticClass(), FTransform(Origin), Parameters))
		{
			Flight->Setup(Instigator, Fairy, FMath::FRand(), nullptr, 60.0f);
			Flight->SendAll(Place, bShadow);
			Flight->SetLifeSpan(Seconds + 5.0f);
		}
		TWeakObjectPtr<APawn> WeakInstigator = Instigator;
		FTimerHandle Timer;
		World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([WeakInstigator, Place, Radius, Seconds, bShadow, Touch]()
		{
			APawn* Gardener = WeakInstigator.Get();
			if (Gardener == nullptr)
			{
				return;
			}
			for (TActorIterator<AVaelCreature> It(Gardener->GetWorld()); It; ++It)
			{
				AVaelCreature* Enemy = *It;
				if (Enemy->IsDead() || !UVaelCombatStatics::CanDamage(Gardener, Enemy) || FVector::Dist2D(Enemy->GetActorLocation(), Place) > Radius)
				{
					continue;
				}
				if (!bShadow && !Enemy->IsHeavy())
				{
					Enemy->LureTo(Gardener, Place, Seconds);
				}
				if (Touch)
				{
					Touch(Gardener, Enemy);
				}
			}
		}), 0.8f, false);
	}

	/** A fan of pitch balls of the Harzkolben */
	void SymbiosisPitch(APawn* Instigator, const UVaelPlantArmData* Pitch, const FVector& Origin, const FVector& Ahead, int32 Count, float Spread, float Size, float Power,
		bool bShadow, float Reach)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Angle = Count > 1 ? FMath::Lerp(-Spread, Spread, Index / static_cast<float>(Count - 1)) : 0.0f;
			VaelSymbiosis::FireShot(Instigator, Pitch, Origin, SymbiosisTurn(Ahead, Angle), Power, bShadow, Size, 1.0f, Reach * FMath::FRandRange(0.85f, 1.15f));
		}
	}

	/** A veil of the Mondtraufe on the ground before the gardener (hands) or between the tower and its target */
	AVaelMoonVeil* SymbiosisVeil(APawn* Instigator, const UVaelPlantArmData* Moon, EVaelSymbiosis Kind, const UVaelPlantArmData* Other, const FVector& Origin,
		const FVector& Ahead, float Reach, float Size = 1.0f, float LifetimeScale = 1.0f, const FVector* At = nullptr, const FVector* Facing = nullptr)
	{
		const FVector Flat = Ahead.GetSafeNormal2D().IsNearlyZero() ? Instigator->GetActorForwardVector() : Ahead.GetSafeNormal2D();
		FVector Place = At != nullptr ? *At : (Reach > 0.0f ? Origin + Flat * FMath::Min(Reach * 0.5f, 400.0f) : Instigator->GetActorLocation() + Flat * Moon->VeilDistance);
		FHitResult Ground;
		if (VaelGround::TraceGround(Instigator->GetWorld(), Place + FVector(0.0f, 0.0f, 400.0f), Place - FVector(0.0f, 0.0f, 1200.0f), Ground, Instigator))
		{
			Place = Ground.ImpactPoint;
		}
		AVaelMoonVeil* Veil = AVaelMoonVeil::Raise(Instigator, Moon, Place, Facing != nullptr ? *Facing : Flat, Size);
		if (Veil != nullptr)
		{
			Veil->SetPair(Kind, Other, LifetimeScale);
		}
		return Veil;
	}

	/** A glass dome of the Glimmklee around the gardener for a while (pairs crossed at the hands) */
	void SymbiosisDome(APawn* Instigator, const UVaelPlantArmData* Glimm, float Radius, float Seconds, bool bShadow)
	{
		if (AVaelGlimmDome* Dome = AVaelGlimmDome::Raise(Instigator, Instigator, Glimm, EVaelSymbiosis::None, Radius, bShadow))
		{
			TWeakObjectPtr<AVaelGlimmDome> WeakDome = Dome;
			FTimerHandle Timer;
			Instigator->GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([WeakDome]()
			{
				if (WeakDome.IsValid())
				{
					WeakDome->Lower();
				}
			}), Seconds, false);
		}
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
		if (Is(TEXT("Funkenmohn"), TEXT("Funkenmohn")))
		{
			return EVaelSymbiosis::Mohnfeuerwerk;
		}
		if (Is(TEXT("Funkenmohn"), TEXT("Glutdistel")))
		{
			return EVaelSymbiosis::Zuendschnur;
		}
		if (Is(TEXT("Funkenmohn"), TEXT("Windhalm")))
		{
			return EVaelSymbiosis::Funkenwirbel;
		}
		if (Is(TEXT("Funkenmohn"), TEXT("Taublatt")))
		{
			return EVaelSymbiosis::Gewittermohn;
		}
		if (Is(TEXT("Funkenmohn"), TEXT("Glimmklee")))
		{
			return EVaelSymbiosis::Glutblase;
		}
		if (Is(TEXT("Funkenmohn"), TEXT("Russrose")))
		{
			return EVaelSymbiosis::Brandsalbe;
		}
		if (Is(TEXT("Schlotwinde"), TEXT("Schlotwinde")))
		{
			return EVaelSymbiosis::Windenzaun;
		}
		if (Is(TEXT("Schlotwinde"), TEXT("Windhalm")))
		{
			return EVaelSymbiosis::Wirbelwinde;
		}
		if (Is(TEXT("Schlotwinde"), TEXT("Glutdistel")))
		{
			return EVaelSymbiosis::Glutpeitsche;
		}
		if (Is(TEXT("Schlotwinde"), TEXT("Taublatt")))
		{
			return EVaelSymbiosis::Taunetz;
		}
		if (Is(TEXT("Schlotwinde"), TEXT("Glimmklee")))
		{
			return EVaelSymbiosis::Laube;
		}
		if (Is(TEXT("Schlotwinde"), TEXT("Russrose")))
		{
			return EVaelSymbiosis::Rankenrettung;
		}
		if (Is(TEXT("Schlotwinde"), TEXT("Funkenmohn")))
		{
			return EVaelSymbiosis::Haftmohn;
		}
		if (Is(TEXT("Feenragwurz"), TEXT("Feenragwurz")))
		{
			return EVaelSymbiosis::Feenreigen;
		}
		if (Is(TEXT("Feenragwurz"), TEXT("Windhalm")))
		{
			return EVaelSymbiosis::Feenwind;
		}
		if (Is(TEXT("Feenragwurz"), TEXT("Glutdistel")))
		{
			return EVaelSymbiosis::Gluehfeen;
		}
		if (Is(TEXT("Feenragwurz"), TEXT("Taublatt")))
		{
			return EVaelSymbiosis::Taufluegel;
		}
		if (Is(TEXT("Feenragwurz"), TEXT("Glimmklee")))
		{
			return EVaelSymbiosis::Laternenfeen;
		}
		if (Is(TEXT("Feenragwurz"), TEXT("Russrose")))
		{
			return EVaelSymbiosis::Rosenboten;
		}
		if (Is(TEXT("Feenragwurz"), TEXT("Funkenmohn")))
		{
			return EVaelSymbiosis::Mohnfeen;
		}
		if (Is(TEXT("Feenragwurz"), TEXT("Schlotwinde")))
		{
			return EVaelSymbiosis::Feenfalle;
		}
		if (Is(TEXT("Harzkolben"), TEXT("Harzkolben")))
		{
			return EVaelSymbiosis::Pechsumpf;
		}
		if (Is(TEXT("Harzkolben"), TEXT("Windhalm")))
		{
			return EVaelSymbiosis::Pechregen;
		}
		if (Is(TEXT("Harzkolben"), TEXT("Glutdistel")))
		{
			return EVaelSymbiosis::Brandharz;
		}
		if (Is(TEXT("Harzkolben"), TEXT("Taublatt")))
		{
			return EVaelSymbiosis::Bernstein;
		}
		if (Is(TEXT("Harzkolben"), TEXT("Glimmklee")))
		{
			return EVaelSymbiosis::Harzglas;
		}
		if (Is(TEXT("Harzkolben"), TEXT("Russrose")))
		{
			return EVaelSymbiosis::Wundharz;
		}
		if (Is(TEXT("Harzkolben"), TEXT("Funkenmohn")))
		{
			return EVaelSymbiosis::Pechbombe;
		}
		if (Is(TEXT("Harzkolben"), TEXT("Schlotwinde")))
		{
			return EVaelSymbiosis::Klebranke;
		}
		if (Is(TEXT("Mondtraufe"), TEXT("Mondtraufe")))
		{
			return EVaelSymbiosis::Mondhalle;
		}
		if (Is(TEXT("Mondtraufe"), TEXT("Windhalm")))
		{
			return EVaelSymbiosis::WehenderSchleier;
		}
		if (Is(TEXT("Mondtraufe"), TEXT("Glutdistel")))
		{
			return EVaelSymbiosis::Glutvorhang;
		}
		if (Is(TEXT("Mondtraufe"), TEXT("Taublatt")))
		{
			return EVaelSymbiosis::Taustrom;
		}
		if (Is(TEXT("Mondtraufe"), TEXT("Glimmklee")))
		{
			return EVaelSymbiosis::Mondkuppel;
		}
		if (Is(TEXT("Mondtraufe"), TEXT("Russrose")))
		{
			return EVaelSymbiosis::Mondrose;
		}
		if (Is(TEXT("Mondtraufe"), TEXT("Funkenmohn")))
		{
			return EVaelSymbiosis::Sternenfall;
		}
		if (Is(TEXT("Mondtraufe"), TEXT("Schlotwinde")))
		{
			return EVaelSymbiosis::Mondranken;
		}
		if (Is(TEXT("Feenragwurz"), TEXT("Harzkolben")))
		{
			return EVaelSymbiosis::Pechfeen;
		}
		if (Is(TEXT("Feenragwurz"), TEXT("Mondtraufe")))
		{
			return EVaelSymbiosis::Feentor;
		}
		if (Is(TEXT("Harzkolben"), TEXT("Mondtraufe")))
		{
			return EVaelSymbiosis::Bernsteinschleier;
		}
		if (Is(TEXT("Wachbeere"), TEXT("Wachbeere")))
		{
			return EVaelSymbiosis::Argusstrauch;
		}
		if (Is(TEXT("Wachbeere"), TEXT("Windhalm")))
		{
			return EVaelSymbiosis::Blinzelwind;
		}
		if (Is(TEXT("Wachbeere"), TEXT("Glutdistel")))
		{
			return EVaelSymbiosis::Glutblick;
		}
		if (Is(TEXT("Wachbeere"), TEXT("Taublatt")))
		{
			return EVaelSymbiosis::Traenenauge;
		}
		if (Is(TEXT("Wachbeere"), TEXT("Glimmklee")))
		{
			return EVaelSymbiosis::Leuchtauge;
		}
		if (Is(TEXT("Wachbeere"), TEXT("Russrose")))
		{
			return EVaelSymbiosis::WachendeRose;
		}
		if (Is(TEXT("Wachbeere"), TEXT("Funkenmohn")))
		{
			return EVaelSymbiosis::Schlummerblick;
		}
		if (Is(TEXT("Wachbeere"), TEXT("Schlotwinde")))
		{
			return EVaelSymbiosis::Starrschlinge;
		}
		if (Is(TEXT("Wachbeere"), TEXT("Harzkolben")))
		{
			return EVaelSymbiosis::Bernsteinauge;
		}
		if (Is(TEXT("Wachbeere"), TEXT("Feenragwurz")))
		{
			return EVaelSymbiosis::Irrlicht;
		}
		if (Is(TEXT("Wachbeere"), TEXT("Mondtraufe")))
		{
			return EVaelSymbiosis::Mondauge;
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
		case EVaelSymbiosis::Mohnfeuerwerk:	return NSLOCTEXT("VaelSymbiosis", "Mohnfeuerwerk", "Mohnfeuerwerk");
		case EVaelSymbiosis::Zuendschnur:	return NSLOCTEXT("VaelSymbiosis", "Zuendschnur", "Zündschnur");
		case EVaelSymbiosis::Funkenwirbel:	return NSLOCTEXT("VaelSymbiosis", "Funkenwirbel", "Funkenwirbel");
		case EVaelSymbiosis::Gewittermohn:	return NSLOCTEXT("VaelSymbiosis", "Gewittermohn", "Gewittermohn");
		case EVaelSymbiosis::Glutblase:		return NSLOCTEXT("VaelSymbiosis", "Glutblase", "Glutblase");
		case EVaelSymbiosis::Brandsalbe:	return NSLOCTEXT("VaelSymbiosis", "Brandsalbe", "Brandsalbe");
		case EVaelSymbiosis::Windenzaun:	return NSLOCTEXT("VaelSymbiosis", "Windenzaun", "Windenzaun");
		case EVaelSymbiosis::Wirbelwinde:	return NSLOCTEXT("VaelSymbiosis", "Wirbelwinde", "Wirbelwinde");
		case EVaelSymbiosis::Glutpeitsche:	return NSLOCTEXT("VaelSymbiosis", "Glutpeitsche", "Glutpeitsche");
		case EVaelSymbiosis::Taunetz:		return NSLOCTEXT("VaelSymbiosis", "Taunetz", "Taunetz");
		case EVaelSymbiosis::Laube:			return NSLOCTEXT("VaelSymbiosis", "Laube", "Laube");
		case EVaelSymbiosis::Rankenrettung:	return NSLOCTEXT("VaelSymbiosis", "Rankenrettung", "Rankenrettung");
		case EVaelSymbiosis::Haftmohn:		return NSLOCTEXT("VaelSymbiosis", "Haftmohn", "Haftmohn");
		case EVaelSymbiosis::Feenreigen:	return NSLOCTEXT("VaelSymbiosis", "Feenreigen", "Feenreigen");
		case EVaelSymbiosis::Feenwind:	return NSLOCTEXT("VaelSymbiosis", "Feenwind", "Feenwind");
		case EVaelSymbiosis::Gluehfeen:	return NSLOCTEXT("VaelSymbiosis", "Gluehfeen", "Glühfeen");
		case EVaelSymbiosis::Taufluegel:	return NSLOCTEXT("VaelSymbiosis", "Taufluegel", "Tauflügel");
		case EVaelSymbiosis::Laternenfeen:	return NSLOCTEXT("VaelSymbiosis", "Laternenfeen", "Laternenfeen");
		case EVaelSymbiosis::Rosenboten:	return NSLOCTEXT("VaelSymbiosis", "Rosenboten", "Rosenboten");
		case EVaelSymbiosis::Mohnfeen:	return NSLOCTEXT("VaelSymbiosis", "Mohnfeen", "Mohnfeen");
		case EVaelSymbiosis::Feenfalle:	return NSLOCTEXT("VaelSymbiosis", "Feenfalle", "Feenfalle");
		case EVaelSymbiosis::Pechsumpf:	return NSLOCTEXT("VaelSymbiosis", "Pechsumpf", "Pechsumpf");
		case EVaelSymbiosis::Pechregen:	return NSLOCTEXT("VaelSymbiosis", "Pechregen", "Pechregen");
		case EVaelSymbiosis::Brandharz:	return NSLOCTEXT("VaelSymbiosis", "Brandharz", "Brandharz");
		case EVaelSymbiosis::Bernstein:	return NSLOCTEXT("VaelSymbiosis", "Bernstein", "Bernstein");
		case EVaelSymbiosis::Harzglas:	return NSLOCTEXT("VaelSymbiosis", "Harzglas", "Harzglas");
		case EVaelSymbiosis::Wundharz:	return NSLOCTEXT("VaelSymbiosis", "Wundharz", "Wundharz");
		case EVaelSymbiosis::Pechbombe:	return NSLOCTEXT("VaelSymbiosis", "Pechbombe", "Pechbombe");
		case EVaelSymbiosis::Klebranke:	return NSLOCTEXT("VaelSymbiosis", "Klebranke", "Klebranke");
		case EVaelSymbiosis::Mondhalle:	return NSLOCTEXT("VaelSymbiosis", "Mondhalle", "Mondhalle");
		case EVaelSymbiosis::WehenderSchleier:	return NSLOCTEXT("VaelSymbiosis", "WehenderSchleier", "Wehender Schleier");
		case EVaelSymbiosis::Glutvorhang:	return NSLOCTEXT("VaelSymbiosis", "Glutvorhang", "Glutvorhang");
		case EVaelSymbiosis::Taustrom:	return NSLOCTEXT("VaelSymbiosis", "Taustrom", "Taustrom");
		case EVaelSymbiosis::Mondkuppel:	return NSLOCTEXT("VaelSymbiosis", "Mondkuppel", "Mondkuppel");
		case EVaelSymbiosis::Mondrose:	return NSLOCTEXT("VaelSymbiosis", "Mondrose", "Mondrose");
		case EVaelSymbiosis::Sternenfall:	return NSLOCTEXT("VaelSymbiosis", "Sternenfall", "Sternenfall");
		case EVaelSymbiosis::Mondranken:	return NSLOCTEXT("VaelSymbiosis", "Mondranken", "Mondranken");
		case EVaelSymbiosis::Pechfeen:	return NSLOCTEXT("VaelSymbiosis", "Pechfeen", "Pechfeen");
		case EVaelSymbiosis::Feentor:	return NSLOCTEXT("VaelSymbiosis", "Feentor", "Feentor");
		case EVaelSymbiosis::Bernsteinschleier:	return NSLOCTEXT("VaelSymbiosis", "Bernsteinschleier", "Bernsteinschleier");
		case EVaelSymbiosis::Argusstrauch:	return NSLOCTEXT("VaelSymbiosis", "Argusstrauch", "Argusstrauch");
		case EVaelSymbiosis::Blinzelwind:	return NSLOCTEXT("VaelSymbiosis", "Blinzelwind", "Blinzelwind");
		case EVaelSymbiosis::Glutblick:	return NSLOCTEXT("VaelSymbiosis", "Glutblick", "Glutblick");
		case EVaelSymbiosis::Traenenauge:	return NSLOCTEXT("VaelSymbiosis", "Traenenauge", "Tränenauge");
		case EVaelSymbiosis::Leuchtauge:	return NSLOCTEXT("VaelSymbiosis", "Leuchtauge", "Leuchtauge");
		case EVaelSymbiosis::WachendeRose:	return NSLOCTEXT("VaelSymbiosis", "WachendeRose", "Wachende Rose");
		case EVaelSymbiosis::Schlummerblick:	return NSLOCTEXT("VaelSymbiosis", "Schlummerblick", "Schlummerblick");
		case EVaelSymbiosis::Starrschlinge:	return NSLOCTEXT("VaelSymbiosis", "Starrschlinge", "Starrschlinge");
		case EVaelSymbiosis::Bernsteinauge:	return NSLOCTEXT("VaelSymbiosis", "Bernsteinauge", "Bernsteinauge");
		case EVaelSymbiosis::Irrlicht:	return NSLOCTEXT("VaelSymbiosis", "Irrlicht", "Irrlicht");
		case EVaelSymbiosis::Mondauge:	return NSLOCTEXT("VaelSymbiosis", "Mondauge", "Mondauge");
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
		case EVaelSymbiosis::Mohnfeuerwerk:	return FLinearColor(1.0f, 0.4f, 0.1f);
		case EVaelSymbiosis::Zuendschnur:	return FLinearColor(1.0f, 0.3f, 0.05f);
		case EVaelSymbiosis::Funkenwirbel:	return FLinearColor(1.0f, 0.55f, 0.2f);
		case EVaelSymbiosis::Gewittermohn:	return FLinearColor(0.55f, 0.75f, 1.0f);
		case EVaelSymbiosis::Glutblase:		return FLinearColor(1.0f, 0.65f, 0.25f);
		case EVaelSymbiosis::Brandsalbe:	return FLinearColor(1.0f, 0.7f, 0.35f);
		case EVaelSymbiosis::Windenzaun:	return FLinearColor(1.0f, 0.8f, 0.25f);
		case EVaelSymbiosis::Wirbelwinde:	return FLinearColor(0.85f, 0.95f, 0.45f);
		case EVaelSymbiosis::Glutpeitsche:	return FLinearColor(1.0f, 0.45f, 0.1f);
		case EVaelSymbiosis::Taunetz:		return FLinearColor(0.4f, 0.95f, 0.75f);
		case EVaelSymbiosis::Laube:			return FLinearColor(1.0f, 0.9f, 0.45f);
		case EVaelSymbiosis::Rankenrettung:	return FLinearColor(0.5f, 1.0f, 0.45f);
		case EVaelSymbiosis::Haftmohn:		return FLinearColor(1.0f, 0.55f, 0.15f);
		case EVaelSymbiosis::Feenreigen:	return FLinearColor(0.95f, 0.6f, 0.85f);
		case EVaelSymbiosis::Feenwind:	return FLinearColor(0.75f, 0.95f, 0.85f);
		case EVaelSymbiosis::Gluehfeen:	return FLinearColor(1.0f, 0.55f, 0.25f);
		case EVaelSymbiosis::Taufluegel:	return FLinearColor(0.4f, 0.9f, 0.95f);
		case EVaelSymbiosis::Laternenfeen:	return FLinearColor(1.0f, 0.9f, 0.6f);
		case EVaelSymbiosis::Rosenboten:	return FLinearColor(0.55f, 1.0f, 0.7f);
		case EVaelSymbiosis::Mohnfeen:	return FLinearColor(0.85f, 0.45f, 0.55f);
		case EVaelSymbiosis::Feenfalle:	return FLinearColor(0.95f, 0.85f, 0.4f);
		case EVaelSymbiosis::Pechsumpf:	return FLinearColor(0.7f, 0.3f, 0.05f);
		case EVaelSymbiosis::Pechregen:	return FLinearColor(0.9f, 0.5f, 0.15f);
		case EVaelSymbiosis::Brandharz:	return FLinearColor(1.0f, 0.35f, 0.05f);
		case EVaelSymbiosis::Bernstein:	return FLinearColor(1.0f, 0.65f, 0.15f);
		case EVaelSymbiosis::Harzglas:	return FLinearColor(1.0f, 0.7f, 0.3f);
		case EVaelSymbiosis::Wundharz:	return FLinearColor(0.9f, 0.7f, 0.35f);
		case EVaelSymbiosis::Pechbombe:	return FLinearColor(1.0f, 0.45f, 0.1f);
		case EVaelSymbiosis::Klebranke:	return FLinearColor(0.95f, 0.6f, 0.2f);
		case EVaelSymbiosis::Mondhalle:	return FLinearColor(1.0f, 0.7f, 0.9f);
		case EVaelSymbiosis::WehenderSchleier:	return FLinearColor(0.85f, 0.85f, 1.0f);
		case EVaelSymbiosis::Glutvorhang:	return FLinearColor(1.0f, 0.45f, 0.35f);
		case EVaelSymbiosis::Taustrom:	return FLinearColor(0.55f, 0.85f, 1.0f);
		case EVaelSymbiosis::Mondkuppel:	return FLinearColor(1.0f, 0.85f, 0.8f);
		case EVaelSymbiosis::Mondrose:	return FLinearColor(0.9f, 0.6f, 0.75f);
		case EVaelSymbiosis::Sternenfall:	return FLinearColor(1.0f, 0.7f, 0.45f);
		case EVaelSymbiosis::Mondranken:	return FLinearColor(0.95f, 0.75f, 0.75f);
		case EVaelSymbiosis::Pechfeen:	return FLinearColor(0.85f, 0.5f, 0.3f);
		case EVaelSymbiosis::Feentor:	return FLinearColor(1.0f, 0.65f, 0.95f);
		case EVaelSymbiosis::Bernsteinschleier:	return FLinearColor(1.0f, 0.6f, 0.45f);
		case EVaelSymbiosis::Argusstrauch:	return FLinearColor(0.3f, 0.95f, 0.9f);
		case EVaelSymbiosis::Blinzelwind:	return FLinearColor(0.6f, 1.0f, 0.9f);
		case EVaelSymbiosis::Glutblick:	return FLinearColor(1.0f, 0.45f, 0.15f);
		case EVaelSymbiosis::Traenenauge:	return FLinearColor(0.35f, 0.75f, 1.0f);
		case EVaelSymbiosis::Leuchtauge:	return FLinearColor(1.0f, 0.95f, 0.65f);
		case EVaelSymbiosis::WachendeRose:	return FLinearColor(0.45f, 1.0f, 0.65f);
		case EVaelSymbiosis::Schlummerblick:	return FLinearColor(0.8f, 0.5f, 0.9f);
		case EVaelSymbiosis::Starrschlinge:	return FLinearColor(0.9f, 0.9f, 0.4f);
		case EVaelSymbiosis::Bernsteinauge:	return FLinearColor(1.0f, 0.55f, 0.1f);
		case EVaelSymbiosis::Irrlicht:	return FLinearColor(0.6f, 1.0f, 0.8f);
		case EVaelSymbiosis::Mondauge:	return FLinearColor(0.9f, 0.75f, 1.0f);
		default:							return FLinearColor::White;
		}
	}

	bool HasDome(EVaelSymbiosis Kind)
	{
		return Kind == EVaelSymbiosis::Lichtkuppel || Kind == EVaelSymbiosis::Glutglas || Kind == EVaelSymbiosis::Funkelwind || Kind == EVaelSymbiosis::Tauglocke
			|| Kind == EVaelSymbiosis::Glashaus || Kind == EVaelSymbiosis::Laube;
	}

	bool IsPoppy(EVaelSymbiosis Kind)
	{
		return Kind == EVaelSymbiosis::Mohnfeuerwerk || Kind == EVaelSymbiosis::Zuendschnur || Kind == EVaelSymbiosis::Funkenwirbel || Kind == EVaelSymbiosis::Gewittermohn
			|| Kind == EVaelSymbiosis::Glutblase || Kind == EVaelSymbiosis::Brandsalbe;
	}

	bool HasRose(EVaelSymbiosis Kind)
	{
		return Kind == EVaelSymbiosis::Rosenherz || Kind == EVaelSymbiosis::Bluetensturm || Kind == EVaelSymbiosis::Aschenrose || Kind == EVaelSymbiosis::Rosentau
			|| Kind == EVaelSymbiosis::Glashaus || Kind == EVaelSymbiosis::Rankenrettung;
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
		case EVaelSymbiosis::Mohnfeuerwerk:	return 2.4f;
		case EVaelSymbiosis::Zuendschnur:	return 2.6f;
		case EVaelSymbiosis::Funkenwirbel:	return 4.0f;
		case EVaelSymbiosis::Gewittermohn:	return 2.6f;
		case EVaelSymbiosis::Glutblase:		return 3.5f;
		case EVaelSymbiosis::Brandsalbe:	return 6.0f;
		case EVaelSymbiosis::Windenzaun:	return 7.0f;
		case EVaelSymbiosis::Wirbelwinde:	return 3.5f;
		case EVaelSymbiosis::Glutpeitsche:	return 3.0f;
		case EVaelSymbiosis::Taunetz:		return 4.0f;
		case EVaelSymbiosis::Laube:			return 4.0f;
		case EVaelSymbiosis::Rankenrettung:	return 4.0f;
		case EVaelSymbiosis::Haftmohn:		return 3.0f;
		case EVaelSymbiosis::Feenreigen:	return 6.0f;
		case EVaelSymbiosis::Feenwind:	return 6.0f;
		case EVaelSymbiosis::Gluehfeen:	return 6.0f;
		case EVaelSymbiosis::Taufluegel:	return 6.0f;
		case EVaelSymbiosis::Laternenfeen:	return 7.0f;
		case EVaelSymbiosis::Rosenboten:	return 6.0f;
		case EVaelSymbiosis::Mohnfeen:	return 7.0f;
		case EVaelSymbiosis::Feenfalle:	return 7.0f;
		case EVaelSymbiosis::Pechsumpf:	return 3.5f;
		case EVaelSymbiosis::Pechregen:	return 3.0f;
		case EVaelSymbiosis::Brandharz:	return 3.5f;
		case EVaelSymbiosis::Bernstein:	return 4.0f;
		case EVaelSymbiosis::Harzglas:	return 5.0f;
		case EVaelSymbiosis::Wundharz:	return 5.0f;
		case EVaelSymbiosis::Pechbombe:	return 3.5f;
		case EVaelSymbiosis::Klebranke:	return 4.0f;
		case EVaelSymbiosis::Mondhalle:	return 10.0f;
		case EVaelSymbiosis::WehenderSchleier:	return 10.0f;
		case EVaelSymbiosis::Glutvorhang:	return 10.0f;
		case EVaelSymbiosis::Taustrom:	return 10.0f;
		case EVaelSymbiosis::Mondkuppel:	return 11.0f;
		case EVaelSymbiosis::Mondrose:	return 10.0f;
		case EVaelSymbiosis::Sternenfall:	return 10.0f;
		case EVaelSymbiosis::Mondranken:	return 10.0f;
		case EVaelSymbiosis::Pechfeen:	return 6.0f;
		case EVaelSymbiosis::Feentor:	return 10.0f;
		case EVaelSymbiosis::Bernsteinschleier:	return 12.0f;
		case EVaelSymbiosis::Argusstrauch:	return 6.0f;
		case EVaelSymbiosis::Blinzelwind:	return 6.0f;
		case EVaelSymbiosis::Glutblick:	return 6.0f;
		case EVaelSymbiosis::Traenenauge:	return 6.0f;
		case EVaelSymbiosis::Leuchtauge:	return 6.5f;
		case EVaelSymbiosis::WachendeRose:	return 6.5f;
		case EVaelSymbiosis::Schlummerblick:	return 7.0f;
		case EVaelSymbiosis::Starrschlinge:	return 7.0f;
		case EVaelSymbiosis::Bernsteinauge:	return 6.0f;
		case EVaelSymbiosis::Irrlicht:	return 7.0f;
		case EVaelSymbiosis::Mondauge:	return 10.0f;
		default:							return 1.4f;
		}
	}

	void Fire(APawn* Instigator, EVaelSymbiosis Kind, const UVaelPlantArmData* A, const UVaelPlantArmData* B, const FVector& Origin, const FVector& Direction, float Power, float Charge, bool bShadow, float Reach, float VeilScale)
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
		case EVaelSymbiosis::Mohnfeuerwerk:
		case EVaelSymbiosis::Zuendschnur:
		case EVaelSymbiosis::Funkenwirbel:
		case EVaelSymbiosis::Gewittermohn:
		case EVaelSymbiosis::Glutblase:
		case EVaelSymbiosis::Brandsalbe:
		{
			// Every pair of the Funkenmohn throws in an arc: to where the tower aims, else to the enemy ahead or far ahead
			const UVaelPlantArmData* Poppy = Pick(TEXT("Funkenmohn"));
			const UVaelPlantArmData* Other = Poppy == A ? B : A;
			const FVector Flat = Ahead.GetSafeNormal2D().IsNearlyZero() ? Instigator->GetActorForwardVector() : Ahead.GetSafeNormal2D();
			FVector Target = Origin + Flat * (Reach > 0.0f ? Reach : FMath::Lerp(800.0f, 1200.0f, Charge));
			if (Reach <= 0.0f)
			{
				if (const AActor* Enemy = FindTarget(Instigator, Origin + Flat * 900.0f, 550.0f))
				{
					Target = Enemy->GetActorLocation();
				}
			}
			const float Wide = FMath::Sqrt(Grown);
			const bool bTower = Reach > 0.0f;

			FVaelPoppyThrow Throw;
			Throw.Size = Poppy->MaxCapsuleSize * 1.3f;
			Throw.Radius = Poppy->MaxCapsuleBurstRadius * Wide;
			Throw.Damage = Poppy->MaxCapsuleDamage * Power * Grown;
			Throw.Fuse = Poppy->FuseTime;
			Throw.PatchSeconds = Poppy->PatchSeconds;
			Throw.PatchDamagePerSecond = Poppy->PatchDamagePerSecond * Power;
			switch (Kind)
			{
			case EVaelSymbiosis::Mohnfeuerwerk:
				if (bTower)
				{
					// A salvo: three capsules one after the other around the target
					for (int32 Index = 0; Index < 3; ++Index)
					{
						FVaelPoppyThrow Shell = Throw;
						Shell.Burst = EVaelPoppyBurst::Sparks;
						Shell.Size = Poppy->CapsuleSize * 1.2f;
						Shell.Radius = Poppy->CapsuleBurstRadius * 1.2f;
						Shell.Damage = Poppy->CapsuleDamage * 1.3f * Power;
						Shell.Delay = Index * 0.22f;
						const FVector Spot = Target + (Index == 0 ? FVector::ZeroVector : FVector(FMath::FRandRange(-1.0f, 1.0f), FMath::FRandRange(-1.0f, 1.0f), 0.0f) * 160.0f);
						AVaelPoppyCapsule::Throw(Instigator, Poppy, Origin, Spot, Shell, bShadow);
					}
					return;
				}
				// One giant capsule that bursts into many small ones
				Throw.Burst = EVaelPoppyBurst::Firework;
				Throw.Size = Poppy->MaxCapsuleSize * 1.8f;
				Throw.Scatter = Charge > 0.5f ? 10 : 7;
				Throw.Radius = Poppy->CapsuleBurstRadius * Wide;
				break;
			case EVaelSymbiosis::Zuendschnur:
			{
				// A row of poppies sprouting out of the ground from her to the target, bursting one after the other
				const FVector From = Origin + Flat * 120.0f;
				const float Length = FVector::Dist2D(From, Target);
				const int32 Links = FMath::Clamp(FMath::RoundToInt32(Length / 110.0f), 3, Charge > 0.5f ? 12 : 8);
				for (int32 Index = 0; Index < Links; ++Index)
				{
					FVaelPoppyThrow Link = Throw;
					Link.Burst = EVaelPoppyBurst::Fuse;
					Link.Size = Poppy->CapsuleSize;
					Link.Radius = Poppy->CapsuleBurstRadius * 0.9f * Wide;
					Link.Damage = Poppy->CapsuleDamage * 0.8f * Power * Grown;
					Link.Fuse = 0.25f;
					Link.Delay = Index * 0.12f;
					Link.PatchSeconds = Index % 2 == 0 ? Poppy->PatchSeconds * 0.75f : 0.0f;
					const FVector Spot = FMath::Lerp(From, Target, (Index + 1.0f) / Links) + FVector(FMath::FRandRange(-1.0f, 1.0f), FMath::FRandRange(-1.0f, 1.0f), 0.0f) * 20.0f;
					AVaelPoppyCapsule::Throw(Instigator, Poppy, Spot, Spot, Link, bShadow);
				}
				return;
			}
			case EVaelSymbiosis::Funkenwirbel:
				Throw.Burst = EVaelPoppyBurst::Whirl;
				Throw.Radius = Poppy->MaxCapsuleBurstRadius * 0.9f * Wide;
				Throw.Linger = 3.0f + 1.5f * Charge;
				break;
			case EVaelSymbiosis::Gewittermohn:
				Throw.Burst = EVaelPoppyBurst::Storm;
				Throw.Radius = Poppy->CapsuleBurstRadius * 1.3f * Wide;
				Throw.Damage = Poppy->MaxCapsuleDamage * 0.8f * Power * Grown;
				Throw.Linger = 0.8f;
				break;
			case EVaelSymbiosis::Glutblase:
				Throw.Burst = EVaelPoppyBurst::Bubble;
				Throw.Radius = 200.0f * Wide;
				Throw.Linger = 1.5f;
				Throw.Partner = Other;
				break;
			default:
				Throw.Burst = EVaelPoppyBurst::Salve;
				Throw.Radius = 260.0f * Wide;
				Throw.Damage = Poppy->CapsuleDamage * Power;
				Throw.Linger = 5.0f;
				break;
			}
			AVaelPoppyCapsule::Throw(Instigator, Poppy, Origin, Target, Throw, bShadow);
			break;
		}
		case EVaelSymbiosis::Windenzaun:
		case EVaelSymbiosis::Wirbelwinde:
		case EVaelSymbiosis::Glutpeitsche:
		case EVaelSymbiosis::Taunetz:
		case EVaelSymbiosis::Laube:
		case EVaelSymbiosis::Rankenrettung:
		case EVaelSymbiosis::Haftmohn:
			FireBindweed(Instigator, Kind, Pick(TEXT("Schlotwinde")), A == Pick(TEXT("Schlotwinde")) ? B : A, Origin, Ahead, Power * Grown, Charge, bShadow, Reach);
			break;

		// --- Feenragwurz (Boris 2026-11-03): a flight of fairies lures at the place and strews Feenstaub, the partner adds its touch
		case EVaelSymbiosis::Feenreigen:
		case EVaelSymbiosis::Feenwind:
		case EVaelSymbiosis::Gluehfeen:
		case EVaelSymbiosis::Taufluegel:
		case EVaelSymbiosis::Laternenfeen:
		case EVaelSymbiosis::Rosenboten:
		case EVaelSymbiosis::Mohnfeen:
		case EVaelSymbiosis::Feenfalle:
		case EVaelSymbiosis::Pechfeen:
		case EVaelSymbiosis::Feentor:
		{
			const UVaelPlantArmData* Fairy = Pick(TEXT("Feenragwurz"));
			const UVaelPlantArmData* Other = Fairy == A ? B : A;
			const FVector Place = SymbiosisGroundAhead(Instigator, Origin, Ahead, Reach, FMath::Lerp(650.0f, 900.0f, Charge));
			float Radius = Fairy->FairyLureRadius * FMath::Sqrt(Grown);
			float Seconds = Fairy->FairyLureSeconds * Grown;
			TFunction<void(APawn*, AVaelCreature*)> Touch;
			switch (Kind)
			{
			case EVaelSymbiosis::Feenreigen:
				// The fairies dance in a ring and lure everything far around at once
				Radius *= 1.4f;
				Seconds *= 1.3f;
				break;
			case EVaelSymbiosis::Feenwind:
				// They ride on gusts and push the lured away
				Touch = [Place](APawn*, AVaelCreature* Enemy)
				{
					if (!Enemy->IsHeavy())
					{
						Enemy->LaunchCharacter((Enemy->GetActorLocation() - Place).GetSafeNormal2D() * 650.0f + FVector(0.0f, 0.0f, 250.0f), true, true);
					}
				};
				break;
			case EVaelSymbiosis::Gluehfeen:
				Touch = [Power](APawn* Gardener, AVaelCreature* Enemy) { UVaelCombatStatics::ApplyStatus(Gardener, Enemy, EVaelStatus::Burning, 3.0f, 4.0f * Power); };
				break;
			case EVaelSymbiosis::Taufluegel:
				Touch = [](APawn* Gardener, AVaelCreature* Enemy) { UVaelCombatStatics::ApplyStatus(Gardener, Enemy, EVaelStatus::Wet, 6.0f); };
				break;
			case EVaelSymbiosis::Laternenfeen:
				// They shine like lanterns: the lured are dazzled, and at the hands a little glass dome holds off fire
				Touch = [](APawn* Gardener, AVaelCreature* Enemy) { UVaelCombatStatics::ApplyStatus(Gardener, Enemy, EVaelStatus::Confused, 1.5f); };
				if (Reach <= 0.0f)
				{
					SymbiosisDome(Instigator, Other, 260.0f, 6.0f, bShadow);
				}
				break;
			case EVaelSymbiosis::Rosenboten:
			{
				// They carry the rose to the fallen: allies down around her get up sooner
				for (FConstPlayerControllerIterator It = Instigator->GetWorld()->GetPlayerControllerIterator(); It; ++It)
				{
					AVaelCharacter* Player = It->Get() != nullptr ? It->Get()->GetPawn<AVaelCharacter>() : nullptr;
					if (Player != nullptr && Player->IsDowned() && FVector::Dist2D(Player->GetActorLocation(), Instigator->GetActorLocation()) < 1000.0f)
					{
						Player->AddReviveProgress(1.0f);
					}
				}
				break;
			}
			case EVaelSymbiosis::Mohnfeen:
			{
				// They strew sleep pollen over the lured: a sleep cloud of the Funkenmohn bursts at the place
				FVaelPoppyThrow Throw;
				Throw.Burst = EVaelPoppyBurst::Sleep;
				Throw.Size = Other->CapsuleSize;
				Throw.Radius = Radius * 0.7f;
				Throw.Damage = 0.0f;
				Throw.Fuse = 0.8f;
				Throw.Linger = 4.0f;
				AVaelPoppyCapsule::Throw(Instigator, Other, Origin, Place, Throw, bShadow);
				break;
			}
			case EVaelSymbiosis::Feenfalle:
				// The lured walk into a noose of bindweed and hang there
				Touch = [Other, Place, Power](APawn* Gardener, AVaelCreature* Enemy)
				{
					FVaelLassoSpec Spec;
					Spec.HoldSeconds = Other->SnareHold;
					Spec.bOverGround = true;
					Spec.Power = Power;
					Spec.Color = GetColor(EVaelSymbiosis::Feenfalle);
					AVaelBindweedLasso::Throw(Gardener, Other, Place + FVector(0.0f, 0.0f, 5.0f), Enemy, Spec, false);
				};
				break;
			case EVaelSymbiosis::Pechfeen:
				Touch = [](APawn* Gardener, AVaelCreature* Enemy) { UVaelCombatStatics::ApplyStatus(Gardener, Enemy, EVaelStatus::Pitched, 6.0f); };
				break;
			case EVaelSymbiosis::Feentor:
				// Through the gate of the veil the fairies lure twice as long
				Seconds *= 2.0f;
				SymbiosisVeil(Instigator, Other, Kind, Fairy, Origin, Ahead, Reach, VeilScale);
				break;
			default:
				break;
			}
			SymbiosisFairies(Instigator, Fairy, Origin, Place, Radius, Seconds, bShadow, Touch);
			break;
		}

		// --- Harzkolben: pitch balls; the partner sets them alight, spreads, hardens or carries them
		case EVaelSymbiosis::Pechsumpf:
		case EVaelSymbiosis::Pechregen:
		case EVaelSymbiosis::Brandharz:
		case EVaelSymbiosis::Bernstein:
		case EVaelSymbiosis::Harzglas:
		case EVaelSymbiosis::Wundharz:
		case EVaelSymbiosis::Pechbombe:
		case EVaelSymbiosis::Klebranke:
		{
			const UVaelPlantArmData* Pitch = Pick(TEXT("Harzkolben"));
			const UVaelPlantArmData* Other = Pitch == A ? B : A;
			switch (Kind)
			{
			case EVaelSymbiosis::Pechsumpf:
				// Three big balls: a wide sticky swamp
				SymbiosisPitch(Instigator, Pitch, Origin, Ahead, 3, 18.0f, 1.6f * FMath::Sqrt(Grown), Power * Grown, bShadow, Reach);
				break;
			case EVaelSymbiosis::Pechregen:
				// The wind scatters a rain of small drops far over the field
				SymbiosisPitch(Instigator, Pitch, Origin, Ahead, Charge > 0.5f ? 12 : 8, Charge > 0.5f ? 60.0f : 45.0f, 0.6f, Power * Grown * 0.6f, bShadow, Reach);
				break;
			case EVaelSymbiosis::Brandharz:
			{
				// Pitch first, then the burning seed into it: the pitch burns twice as hard
				SymbiosisPitch(Instigator, Pitch, Origin, Ahead, Charge > 0.5f ? 3 : 1, 14.0f, 1.3f, Power * Grown, bShadow, Reach);
				TWeakObjectPtr<APawn> WeakInstigator = Instigator;
				FTimerHandle Timer;
				Instigator->GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([WeakInstigator, Other, Origin, Ahead, Power, Grown, bShadow, Reach]()
				{
					if (WeakInstigator.IsValid())
					{
						FireShot(WeakInstigator.Get(), Other, Origin, Ahead, Power * Grown, bShadow, 1.4f, 1.0f, Reach);
					}
				}), 0.45f, false);
				break;
			}
			case EVaelSymbiosis::Bernstein:
			{
				// Wet pitch hardens: where the ball bursts the enemies are shut in amber for a moment
				FVaelSpellHit Hit;
				Hit.Damage = Pitch->ShotDamage * 1.5f * Power * Grown;
				Hit.Element = EVaelElement::Water;
				Hit.Status = EVaelStatus::Frozen;
				Hit.StatusDuration = 1.5f + Charge;
				FVaelSpellHit Burst = Hit;
				Burst.Damage *= 0.5f;
				if (AVaelSpellProjectile* Shot = SymbiosisProjectile(Instigator, Pitch, Origin, Ahead, Hit, Pitch->ShotRadius * 1.6f, 0, Color, true))
				{
					Shot->SetExplosion(Burst, 200.0f * FMath::Sqrt(Grown));
					Shot->FinishSpawning(FTransform(Ahead.Rotation(), Origin));
				}
				break;
			}
			case EVaelSymbiosis::Harzglas:
				// A dome of amber glass at the hands, enemy fire sticks in it; the pitch flies on
				SymbiosisPitch(Instigator, Pitch, Origin, Ahead, 2, 12.0f, 1.0f, Power * Grown, bShadow, Reach);
				if (Reach <= 0.0f)
				{
					SymbiosisDome(Instigator, Other, 300.0f, 7.0f, bShadow);
				}
				break;
			case EVaelSymbiosis::Wundharz:
			{
				// The pitch seals the wounds of the allies around her: a shield, no healing
				SymbiosisPitch(Instigator, Pitch, Origin, Ahead, 1, 0.0f, 1.2f, Power * Grown, bShadow, Reach);
				for (FConstPlayerControllerIterator It = Instigator->GetWorld()->GetPlayerControllerIterator(); It; ++It)
				{
					AVaelCharacter* Player = It->Get() != nullptr ? It->Get()->GetPawn<AVaelCharacter>() : nullptr;
					if (Player != nullptr && FVector::Dist2D(Player->GetActorLocation(), Origin) < 800.0f)
					{
						Player->AddHeartShield(20.0f * Power * Grown, 8.0f);
					}
				}
				break;
			}
			case EVaelSymbiosis::Pechbombe:
			{
				// A capsule full of pitch: it bursts sticky and burns
				const FVector Place = SymbiosisGroundAhead(Instigator, Origin, Ahead, Reach, FMath::Lerp(800.0f, 1100.0f, Charge));
				FVaelPoppyThrow Throw;
				Throw.Size = Other->MaxCapsuleSize;
				Throw.Radius = Other->MaxCapsuleBurstRadius * FMath::Sqrt(Grown);
				Throw.Damage = Other->MaxCapsuleDamage * Power * Grown;
				Throw.Fuse = Other->FuseTime;
				Throw.PatchSeconds = Other->PatchSeconds * 1.5f;
				Throw.PatchDamagePerSecond = Other->PatchDamagePerSecond * Power;
				AVaelPoppyCapsule::Throw(Instigator, Other, Origin, Place, Throw, bShadow);
				SymbiosisPitch(Instigator, Pitch, Origin, (Place - Origin).GetSafeNormal(), 1, 0.0f, 1.3f, Power * Grown, bShadow, FVector::Dist2D(Place, Origin));
				break;
			}
			case EVaelSymbiosis::Klebranke:
			{
				// A sticky lasso: it catches several and pitches them
				FVaelLassoSpec Spec;
				Spec.HoldSeconds = Other->SnareHold;
				Spec.Power = Power;
				Spec.Color = Color;
				for (AActor* Caught : AVaelBindweedLasso::FindTargets(Instigator, Origin, Ahead.GetSafeNormal2D(), Reach > 0.0f ? Reach + 200.0f : 900.0f, 40.0f, Charge > 0.5f ? 4 : 3))
				{
					AVaelBindweedLasso::Throw(Instigator, Other, Origin, Caught, Spec, bShadow);
					UVaelCombatStatics::ApplyStatus(Instigator, Caught, EVaelStatus::Pitched, 6.0f);
				}
				break;
			}
			default:
				break;
			}
			break;
		}


		// --- Wachbeere (Boris 2026-11-04): the eye berry lands at the place and looks around, the partner adds its touch
		case EVaelSymbiosis::Argusstrauch:
		case EVaelSymbiosis::Blinzelwind:
		case EVaelSymbiosis::Glutblick:
		case EVaelSymbiosis::Traenenauge:
		case EVaelSymbiosis::Leuchtauge:
		case EVaelSymbiosis::WachendeRose:
		case EVaelSymbiosis::Schlummerblick:
		case EVaelSymbiosis::Starrschlinge:
		case EVaelSymbiosis::Bernsteinauge:
		case EVaelSymbiosis::Irrlicht:
		case EVaelSymbiosis::Mondauge:
		{
			const UVaelPlantArmData* Berry = Pick(TEXT("Wachbeere"));
			const UVaelPlantArmData* Other = Berry == A ? B : A;
			const FVector Place = SymbiosisGroundAhead(Instigator, Origin, Ahead, Reach, FMath::Lerp(600.0f, 850.0f, Charge));
			const FVector Flat = Ahead.GetSafeNormal2D().IsNearlyZero() ? Instigator->GetActorForwardVector() : Ahead.GetSafeNormal2D();
			const auto ThrowEye = [&](const FVector& At, float RadiusScale, float SecondsScale, const FVector& Roll)
			{
				if (AVaelEyeBerry* Eye = AVaelEyeBerry::Throw(Instigator, Berry, Origin, At, bShadow))
				{
					Eye->SetPair(Kind, Other, RadiusScale * FMath::Sqrt(Grown), SecondsScale, Roll);
				}
			};
			switch (Kind)
			{
			case EVaelSymbiosis::Argusstrauch:
			{
				// Many eyes in a ring, seeing farther and longer
				const FVector Side = FVector::CrossProduct(FVector::UpVector, Flat);
				for (const FVector& Offset : { FVector::ZeroVector, Side * 260.0f + Flat * 120.0f, Side * -260.0f + Flat * 120.0f })
				{
					ThrowEye(Place + Offset, 1.15f, 1.4f, FVector::ZeroVector);
				}
				break;
			}
			case EVaelSymbiosis::Blinzelwind:
				// The wind rolls the eye on over the field, it looks round wherever it passes
				ThrowEye(Place, 0.9f, 1.2f, Flat * 160.0f);
				break;
			case EVaelSymbiosis::Leuchtauge:
				ThrowEye(Place, 1.0f, 1.0f, FVector::ZeroVector);
				if (Reach <= 0.0f)
				{
					SymbiosisDome(Instigator, Other, 260.0f, 6.0f, bShadow);
				}
				break;
			case EVaelSymbiosis::Irrlicht:
				// The fairies lure the enemies to the eye, which then confuses them
				ThrowEye(Place, 1.0f, 1.2f, FVector::ZeroVector);
				SymbiosisFairies(Instigator, Other, Origin, Place, Other->FairyLureRadius * 1.2f, Other->FairyLureSeconds, bShadow);
				break;
			case EVaelSymbiosis::Mondauge:
				// The veil gets an eye: who wants through is confused and thrown back
				SymbiosisVeil(Instigator, Other, Kind, Berry, Origin, Ahead, Reach, VeilScale);
				ThrowEye(Place, 0.9f, 1.4f, FVector::ZeroVector);
				break;
			default:
				ThrowEye(Place, 1.0f, 1.0f, FVector::ZeroVector);
				break;
			}
			break;
		}

		// --- Mondtraufe: the veil, changed by the partner (AVaelMoonVeil::SetPair)
		case EVaelSymbiosis::Mondhalle:
		{
			// Two veils along the way form a hall; between them the allies are cleansed again and again
			const FVector Flat = Ahead.GetSafeNormal2D().IsNearlyZero() ? Instigator->GetActorForwardVector() : Ahead.GetSafeNormal2D();
			const FVector Side = FVector::CrossProduct(FVector::UpVector, Flat);
			const FVector Middle = (Reach > 0.0f ? Origin : Instigator->GetActorLocation()) + Flat * 260.0f;
			for (const float Sign : { 1.0f, -1.0f })
			{
				const FVector At = Middle + Side * Sign * 230.0f * VeilScale;
				const FVector Facing = Side * Sign;
				SymbiosisVeil(Instigator, A, Kind, B, Origin, Ahead, Reach, VeilScale, 1.0f, &At, &Facing);
			}
			break;
		}
		case EVaelSymbiosis::Mondkuppel:
		{
			// The veil closes into a small round dome of six, shielding her on all sides
			const UVaelPlantArmData* Moon = Pick(TEXT("Mondtraufe"));
			const FVector Middle = Reach > 0.0f ? Origin : Instigator->GetActorLocation();
			for (int32 Index = 0; Index < 6; ++Index)
			{
				const FVector Out = FRotator(0.0f, Index * 60.0f, 0.0f).Vector();
				const FVector At = Middle + Out * 290.0f * VeilScale;
				SymbiosisVeil(Instigator, Moon, Kind, Moon == A ? B : A, Origin, Ahead, Reach, 0.75f * VeilScale, 0.8f, &At, &Out);
			}
			break;
		}
		case EVaelSymbiosis::WehenderSchleier:
		case EVaelSymbiosis::Glutvorhang:
		case EVaelSymbiosis::Taustrom:
		case EVaelSymbiosis::Mondrose:
		case EVaelSymbiosis::Sternenfall:
		case EVaelSymbiosis::Mondranken:
		case EVaelSymbiosis::Bernsteinschleier:
		{
			const UVaelPlantArmData* Moon = Pick(TEXT("Mondtraufe"));
			SymbiosisVeil(Instigator, Moon, Kind, Moon == A ? B : A, Origin, Ahead, Reach, FMath::Sqrt(Grown) * VeilScale, Kind == EVaelSymbiosis::Bernsteinschleier ? 1.5f : 1.0f);
			break;
		}
		default:
			break;
		}
	}

	void FireBindweed(APawn* Instigator, EVaelSymbiosis Kind, const UVaelPlantArmData* Vine, const UVaelPlantArmData* Other, const FVector& Origin, const FVector& Ahead, float Power, float Charge, bool bShadow, float Reach)
	{
		UWorld* World = Instigator->GetWorld();
		const bool bTower = Reach > 0.0f;
		FVaelLassoSpec Spec;
		Spec.Power = Power;
		Spec.Color = GetColor(Kind);
		Spec.DamagePerSecond = bShadow ? Vine->StrangleDamagePerSecond : 0.0f;
		Spec.DamageGrowth = bShadow ? Vine->StrangleGrowth : 0.0f;
		const auto LassoAll = [&](const TArray<AActor*>& Targets)
		{
			for (AActor* Target : Targets)
			{
				AVaelBindweedLasso::Throw(Instigator, Vine, Origin, Target, Spec, bShadow);
			}
		};

		switch (Kind)
		{
		case EVaelSymbiosis::Windenzaun:
		{
			// A hedge of bindweed across the way, a few steps ahead, on the ground
			FHitResult Ground;
			const FVector Ahead2 = Origin + Ahead.GetSafeNormal2D() * (bTower ? FMath::Min(Reach * 0.5f, 400.0f) : 250.0f);
			const FVector Foot = VaelGround::TraceGround(World, Ahead2 + FVector(0.0f, 0.0f, 300.0f), Ahead2 - FVector(0.0f, 0.0f, 1500.0f), Ground, Instigator)
				? Ground.ImpactPoint : Instigator->GetActorLocation() + Ahead.GetSafeNormal2D() * 250.0f;
			AVaelVineWall::Raise(World, Foot, Ahead.GetSafeNormal2D().Rotation().Yaw, 6.0f, 1.0f + 0.4f * Charge, Instigator, Vine, bShadow);
			break;
		}
		case EVaelSymbiosis::Wirbelwinde:
			// The vines catch everyone around, whirl them a moment and fling them away
			Spec.HoldSeconds = 0.6f;
			Spec.OnEnd = EVaelLassoEnd::Fling;
			LassoAll(AVaelBindweedLasso::FindTargets(Instigator, Origin, FVector::ZeroVector, 450.0f + 150.0f * Charge, 180.0f, 5));
			break;
		case EVaelSymbiosis::Glutpeitsche:
		{
			// A burning lash in a wide fan; the nearest one is wrapped and keeps burning while it is held
			FVaelSpellHit Hit;
			Hit.Damage = Vine->WhipDamage * 1.4f * Power;
			Hit.Element = EVaelElement::Fire;
			Hit.Knockback = Vine->WhipKnockback * 0.6f;
			Hit.Status = EVaelStatus::Burning;
			Hit.StatusDuration = 3.0f;
			const TArray<AActor*> Lashed = AVaelBindweedLasso::FindTargets(Instigator, Origin, Ahead, Vine->WhipReach * 1.4f + (bTower ? Reach * 0.5f : 0.0f), 55.0f, 10);
			for (AActor* Target : Lashed)
			{
				UVaelCombatStatics::ApplySpellHit(Instigator, Target, Hit, (Target->GetActorLocation() - Origin).GetSafeNormal2D());
			}
			Spec.HoldSeconds = 2.0f;
			Spec.Status = EVaelStatus::Burning;
			LassoAll(AVaelBindweedLasso::FindTargets(Instigator, Origin, Ahead, Vine->LassoRange, Vine->LassoHalfAngle, 1));
			MuzzlePuff(Instigator, Other, Origin, bShadow, 2.0f);
			break;
		}
		case EVaelSymbiosis::Taunetz:
		{
			// A wet net thrown to where the enemies stand: it catches all of them there and soaks them
			const TArray<AActor*> First = AVaelBindweedLasso::FindTargets(Instigator, Origin, Ahead, Vine->LassoRange, 45.0f, 1);
			if (First.IsEmpty())
			{
				break;
			}
			Spec.HoldSeconds = 2.5f;
			Spec.Status = EVaelStatus::Wet;
			LassoAll(AVaelBindweedLasso::FindTargets(Instigator, First[0]->GetActorLocation(), FVector::ZeroVector, 250.0f + 100.0f * Charge, 180.0f, 6));
			break;
		}
		case EVaelSymbiosis::Laube:
			// A glittering noose catches up to three ahead and blinds them
			Spec.HoldSeconds = 2.0f;
			Spec.BlindSeconds = 2.0f;
			LassoAll(AVaelBindweedLasso::FindTargets(Instigator, Origin, Ahead, Vine->LassoRange * 0.8f, 40.0f, 3));
			break;
		case EVaelSymbiosis::Rankenrettung:
		{
			// The vines grab the most hurt ally, pull them to her and heal them; alone she heals herself a little
			AVaelCharacter* Gardener = Cast<AVaelCharacter>(Instigator);
			AVaelCharacter* Ally = Gardener != nullptr ? VaelRoseCare::FindHurtAlly(Gardener, Origin, 1100.0f, false, true) : nullptr;
			if (Ally == nullptr)
			{
				VaelRoseCare::HealAround(Instigator, Instigator->GetActorLocation(), 60.0f, 0.1f * Power);
				break;
			}
			Spec.HoldSeconds = 0.2f;
			Spec.OnEnd = bTower ? EVaelLassoEnd::Release : EVaelLassoEnd::Yank;
			Spec.Color = FLinearColor(0.4f, 1.0f, 0.5f);
			AVaelBindweedLasso::Throw(Instigator, Vine, Origin, Ally, Spec, bShadow);
			VaelRoseCare::HealAround(Instigator, Ally->GetActorLocation(), 60.0f, 0.15f * Power);
			if (Ally->IsDowned())
			{
				Ally->AddReviveProgress(0.5f);
			}
			break;
		}
		case EVaelSymbiosis::Haftmohn:
			// The vine wraps the enemy, a poppy capsule blooms on it and bursts a moment later
			Spec.HoldSeconds = 1.0f;
			Spec.OnEnd = EVaelLassoEnd::Bloom;
			Spec.BloomPlant = Other;
			LassoAll(AVaelBindweedLasso::FindTargets(Instigator, Origin, Ahead, Vine->LassoRange, 45.0f, bTower ? 1 : 4));
			break;
		default:
			break;
		}
	}

	void ThrowCapsule(APawn* Instigator, const UVaelPlantArmData* Plant, const FVector& Muzzle, const FVector& Target, float Charge, float Power, bool bShadow, int32 Split, float SizeScale)
	{
		if (Plant == nullptr)
		{
			return;
		}
		FVaelPoppyThrow Throw;
		Throw.Burst = EVaelPoppyBurst::Sparks;
		Throw.Size = FMath::Lerp(Plant->CapsuleSize, Plant->MaxCapsuleSize, Charge) * SizeScale;
		Throw.Radius = FMath::Lerp(Plant->CapsuleBurstRadius, Plant->MaxCapsuleBurstRadius, Charge) * FMath::Sqrt(SizeScale);
		Throw.Damage = FMath::Lerp(Plant->CapsuleDamage, Plant->MaxCapsuleDamage, Charge) * Power;
		Throw.Fuse = Plant->FuseTime;
		Throw.PatchSeconds = Plant->PatchSeconds;
		Throw.PatchDamagePerSecond = Plant->PatchDamagePerSecond * Power;
		Throw.Split = Split;
		Throw.Drowsy = Plant->DrowsySeconds;
		Throw.Sleep = Plant->SleepSeconds * FMath::Lerp(1.0f, 1.4f, Charge);
		AVaelPoppyCapsule::Throw(Instigator, Plant, Muzzle, Target, Throw, bShadow);
	}

	bool FireShot(APawn* Instigator, const UVaelPlantArmData* Plant, const FVector& Muzzle, const FVector& Direction, float Power, bool bShadow, float SizeScale, float DamageScale, float Reach)
	{
		if (Instigator == nullptr || Plant == nullptr)
		{
			return false;
		}

		// The Feenragwurz shoots nothing, her fairies do the work (no tower of her yet)
		if (Plant->bFairies)
		{
			return false;
		}

		// The Schlotwinde catches the nearest enemy ahead with a lasso and holds it a while
		if (Plant->bLasso)
		{
			const TArray<AActor*> Targets = AVaelBindweedLasso::FindTargets(Instigator, Muzzle, Direction, Reach > 0.0f ? Reach + 100.0f : Plant->LassoRange, Plant->LassoHalfAngle, 1);
			if (Targets.IsEmpty())
			{
				return false;
			}
			FVaelLassoSpec Spec;
			Spec.HoldSeconds = Plant->SnareHold;
			Spec.DamagePerSecond = bShadow ? Plant->StrangleDamagePerSecond : 0.0f;
			Spec.DamageGrowth = bShadow ? Plant->StrangleGrowth : 0.0f;
			Spec.Power = Power * DamageScale;
			Spec.Color = bShadow ? Plant->StrangleColor : Plant->PollenColor;
			return AVaelBindweedLasso::Throw(Instigator, Plant, Muzzle, Targets[0], Spec, bShadow) != nullptr;
		}

		// The Funkenmohn throws its capsule in an arc to the target instead of shooting straight
		if (Plant->bMortar)
		{
			const FVector Flat = Direction.GetSafeNormal2D();
			ThrowCapsule(Instigator, Plant, Muzzle, Muzzle + Flat * (Reach > 0.0f ? Reach : Plant->MinThrow), 0.3f, Power * DamageScale, bShadow, 0, SizeScale);
			return true;
		}

		FVaelSpellHit Hit;
		Hit.Damage = Plant->ShotDamage * Power * DamageScale;
		Hit.Element = Plant->ShotElement;
		Hit.Status = Plant->ShotStatus;
		Hit.StatusDuration = Plant->ShotStatusDuration;
		// A small push, so a hit is felt; the Windhalm pushes harder (SymbiosisProjectile)
		Hit.Knockback = 60.0f;

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

	FLinearColor GetPlantColor(const UVaelPlantArmData* Plant, bool bShadow)
	{
		if (Plant == nullptr)
		{
			return FLinearColor::White;
		}
		if (Plant->bMortar)
		{
			return bShadow ? Plant->SleepColor : Plant->EmberColor;
		}
		return Plant->GetColor(bShadow);
	}

	void MuzzlePuff(const UObject* WorldContext, const UVaelPlantArmData* Plant, const FVector& Location, bool bShadow, float Scale)
	{
		if (Plant == nullptr)
		{
			return;
		}
		FVaelDebris Puff;
		Puff.Count = FMath::RoundToInt(12.0f * Scale);
		Puff.Spread = 4.0f * Scale;
		Puff.Speed = 220.0f * Scale;
		Puff.Lift = 60.0f;
		Puff.Gravity = 120.0f;
		Puff.Drag = 3.0f;
		Puff.Lifetime = 0.35f;
		Puff.MinSize = 1.5f;
		Puff.MaxSize = 4.5f * Scale;
		Puff.Color = GetPlantColor(Plant, bShadow);
		Puff.Glow = 7.0f;
		Puff.bLandOnGround = false;
		AVaelDebrisBurst::Spawn(WorldContext, Location, Puff);
	}
}
