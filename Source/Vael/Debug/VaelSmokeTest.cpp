// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debug/VaelSmokeTest.h"
#include "AbilitySystemComponent.h"
#include "Combat/VaelAttributeSet.h"
#include "Combat/VaelCombatStatics.h"
#include "Compendium/VaelCompendiumSubsystem.h"
#include "Creatures/VaelCreature.h"
#include "Creatures/VaelCreatureData.h"
#include "Creatures/VaelEmberQueen.h"
#include "Creatures/VaelHornBeetle.h"
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
#include "World/VaelSafeZone.h"
#include "Story/VaelNpc.h"
#include "Story/VaelQuestMarker.h"
#include "Story/VaelQuestSubsystem.h"
#include "Player/VaelCharacter.h"
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

/** Catches warnings and errors the log writes while the check runs */
class FLogCatcher : public FOutputDevice
{
public:

	virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		const ELogVerbosity::Type Level = static_cast<ELogVerbosity::Type>(Verbosity & ELogVerbosity::VerbosityMask);
		if (Level != ELogVerbosity::Error && Level != ELogVerbosity::Warning && Level != ELogVerbosity::Fatal)
		{
			return;
		}

		FScopeLock Lock(&Guard);
		(Level == ELogVerbosity::Warning ? Warnings : Errors).Add(FString::Printf(TEXT("%s: %s"), *Category.ToString(), Message));
	}

	virtual bool CanBeUsedOnAnyThread() const override { return true; }

	/** Hands out what was caught since the last call and forgets it */
	void Take(TArray<FString>& OutWarnings, TArray<FString>& OutErrors)
	{
		FScopeLock Lock(&Guard);
		OutWarnings = MoveTemp(Warnings);
		OutErrors = MoveTemp(Errors);
		Warnings.Reset();
		Errors.Reset();
	}

private:

	FCriticalSection Guard;
	TArray<FString> Warnings;
	TArray<FString> Errors;
};

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
		GetMutableDefault<UVaelEmberQueenData>(),
		GetMutableDefault<UVaelSourceGuardianData>(),
	};

	for (UVaelCreatureData* Data : Only.IsEmpty() ? Creatures : TArray<UVaelCreatureData*>())
	{
		Steps.Add({ FString::Printf(TEXT("Creature %s"), *Data->DisplayName.ToString()), [this, Data]() { return PrepareArena(Data, 2); }, SmokeFightTime });
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
