// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debug/VaelSmokeTest.h"
#include "Debug/VaelLogCatcher.h"
#include "AbilitySystemComponent.h"
#include "Combat/VaelAttributeSet.h"
#include "Combat/VaelCombatStatics.h"
#include "Compendium/VaelCompendiumSubsystem.h"
#include "Creatures/VaelCreature.h"
#include "Creatures/VaelCreatureData.h"
#include "Creatures/VaelEmberQueen.h"
#include "Creatures/VaelHornBeetle.h"
#include "Creatures/VaelAshRoller.h"
#include "Creatures/VaelBloodMoth.h"
#include "Creatures/VaelDraculaupe.h"
#include "Nature/VaelGarden.h"
#include "Nature/VaelRainBloom.h"
#include "Magic/VaelWaterPuddle.h"
#include "Creatures/VaelSickleDoll.h"
#include "Creatures/VaelVeilMonkey.h"
#include "Creatures/VaelThistleSpider.h"
#include "Creatures/VaelKelchreisser.h"
#include "Creatures/VaelPfuhlnatter.h"
#include "Creatures/VaelPoisonPuddle.h"
#include "Items/VaelMaterial.h"
#include "Items/VaelMaterialBag.h"
#include "Magic/VaelRockWall.h"
#include "Creatures/VaelLegIKComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Magic/VaelElementComponent.h"
#include "Magic/VaelFormula.h"
#include "Magic/VaelGrimoireSubsystem.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDevice.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Items/VaelInventory.h"
#include "Items/VaelMaterial.h"
#include "Items/VaelMaterialBag.h"
#include "Nature/VaelHarvestable.h"
#include "World/VaelRegion.h"
#include "World/VaelAtmosphere.h"
#include "World/VaelLocalWeather.h"
#include "World/VaelSky.h"
#include "World/VaelTimeOfDay.h"
#include "World/VaelWeatherRain.h"
#include "World/VaelWeatherWind.h"
#include "Magic/VaelEnvironmentStatics.h"
#include "World/VaelSafeZone.h"
#include "Story/VaelNpc.h"
#include "Story/VaelQuestMarker.h"
#include "Story/VaelQuestObject.h"
#include "Save/VaelSaveSubsystem.h"
#include "Story/VaelQuestSubsystem.h"
#include "Player/VaelCharacter.h"
#include "Floromant/VaelBindweedLasso.h"
#include "Floromant/VaelFloromantComponent.h"
#include "Floromant/VaelPlant.h"
#include "Floromant/VaelPlantArmComponent.h"
#include "Floromant/VaelPlantArmData.h"
#include "Floromant/VaelFloromantSettings.h"
#include "Floromant/VaelPlantData.h"
#include "Player/VaelPlayerController.h"
#include "Vael.h"
#include "VaelGameMode.h"

namespace
{
	/** Seconds before the first step, so the level and the player are ready */
	constexpr float SmokeWarmUp = 3.0f;

	/** Talks to a person and reads the whole dialogue at once, so the game does not stay paused */
	void SmokeTalk(AVaelNpc* Npc, AVaelCharacter* Player)
	{
		Npc->Interact(Player);
		if (AVaelPlayerController* Controller = Player->GetController<AVaelPlayerController>())
		{
			Controller->EndDialogue();

			// A trader opens their shop after the talk, which would keep the game paused
			Controller->CloseGrimoire();
		}
	}

	/** Seconds each creature fights, each formula plays out, and the idle time before the actors are counted */
	constexpr float SmokeFightTime = 6.0f;
	constexpr float SmokeCastTime = 3.5f;
	constexpr float SmokeSettleTime = 4.0f;

	/** Enemies stand this far in front of the player, in cm: inside the reach of cones and storms around the caster */
	constexpr float SmokeEnemyDistance = 400.0f;

	/** The enemies stand this close together, in cm, so every kind of spell can reach them */
	constexpr float SmokeEnemySpread = 20.0f;

	/** More actors of one class than this after the check are reported as lingering */
	constexpr int32 SmokeLingerLimit = 20;

	/** Lines of one kind the report shows per step at most */
	constexpr int32 SmokeMaxLinesPerStep = 8;
}

AVaelSmokeTest::AVaelSmokeTest()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

bool AVaelSmokeTest::IsRequested()
{
	return FParse::Param(FCommandLine::Get(), TEXT("VaelSmokeTest"));
}

AVaelCharacter* AVaelSmokeTest::GetPlayer() const
{
	const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	return PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
}

void AVaelSmokeTest::BeginPlay()
{
	Super::BeginPlay();

	LogCatcher = new FLogCatcher();
	GLog->AddOutputDevice(LogCatcher);

	Report.Add(FString::Printf(TEXT("Vael smoke test, %s, level %s"), *FDateTime::Now().ToString(), *GetWorld()->GetMapName()));
	Report.Add(TEXT(""));

	// Everything known and the Mark awake, so every formula can be cast
	Steps.Add({ TEXT("Setup"), [this]()
	{
		AVaelCharacter* Player = GetPlayer();
		if (Player == nullptr)
		{
			return FString(TEXT("FAILED: no player"));
		}

		StartLocation = Player->GetActorLocation();
		StartRotation = Player->GetActorRotation();

		// The fights take place on open ground: in the crater of the Glutkoenigin, who is put out of the way first;
		// in the camp, its tables and logs would catch the projectiles
		for (TActorIterator<AVaelEmberQueen> It(GetWorld()); It; ++It)
		{
			StartLocation = It->GetActorLocation() + FVector(0.0f, 0.0f, Player->GetSimpleCollisionHalfHeight() - It->GetSimpleCollisionHalfHeight() + 20.0f);
			UVaelCombatStatics::DealDamage(nullptr, *It, It->GetHealth() + 100000.0f, EVaelElement::Earth);
			break;
		}

		Player->TeleportTo(StartLocation, StartRotation);

		UVaelGrimoireSubsystem* Grimoire = GetGameInstance()->GetSubsystem<UVaelGrimoireSubsystem>();
		for (UVaelFormula* Formula : Grimoire->GetAllFormulas())
		{
			Grimoire->LearnFormula(Formula);
		}

		Grimoire->AwakenMark(FText::GetEmpty());
		return FString::Printf(TEXT("%d formulas known"), Grimoire->GetKnownFormulas().Num());
	}, 1.0f });

	// -VaelSmokeOnly=Name checks only the formulas whose name contains it, and no creatures
	FString Only;
	FParse::Value(FCommandLine::Get(), TEXT("VaelSmokeOnly="), Only, false);

	TArray<FString> OnlyNames;
	Only.ParseIntoArray(OnlyNames, TEXT(","));

	// Every creature fights the player for a while
	const TArray<UVaelCreatureData*> Creatures =
	{
		GetMutableDefault<UVaelEmberCrawlerData>(),
		GetMutableDefault<UVaelAshHarpyData>(),
		GetMutableDefault<UVaelHarpyElderData>(),
		GetMutableDefault<UVaelPreacherData>(),
		GetMutableDefault<UVaelMarkWaterPreacherData>(),
		GetMutableDefault<UVaelEmberQueenData>(),
		GetMutableDefault<UVaelSourceGuardianData>(),
	};

	for (UVaelCreatureData* Data : Only.IsEmpty() ? Creatures : TArray<UVaelCreatureData*>())
	{
		Steps.Add({ FString::Printf(TEXT("Creature %s"), *Data->DisplayName.ToString()), [this, Data]() { return PrepareArena(Data, 2); }, SmokeFightTime });
	}

	// The Pfuhlnatter: its baked mesh and animations load, it slithers up and spits (it no longer coils)
	if (Only.IsEmpty())
	{
		Steps.Add({ TEXT("Pfuhlnatter arena"), [this]()
		{
			const FString Result = PrepareArena(GetMutableDefault<UVaelPfuhlnatterData>(), 1);
			return Result;
		}, 8.0f });
		Steps.Add({ TEXT("Pfuhlnatter"), [this]()
		{
			AVaelPfuhlnatter* Snake = nullptr;
			for (TActorIterator<AVaelPfuhlnatter> It(GetWorld()); It; ++It)
			{
				Snake = !It->IsDead() ? *It : Snake;
			}
			if (Snake == nullptr)
			{
				return FString(TEXT("FAILED: no living Pfuhlnatter"));
			}
			const USkeletalMesh* Mesh = Snake->GetMesh()->GetSkeletalMeshAsset();
			const bool bOk = Mesh != nullptr && Snake->GetConstrictCount() == 0;
			return FString::Printf(TEXT("%smesh %s, state %s, %d coils (none wanted)"), bOk ? TEXT("") : TEXT("FAILED: "), Mesh != nullptr ? *Mesh->GetName() : TEXT("MISSING"),
				*StaticEnum<EVaelSnakeState>()->GetNameStringByValue(static_cast<int64>(Snake->GetSnakeState())), Snake->GetConstrictCount());
		}, 0.1f });
		Steps.Add({ TEXT("Pfuhlnatter spit"), [this]()
		{
			for (TActorIterator<AVaelPfuhlnatter> It(GetWorld()); It; ++It)
			{
				It->SetNextAttackSpit(true);
			}
			return FString(TEXT("next attack: spit"));
		}, 7.0f });
		Steps.Add({ TEXT("Pfuhlnatter poison"), [this]()
		{
			int32 Spits = 0;
			for (TActorIterator<AVaelPfuhlnatter> It(GetWorld()); It; ++It)
			{
				Spits += It->GetSpitCount();
			}
			const int32 Puddles = AVaelPoisonPuddle::CountPuddles(GetWorld());
			// Fire on a puddle boils it off into a cloud
			bool bCloud = false;
			for (TActorIterator<AVaelPoisonPuddle> It(GetWorld()); It; ++It)
			{
				if (It->GetPoisonState() == EVaelPoisonState::Liquid)
				{
					FVaelSpellHit Fire;
					Fire.Element = EVaelElement::Fire;
					AVaelPoisonPuddle::NotifySpellImpact(GetWorld(), It->GetMiddle(), 50.0f, Fire);
					bCloud = It->GetPoisonState() == EVaelPoisonState::Cloud;
					break;
				}
			}
			const bool bOk = Spits > 0 && Puddles > 0 && bCloud;
			return FString::Printf(TEXT("%s%d spits, %d puddles, fire: %s"), bOk ? TEXT("") : TEXT("FAILED: "), Spits, Puddles, bCloud ? TEXT("poison cloud") : TEXT("NO cloud"));
		}, 0.1f });
	}

	// The Kelchreisser: its baked mesh and animations load, it is an elite, takes the scent, screams and attacks
	if (Only.IsEmpty())
	{
		Steps.Add({ TEXT("Kelchreisser arena"), [this]()
		{
			const FString Result = PrepareArena(GetMutableDefault<UVaelKelchreisserData>(), 1);
			for (TActorIterator<AVaelKelchreisser> It(GetWorld()); It; ++It)
			{
				It->TakeScentNow();
			}
			return Result;
		}, 9.0f });
		Steps.Add({ TEXT("Kelchreisser"), [this]()
		{
			AVaelKelchreisser* Beast = nullptr;
			for (TActorIterator<AVaelKelchreisser> It(GetWorld()); It; ++It)
			{
				Beast = !It->IsDead() ? *It : Beast;
			}
			if (Beast == nullptr)
			{
				return FString(TEXT("FAILED: no living Kelchreisser"));
			}
			const USkeletalMesh* Mesh = Beast->GetMesh()->GetSkeletalMeshAsset();
			const bool bOk = Mesh != nullptr && Beast->IsElite() && Beast->GetAttackCount() > 0 && Beast->GetVineStrikeCount() > 0;
			return FString::Printf(TEXT("%smesh %s, elite %s, %.0f health, state %s, %d attacks (%d vine strikes), %d hits"), bOk ? TEXT("") : TEXT("FAILED: "),
				Mesh != nullptr ? *Mesh->GetName() : TEXT("MISSING"), Beast->IsElite() ? TEXT("yes") : TEXT("NO"), Beast->GetMaxHealth(),
				*StaticEnum<EVaelKelchreisserState>()->GetNameStringByValue(static_cast<int64>(Beast->GetKelchreisserState())), Beast->GetAttackCount(), Beast->GetVineStrikeCount(), Beast->GetHitCount());
		}, 0.1f });
	}

	// The first creature of the creature factory: its legs walk, it shoots, and fire burns its thread
	if (Only.IsEmpty())
	{
		Steps.Add({ TEXT("Beetle arena"), [this]() { return PrepareArena(GetMutableDefault<UVaelHornBeetleData>(), 1); }, 6.0f });
		Steps.Add({ TEXT("Beetle"), [this]()
		{
			AVaelHornBeetle* Beetle = nullptr;
			for (TActorIterator<AVaelHornBeetle> It(GetWorld()); It; ++It)
			{
				Beetle = !It->IsDead() ? *It : Beetle;
			}
			if (Beetle == nullptr)
			{
				return FString(TEXT("FAILED: no living beetle"));
			}

			const int32 NumLegs = Beetle->GetLegs()->GetNumLegs();
			const int32 NumSteps = Beetle->GetLegs()->GetStepCount();
			const int32 NumShots = Beetle->GetShotCount();

			// Fire on the tensed thread
			Beetle->StartTension();
			UVaelCombatStatics::DealDamage(nullptr, Beetle, 1.0f, EVaelElement::Fire);
			const bool bBurned = Beetle->GetBeetleState() == EVaelBeetleState::Defenseless;

			const bool bOk = NumLegs == 6 && NumSteps > 0 && NumShots > 0 && bBurned;
			return FString::Printf(TEXT("%s%d legs, %d steps, %d shots, fire on the thread: %s"), bOk ? TEXT("") : TEXT("FAILED: "), NumLegs, NumSteps, NumShots,
				bBurned ? TEXT("burned, defenseless") : TEXT("NOT burned"));
		}, 0.1f });
	}

	// The Distelspinne: eight legs walk, fire makes it burst into a burning swarm of seed spiders
	if (Only.IsEmpty())
	{
		Steps.Add({ TEXT("Thistle arena"), [this]() { return PrepareArena(GetMutableDefault<UVaelThistleSpiderData>(), 1); }, 2.0f });
		Steps.Add({ TEXT("Thistle fire"), [this]()
		{
			AVaelThistleSpider* Spider = nullptr;
			for (TActorIterator<AVaelThistleSpider> It(GetWorld()); It; ++It)
			{
				Spider = !It->IsDead() ? *It : Spider;
			}
			if (Spider == nullptr)
			{
				return FString(TEXT("FAILED: no living Distelspinne"));
			}
			const int32 NumLegs = Spider->GetLegCount();
			const int32 NumSteps = Spider->GetStepCount();
			// The weather of the arena may have soaked it, and a wet thistle never bursts: dry it first
			UVaelCombatStatics::RemoveStatus(Spider, EVaelStatus::Wet);
			UVaelCombatStatics::ApplyStatus(nullptr, Spider, EVaelStatus::Burning, 3.0f, 1.0f);
			UVaelCombatStatics::DealDamage(nullptr, Spider, 1.0f, EVaelElement::Fire);
			const bool bBursting = Spider->GetThistleState() == EVaelThistleState::Bursting;
			const bool bOk = NumLegs == 8 && bBursting;
			// A wet thistle can't burst by design, so say so if that is why
			return FString::Printf(TEXT("%s%d legs, %d steps, fire: %s%s"), bOk ? TEXT("") : TEXT("FAILED: "), NumLegs, NumSteps,
				bBursting ? TEXT("bursts") : TEXT("does NOT burst"), UVaelCombatStatics::HasStatus(Spider, EVaelStatus::Wet) ? TEXT(" (it is wet)") : TEXT(""));
		}, 1.5f });
		Steps.Add({ TEXT("Thistle swarm"), [this]()
		{
			int32 Seedlings = 0;
			int32 Burning = 0;
			for (TActorIterator<AVaelThistleSeedling> It(GetWorld()); It; ++It)
			{
				if (!It->IsDead())
				{
					++Seedlings;
					Burning += UVaelCombatStatics::HasStatus(*It, EVaelStatus::Burning) ? 1 : 0;
				}
			}
			const bool bOk = Seedlings > 0 && Burning > 0;
			return FString::Printf(TEXT("%s%d seed spiders, %d burning"), bOk ? TEXT("") : TEXT("FAILED: "), Seedlings, Burning);
		}, 0.1f });
		// Seed spiders leap at the player and hurt on contact
		Steps.Add({ TEXT("Seedling arena"), [this]() { return PrepareArena(GetMutableDefault<UVaelThistleSeedlingData>(), 6); }, 4.0f });
		Steps.Add({ TEXT("Seedling leaps"), [this]()
		{
			int32 Leaps = 0;
			int32 Hits = 0;
			for (TActorIterator<AVaelThistleSeedling> It(GetWorld()); It; ++It)
			{
				Leaps += It->GetLeapCount();
				Hits += It->GetLeapHitCount();
			}
			const bool bOk = Leaps > 0 && Hits > 0;
			return FString::Printf(TEXT("%s%d leaps, %d hits on the player"), bOk ? TEXT("") : TEXT("FAILED: "), Leaps, Hits);
		}, 0.1f });
	}

	// The ash roller rolls at the player, and a rock wall in its lane throws it on its back
	if (Only.IsEmpty())
	{
		const auto FindRoller = [this]()
		{
			AVaelAshRoller* Roller = nullptr;
			for (TActorIterator<AVaelAshRoller> It(GetWorld()); It; ++It)
			{
				Roller = !It->IsDead() ? *It : Roller;
			}
			return Roller;
		};

		Steps.Add({ TEXT("Roller arena"), [this]() { return PrepareArena(GetMutableDefault<UVaelAshRollerData>(), 1); }, 6.0f });
		Steps.Add({ TEXT("Roller wall"), [this, FindRoller]()
		{
			AVaelAshRoller* Roller = FindRoller();
			if (Roller == nullptr || GetPlayer() == nullptr)
			{
				return FString(TEXT("FAILED: no living roller"));
			}

			RollsBeforeWall = Roller->GetRollCount();

			// Away from the player, so nobody stands in the lane
			FVector Direction = (Roller->GetActorLocation() - GetPlayer()->GetActorLocation()).GetSafeNormal2D();
			Direction = Direction.IsNearlyZero() ? FVector::ForwardVector : Direction;

			FVector Ground = Roller->GetActorLocation() + Direction * 450.0f;
			AVaelCreature::FindGround(GetWorld(), Ground, Ground);
			AVaelRockWall::RaiseBlock(GetWorld(), Ground, Direction.Rotation().Yaw, 220.0f, 220.0f, 6.0f, GetPlayer());

			Roller->StartRoll(Direction);
			return FString::Printf(TEXT("%d rolls so far, wall raised in the lane"), RollsBeforeWall);
		}, 2.5f });
		Steps.Add({ TEXT("Roller"), [this, FindRoller]()
		{
			const AVaelAshRoller* Roller = FindRoller();
			if (Roller == nullptr)
			{
				return FString(TEXT("FAILED: no living roller"));
			}

			const bool bOnBack = Roller->GetRollerState() == EVaelRollerState::OnBack;
			const bool bOk = RollsBeforeWall > 0 && bOnBack;
			return FString::Printf(TEXT("%s%d rolls at the player, at the wall: %s"), bOk ? TEXT("") : TEXT("FAILED: "), RollsBeforeWall, bOnBack ? TEXT("on its back") : TEXT("NOT on its back"));
		}, 0.1f });
	}

	// The sickle doll whirls at the player, and a hit of water lets it collapse
	if (Only.IsEmpty())
	{
		Steps.Add({ TEXT("Doll arena"), [this]() { return PrepareArena(GetMutableDefault<UVaelSickleDollData>(), 1); }, 6.0f });
		Steps.Add({ TEXT("Doll"), [this]()
		{
			AVaelSickleDoll* Doll = nullptr;
			for (TActorIterator<AVaelSickleDoll> It(GetWorld()); It; ++It)
			{
				Doll = !It->IsDead() ? *It : Doll;
			}
			if (Doll == nullptr)
			{
				return FString(TEXT("FAILED: no living doll"));
			}

			const int32 NumWhirls = Doll->GetWhirlCount();
			UVaelCombatStatics::DealDamage(nullptr, Doll, 1.0f, EVaelElement::Water);
			const bool bSoaked = Doll->GetDollState() == EVaelDollState::Soaked;

			const bool bOk = NumWhirls > 0 && bSoaked;
			return FString::Printf(TEXT("%s%d whirls, water: %s"), bOk ? TEXT("") : TEXT("FAILED: "), NumWhirls, bSoaked ? TEXT("collapsed") : TEXT("NOT collapsed"));
		}, 0.1f });
	}

	// The Floromant: sows from the belt, calls the garden: the Glutdistel shoots at a crawler, the Rußrose heals, the Schlotwinde raises a wall;
	// the root grip holds; only fire hurts the plants
	if (Only.IsEmpty())
	{
		Steps.Add({ TEXT("Floromant garden"), [this]()
		{
			const FString Arena = PrepareArena(GetMutableDefault<UVaelEmberCrawlerData>(), 2);
			AVaelCharacter* Player = GetPlayer();
			if (Player == nullptr)
			{
				return FString(TEXT("FAILED: no player"));
			}

			Player->SetHero(EVaelHeroClass::Floromant, true);
			UVaelFloromantComponent* Floromant = Player->GetFloromant();
			const FVector Forward = Player->GetActorForwardVector();
			const FVector Feet = Player->GetActorLocation() - FVector(0.0f, 0.0f, Player->GetSimpleCollisionHalfHeight());

			// A seed flies from the belt (Rußrose), the thistle and the bindweed are planted at once, so they have grown when the crawlers come
			Floromant->SelectBelt(1);
			const bool bSown = Floromant->Sow(Feet - Forward * 150.0f);
			Floromant->PlantSeed(UVaelFloromantComponent::FindPlantData(TEXT("Glutdistel")), false, Feet + Forward * 150.0f, Forward.Rotation().Yaw);
			Floromant->PlantSeed(UVaelFloromantComponent::FindPlantData(TEXT("Schlotwinde")), false, Feet + Forward.RotateAngleAxis(90.0f, FVector::UpVector) * 300.0f, Forward.Rotation().Yaw);
			return FString::Printf(TEXT("%s, Floromantin, Saft %.0f, Russrose %s"), *Arena, Player->GetMana(), bSown ? TEXT("sown") : TEXT("NOT sown"));
		}, 1.6f });
		Steps.Add({ TEXT("Floromant call"), [this]()
		{
			AVaelCharacter* Player = GetPlayer();
			if (Player == nullptr)
			{
				return FString(TEXT("FAILED: no player"));
			}
			UVaelFloromantComponent* Floromant = Player->GetFloromant();
			const int32 NumActing = Floromant->CallGarden();
			AVaelCreature* Crawler = nullptr;
			for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
			{
				Crawler = !It->IsDead() ? *It : Crawler;
			}
			const bool bGrip = Crawler != nullptr && Floromant->RootGrip(Crawler->GetActorLocation());
			const float CrawlerDistance = Crawler != nullptr ? FVector::Dist2D(Crawler->GetActorLocation(), Player->GetActorLocation()) : -1.0f;
			return FString::Printf(TEXT("%s%d plants act, root grip %s, crawler %.0f cm away, Saft %.0f"), NumActing >= 3 ? TEXT("") : TEXT("FAILED: "), NumActing,
				bGrip ? TEXT("at the crawler") : TEXT("NOT cast"), CrawlerDistance, Player->GetMana());
		}, 2.4f });
		Steps.Add({ TEXT("Floromant"), [this]()
		{
			AVaelCharacter* Player = GetPlayer();
			AVaelPlant* Thistle = nullptr;
			AVaelPlant* Rose = nullptr;
			for (TActorIterator<AVaelPlant> It(GetWorld()); It; ++It)
			{
				const FName Id = It->GetData() != nullptr && !It->IsWild() ? It->GetData()->PlantId : NAME_None;
				Thistle = Id == TEXT("Glutdistel") ? *It : Thistle;
				Rose = Id == TEXT("Russrose") ? *It : Rose;
			}
			const AVaelVineWall* Wall = nullptr;
			for (TActorIterator<AVaelVineWall> It(GetWorld()); It; ++It)
			{
				Wall = *It;
			}
			int32 NumHeld = 0;
			for (TActorIterator<AVaelRootGrip> It(GetWorld()); It; ++It)
			{
				NumHeld += It->GetHeldCount();
			}
			if (Player == nullptr || Thistle == nullptr || Rose == nullptr)
			{
				return FString::Printf(TEXT("FAILED: plants missing (Glutdistel %d, Russrose %d)"), Thistle != nullptr, Rose != nullptr);
			}

			const int32 NumPlants = Player->GetFloromant()->GetNumPlants();
			const int32 NumShots = Thistle->GetShotCount();
			const int32 NumPulses = Rose->GetPulseCount();
			const bool bHolding = Wall != nullptr && Wall->IsHolding();

			// Earth leaves a plant alone, fire hurts it
			const float Before = Rose->GetHealth();
			UVaelCombatStatics::DealDamage(nullptr, Rose, 5.0f, EVaelElement::Earth);
			const bool bEarthHarmless = FMath::IsNearlyEqual(Rose->GetHealth(), Before);
			UVaelCombatStatics::DealDamage(nullptr, Rose, 5.0f, EVaelElement::Fire);
			const bool bFireHurts = Rose->GetHealth() < Before;

			// Back to the mage: the plants wither, mana is full again
			Player->SetHero(EVaelHeroClass::Mage, false);
			const bool bMage = !Player->IsFloromant() && Player->GetMaxMana() > 0.0f && FMath::IsNearlyEqual(Player->GetMana(), Player->GetMaxMana());

			const bool bOk = NumPlants == 3 && NumShots > 0 && NumPulses > 0 && bHolding && NumHeld > 0 && bEarthHarmless && bFireHurts && bMage;
			return FString::Printf(TEXT("%s%d plants, Glutdistel %d shots, Russrose %d pulses, vine wall %s, root grip held %d times, earth %s, fire %s, back to mage %s"), bOk ? TEXT("") : TEXT("FAILED: "),
				NumPlants, NumShots, NumPulses, bHolding ? TEXT("holds") : TEXT("does NOT hold"), NumHeld, bEarthHarmless ? TEXT("harmless") : TEXT("HURTS"),
				bFireHurts ? TEXT("hurts") : TEXT("does NOT hurt"), bMage ? TEXT("ok") : TEXT("FAILED"));
		}, 1.0f });

		// Plant arms: the Windhalm grows on both hands, they fire at crawlers, the fingers take turns and the bars run down
		const auto EnemyHealth = [this]()
		{
			float Health = 0.0f;
			for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
			{
				Health += It->IsDead() ? 0.0f : It->GetHealth();
			}
			return Health;
		};
		Steps.Add({ TEXT("Plant arm grows"), [this]()
		{
			const FString Arena = PrepareArena(GetMutableDefault<UVaelEmberCrawlerData>(), 2);
			AVaelCharacter* Player = GetPlayer();
			if (Player == nullptr)
			{
				return FString(TEXT("FAILED: no player"));
			}
			Player->SetHero(EVaelHeroClass::Floromant, true);
			UVaelFloromantComponent* Floromant = Player->GetFloromant();
			Floromant->PutOnHand(true);
			Floromant->PutOnHand(false);
			const UVaelPlantArmData* Plant = Floromant->GetHand(true)->GetPlant();
			return FString::Printf(TEXT("%s%s, %s on both hands"), Plant != nullptr ? TEXT("") : TEXT("FAILED: "), *Arena, Plant != nullptr ? *Plant->GetName() : TEXT("nothing"));
		}, 0.8f });
		Steps.Add({ TEXT("Plant arm fires"), [this, EnemyHealth]()
		{
			AVaelCharacter* Player = GetPlayer();
			if (Player == nullptr)
			{
				return FString(TEXT("FAILED: no player"));
			}

			// Faces the nearest crawler, so the aim points at it
			const AVaelCreature* Crawler = nullptr;
			for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
			{
				Crawler = !It->IsDead() ? *It : Crawler;
			}
			if (Crawler != nullptr)
			{
				Player->SetActorRotation(FRotator(0.0f, (Crawler->GetActorLocation() - Player->GetActorLocation()).Rotation().Yaw, 0.0f));
			}
			PlantArmEnemyHealth = EnemyHealth();
			Player->GetFloromant()->SetHandFiring(true, true);
			Player->GetFloromant()->SetHandFiring(false, true);
			return FString::Printf(TEXT("firing, enemies have %.0f health"), PlantArmEnemyHealth);
		}, 2.0f });
		Steps.Add({ TEXT("Plant arm"), [this, EnemyHealth]()
		{
			AVaelCharacter* Player = GetPlayer();
			if (Player == nullptr)
			{
				return FString(TEXT("FAILED: no player"));
			}
			UVaelFloromantComponent* Floromant = Player->GetFloromant();
			const UVaelPlantArmComponent* Left = Floromant->GetHand(true);
			const UVaelPlantArmComponent* Right = Floromant->GetHand(false);
			const UVaelPlantArmData* Plant = Left->GetPlant();
			const int32 Fired = Plant != nullptr ? (Plant->BarSize - Left->GetBarShots()) + (Plant->BarSize - Right->GetBarShots()) : 0;
			const float Stance = FMath::Min(Left->GetStance(), Right->GetStance());
			const float Damage = PlantArmEnemyHealth - EnemyHealth();

			Floromant->SetHandFiring(true, false);
			Floromant->SetHandFiring(false, false);

			// Reloading from the Keimpelz pelt: it pays for the missing shots, the bars are full once the stalks have drawn it in
			PlantArmKeimpelz = Floromant->GetKeimpelz();
			const int32 Reloaded = Floromant->ReloadHands();

			const bool bOk = Fired >= 10 && Stance > 0.5f && Damage > 0.0f && Reloaded == Fired;
			return FString::Printf(TEXT("%s%d shots from both hands, stance %.2f, %.0f damage, %d shots reloading from %.0f Keimpelz"), bOk ? TEXT("") : TEXT("FAILED: "),
				Fired, Stance, Damage, Reloaded, PlantArmKeimpelz);
		}, UVaelFloromantSettings::Get()->ReloadTime + 0.3f });
		Steps.Add({ TEXT("Keimpelz reload"), [this]()
		{
			AVaelCharacter* Player = GetPlayer();
			if (Player == nullptr)
			{
				return FString(TEXT("FAILED: no player"));
			}
			UVaelFloromantComponent* Floromant = Player->GetFloromant();
			const int32 Missing = Floromant->GetHand(true)->GetMissingShots() + Floromant->GetHand(false)->GetMissingShots();
			const float Used = PlantArmKeimpelz - Floromant->GetKeimpelz();

			// Back to the mage: the hands are bare again
			Player->SetHero(EVaelHeroClass::Mage, false);
			const bool bCleared = Floromant->GetHand(true)->GetPlant() == nullptr && Floromant->GetHand(false)->GetPlant() == nullptr;

			const bool bOk = Missing == 0 && Used > 5.0f && bCleared;
			return FString::Printf(TEXT("%sbars %s, %.0f Keimpelz used, hands %s after the mage"), bOk ? TEXT("") : TEXT("FAILED: "),
				Missing == 0 ? TEXT("full") : TEXT("NOT full"), Used, bCleared ? TEXT("bare") : TEXT("NOT bare"));
		}, 0.2f });

		// The Funkenmohn (Boris 2026-10-26): both hands become mortar buds, a tap throws a capsule at the crawlers in an arc,
		// then the crossed hands throw the Mohnfeuerwerk
		Steps.Add({ TEXT("Mortar hands"), [this]()
		{
			const FString Arena = PrepareArena(GetMutableDefault<UVaelEmberCrawlerData>(), 3);
			AVaelCharacter* Player = GetPlayer();
			UVaelPlantArmData* Poppy = LoadObject<UVaelPlantArmData>(nullptr, TEXT("/Game/Vael/Characters/Floromant/Pflanzenarm/DA_Pflanzenarm_Funkenmohn.DA_Pflanzenarm_Funkenmohn"));
			if (Player == nullptr || Poppy == nullptr)
			{
				return FString(TEXT("FAILED: no player or no DA_Pflanzenarm_Funkenmohn"));
			}
			Player->SetHero(EVaelHeroClass::Floromant, true);
			Player->GetFloromant()->GetHand(true)->SetPlant(Poppy);
			Player->GetFloromant()->GetHand(false)->SetPlant(Poppy);
			return FString::Printf(TEXT("%s, Funkenmohn on both hands, capsule %s"), *Arena, Poppy->CapsuleMesh.IsNull() ? TEXT("MISSING") : TEXT("set"));
		}, 1.0f });
		Steps.Add({ TEXT("Mortar tap"), [this, EnemyHealth]()
		{
			AVaelCharacter* Player = GetPlayer();
			if (Player == nullptr)
			{
				return FString(TEXT("FAILED: no player"));
			}
			const AVaelCreature* Crawler = nullptr;
			for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
			{
				Crawler = !It->IsDead() ? *It : Crawler;
			}
			if (Crawler != nullptr)
			{
				Player->SetActorRotation(FRotator(0.0f, (Crawler->GetActorLocation() - Player->GetActorLocation()).Rotation().Yaw, 0.0f));
			}
			PlantArmEnemyHealth = EnemyHealth();
			Player->GetFloromant()->SetHandFiring(true, true);
			return FString::Printf(TEXT("tapping, enemies have %.0f health, crawler %.0f cm away"), PlantArmEnemyHealth,
				Crawler != nullptr ? FVector::Dist2D(Crawler->GetActorLocation(), Player->GetActorLocation()) : 0.0f);
		}, 0.15f });
		Steps.Add({ TEXT("Mortar release"), [this]()
		{
			AVaelCharacter* Player = GetPlayer();
			if (Player == nullptr)
			{
				return FString(TEXT("FAILED: no player"));
			}
			Player->GetFloromant()->SetHandFiring(true, false);
			return FString(TEXT("let go"));
		}, 2.4f });
		Steps.Add({ TEXT("Mortar"), [this, EnemyHealth]()
		{
			AVaelCharacter* Player = GetPlayer();
			if (Player == nullptr)
			{
				return FString(TEXT("FAILED: no player"));
			}
			UVaelPlantArmComponent* Left = Player->GetFloromant()->GetHand(true);
			const int32 Thrown = Left->GetPlant() != nullptr ? Left->GetPlant()->BarSize - Left->GetBarShots() : 0;
			const float Damage = PlantArmEnemyHealth - EnemyHealth();

			// Crossed: both hands held, the pair throws its giant capsule
			PrepareArena(GetMutableDefault<UVaelEmberCrawlerData>(), 3);
			PlantArmEnemyHealth = EnemyHealth();
			Player->GetFloromant()->SetHandFiring(true, true);
			Player->GetFloromant()->SetHandFiring(false, true);
			Player->GetFloromant()->CrossHands(false);
			const bool bOk = Thrown == 1 && Damage > 0.0f;
			return FString::Printf(TEXT("%s%d capsule thrown, %.0f damage; crossing the hands"), bOk ? TEXT("") : TEXT("FAILED: "), Thrown, Damage);
		}, 3.0f });
		Steps.Add({ TEXT("Mohnfeuerwerk"), [this, EnemyHealth]()
		{
			AVaelCharacter* Player = GetPlayer();
			if (Player == nullptr)
			{
				return FString(TEXT("FAILED: no player"));
			}
			const float Damage = PlantArmEnemyHealth - EnemyHealth();
			Player->GetFloromant()->SetHandFiring(true, false);
			Player->GetFloromant()->SetHandFiring(false, false);
			Player->SetHero(EVaelHeroClass::Mage, false);
			return FString::Printf(TEXT("%s%.0f damage from the firework"), Damage > 0.0f ? TEXT("") : TEXT("FAILED: "), Damage);
		}, 0.2f });

		// The Schlotwinde (Boris 2026-10-27): held, a tendril lassos a sickle doll and holds it; let go, the doll is yanked to her;
		// then the crossed hands raise the Windenzaun hedge
		Steps.Add({ TEXT("Lasso hands"), [this]()
		{
			const FString Arena = PrepareArena(GetMutableDefault<UVaelSickleDollData>(), 1);
			AVaelCharacter* Player = GetPlayer();
			UVaelPlantArmData* Vine = LoadObject<UVaelPlantArmData>(nullptr, TEXT("/Game/Vael/Characters/Floromant/Pflanzenarm/DA_Pflanzenarm_Schlotwinde.DA_Pflanzenarm_Schlotwinde"));
			if (Player == nullptr || Vine == nullptr)
			{
				return FString(TEXT("FAILED: no player or no DA_Pflanzenarm_Schlotwinde"));
			}
			Player->SetHero(EVaelHeroClass::Floromant, true);
			Player->GetFloromant()->GetHand(true)->SetPlant(Vine);
			Player->GetFloromant()->GetHand(false)->SetPlant(Vine);
			return FString::Printf(TEXT("%s, Schlotwinde on both hands, lasso %s, trumpet %s"), *Arena, Vine->bLasso ? TEXT("on") : TEXT("OFF"),
				Vine->LeafMesh.IsNull() ? TEXT("MISSING") : TEXT("set"));
		}, 1.0f });
		Steps.Add({ TEXT("Lasso hold"), [this]()
		{
			AVaelCharacter* Player = GetPlayer();
			if (Player == nullptr)
			{
				return FString(TEXT("FAILED: no player"));
			}
			const AVaelCreature* Doll = nullptr;
			for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
			{
				Doll = !It->IsDead() ? *It : Doll;
			}
			if (Doll != nullptr)
			{
				Player->SetActorRotation(FRotator(0.0f, (Doll->GetActorLocation() - Player->GetActorLocation()).Rotation().Yaw, 0.0f));
				LassoStartDistance = FVector::Dist2D(Doll->GetActorLocation(), Player->GetActorLocation());
			}
			Player->GetFloromant()->SetHandFiring(true, true);
			return FString::Printf(TEXT("holding, doll %.0f cm away"), LassoStartDistance);
		}, 1.2f });
		Steps.Add({ TEXT("Lasso release"), [this]()
		{
			AVaelCharacter* Player = GetPlayer();
			if (Player == nullptr)
			{
				return FString(TEXT("FAILED: no player"));
			}
			int32 Holding = 0;
			for (TActorIterator<AVaelBindweedLasso> It(GetWorld()); It; ++It)
			{
				Holding += It->IsHolding() ? 1 : 0;
			}
			Player->GetFloromant()->SetHandFiring(true, false);
			return FString::Printf(TEXT("%s%d lasso holding, let go"), Holding > 0 ? TEXT("") : TEXT("FAILED: "), Holding);
		}, 1.2f });
		Steps.Add({ TEXT("Lasso"), [this]()
		{
			AVaelCharacter* Player = GetPlayer();
			if (Player == nullptr)
			{
				return FString(TEXT("FAILED: no player"));
			}
			float Distance = 0.0f;
			for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
			{
				Distance = !It->IsDead() ? FVector::Dist2D(It->GetActorLocation(), Player->GetActorLocation()) : Distance;
			}

			// Crossed: both hands held, the pair raises its hedge
			Player->GetFloromant()->SetHandFiring(true, true);
			Player->GetFloromant()->SetHandFiring(false, true);
			Player->GetFloromant()->CrossHands(false);
			const bool bOk = Distance < FMath::Min(LassoStartDistance - 80.0f, 220.0f);
			return FString::Printf(TEXT("%sdoll yanked from %.0f to %.0f cm; crossing the hands"), bOk ? TEXT("") : TEXT("FAILED: "), LassoStartDistance, Distance);
		}, 1.0f });
		Steps.Add({ TEXT("Windenzaun"), [this]()
		{
			AVaelCharacter* Player = GetPlayer();
			if (Player == nullptr)
			{
				return FString(TEXT("FAILED: no player"));
			}
			int32 Walls = 0;
			for (TActorIterator<AVaelVineWall> It(GetWorld()); It; ++It)
			{
				++Walls;
				// Out of the way of the next steps
				It->Destroy();
			}
			Player->GetFloromant()->SetHandFiring(true, false);
			Player->GetFloromant()->SetHandFiring(false, false);
			Player->SetHero(EVaelHeroClass::Mage, false);
			return FString::Printf(TEXT("%s%d hedge standing"), Walls > 0 ? TEXT("") : TEXT("FAILED: "), Walls);
		}, 0.2f });

		// The new controls (Boris 2026-10-27): Square roots her feet, Cross charges the Kreuzschuss for a moment and lets it go
		Steps.Add({ TEXT("Root and cross"), [this]()
		{
			AVaelCharacter* Player = GetPlayer();
			UVaelPlantArmData* Vine = LoadObject<UVaelPlantArmData>(nullptr, TEXT("/Game/Vael/Characters/Floromant/Pflanzenarm/DA_Pflanzenarm_Schlotwinde.DA_Pflanzenarm_Schlotwinde"));
			if (Player == nullptr || Vine == nullptr)
			{
				return FString(TEXT("FAILED: no player or no DA_Pflanzenarm_Schlotwinde"));
			}
			Player->SetHero(EVaelHeroClass::Floromant, true);
			UVaelFloromantComponent* Floromant = Player->GetFloromant();
			Floromant->GetHand(true)->SetPlant(Vine);
			Floromant->GetHand(false)->SetPlant(Vine);
			Floromant->ToggleRoot();
			return FString(TEXT("rooting, Schlotwinde on both hands"));
		}, 1.0f });
		Steps.Add({ TEXT("Kreuzschuss"), [this]()
		{
			AVaelCharacter* Player = GetPlayer();
			if (Player == nullptr)
			{
				return FString(TEXT("FAILED: no player"));
			}
			UVaelFloromantComponent* Floromant = Player->GetFloromant();
			Floromant->StartCross();
			const bool bCharging = Floromant->IsCrossCharging();
			Floromant->ReleaseCross();
			return FString::Printf(TEXT("%s%s, cross %s"), Floromant->IsRooted() ? TEXT("") : TEXT("FAILED: "), Floromant->IsRooted() ? TEXT("rooted") : TEXT("NOT rooted"),
				bCharging ? TEXT("charging") : TEXT("NOT charging"));
		}, 1.0f });
		Steps.Add({ TEXT("Kreuzschuss done"), [this]()
		{
			AVaelCharacter* Player = GetPlayer();
			if (Player == nullptr)
			{
				return FString(TEXT("FAILED: no player"));
			}
			UVaelFloromantComponent* Floromant = Player->GetFloromant();
			int32 Walls = 0;
			for (TActorIterator<AVaelVineWall> It(GetWorld()); It; ++It)
			{
				++Walls;
				It->Destroy();
			}
			const bool bOk = Walls > 0 && !Floromant->IsCrossCharging();
			Floromant->ToggleRoot();
			Player->SetHero(EVaelHeroClass::Mage, false);
			return FString::Printf(TEXT("%sthe charged cross raised %d hedge"), bOk ? TEXT("") : TEXT("FAILED: "), Walls);
		}, 0.2f });
	}

	// The veil monkey leaps onto the player's back and robs them; fire blinds it and the loot comes back
	if (Only.IsEmpty())
	{
		const auto FindMonkey = [this]()
		{
			AVaelVeilMonkey* Monkey = nullptr;
			for (TActorIterator<AVaelVeilMonkey> It(GetWorld()); It; ++It)
			{
				Monkey = !It->IsDead() ? *It : Monkey;
			}
			return Monkey;
		};

		Steps.Add({ TEXT("Monkey arena"), [this, FindMonkey]()
		{
			const FString Arena = PrepareArena(GetMutableDefault<UVaelVeilMonkeyData>(), 1);
			AVaelVeilMonkey* Monkey = FindMonkey();
			AVaelCharacter* Player = GetPlayer();
			const UVaelMaterial* Gland = LoadObject<UVaelMaterial>(nullptr, TEXT("/Game/Vael/Items/Materials/DA_Material_Glutdruese.DA_Material_Glutdruese"));
			if (Monkey == nullptr || Player == nullptr || Gland == nullptr)
			{
				return FString(TEXT("FAILED: no monkey, player or material"));
			}

			Player->GetMaterialBag()->AddMaterial(Gland, 10);
			Monkey->StartPounce(Player);
			return Arena + TEXT(", leaps at the player");
		}, 3.0f });
		Steps.Add({ TEXT("Monkey"), [this, FindMonkey]()
		{
			AVaelVeilMonkey* Monkey = FindMonkey();
			if (Monkey == nullptr)
			{
				return FString(TEXT("FAILED: no living monkey"));
			}

			const int32 NumThefts = Monkey->GetTheftCount();
			const int32 NumCarried = Monkey->GetNumStolen();
			UVaelCombatStatics::DealDamage(nullptr, Monkey, 1.0f, EVaelElement::Fire);
			const bool bBlinded = Monkey->GetMonkeyState() == EVaelMonkeyState::Blinded;
			const bool bReturned = Monkey->GetNumStolen() == 0;

			const bool bOk = NumThefts > 0 && NumCarried > 0 && bBlinded && bReturned;
			return FString::Printf(TEXT("%s%d thefts, carried %d, fire: %s, loot %s"), bOk ? TEXT("") : TEXT("FAILED: "), NumThefts, NumCarried,
				bBlinded ? TEXT("blinded") : TEXT("NOT blinded"), bReturned ? TEXT("given back") : TEXT("NOT given back"));
		}, 1.0f });

		// A block of rock in its way: it climbs over instead of walking around
		Steps.Add({ TEXT("Monkey block"), [this, FindMonkey]()
		{
			AVaelVeilMonkey* Monkey = FindMonkey();
			if (Monkey == nullptr || GetPlayer() == nullptr)
			{
				return FString(TEXT("FAILED: no living monkey"));
			}

			// Away from the player, so nobody stands behind the block
			MonkeyClimbDirection = (Monkey->GetActorLocation() - GetPlayer()->GetActorLocation()).GetSafeNormal2D();
			MonkeyClimbDirection = MonkeyClimbDirection.IsNearlyZero() ? FVector::ForwardVector : MonkeyClimbDirection;
			MonkeyClimbStart = Monkey->GetActorLocation();

			FVector Ground = MonkeyClimbStart + MonkeyClimbDirection * 120.0f;
			AVaelCreature::FindGround(GetWorld(), Ground, Ground);
			AVaelRockWall::RaiseBlock(GetWorld(), Ground, MonkeyClimbDirection.Rotation().Yaw, 120.0f, 120.0f, 8.0f, GetPlayer());

			bMonkeyClimbStarted = Monkey->StartClimb(MonkeyClimbStart + MonkeyClimbDirection * 600.0f);
			return FString::Printf(TEXT("block raised in its way, %s"), bMonkeyClimbStarted ? TEXT("starts to climb") : TEXT("does NOT climb"));
		}, 1.6f });
		Steps.Add({ TEXT("Monkey climb"), [this, FindMonkey]()
		{
			const AVaelVeilMonkey* Monkey = FindMonkey();
			if (Monkey == nullptr)
			{
				return FString(TEXT("FAILED: no living monkey"));
			}

			const float Way = FVector::DotProduct(Monkey->GetActorLocation() - MonkeyClimbStart, MonkeyClimbDirection);
			const bool bDone = Monkey->GetClimbCount() > 0 && Monkey->GetMonkeyState() != EVaelMonkeyState::Climb;
			const bool bOk = bMonkeyClimbStarted && bDone && Way > 180.0f;
			return FString::Printf(TEXT("%s%d climbs, %.0f cm beyond where it stood"), bOk ? TEXT("") : TEXT("FAILED: "), Monkey->GetClimbCount(), Way);
		}, 0.1f });
	}

	// A caterpillar touches the player: it spins itself in and calls the other; a moth hatches and rains
	if (Only.IsEmpty())
	{
		Steps.Add({ TEXT("Caterpillar arena"), [this]()
		{
			const FString Arena = PrepareArena(GetMutableDefault<UVaelDraculaupeData>(), 2);
			AVaelCharacter* Player = GetPlayer();

			AVaelDraculaupe* First = nullptr;
			AVaelDraculaupe* Second = nullptr;
			for (TActorIterator<AVaelDraculaupe> It(GetWorld()); It; ++It)
			{
				if (!It->IsDead())
				{
					Second = First;
					First = *It;
				}
			}
			if (First == nullptr || Second == nullptr || Player == nullptr)
			{
				return FString(TEXT("FAILED: no two caterpillars"));
			}

			AVaelBloodMoth::ResetFirstHarvest();
			First->Touch(Player);

			const bool bOk = First->GetCaterpillarState() == EVaelCaterpillarState::Cocoon && Second->IsAlarmed();
			return FString::Printf(TEXT("%s%s, one touches the player: %s, the other: %s"), bOk ? TEXT("") : TEXT("FAILED: "), *Arena,
				First->GetCaterpillarState() == EVaelCaterpillarState::Cocoon ? TEXT("cocoon") : TEXT("NO cocoon"), Second->IsAlarmed() ? TEXT("alarmed") : TEXT("NOT alarmed"));
		}, 24.0f });
		Steps.Add({ TEXT("Moth"), [this]()
		{
			int32 NumMoths = 0;
			int32 NumDrops = 0;
			int32 NumBlooms = 0;
			for (TActorIterator<AVaelBloodMoth> It(GetWorld()); It; ++It)
			{
				++NumMoths;
				NumDrops += It->GetDropCount();
				NumBlooms += It->GetBloomCount();
			}

			int32 NumHarvest = 0;
			for (TActorIterator<AVaelRainBloom> It(GetWorld()); It; ++It)
			{
				NumHarvest += It->GetBloomKind() == EVaelBloomKind::Harvest ? 1 : 0;
			}

			const bool bOk = NumMoths > 0 && NumDrops > 0;
			return FString::Printf(TEXT("%s%d moths hatched, %d drops landed, %d flowers grew, %d of them to harvest"), bOk ? TEXT("") : TEXT("FAILED: "), NumMoths, NumDrops, NumBlooms, NumHarvest);
		}, 0.1f });
	}

	// Every formula at fresh enemies: preachers, which neither run off nor blow themselves up
	const UVaelGrimoireSubsystem* Grimoire = GetGameInstance()->GetSubsystem<UVaelGrimoireSubsystem>();
	for (UVaelFormula* Formula : Grimoire->GetAllFormulas())
	{
		if (!Only.IsEmpty() && !OnlyNames.ContainsByPredicate([Formula](const FString& Name) { return Formula->DisplayName.ToString().Contains(Name) || Formula->GetName().Contains(Name); }))
		{
			continue;
		}

		Steps.Add({ FString::Printf(TEXT("Formula %s"), *Formula->DisplayName.ToString()), [this, Formula]()
		{
			const FString Arena = PrepareArena(GetMutableDefault<UVaelPreacherData>(), 3);
			return Arena + TEXT(", ") + CastFormula(Formula);
		}, SmokeCastTime });
	}

	// Some quiet, then look for what never went away
	Steps.Add({ TEXT("Settle"), [this]()
	{
		for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
		{
			if (!It->IsDead())
			{
				UVaelCombatStatics::DealDamage(nullptr, *It, It->GetHealth() + 1000.0f, EVaelElement::Earth);
			}
		}

		return FString(TEXT("all creatures killed"));
	}, SmokeSettleTime });

	Steps.Add({ TEXT("Lingering actors"), [this]() { return CountLingeringActors(); }, 0.1f });

	// One of every plant, fungus and stone is gathered: it must give materials and a sample, then lie bare.
	// First in the region as it is, then once more under heavy corruption, where the Mark plants grow.
	const TSharedRef<TSet<FName>> Gathered = MakeShared<TSet<FName>>();
	const TSharedRef<float> CorruptionBefore = MakeShared<float>(0.0f);

	const auto Harvest = [this, Gathered]()
	{
		AVaelCharacter* Player = GetPlayer();
		if (Player == nullptr)
		{
			return FString(TEXT("FAILED: no player"));
		}

		TSet<FName> All;
		TArray<FString> Failed;
		int32 NumNew = 0;

		for (TActorIterator<AVaelHarvestable> It(GetWorld()); It; ++It)
		{
			const FName SubjectId = It->GetCompendiumId();
			All.Add(SubjectId);

			if (Gathered->Contains(SubjectId) || !It->CanInteract(Player))
			{
				continue;
			}

			int32 Before = 0;
			for (const FVaelMaterialStack& Stack : Player->GetMaterialBag()->GetStacks())
			{
				Before += Stack.Count;
			}

			It->Interact(Player);
			Gathered->Add(SubjectId);
			++NumNew;

			int32 After = 0;
			for (const FVaelMaterialStack& Stack : Player->GetMaterialBag()->GetStacks())
			{
				After += Stack.Count;
			}

			if (After <= Before || !It->IsHarvested() || It->CanInteract(Player))
			{
				Failed.Add(SubjectId.ToString());
			}
		}

		TArray<FString> Missing;
		for (const FName SubjectId : All)
		{
			if (!Gathered->Contains(SubjectId))
			{
				Missing.Add(SubjectId.ToString());
			}
		}

		const FString Summary = FString::Printf(TEXT("%d kinds gathered now, %d of %d in all; not available: %s"), NumNew, Gathered->Num(), All.Num(),
			Missing.IsEmpty() ? TEXT("none") : *FString::Join(Missing, TEXT(", ")));
		return Failed.IsEmpty() ? Summary : FString::Printf(TEXT("FAILED: no loot or not bare: %s; %s"), *FString::Join(Failed, TEXT(", ")), *Summary);
	};

	// The fights may have corrupted the region: plants that need a pure land get a clean one first
	const auto SetRegionCorruption = [this, CorruptionBefore](float Corruption, bool bRemember)
	{
		AVaelCharacter* Player = GetPlayer();
		AVaelRegion* Region = Player != nullptr ? AVaelRegion::GetRegionAt(GetWorld(), Player->GetActorLocation()) : nullptr;
		if (Region == nullptr)
		{
			return FString(TEXT("no region"));
		}

		if (bRemember)
		{
			*CorruptionBefore = Region->GetCorruption();
		}

		const float Old = Region->GetCorruption();
		Region->SetCorruption(Corruption);
		return FString::Printf(TEXT("corruption %.0f -> %.0f"), Old, Corruption);
	};

	// Gamepad aim locks on to an enemy that comes close, and the player faces it
	Steps.Add({ TEXT("Auto aim arena"), [this]() { return PrepareArena(GetMutableDefault<UVaelPreacherData>(), 1); }, 1.0f });
	Steps.Add({ TEXT("Auto aim"), [this]()
	{
		const AVaelCharacter* Player = GetPlayer();
		const AVaelPlayerController* PlayerController = Player != nullptr ? Cast<AVaelPlayerController>(Player->GetController()) : nullptr;
		const AActor* Target = PlayerController != nullptr ? PlayerController->GetAimTarget() : nullptr;
		if (Target == nullptr)
		{
			return FString(TEXT("FAILED: no enemy locked on"));
		}

		const FVector ToTarget = (Target->GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D();
		const float Angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Player->GetActorForwardVector().GetSafeNormal2D(), ToTarget)));
		return FString::Printf(TEXT("%slocked on %s at %.0f cm, player faces it within %.0f degrees"), Angle > 20.0f ? TEXT("FAILED: ") : TEXT(""),
			*GetNameSafe(Target), FVector::Dist2D(Target->GetActorLocation(), Player->GetActorLocation()), Angle);
	}, 0.1f });

	// The main story of Act I once through: real talks with Edda, the village marker reached by walking there, the rest reported
	Steps.Add({ TEXT("Quests first half"), [this]()
	{
		AVaelCharacter* Player = GetPlayer();
		UVaelQuestSubsystem* Quests = UVaelQuestSubsystem::Get(this);
		AVaelNpc* Edda = nullptr;
		for (TActorIterator<AVaelNpc> It(GetWorld()); It; ++It)
		{
			Edda = It->GetDisplayName().ToString().StartsWith(TEXT("Edda")) ? *It : Edda;
		}

		AVaelQuestMarker* Village = nullptr;
		for (TActorIterator<AVaelQuestMarker> It(GetWorld()); It; ++It)
		{
			Village = It->GetMarkerId() == TEXT("Dorf") ? *It : Village;
		}

		if (Player == nullptr || Quests == nullptr || Edda == nullptr || Village == nullptr)
		{
			return FString(TEXT("FAILED: no player, quests, Edda or village marker"));
		}

		const FString Before = Quests->Describe();

		SmokeTalk(Edda, Player);
		for (int32 Cast = 0; Cast < 3; ++Cast)
		{
			Quests->NotifyCast(NAME_None);
		}
		SmokeTalk(Edda, Player);

		for (int32 Kill = 0; Kill < 6; ++Kill)
		{
			Quests->NotifyKill(TEXT("Glutkriecher"));
		}
		for (int32 Kill = 0; Kill < 3; ++Kill)
		{
			Quests->NotifyKill(TEXT("Aschharpyie"));
		}
		SmokeTalk(Edda, Player);

		// Beside the marker, on the road: the marker itself stands at the well
		bool bArrived = false;
		for (const FVector Offset : { FVector(350.0f, 0.0f, 0.0f), FVector(-350.0f, 0.0f, 0.0f), FVector(0.0f, 350.0f, 0.0f), FVector(0.0f, -350.0f, 0.0f) })
		{
			if (Player->TeleportTo(Village->GetActorLocation() + Offset + FVector(0.0f, 0.0f, 60.0f), Player->GetActorRotation()))
			{
				bArrived = true;
				break;
			}
		}

		return FString::Printf(TEXT("before: %s; %s"), *Before, bArrived ? TEXT("walking to the village") : TEXT("could not get into the village"));
	}, 1.2f });

	Steps.Add({ TEXT("Quests second half"), [this]()
	{
		AVaelCharacter* Player = GetPlayer();
		UVaelQuestSubsystem* Quests = UVaelQuestSubsystem::Get(this);
		AVaelNpc* Edda = nullptr;
		for (TActorIterator<AVaelNpc> It(GetWorld()); It; ++It)
		{
			Edda = It->GetDisplayName().ToString().StartsWith(TEXT("Edda")) ? *It : Edda;
		}

		const UVaelQuest* Village = Quests != nullptr ? Quests->FindQuest(TEXT("Q3_Kesselgrund")) : nullptr;
		if (Player == nullptr || Edda == nullptr || Village == nullptr)
		{
			return FString(TEXT("FAILED: no player, Edda or quest Q3"));
		}

		const bool bReachedByWalking = Quests->GetProgress(Village).Step >= 1;

		Quests->NotifyReach(TEXT("Brunnen"));
		SmokeTalk(Edda, Player);

		Quests->NotifyReach(TEXT("Orden"));
		for (int32 Kill = 0; Kill < 3; ++Kill)
		{
			Quests->NotifyKill(TEXT("Prediger"));
		}
		SmokeTalk(Edda, Player);

		Quests->NotifyReach(TEXT("Krater"));
		Quests->NotifyKill(TEXT("Glutkoenigin"));
		SmokeTalk(Edda, Player);

		const UVaelQuest* Last = Quests->FindQuest(TEXT("Q5_DieMutter"));
		const bool bOk = bReachedByWalking && Last != nullptr && Quests->GetProgress(Last).bDone;
		return FString::Printf(TEXT("%svillage marker %s; %s"), bOk ? TEXT("") : TEXT("FAILED: "), bReachedByWalking ? TEXT("reached by walking") : TEXT("NOT reached"), *Quests->Describe());
	}, 0.1f });

	// Gildenkontor: Maren takes the group in, a repeatable contract pays every player and is offered again
	Steps.Add({ TEXT("Contracts"), [this]()
	{
		AVaelCharacter* Player = GetPlayer();
		UVaelQuestSubsystem* Quests = UVaelQuestSubsystem::Get(this);
		AVaelNpc* Maren = nullptr;
		for (TActorIterator<AVaelNpc> It(GetWorld()); It; ++It)
		{
			Maren = It->GetDisplayName().ToString().StartsWith(TEXT("Maren")) ? *It : Maren;
		}

		const UVaelQuest* Contract = Quests != nullptr ? Quests->FindQuest(TEXT("K1_Brutpflege")) : nullptr;
		if (Player == nullptr || Maren == nullptr || Contract == nullptr)
		{
			return FString(TEXT("FAILED: no player, Maren or contract K1"));
		}

		const int32 CoinsBefore = Player->GetInventory()->GetCoins();
		Quests->NotifyReach(TEXT("Kontor"));
		SmokeTalk(Maren, Player);
		for (int32 Kill = 0; Kill < Contract->Steps[0].Count; ++Kill)
		{
			Quests->NotifyKill(TEXT("Glutkriecher"));
		}
		SmokeTalk(Maren, Player);

		const FVaelQuestProgress Progress = Quests->GetProgress(Contract);
		const int32 Earned = Player->GetInventory()->GetCoins() - CoinsBefore;
		const bool bOk = Earned == Contract->CoinReward && Progress.TimesDone == 1 && Progress.bActive && !Progress.bDone && Progress.Step == 0;
		return FString::Printf(TEXT("%s%d coins earned, K1 done %d times, offered again: %s"), bOk ? TEXT("") : TEXT("FAILED: "), Earned, Progress.TimesDone,
			Progress.bActive && !Progress.bDone ? TEXT("yes") : TEXT("no"));
	}, 0.1f });

	// The twist: the rumour, the proof in the Gildenspeicher, and the choice at the end of the act (kept in the camp, which changes nothing else)
	const TFunction<FString()> TwistStep = [this]()
	{
		AVaelCharacter* Player = GetPlayer();
		UVaelQuestSubsystem* Quests = UVaelQuestSubsystem::Get(this);
		AVaelNpc* Edda = nullptr;
		for (TActorIterator<AVaelNpc> It(GetWorld()); It; ++It)
		{
			Edda = It->GetDisplayName().ToString().StartsWith(TEXT("Edda")) ? *It : Edda;
		}

		AVaelQuestObject* Proof = nullptr;
		for (TActorIterator<AVaelQuestObject> It(GetWorld()); It; ++It)
		{
			Proof = It->GetEventName() == TEXT("BeweisGefunden") ? *It : Proof;
		}

		const UVaelQuest* Twist = Quests != nullptr ? Quests->FindQuest(TEXT("Q6_WasDieGildeWusste")) : nullptr;
		const UVaelQuest* After = Quests != nullptr ? Quests->FindQuest(TEXT("E3_ZwischenDenStuehlen")) : nullptr;
		AVaelPlayerController* Controller = Player != nullptr ? Player->GetController<AVaelPlayerController>() : nullptr;
		if (Controller == nullptr || Edda == nullptr || Proof == nullptr || Twist == nullptr || After == nullptr || Twist->Steps.Num() < 4 || Twist->Steps[3].Choices.Num() < 3)
		{
			return FString(TEXT("FAILED: no player, Edda, proof in the Gildenspeicher, quest Q6 with three answers or quest E3"));
		}

		// The first half in one go, the talk with the answers in a step of its own: a dialogue only opens once the pause of the last one is over
		if (Quests->GetProgress(Twist).Step < 3)
		{
			SmokeTalk(Edda, Player);
			const bool bLocked = !Proof->CanInteract(Player);
			Quests->NotifyReach(TEXT("Gildenspeicher"));
			const bool bWanted = Proof->CanInteract(Player);
			Proof->Interact(Player);
			Controller->EndDialogue();

			const bool bFound = bLocked && bWanted && Quests->GetProgress(Twist).Step == 3;
			return FString::Printf(TEXT("%sproof %s, %s"), bFound ? TEXT("") : TEXT("FAILED: "), bLocked && bWanted ? TEXT("only usable when wanted") : TEXT("usable at the wrong time"),
				Quests->GetProgress(Twist).Step == 3 ? TEXT("found") : TEXT("NOT found"));
		}

		// The talk offers the answers; closing it decides nothing
		const bool bLocked = true, bWanted = true;
		Edda->Interact(Player);
		const int32 NumAnswers = Controller->GetChoiceLabels().Num();
		Controller->EndDialogue();
		const bool bUndecided = Quests->GetProgress(Twist).Step == 3 && !Quests->GetProgress(Twist).bDone;

		TArray<FText> Reply;
		const bool bChosen = Quests->Choose(Twist, 2, Reply);
		const FVaelQuestChoice& Order = Twist->Steps[3].Choices[0];

		const bool bOk = bLocked && bWanted && NumAnswers == 4 && bUndecided && bChosen && !Reply.IsEmpty() && Quests->GetProgress(Twist).bDone
			&& Quests->HasFlag(TEXT("Beweis.Lager")) && Quests->GetProgress(After).bActive && FMath::IsNearlyEqual(Quests->GetGuildPriceFactor(), 1.0f)
			&& Order.GuildPriceFactor > 1.0f && Order.AggroFactors.Contains(TEXT("Prediger"));
		return FString::Printf(TEXT("%sproof %s, %d answers, undecided after closing: %s, kept in the camp: %s, follow-up %s"), bOk ? TEXT("") : TEXT("FAILED: "),
			bLocked && bWanted ? TEXT("only usable when wanted") : TEXT("usable at the wrong time"), NumAnswers, bUndecided ? TEXT("yes") : TEXT("no"),
			Quests->HasFlag(TEXT("Beweis.Lager")) ? TEXT("yes") : TEXT("no"), Quests->GetProgress(After).bActive ? TEXT("active") : TEXT("missing"));
	};
	Steps.Add({ TEXT("Twist"), TwistStep, 0.5f });
	Steps.Add({ TEXT("Choice at the end of the act"), TwistStep, 0.1f });

	// Albrun at the camp studies the samples of the group; bystanders stand around for the gossip
	Steps.Add({ TEXT("Albrun and bystanders"), [this]()
	{
		AVaelCharacter* Player = GetPlayer();
		UVaelQuestSubsystem* Quests = UVaelQuestSubsystem::Get(this);
		UVaelCompendiumSubsystem* Compendium = UVaelCompendiumSubsystem::Get(this);
		AVaelNpc* Albrun = nullptr;
		int32 NumPeople = 0;
		for (TActorIterator<AVaelNpc> It(GetWorld()); It; ++It)
		{
			Albrun = It->GetDisplayName().ToString() == TEXT("Albrun") ? *It : Albrun;
			++NumPeople;
		}

		const UVaelQuest* Study = Quests != nullptr ? Quests->FindQuest(TEXT("A1_AlbrunsKompendium")) : nullptr;
		const TArray<const UVaelCompendiumEntry*> Beasts = Compendium != nullptr ? Compendium->GetEntries(UVaelCompendiumSubsystem::GetBooks()[0]) : TArray<const UVaelCompendiumEntry*>();
		if (Player == nullptr || Albrun == nullptr || Study == nullptr || Beasts.IsEmpty())
		{
			return FString(TEXT("FAILED: no player, Albrun, quest A1 or bestiary entry"));
		}

		const UVaelCompendiumEntry* Entry = Beasts[0];
		for (int32 Sample = 0; Sample < Entry->StudyCount; ++Sample)
		{
			Compendium->AddSample(Entry->SubjectId);
		}

		const bool bNews = Albrun->HasNews();
		SmokeTalk(Albrun, Player);
		const bool bResearched = Compendium->GetProgress(Entry->SubjectId).Stage == EVaelResearchStage::Researched;
		const int32 QuestStep = Quests->GetProgress(Study).Step;

		const bool bOk = bNews && bResearched && QuestStep == 2 && NumPeople >= 6;
		return FString::Printf(TEXT("%s%s %s by Albrun, quest A1 at step %d of 3, %d people in the level"), bOk ? TEXT("") : TEXT("FAILED: "), *Entry->DisplayName.ToString(),
			bResearched ? TEXT("researched") : TEXT("NOT researched"), QuestStep + 1, NumPeople);
	}, 0.5f });

	// Healing potion from the start, Maren's shop, and the camp as a safe zone
	Steps.Add({ TEXT("Potion and safe zone"), [this]()
	{
		AVaelCharacter* Player = GetPlayer();
		AVaelNpc* Edda = nullptr;
		for (TActorIterator<AVaelNpc> It(GetWorld()); It; ++It)
		{
			Edda = It->GetDisplayName().ToString().StartsWith(TEXT("Edda")) ? *It : Edda;
		}
		if (Player == nullptr || Edda == nullptr)
		{
			return FString(TEXT("FAILED: no player or Edda"));
		}

		// Out in the field: hurt, then drink
		Player->TeleportTo(StartLocation, StartRotation);
		const int32 PotionsBefore = Player->GetPotionCount();
		UVaelCombatStatics::DealDamage(nullptr, Player, Player->GetMaxHealth() * 0.6f, EVaelElement::Fire);
		const float Hurt = Player->GetHealth();
		const bool bDrank = Player->DrinkPotion();
		const float Healed = Player->GetHealth() - Hurt;

		// At the campfire nothing hurts
		const FVector Camp = Edda->GetActorLocation() + Edda->GetActorForwardVector() * 150.0f;
		Player->TeleportTo(Camp, Player->GetActorRotation());
		const bool bSafe = AVaelSafeZone::IsSafe(GetWorld(), Player->GetActorLocation());
		const float BeforeHit = Player->GetHealth();
		UVaelCombatStatics::DealDamage(nullptr, Player, 30.0f, EVaelElement::Fire);
		const bool bUnhurt = FMath::IsNearlyEqual(Player->GetHealth(), BeforeHit);
		Player->TeleportTo(StartLocation, StartRotation);
		RefillPlayer();

		const UVaelShop* Shop = LoadObject<UVaelShop>(nullptr, TEXT("/Game/Vael/Items/Shops/DA_Shop_Gildenkontor.DA_Shop_Gildenkontor"));
		const bool bOk = PotionsBefore >= 2 && bDrank && Healed > 1.0f && Player->GetPotionCount() == PotionsBefore - 1 && bSafe && bUnhurt && Shop != nullptr && !Shop->Offers.IsEmpty();
		return FString::Printf(TEXT("%s%d potions at start, drank: %s (+%.0f), camp safe: %s, hurt in camp: %s, shop offers: %d"), bOk ? TEXT("") : TEXT("FAILED: "),
			PotionsBefore, bDrank ? TEXT("yes") : TEXT("no"), Healed, bSafe ? TEXT("yes") : TEXT("no"), bUnhurt ? TEXT("no") : TEXT("YES"), Shop != nullptr ? Shop->Offers.Num() : 0);
	}, 0.1f });

	// Glowing stones: water lets out their heat and they stop giving fire, fire heats them up again
	Steps.Add({ TEXT("Steam stones"), [this]()
	{
		int32 NumStones = 0;
		int32 NumCooled = 0;
		int32 NumReheated = 0;

		for (TActorIterator<AVaelHarvestable> It(GetWorld()); It; ++It)
		{
			const FVector Location = It->GetActorLocation();
			if (It->GetCompendiumId() != TEXT("Glutstein") || !It->ProvidesElement(EVaelElement::Fire, Location))
			{
				continue;
			}

			++NumStones;
			AVaelHarvestable::NotifySpellImpact(GetWorld(), Location, 50.0f, EVaelElement::Water);
			NumCooled += It->ProvidesElement(EVaelElement::Fire, Location) ? 0 : 1;

			AVaelHarvestable::NotifySpellImpact(GetWorld(), Location, 50.0f, EVaelElement::Fire);
			NumReheated += It->ProvidesElement(EVaelElement::Fire, Location) ? 1 : 0;
		}

		const bool bOk = NumStones > 0 && NumCooled == NumStones && NumReheated == NumStones;
		return FString::Printf(TEXT("%s%d glowing stones, %d cooled by water, %d heated again by fire"), bOk ? TEXT("") : TEXT("FAILED: "), NumStones, NumCooled, NumReheated);
	}, 0.1f });

	// A storm makes the whistling caps whistle (seen in the log with LogVael Verbose)
	Steps.Add({ TEXT("Storm"), [this]()
	{
		AVaelCharacter* Player = GetPlayer();
		AVaelRegion* Region = Player != nullptr ? AVaelRegion::GetRegionAt(GetWorld(), Player->GetActorLocation()) : nullptr;
		if (Region == nullptr)
		{
			return FString(TEXT("no region"));
		}

		Region->SetWeather(EVaelWeather::Storm);
		return FString(TEXT("storm"));
	}, 7.0f });

	Steps.Add({ TEXT("Cleanse the region"), [SetRegionCorruption]() { return SetRegionCorruption(0.0f, true); }, 1.5f });

	// Wind: it rises, blows from one side, gives air everywhere and shows around the player
	Steps.Add({ TEXT("Wind"), [this]()
	{
		AVaelCharacter* Player = GetPlayer();
		AVaelRegion* Region = Player != nullptr ? AVaelRegion::GetRegionAt(GetWorld(), Player->GetActorLocation()) : nullptr;
		if (Region == nullptr)
		{
			return FString(TEXT("no region"));
		}

		Region->SetWeather(EVaelWeather::Wind);
		return FString(TEXT("wind"));
	}, 5.0f });
	Steps.Add({ TEXT("Wind blows"), [this]()
	{
		AVaelCharacter* Player = GetPlayer();
		const AVaelRegion* Region = Player != nullptr ? AVaelRegion::GetRegionAt(GetWorld(), Player->GetActorLocation()) : nullptr;
		const float Speed = Region != nullptr ? Region->GetWind().Size() : 0.0f;
		const bool bAir = UVaelEnvironmentStatics::IsElementInEnvironment(Player, EVaelElement::Air);
		int32 NumLooks = 0;
		for (TActorIterator<AVaelWeatherWind> It(GetWorld()); It; ++It)
		{
			++NumLooks;
		}

		const bool bOk = Speed > 100.0f && bAir && NumLooks == 1;
		return FString::Printf(TEXT("%swind %.0f cm/s, air nearby %s, %d wind looks"), bOk ? TEXT("") : TEXT("FAILED: "), Speed, bAir ? TEXT("yes") : TEXT("no"), NumLooks);
	}, 0.1f });

	// Rain lets the Keimpelz sprout, and about half of them carry seeds
	Steps.Add({ TEXT("Rain"), [this]()
	{
		AVaelCharacter* Player = GetPlayer();
		AVaelRegion* Region = Player != nullptr ? AVaelRegion::GetRegionAt(GetWorld(), Player->GetActorLocation()) : nullptr;
		if (Region == nullptr)
		{
			return FString(TEXT("no region"));
		}

		Region->SetWeather(EVaelWeather::Rain);
		return FString(TEXT("rain"));
	}, 6.0f });
	Steps.Add({ TEXT("Rain falls"), [this]()
	{
		AVaelCharacter* Player = GetPlayer();
		const AVaelRegion* Region = Player != nullptr ? AVaelRegion::GetRegionAt(GetWorld(), Player->GetActorLocation()) : nullptr;
		int32 NumLooks = 0;
		for (TActorIterator<AVaelWeatherRain> It(GetWorld()); It; ++It)
		{
			++NumLooks;
		}
		const float Strength = Region != nullptr ? Region->GetRainStrength() : 0.0f;
		const bool bOk = Strength > 0.5f && NumLooks == 1;
		return FString::Printf(TEXT("%srain strength %.2f, %d rain looks"), bOk ? TEXT("") : TEXT("FAILED: "), Strength, NumLooks);
	}, 0.1f });

	// Day and night: the clock jumps into the night, the sky follows, the Prophet's shadow comes easier
	Steps.Add({ TEXT("Night falls"), [this]()
	{
		UVaelTimeOfDay* Clock = UVaelTimeOfDay::Get(GetWorld());
		if (Clock == nullptr)
		{
			return FString(TEXT("FAILED: no clock"));
		}
		Clock->SetHour(23.0f);
		return FString(TEXT("23:00"));
	}, 1.0f });
	Steps.Add({ TEXT("Night"), [this]()
	{
		UVaelTimeOfDay* Clock = UVaelTimeOfDay::Get(GetWorld());
		int32 NumSkies = 0;
		for (TActorIterator<AVaelSky> It(GetWorld()); It; ++It)
		{
			++NumSkies;
		}
		const float Night = Clock != nullptr ? Clock->GetNightFactor() : 0.0f;
		const float Shadow = Clock != nullptr ? Clock->GetPathGain(EVaelPath::Shadow) : 0.0f;
		const bool bOk = NumSkies == 1 && Night > 0.99f && Shadow > 1.0f && Clock->GetPhase() == EVaelDayPhase::Night;
		if (Clock != nullptr)
		{
			Clock->SetHour(12.0f);
		}
		return FString::Printf(TEXT("%s%d skies, night %.2f, shadow gain %.2f; back to noon"), bOk ? TEXT("") : TEXT("FAILED: "), NumSkies, Night, Shadow);
	}, 0.5f });

	// The air: enough steam over one place rains down as a shower, enough heat rises as a thermal that gives air
	const auto FindLocalWeathers = [this](EVaelLocalWeather Kind)
	{
		int32 Count = 0;
		for (TActorIterator<AVaelLocalWeather> It(GetWorld()); It; ++It)
		{
			Count += It->GetKind() == Kind ? 1 : 0;
		}
		return Count;
	};
	const auto ClearLocalWeathers = [this]()
	{
		for (TActorIterator<AVaelLocalWeather> It(GetWorld()); It; ++It)
		{
			It->Destroy();
		}
	};
	Steps.Add({ TEXT("Air gathers steam"), [this]()
	{
		const AVaelCharacter* Player = GetPlayer();
		if (Player == nullptr)
		{
			return FString(TEXT("no player"));
		}
		UVaelAtmosphere::Add(GetWorld(), Player->GetActorLocation(), EVaelAir::Moisture, 100.0f);
		return FString(TEXT("100 moisture"));
	}, 1.5f });
	Steps.Add({ TEXT("Shower"), [this, FindLocalWeathers, ClearLocalWeathers]()
	{
		const AVaelCharacter* Player = GetPlayer();
		const int32 NumShowers = FindLocalWeathers(EVaelLocalWeather::Shower);
		const bool bRaining = Player != nullptr && AVaelLocalWeather::IsRainingAt(GetWorld(), Player->GetActorLocation());
		ClearLocalWeathers();
		if (Player != nullptr)
		{
			UVaelAtmosphere::Add(GetWorld(), Player->GetActorLocation(), EVaelAir::Heat, 200.0f);
		}
		return FString::Printf(TEXT("%s%d showers, raining on the player: %s; 200 heat added"), NumShowers == 1 ? TEXT("") : TEXT("FAILED: "), NumShowers, bRaining ? TEXT("yes") : TEXT("no (still rising)"));
	}, 6.0f });
	Steps.Add({ TEXT("Thermal"), [this, FindLocalWeathers, ClearLocalWeathers]()
	{
		const AVaelCharacter* Player = GetPlayer();
		const int32 NumThermals = FindLocalWeathers(EVaelLocalWeather::Thermal);
		const FVector Wind = Player != nullptr ? AVaelLocalWeather::GetWindAt(GetWorld(), Player->GetActorLocation() + FVector(1200.0f, 0.0f, 0.0f)) : FVector::ZeroVector;
		ClearLocalWeathers();
		const bool bOk = NumThermals == 1 && Wind.Size() > 50.0f;
		return FString::Printf(TEXT("%s%d thermals, wind 12 m off the middle %.0f cm/s"), bOk ? TEXT("") : TEXT("FAILED: "), NumThermals, Wind.Size());
	}, 0.5f });
	Steps.Add({ TEXT("Keimpelz"), [this]()
	{
		int32 NumSprouted = 0;
		int32 NumSeeded = 0;
		for (TActorIterator<AVaelHarvestable> It(GetWorld()); It; ++It)
		{
			if (It->GetCompendiumId() == TEXT("Russmoos"))
			{
				NumSprouted += It->IsAvailable() ? 1 : 0;
				NumSeeded += It->IsSeeded() ? 1 : 0;
			}
		}

		return FString::Printf(TEXT("%s%d sprouted in the rain, %d of them carry seeds"), NumSprouted > 0 ? TEXT("") : TEXT("FAILED: "), NumSprouted, NumSeeded);
	}, 0.1f });

	// Rain water gathers into a few puddles around the player
	Steps.Add({ TEXT("Rain puddles"), [this]()
	{
		int32 NumPuddles = 0;
		float Water = 0.0f;
		for (TActorIterator<AVaelWaterPuddle> It(GetWorld()); It; ++It)
		{
			if (It->GetInstigator() == nullptr)
			{
				++NumPuddles;
				Water += It->GetWater();
			}
		}

		return FString::Printf(TEXT("%s%d rain puddles holding %.1f water"), NumPuddles > 0 ? TEXT("") : TEXT("FAILED: "), NumPuddles, Water);
	}, 0.1f });

	// A surprise seed goes into Albrun's pot, grows, is researched; then one of its seeds is sown
	Steps.Add({ TEXT("Garden"), [this]()
	{
		AVaelCharacter* Player = GetPlayer();
		UVaelGardenSubsystem* Garden = UVaelGardenSubsystem::Get(this);
		AVaelSeedPot* Pot = nullptr;
		for (TActorIterator<AVaelSeedPot> It(GetWorld()); It; ++It)
		{
			Pot = *It;
		}

		const UVaelMaterial* Surprise = UVaelGardenSettings::Get()->SurpriseSeed.LoadSynchronous();
		if (Player == nullptr || Garden == nullptr || Pot == nullptr || Surprise == nullptr)
		{
			return FString::Printf(TEXT("FAILED: %s"), Pot == nullptr ? TEXT("no pot beside Albrun") : TEXT("no player, garden or surprise seed"));
		}

		const int32 KnownBefore = Garden->GetNumResearched();
		Player->GetMaterialBag()->AddMaterial(Surprise, 1);
		Pot->Interact(Player);
		const bool bGrowing = Pot->GetPotState() == EVaelPotState::Growing;
		const FName Kind = Garden->PotPlant;

		Pot->FinishGrowing();
		Pot->Interact(Player);
		const bool bResearched = Garden->GetNumResearched() == KnownBefore + 1 && Garden->IsResearched(Kind);

		const int32 SownBefore = Garden->GetNumSown();
		const bool bSown = Garden->Sow(Player) != nullptr && Garden->GetNumSown() == SownBefore + 1;

		const bool bOk = bGrowing && bResearched && bSown;
		return FString::Printf(TEXT("%spot: %s, %s %s, a seed of it %s"), bOk ? TEXT("") : TEXT("FAILED: "), bGrowing ? TEXT("seed grows") : TEXT("does NOT grow"),
			*Kind.ToString(), bResearched ? TEXT("researched") : TEXT("NOT researched"), bSown ? TEXT("sown") : TEXT("NOT sown"));
	}, 8.0f });

	// Green spreads around what was sown, and tufts sprout in it
	Steps.Add({ TEXT("Green"), [this]()
	{
		int32 NumPlants = 0;
		int32 NumTufts = 0;
		float Radius = 0.0f;
		for (TActorIterator<AVaelSownPlant> It(GetWorld()); It; ++It)
		{
			++NumPlants;
			NumTufts += It->GetNumTufts();
			Radius = FMath::Max(Radius, It->GetGreenRadius());
		}

		const bool bOk = NumPlants > 0 && Radius > UVaelGardenSettings::Get()->GreenStartRadius;
		return FString::Printf(TEXT("%s%d sown plants, green reaches %.0f cm, %d tufts"), bOk ? TEXT("") : TEXT("FAILED: "), NumPlants, Radius, NumTufts);
	}, 0.1f });

	// A marked sower grows the shadow form; the grown plant is harvested once and gives its ingredients
	Steps.Add({ TEXT("Garden forms"), [this]()
	{
		AVaelCharacter* Player = GetPlayer();
		UVaelGardenSubsystem* Garden = UVaelGardenSubsystem::Get(this);
		UAbilitySystemComponent* AbilitySystem = Player != nullptr ? Player->GetAbilitySystemComponent() : nullptr;
		AVaelSownPlant* Grown = nullptr;
		for (TActorIterator<AVaelSownPlant> It(GetWorld()); It && Grown == nullptr; ++It)
		{
			Grown = It->IsGrown() && !It->IsHarvested() ? *It : nullptr;
		}
		if (Garden == nullptr || AbilitySystem == nullptr || Grown == nullptr)
		{
			return FString(TEXT("FAILED: no player, garden or grown plant"));
		}

		const bool bFormsRight = !UVaelGardenSubsystem::RollShadowForm(0.0f) && UVaelGardenSubsystem::RollShadowForm(100.0f);

		// Sown a few steps aside, by a player deep in the Mark
		const int32 ShadowBefore = Garden->GetNumShadow();
		const float CorruptionBefore = AbilitySystem->GetNumericAttribute(UVaelAttributeSet::GetCorruptionAttribute());
		AbilitySystem->SetNumericAttributeBase(UVaelAttributeSet::GetCorruptionAttribute(), 100.0f);
		const FVector Home = Player->GetActorLocation();
		Player->SetActorLocation(Home + Player->GetActorRightVector() * 400.0f);
		const AVaelSownPlant* ShadowPlant = Garden->CanSow(Player) ? Garden->Sow(Player) : nullptr;
		Player->SetActorLocation(Home);
		const bool bShadowSown = ShadowPlant == nullptr || (ShadowPlant->IsShadowForm() && Garden->GetNumShadow() == ShadowBefore + 1);
		AbilitySystem->SetNumericAttributeBase(UVaelAttributeSet::GetCorruptionAttribute(), CorruptionBefore);

		// Harvested once, never twice
		const FVaelSeedPlant* Kind = UVaelGardenSubsystem::FindPlant(Grown->GetPlantId());
		const UVaelMaterial* First = Kind != nullptr && !Kind->LightHarvest.IsEmpty() ? (Grown->IsShadowForm() ? Kind->ShadowHarvest[0] : Kind->LightHarvest[0]).LoadSynchronous() : nullptr;
		const int32 Before = First != nullptr ? Player->GetMaterialBag()->GetCount(First) : 0;
		const bool bHarvested = Garden->Harvest(Grown, Player) && Grown->IsHarvested();
		const int32 Got = First != nullptr ? Player->GetMaterialBag()->GetCount(First) - Before : 0;
		const bool bOnce = !Garden->Harvest(Grown, Player) && !Grown->CanInteract(Player);

		const bool bOk = bFormsRight && bShadowSown && bHarvested && Got > 0 && bOnce;
		return FString::Printf(TEXT("%sforms follow the sower: %s; shadow form %s; %s harvested: %s, %d %s; second harvest %s"), bOk ? TEXT("") : TEXT("FAILED: "),
			bFormsRight ? TEXT("yes") : TEXT("NO"), ShadowPlant == nullptr ? TEXT("not tried (no seed left)") : bShadowSown ? TEXT("sown") : TEXT("NOT sown"),
			*Grown->GetFormName().ToString(), bHarvested ? TEXT("yes") : TEXT("NO"), Got, First != nullptr ? *First->DisplayName.ToString() : TEXT("(no ingredient)"),
			bOnce ? TEXT("refused") : TEXT("NOT refused"));
	}, 0.1f });

	Steps.Add({ TEXT("Harvest"), Harvest, 0.1f });
	Steps.Add({ TEXT("Corrupt the region"), [SetRegionCorruption]() { return SetRegionCorruption(70.0f, false); }, 1.5f });

	Steps.Add({ TEXT("Harvest under the Mark"), [this, Harvest, CorruptionBefore]()
	{
		const FString Result = Harvest();

		AVaelCharacter* Player = GetPlayer();
		if (AVaelRegion* Region = Player != nullptr ? AVaelRegion::GetRegionAt(GetWorld(), Player->GetActorLocation()) : nullptr)
		{
			Region->SetCorruption(*CorruptionBefore);
		}

		return Result;
	}, 0.1f });

	// Save game: everything written to memory, wiped and read back has to be the same (no file is touched)
	Steps.Add({ TEXT("Save game"), [this]()
	{
		AVaelCharacter* Player = GetPlayer();
		UVaelSaveSubsystem* Save = UVaelSaveSubsystem::Get(this);
		AVaelPlayerController* Controller = Player != nullptr ? Player->GetController<AVaelPlayerController>() : nullptr;
		if (Controller == nullptr || Save == nullptr)
		{
			return FString(TEXT("FAILED: no player or save subsystem"));
		}

		Player->GetInventory()->AddCoins(7);
		return Save->TestRoundTrip(Player, Controller->GetPlayerSlot());
	}, 0.1f });

	// The fights should have filled the bestiary: sighted, observed and defeated
	Steps.Add({ TEXT("Compendium"), [this]()
	{
		UVaelCompendiumSubsystem* Compendium = UVaelCompendiumSubsystem::Get(this);
		if (Compendium == nullptr)
		{
			return FString(TEXT("FAILED: no compendium"));
		}

		const FString Before = Compendium->Describe();
		const int32 NumStudied = Compendium->StudyAtCamp();
		return FString::Printf(TEXT("%s; %d researched by study"), *Before, NumStudied);
	}, 0.1f });

	StepTimeLeft = SmokeWarmUp;
	UE_LOG(LogVael, Display, TEXT("Smoke test: %d steps planned"), Steps.Num());
}

void AVaelSmokeTest::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (LogCatcher != nullptr)
	{
		GLog->RemoveOutputDevice(LogCatcher);
		delete LogCatcher;
		LogCatcher = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void AVaelSmokeTest::RefillPlayer() const
{
	AVaelCharacter* Player = GetPlayer();
	UAbilitySystemComponent* AbilitySystem = Player != nullptr ? Player->GetAbilitySystemComponent() : nullptr;
	if (AbilitySystem == nullptr)
	{
		return;
	}

	AbilitySystem->SetNumericAttributeBase(UVaelAttributeSet::GetHealthAttribute(), Player->GetMaxHealth());
	AbilitySystem->SetNumericAttributeBase(UVaelAttributeSet::GetManaAttribute(), Player->GetMaxMana());
	AbilitySystem->SetNumericAttributeBase(UVaelAttributeSet::GetCorruptionAttribute(), 0.0f);
}

FString AVaelSmokeTest::PrepareArena(UVaelCreatureData* Data, int32 Count) const
{
	AVaelCharacter* Player = GetPlayer();
	AVaelGameMode* GameMode = GetWorld()->GetAuthGameMode<AVaelGameMode>();
	if (Player == nullptr || GameMode == nullptr)
	{
		return TEXT("FAILED: no player or game mode");
	}

	for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
	{
		if (!It->IsDead())
		{
			UVaelCombatStatics::DealDamage(nullptr, *It, It->GetHealth() + 1000.0f, EVaelElement::Earth);
		}
	}

	Player->TeleportTo(StartLocation, StartRotation);
	RefillPlayer();

	const FVector Center = StartLocation + StartRotation.Vector().GetSafeNormal2D() * SmokeEnemyDistance;

	TSet<AVaelCreature*> Before;
	for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
	{
		Before.Add(*It);
	}

	const int32 NumSpawned = GameMode->SpawnCreatureGroup(Data, Center, Count, SmokeEnemySpread + Data->CollisionRadius);

	// Remember the new ones to see later what the step did to them
	ArenaCreatures.Reset();
	ArenaStartHealth.Reset();
	for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
	{
		if (!Before.Contains(*It))
		{
			ArenaCreatures.Add(*It);
			ArenaStartHealth.Add(It->GetHealth());
		}
	}

	return FString::Printf(TEXT("%d x %s spawned"), NumSpawned, *Data->DisplayName.ToString());
}

FString AVaelSmokeTest::CastFormula(const UVaelFormula* Formula) const
{
	AVaelCharacter* Player = GetPlayer();
	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if (Player == nullptr || PlayerController == nullptr)
	{
		return TEXT("FAILED: no player");
	}

	UVaelElementComponent* ElementComponent = Player->GetElementComponent();
	ElementComponent->ClearQueue();

	for (const EVaelElement Element : Formula->Elements)
	{
		ElementComponent->AddElement(Element);
	}

	// Aim at the middle of the enemies, as a player would
	FVector Middle = FVector::ZeroVector;
	int32 NumAlive = 0;
	for (const TWeakObjectPtr<AVaelCreature>& Creature : ArenaCreatures)
	{
		if (Creature.IsValid() && !Creature->IsDead())
		{
			Middle += Creature->GetActorLocation();
			++NumAlive;
		}
	}

	const FRotator Aim = NumAlive > 0 ? (Middle / NumAlive - Player->GetActorLocation()).GetSafeNormal2D().Rotation() : StartRotation;
	PlayerController->SetControlRotation(Aim);
	Player->SetActorRotation(Aim);

	const EVaelCastResult Result = ElementComponent->CastQueue();
	return FString::Printf(TEXT("cast: %s"), *UEnum::GetValueAsString(Result));
}

FString AVaelSmokeTest::CountLingeringActors() const
{
	TMap<FString, int32> Counts;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		Counts.FindOrAdd(It->GetClass()->GetName())++;
	}

	Counts.ValueSort([](int32 A, int32 B) { return A > B; });

	TArray<FString> Lines;
	for (const TPair<FString, int32>& Count : Counts)
	{
		if (Count.Value > SmokeLingerLimit && (Count.Key.StartsWith(TEXT("Vael")) || Count.Key.StartsWith(TEXT("BP_"))))
		{
			Lines.Add(FString::Printf(TEXT("%s x%d"), *Count.Key, Count.Value));
		}
	}

	return Lines.IsEmpty() ? FString(TEXT("none above the limit")) : TEXT("LINGERING: ") + FString::Join(Lines, TEXT(", "));
}

void AVaelSmokeTest::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	RefillPlayer();

	StepTimeLeft -= DeltaSeconds;
	if (StepTimeLeft > 0.0f)
	{
		return;
	}

	// What the log said during the step that just ended belongs to it
	if (Steps.IsValidIndex(CurrentStep))
	{
		if (!ArenaCreatures.IsEmpty())
		{
			Report.Add(TEXT("    ") + DescribeArena());
			ArenaCreatures.Reset();
		}

		TArray<FString> Warnings;
		TArray<FString> Errors;
		LogCatcher->Take(Warnings, Errors);

		TotalWarnings += Warnings.Num();
		TotalErrors += Errors.Num();

		for (int32 LineIndex = 0; LineIndex < FMath::Min(Errors.Num(), SmokeMaxLinesPerStep); ++LineIndex)
		{
			Report.Add(TEXT("    ERROR ") + Errors[LineIndex]);
		}

		for (int32 LineIndex = 0; LineIndex < FMath::Min(Warnings.Num(), SmokeMaxLinesPerStep); ++LineIndex)
		{
			Report.Add(TEXT("    warning ") + Warnings[LineIndex]);
		}

		if (Errors.Num() + Warnings.Num() > 2 * SmokeMaxLinesPerStep)
		{
			Report.Add(FString::Printf(TEXT("    (%d errors, %d warnings in this step)"), Errors.Num(), Warnings.Num()));
		}
	}

	++CurrentStep;
	if (!Steps.IsValidIndex(CurrentStep))
	{
		Finish();
		return;
	}

	const FStep& Step = Steps[CurrentStep];
	const FString Outcome = Step.Run();
	Report.Add(FString::Printf(TEXT("[%02d] %s: %s"), CurrentStep, *Step.Name, *Outcome));
	UE_LOG(LogVael, Display, TEXT("Smoke test step %d/%d %s: %s"), CurrentStep + 1, Steps.Num(), *Step.Name, *Outcome);

	StepTimeLeft = Step.Duration;
}

void AVaelSmokeTest::Finish()
{
	Report.Add(TEXT(""));
	Report.Add(FString::Printf(TEXT("Done: %d errors, %d warnings"), TotalErrors, TotalWarnings));

	const FString ReportPath = FPaths::ProjectSavedDir() / TEXT("Logs") / TEXT("VaelSmokeTest.txt");
	FFileHelper::SaveStringArrayToFile(Report, *ReportPath, FFileHelper::EEncodingOptions::ForceUTF8);

	UE_LOG(LogVael, Display, TEXT("Smoke test finished: %d errors, %d warnings, report in %s"), TotalErrors, TotalWarnings, *ReportPath);

	SetActorTickEnabled(false);
	FPlatformMisc::RequestExit(false);
}

FString AVaelSmokeTest::DescribeArena() const
{
	float Damage = 0.0f;
	int32 NumKilled = 0;
	int32 NumServants = 0;

	for (int32 Index = 0; Index < ArenaCreatures.Num(); ++Index)
	{
		const AVaelCreature* Creature = ArenaCreatures[Index].Get();
		if (Creature == nullptr || Creature->IsDead())
		{
			++NumKilled;
			Damage += ArenaStartHealth[Index];
			continue;
		}

		Damage += FMath::Max(0.0f, ArenaStartHealth[Index] - Creature->GetHealth());
	}

	for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
	{
		NumServants += It->IsOnPlayerSide() && !It->IsDead() ? 1 : 0;
	}

	return FString::Printf(TEXT("result: %.0f damage, %d of %d killed%s"), Damage, NumKilled, ArenaCreatures.Num(),
		NumServants > 0 ? *FString::Printf(TEXT(", %d risen servants"), NumServants) : TEXT(""));
}
