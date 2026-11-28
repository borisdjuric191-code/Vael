// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/VaelCheatManager.h"
#include "Engine/StaticMeshActor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Nature/VaelHarvestable.h"
#include "Nature/VaelHarvestableData.h"
#include "Nature/VaelPlantLifeComponent.h"
#include "Nature/VaelSparkleComponent.h"
#include "World/VaelGround.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "UI/VaelWorldMap.h"
#include "World/VaelProgressSubsystem.h"
#include "World/VaelTravel.h"
#include "World/VaelIndoorLevel.h"
#include "World/VaelBalance.h"
#include "World/VaelRefugiumClutch.h"
#include "Items/VaelBrewing.h"
#include "Items/VaelPotionEffects.h"
#include "Creatures/VaelTrainingDummy.h"
#include "World/VaelCave.h"
#include "Combat/VaelAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Compendium/VaelCompendiumSubsystem.h"
#include "Engineer/VaelEngineerIdeas.h"
#include "Engineer/VaelForge.h"
#include "Engineer/VaelWorkbench.h"
#include "Items/VaelStorageChest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelCombatStatics.h"
#include "Creatures/VaelCreature.h"
#include "Creatures/VaelDeathMotion.h"
#include "Creatures/VaelAshRoller.h"
#include "Combat/VaelCravingComponent.h"
#include "Compendium/VaelCompendiumSettings.h"
#include "Creatures/VaelCreatureData.h"
#include "Creatures/VaelEncounterDirector.h"
#include "Creatures/VaelKelchreisser.h"
#include "Creatures/VaelRankenluchs.h"
#include "Creatures/VaelNightCreature.h"
#include "Creatures/VaelPfuhlnatter.h"
#include "Creatures/VaelPfuhlnatterPopulation.h"
#include "Creatures/VaelThistleSpider.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "World/VaelCaveDrips.h"
#include "HAL/IConsoleManager.h"
#include "Containers/Ticker.h"
#include "GameFramework/PlayerController.h"
#include "Items/VaelInventory.h"
#include "Engineer/VaelEngineerComponent.h"
#include "Engineer/VaelEngineerDevice.h"
#include "Magic/VaelGroundArea.h"
#include "World/VaelSafeZone.h"
#include "Engineer/VaelEngineerGear.h"
#include "Engineer/VaelEngineerShot.h"
#include "Items/VaelItemData.h"
#include "Items/VaelItemDrop.h"
#include "Items/VaelItemSettings.h"
#include "Items/VaelMaterial.h"
#include "Items/VaelMaterialBag.h"
#include "Items/VaelPickupOrb.h"
#include "Items/VaelRechargeGlow.h"
#include "Nature/VaelMoonEave.h"
#include "Magic/VaelFloatingGrimoireComponent.h"
#include "Magic/VaelMagicSettings.h"
#include "Nature/VaelGarden.h"
#include "UI/VaelNoticeSubsystem.h"
#include "Story/VaelQuestSubsystem.h"
#include "Save/VaelSaveSubsystem.h"
#include "Magic/VaelElementComponent.h"
#include "Magic/VaelFormula.h"
#include "Magic/VaelFormulaScroll.h"
#include "Magic/VaelGrimoireSubsystem.h"
#include "Player/VaelCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "LandscapeProxy.h"
#include "Player/VaelHeroSubsystem.h"
#include "UI/VaelHUD.h"
#include "Player/VaelPlayerController.h"
#include "Floromant/VaelFloromantComponent.h"
#include "Floromant/VaelPlantArmComponent.h"
#include "Floromant/VaelPlantData.h"
#include "Floromant/VaelPlantArmData.h"
#include "Floromant/VaelFloromantSettings.h"
#include "World/VaelFireSpot.h"
#include "Vael.h"
#include "VaelAssets.h"
#include "VaelGameMode.h"
#include "UI/VaelUISettings.h"
#include "AbilitySystemComponent.h"
#include "Combat/VaelAttributeSet.h"
#include "World/VaelMarkSource.h"
#include "World/VaelRegion.h"
#include "World/VaelAtmosphere.h"
#include "World/VaelLocalWeather.h"
#include "World/VaelTimeOfDay.h"
#include "Story/VaelNpc.h"
#include "World/VaelAshDrift.h"
#include "World/VaelWorldSettings.h"
#include "Magic/VaelGameplayTags.h"
#include "Creatures/VaelSchleimgaenger.h"
#include "Creatures/VaelKobold.h"
#include "Creatures/VaelSchuerfameise.h"
#include "Creatures/VaelErztochter.h"
#include "Items/VaelChest.h"
#include "Engineer/VaelDoubleSickle.h"
#include "Engineer/VaelEngineerStage2Actors.h"
#include "Creatures/VaelEmberCrawler.h"
#include "World/VaelNidVeins.h"

namespace
{
	/** Queues the elements named by the letters of the prototype into an empty queue */
	void QueueLetters(UVaelElementComponent& ElementComponent, const FString& Elements)
	{
		ElementComponent.ClearQueue();

		for (const TCHAR Letter : Elements.ToUpper())
		{
			switch (Letter)
			{
			case TEXT('F'): ElementComponent.AddElement(EVaelElement::Fire); break;
			case TEXT('W'): ElementComponent.AddElement(EVaelElement::Water); break;
			case TEXT('E'): ElementComponent.AddElement(EVaelElement::Earth); break;
			case TEXT('L'): ElementComponent.AddElement(EVaelElement::Air); break;
			case TEXT('M'): ElementComponent.AddElement(EVaelElement::Mark); break;
			default: break;
			}
		}
	}
}

void UVaelCheatManager::VaelCast(const FString& Elements, float AimYaw)
{
	APlayerController* PlayerController = GetOuterAPlayerController();
	const AVaelCharacter* VaelCharacter = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
	if (VaelCharacter == nullptr)
	{
		return;
	}

	UVaelElementComponent* ElementComponent = VaelCharacter->GetElementComponent();
	QueueLetters(*ElementComponent, Elements);

	PlayerController->SetControlRotation(FRotator(0.f, AimYaw, 0.f));

	const EVaelCastResult Result = ElementComponent->CastQueue();

	UE_LOG(LogVael, Log, TEXT("VaelCast %s: %s, mana now %.1f"), *Elements, *UEnum::GetValueAsString(Result), VaelCharacter->GetMana());
}

void UVaelCheatManager::VaelQueue(const FString& Elements)
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	const AVaelCharacter* VaelCharacter = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
	if (VaelCharacter != nullptr)
	{
		QueueLetters(*VaelCharacter->GetElementComponent(), Elements);
	}
}

void UVaelCheatManager::VaelHoehle(const FString& Level)
{
	if (UVaelCaveTravel* Travel = UVaelCaveTravel::Get(GetOuterAPlayerController()))
	{
		Travel->Travel(GetOuterAPlayerController(), FName(*(Level.IsEmpty() ? FString(TEXT("L_Gildenmine")) : Level)), FVector2D::ZeroVector, FText::FromString(Level));
	}
}

void UVaelCheatManager::VaelTeleport(float X, float Y)
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	APawn* Pawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
	if (Pawn == nullptr)
	{
		return;
	}

	// Into the caves inside the open world (under their roof): the ground is looked for just above their floor
	for (TActorIterator<AVaelIndoorLevel> It(GetWorld()); It; ++It)
	{
		const float Floor = It->GetActorLocation().Z;
		if (!It->bWholeLevel && It->Covers(FVector(X * 100.0f, Y * 100.0f, Floor)))
		{
			VaelTravel::Land(Pawn, FVector2D(X * 100.0f, Y * 100.0f), Floor + 3000.0f, Floor - 150.0f);
			return;
		}
	}
	VaelTravel::Land(Pawn, FVector2D(X * 100.0f, Y * 100.0f));
}

void UVaelCheatManager::VaelReise(int32 Index)
{
	AVaelPlayerController* PlayerController = Cast<AVaelPlayerController>(GetOuterAPlayerController());
	if (PlayerController != nullptr && !PlayerController->TravelToCamp(Index))
	{
		UE_LOG(LogVael, Warning, TEXT("VaelReise: no found camp %d"), Index);
	}
}

void UVaelCheatManager::VaelNebel(const FString& What)
{
	const UVaelWorldMap* Map = UVaelWorldMap::ForWorld(GetWorld());
	UVaelProgressSubsystem* Progress = GetWorld()->GetGameInstance()->GetSubsystem<UVaelProgressSubsystem>();
	if (Map == nullptr || Progress == nullptr)
	{
		UE_LOG(LogVael, Warning, TEXT("VaelNebel: this level has no map"));
		return;
	}
	if (What == TEXT("alles"))
	{
		Progress->RevealWholeMap(Map);
	}
	const TArray<uint8>* Cells = Progress->GetMapFog(Map);
	int32 Seen = 0;
	if (Cells != nullptr)
	{
		for (const uint8 Cell : *Cells)
		{
			Seen += Cell > 127 ? 1 : 0;
		}
	}
	TArray<FString> Names;
	for (const FVaelMapLabel& Camp : Progress->GetFoundCamps(Map))
	{
		Names.Add(Camp.Name.ToString());
	}
	UE_LOG(LogVael, Log, TEXT("VaelNebel: %s fog %s, seen %.1f %%, camps found: %s"), *Map->GetName(), Map->bFogOfWar ? TEXT("on") : TEXT("off"),
		100.0f * Seen / (UVaelWorldMap::FogCells * UVaelWorldMap::FogCells), *FString::Join(Names, TEXT(", ")));
}

void UVaelCheatManager::VaelKarte()
{
	if (AVaelPlayerController* PlayerController = Cast<AVaelPlayerController>(GetOuterAPlayerController()))
	{
		PlayerController->ToggleMap();
	}
}

void UVaelCheatManager::VaelLearnAll()
{
	const UGameInstance* GameInstance = GetWorld() != nullptr ? GetWorld()->GetGameInstance() : nullptr;
	UVaelGrimoireSubsystem* Grimoire = GameInstance != nullptr ? GameInstance->GetSubsystem<UVaelGrimoireSubsystem>() : nullptr;
	if (Grimoire == nullptr)
	{
		return;
	}

	int32 NumLearned = 0;
	for (UVaelFormula* Formula : Grimoire->GetAllFormulas())
	{
		NumLearned += Grimoire->LearnFormula(Formula) ? 1 : 0;
	}

	UE_LOG(LogVael, Log, TEXT("VaelLearnAll: %d formulas learned, %d known now"), NumLearned, Grimoire->GetKnownFormulas().Num());
}

void UVaelCheatManager::VaelStatus(const FString& Status, float Duration)
{
	APlayerController* PlayerController = GetOuterAPlayerController();
	APawn* PlayerPawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;

	const int64 StatusValue = StaticEnum<EVaelStatus>()->GetValueByNameString(Status);
	if (PlayerPawn == nullptr || StatusValue == INDEX_NONE)
	{
		return;
	}

	// Burning needs a strength; the value of the spark is a good test value
	const float TestBurnDamagePerSecond = 6.0f;

	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		if (UVaelCombatStatics::CanDamage(PlayerPawn, *It))
		{
			UVaelCombatStatics::ApplyStatus(PlayerPawn, *It, static_cast<EVaelStatus>(StatusValue), Duration, TestBurnDamagePerSecond);

			UE_LOG(LogVael, Log, TEXT("VaelStatus: '%s' is now: %s"), *GetNameSafe(*It), *It->GetStatusText().ToString());
		}
	}
}

void UVaelCheatManager::VaelSpawn(const FString& Kind, int32 Count)
{
	APlayerController* PlayerController = GetOuterAPlayerController();
	const APawn* PlayerPawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
	AVaelGameMode* GameMode = GetWorld()->GetAuthGameMode<AVaelGameMode>();
	if (PlayerPawn == nullptr || GameMode == nullptr)
	{
		return;
	}

	// German and English names, shortened is fine
	const FString Name = Kind.ToLower();
	UVaelCreatureData* Data = nullptr;

	if (Name.StartsWith(TEXT("taufal")) || Name.StartsWith(TEXT("dewm")))
	{
		Data = GetMutableDefault<UVaelDewMothData>();
	}
	else if (Name.StartsWith(TEXT("tau")))
	{
		Data = GetMutableDefault<UVaelTauraupeData>();
	}
	else if (Name.StartsWith(TEXT("nosf")) || Name.StartsWith(TEXT("falter")) || Name.StartsWith(TEXT("blutf")) || Name.StartsWith(TEXT("moth")))
	{
		Data = GetMutableDefault<UVaelBloodMothData>();
	}
	else if (Name.StartsWith(TEXT("drac")) || Name.StartsWith(TEXT("raupe")) || Name.StartsWith(TEXT("cater")))
	{
		Data = GetMutableDefault<UVaelDraculaupeData>();
	}
	else if (Name.StartsWith(TEXT("glut")) || Name.StartsWith(TEXT("krie")) || Name.StartsWith(TEXT("crawl")))
	{
		Data = GetMutableDefault<UVaelEmberCrawlerData>();
	}
	else if (Name.StartsWith(TEXT("distelsa")) || Name.StartsWith(TEXT("saem")) || Name.StartsWith(TEXT("seedling")))
	{
		Data = GetMutableDefault<UVaelThistleSeedlingData>();
	}
	else if (Name.StartsWith(TEXT("distel")) || Name.StartsWith(TEXT("spinne")) || Name.StartsWith(TEXT("spider")))
	{
		Data = GetMutableDefault<UVaelThistleSpiderData>();
	}
	else if (Name.StartsWith(TEXT("schleier")) || Name.StartsWith(TEXT("affe")) || Name.StartsWith(TEXT("monkey")))
	{
		Data = GetMutableDefault<UVaelVeilMonkeyData>();
	}
	else if (Name.StartsWith(TEXT("sichel")) || Name.StartsWith(TEXT("puppe")) || Name.StartsWith(TEXT("doll")))
	{
		Data = GetMutableDefault<UVaelSickleDollData>();
	}
	else if (Name.StartsWith(TEXT("aschenr")) || Name.StartsWith(TEXT("roll")))
	{
		Data = GetMutableDefault<UVaelAshRollerData>();
	}
	else if (Name.StartsWith(TEXT("asch")) || Name.StartsWith(TEXT("harp")))
	{
		Data = GetMutableDefault<UVaelAshHarpyData>();
	}
	else if (Name.StartsWith(TEXT("ael")) || Name.StartsWith(TEXT("alt")) || Name.StartsWith(TEXT("elder")))
	{
		Data = GetMutableDefault<UVaelHarpyElderData>();
	}
	else if (Name.StartsWith(TEXT("markp")) || Name.StartsWith(TEXT("markw")))
	{
		Data = GetMutableDefault<UVaelMarkWaterPreacherData>();
	}
	else if (Name.StartsWith(TEXT("pred")) || Name.StartsWith(TEXT("preach")))
	{
		Data = GetMutableDefault<UVaelPreacherData>();
	}
	else if (Name.StartsWith(TEXT("koe")) || Name.StartsWith(TEXT("kon")) || Name.StartsWith(TEXT("queen")))
	{
		Data = GetMutableDefault<UVaelEmberQueenData>();
	}
	else if (Name.StartsWith(TEXT("wae")) || Name.StartsWith(TEXT("wäch")) || Name.StartsWith(TEXT("quell")) || Name.StartsWith(TEXT("guard")))
	{
		Data = GetMutableDefault<UVaelSourceGuardianData>();
	}
	else if (Name.StartsWith(TEXT("spann")) || Name.StartsWith(TEXT("kaef")) || Name.StartsWith(TEXT("käf")) || Name.StartsWith(TEXT("beetle")))
	{
		Data = GetMutableDefault<UVaelHornBeetleData>();
	}
	else if (Name.StartsWith(TEXT("kelch")) || Name.StartsWith(TEXT("reiss")) || Name.StartsWith(TEXT("reiß")))
	{
		Data = GetMutableDefault<UVaelKelchreisserData>();
	}
	else if (Name.StartsWith(TEXT("ranken")) || Name.StartsWith(TEXT("luchs")) || Name.StartsWith(TEXT("lynx")))
	{
		Data = GetMutableDefault<UVaelRankenluchsData>();
	}
	else if (Name.StartsWith(TEXT("pfuhl")) || Name.StartsWith(TEXT("natter")) || Name.StartsWith(TEXT("snake")))
	{
		Data = GetMutableDefault<UVaelPfuhlnatterData>();
	}

	else if (Name.StartsWith(TEXT("moder")))
	{
		Data = GetMutableDefault<UVaelModerglockeData>();
	}
	else if (Name.StartsWith(TEXT("kahl")))
	{
		Data = GetMutableDefault<UVaelKahlschwingeData>();
	}
	else if (Name.StartsWith(TEXT("peitsch")) || Name.StartsWith(TEXT("unke")))
	{
		Data = GetMutableDefault<UVaelPeitschenunkeData>();
	}
	else if (Name.StartsWith(TEXT("schleim")))
	{
		Data = GetMutableDefault<UVaelSchleimgaengerData>();
	}
	else if (Name.StartsWith(TEXT("schnapp")))
	{
		Data = GetMutableDefault<UVaelSchnappwurzData>();
	}

	if (Data == nullptr)
	{
		UE_LOG(LogVael, Warning, TEXT("VaelSpawn: unknown creature '%s'. Use Glutkriecher, Aschharpyie, Aelteste, Prediger, Markprediger, Koenigin, Waechter, Spannhornkaefer, Rankenluchs, Kelchreisser, Pfuhlnatter, Distelspinne, Distelsaemling, Aschenroller, Sichelpuppe, Schleieraffe, Moderglocke, Kahlschwinge, Peitschenunke or Schnappwurz."), *Kind);
		return;
	}

	const FVector Center = PlayerPawn->GetActorLocation() + PlayerPawn->GetActorForwardVector().GetSafeNormal2D() * 800.0f;
	const int32 NumSpawned = GameMode->SpawnCreatureGroup(Data, Center, FMath::Max(Count, 1), 150.0f + Data->CollisionRadius);

	UE_LOG(LogVael, Log, TEXT("VaelSpawn: %d x %s"), NumSpawned, *Data->DisplayName.ToString());

	// "LuchsWach" brings them awake, hunting at once, to watch them run (the plain kind lies in ambush)
	if (Name.EndsWith(TEXT("wach")))
	{
		for (TActorIterator<AVaelRankenluchs> It(GetWorld()); It; ++It)
		{
			if (FVector::Dist2D(It->GetActorLocation(), Center) < 1500.0f)
			{
				It->WakeUp(FMath::FRandRange(0.0f, 0.4f));
			}
		}
	}

	// "KelchWach" brings it with the scent of the nearest player already (screams, then hunts); the plain kind roams its
	// new territory and has to hear the players first
	if (Name.StartsWith(TEXT("kelch")) && Name.EndsWith(TEXT("wach")))
	{
		for (TActorIterator<AVaelKelchreisser> It(GetWorld()); It; ++It)
		{
			if (FVector::Dist2D(It->GetActorLocation(), Center) < 1500.0f)
			{
				It->TakeScentNow();
			}
		}
	}

	// "NatterAlt" brings a grown old giant (an elite), "NatterJung" a young one
	if (Name.EndsWith(TEXT("alt")) || Name.EndsWith(TEXT("jung")))
	{
		for (TActorIterator<AVaelPfuhlnatter> It(GetWorld()); It; ++It)
		{
			if (FVector::Dist2D(It->GetActorLocation(), Center) < 1500.0f)
			{
				It->SetAgeDays(Name.EndsWith(TEXT("alt")) ? 60.0f : 3.0f);
			}
		}
	}

	// "NatterSpuck" makes them spit as their first attack (rearing, balls, puddle); "NatterWuerg" asks for a coil, which
	// only happens where the data allows it (bCanConstrict, off by default)
	if (Name.EndsWith(TEXT("spuck")) || Name.EndsWith(TEXT("wuerg")))
	{
		for (TActorIterator<AVaelPfuhlnatter> It(GetWorld()); It; ++It)
		{
			if (FVector::Dist2D(It->GetActorLocation(), Center) < 1500.0f)
			{
				It->SetNextAttackSpit(Name.EndsWith(TEXT("spuck")));
			}
		}
	}
}

void UVaelCheatManager::VaelScroll(const FString& Elements, float Distance)
{
	APlayerController* PlayerController = GetOuterAPlayerController();
	const APawn* PlayerPawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
	if (PlayerPawn == nullptr)
	{
		return;
	}

	TArray<EVaelElement> ScrollElements;
	for (const TCHAR Letter : Elements.ToUpper())
	{
		switch (Letter)
		{
		case TEXT('F'): ScrollElements.Add(EVaelElement::Fire); break;
		case TEXT('W'): ScrollElements.Add(EVaelElement::Water); break;
		case TEXT('E'): ScrollElements.Add(EVaelElement::Earth); break;
		case TEXT('L'): ScrollElements.Add(EVaelElement::Air); break;
		case TEXT('M'): ScrollElements.Add(EVaelElement::Mark); break;
		default: break;
		}
	}

	const FVector Spot = PlayerPawn->GetActorLocation() + PlayerPawn->GetActorForwardVector().GetSafeNormal2D() * Distance;

	FVector Ground;
	if (ScrollElements.IsEmpty() || !AVaelCreature::FindGround(GetWorld(), Spot, Ground))
	{
		UE_LOG(LogVael, Warning, TEXT("VaelScroll: no elements in '%s' or no ground in front of the player"), *Elements);
		return;
	}

	AVaelFormulaScroll::SpawnScroll(GetWorld(), Ground, ScrollElements, NSLOCTEXT("VaelCheats", "TestScroll", "Test-Schriftrolle"));

	UE_LOG(LogVael, Log, TEXT("VaelScroll: scroll for %s laid out"), *Elements);
}

void UVaelCheatManager::VaelDistelPlatzen()
{
	int32 Count = 0;
	for (TActorIterator<AVaelThistleSpider> It(GetWorld()); It; ++It)
	{
		if (!It->IsDead())
		{
			It->ForceBurst();
			++Count;
		}
	}
	UE_LOG(LogVael, Log, TEXT("VaelDistelPlatzen: %d Distelspinnen burst"), Count);
}

void UVaelCheatManager::VaelBegegnung()
{
	AVaelEncounterDirector* Director = AVaelEncounterDirector::Get(GetWorld());
	if (Director == nullptr)
	{
		UE_LOG(LogVael, Warning, TEXT("VaelBegegnung: this level has no encounter director"));
		return;
	}
	UE_LOG(LogVael, Log, TEXT("VaelBegegnung: %d creatures sent"), Director->SpawnPackNow());
}

void UVaelCheatManager::VaelKillAll()
{
	int32 NumKilled = 0;

	for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
	{
		if (!It->IsDead())
		{
			UVaelCombatStatics::DealDamage(nullptr, *It, It->GetHealth() + 1000.0f, EVaelElement::Earth);
			++NumKilled;
		}
	}

	UE_LOG(LogVael, Log, TEXT("VaelKillAll: %d creatures killed"), NumKilled);
}

void UVaelCheatManager::VaelTreffen(float Schaden, int32 Element)
{
	APlayerController* Controller = GetOuterAPlayerController();
	APawn* Player = Controller != nullptr ? Controller->GetPawn() : nullptr;
	if (Player == nullptr)
	{
		return;
	}
	const EVaelElement HitElement = static_cast<EVaelElement>(FMath::Clamp(Element, 0, 3));
	int32 NumHit = 0;
	for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
	{
		if (!It->IsDead() && FVector::Dist(It->GetActorLocation(), Player->GetActorLocation()) < 3000.0f)
		{
			UVaelCombatStatics::DealDamage(Player, *It, Schaden, HitElement);
			++NumHit;
		}
	}
	UE_LOG(LogVael, Log, TEXT("VaelTreffen: %d creatures hit for %.0f"), NumHit, Schaden);
}

void UVaelCheatManager::VaelKisteOeffnen()
{
	APlayerController* Controller = GetOuterAPlayerController();
	AVaelCharacter* Player = Controller != nullptr ? Controller->GetPawn<AVaelCharacter>() : nullptr;
	if (Player == nullptr)
	{
		UE_LOG(LogVael, Warning, TEXT("VaelKisteOeffnen: no player"));
		return;
	}

	AVaelChest* Nearest = nullptr;
	double NearestDistSq = FMath::Square(600.0);
	for (TActorIterator<AVaelChest> It(GetWorld()); It; ++It)
	{
		const double DistSq = FVector::DistSquared(It->GetActorLocation(), Player->GetActorLocation());
		if (It->CanInteract(Player) && DistSq < NearestDistSq)
		{
			Nearest = *It;
			NearestDistSq = DistSq;
		}
	}

	if (Nearest == nullptr)
	{
		UE_LOG(LogVael, Warning, TEXT("VaelKisteOeffnen: no closed chest within 6 m of %s"), *Player->GetActorLocation().ToString());
		return;
	}
	UE_LOG(LogVael, Log, TEXT("VaelKisteOeffnen: opening '%s' at %.0f cm"), *Nearest->GetName(), FMath::Sqrt(NearestDistSq));
	Nearest->Interact(Player);
}

void UVaelCheatManager::VaelGlyphs(const FString& Glyphs)
{
	const int64 Value = StaticEnum<EVaelGamepadGlyphPreference>()->GetValueByNameString(Glyphs);
	if (Value == INDEX_NONE)
	{
		UE_LOG(LogVael, Warning, TEXT("VaelGlyphs: use Auto, Xbox or PlayStation"));
		return;
	}

	// Only for this session; the lasting choice is in the project settings under Vael UI
	GetMutableDefault<UVaelUISettings>()->GamepadGlyphs = static_cast<EVaelGamepadGlyphPreference>(Value);
	UE_LOG(LogVael, Log, TEXT("VaelGlyphs: %s"), *Glyphs);
}

void UVaelCheatManager::VaelTime(float Hour)
{
	if (UVaelTimeOfDay* Clock = UVaelTimeOfDay::Get(GetWorld()))
	{
		Clock->SetHour(Hour);
		UE_LOG(LogVael, Display, TEXT("VaelTime: %.1f h, %s, night %.2f"), Clock->GetHour(), *UEnum::GetValueAsString(Clock->GetPhase()), Clock->GetNightFactor());
	}
}

void UVaelCheatManager::VaelTagesablauf()
{
	int32 Moved = 0;
	for (TActorIterator<AVaelNpc> It(GetWorld()); It; ++It)
	{
		It->FollowDayNow();
		Moved += It->IsAsleep() ? 1 : 0;
	}
	UE_LOG(LogVael, Display, TEXT("VaelTagesablauf: people follow their day, %d asleep"), Moved);
}

void UVaelCheatManager::VaelAschenboe()
{
	for (TActorIterator<AVaelAshDrift> It(GetWorld()); It; ++It)
	{
		It->GustNow();
		return;
	}
	UE_LOG(LogVael, Warning, TEXT("VaelAschenboe: no ash drift in this level"));
}

void UVaelCheatManager::VaelSpannen(float Seconds)
{
	AVaelPlayerController* Controller = Cast<AVaelPlayerController>(GetOuterAPlayerController());
	AVaelCharacter* Player = Controller != nullptr ? Controller->GetPawn<AVaelCharacter>() : nullptr;
	UVaelEngineerComponent* Engineer = Player != nullptr && Player->IsEngineer() ? Player->GetEngineer() : nullptr;
	if (Engineer == nullptr || !Engineer->UsesDraw())
	{
		UE_LOG(LogVael, Warning, TEXT("VaelSpannen: the engineer with slingshot or bow only"));
		return;
	}
	Engineer->StartDraw();
	if (Seconds <= 0.0f)
	{
		return;
	}
	TWeakObjectPtr<UVaelEngineerComponent> Weak(Engineer);
	TWeakObjectPtr<AVaelPlayerController> WeakController(Controller);
	FTimerHandle Timer;
	GetWorld()->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([Weak, WeakController]()
	{
		if (Weak.IsValid() && WeakController.IsValid())
		{
			Weak->ReleaseDraw(WeakController->GetFloromantAim());
		}
	}), Seconds, false);
}

void UVaelCheatManager::VaelTimeSpeed(float Speed)
{
	if (UVaelTimeOfDay* Clock = UVaelTimeOfDay::Get(GetWorld()))
	{
		Clock->SetSpeed(Speed);
	}
}

void UVaelCheatManager::VaelAir(const FString& Kind, float Amount)
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	const APawn* PlayerPawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
	UVaelAtmosphere* Air = UVaelAtmosphere::Get(GetWorld());
	if (PlayerPawn == nullptr || Air == nullptr)
	{
		return;
	}

	const FString Name = Kind.ToLower();
	const EVaelAir AirKind = Name.StartsWith(TEXT("h")) ? EVaelAir::Heat : Name.StartsWith(TEXT("f")) || Name.StartsWith(TEXT("m")) ? EVaelAir::Moisture : EVaelAir::Cold;
	UVaelAtmosphere::Add(GetWorld(), PlayerPawn->GetActorLocation(), AirKind, Amount);

	const FVaelAirCell Cell = Air->Sample(PlayerPawn->GetActorLocation());
	UE_LOG(LogVael, Display, TEXT("VaelAir: heat %.0f, moisture %.0f, cold %.0f around the player"), Cell.Heat, Cell.Moisture, Cell.Cold);
}

void UVaelCheatManager::VaelLocalWeather(const FString& Kind)
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	const APawn* PlayerPawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
	if (PlayerPawn == nullptr)
	{
		return;
	}

	const FString Name = Kind.ToLower();
	EVaelLocalWeather LocalKind = EVaelLocalWeather::Shower;
	if (Name.StartsWith(TEXT("gew")) || Name.StartsWith(TEXT("thu")))
	{
		LocalKind = EVaelLocalWeather::Thunderstorm;
	}
	else if (Name.StartsWith(TEXT("auf")) || Name.StartsWith(TEXT("the")))
	{
		LocalKind = EVaelLocalWeather::Thermal;
	}
	else if (Name.StartsWith(TEXT("win")))
	{
		LocalKind = EVaelLocalWeather::WindFront;
	}
	else if (Name.StartsWith(TEXT("neb")) || Name.StartsWith(TEXT("fog")))
	{
		LocalKind = EVaelLocalWeather::ColdFog;
	}
	else if (Name.StartsWith(TEXT("gra")))
	{
		LocalKind = EVaelLocalWeather::Graupel;
	}

	const UVaelWorldSettings* Settings = UVaelWorldSettings::Get();
	AVaelLocalWeather::Spawn(GetWorld(), LocalKind, PlayerPawn->GetActorLocation(), Settings->LocalWeatherRadius, Settings->LocalWeatherLifetime, PlayerPawn->GetActorForwardVector());
}

void UVaelCheatManager::VaelWeather(const FString& Weather)
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	const APawn* PlayerPawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
	AVaelRegion* Region = PlayerPawn != nullptr ? AVaelRegion::GetRegionAt(GetWorld(), PlayerPawn->GetActorLocation()) : nullptr;
	if (Region == nullptr)
	{
		return;
	}

	const FString Name = Weather.ToLower();
	if (Name.StartsWith(TEXT("reg")) || Name.StartsWith(TEXT("rain")))
	{
		Region->SetWeather(EVaelWeather::Rain);
	}
	else if (Name.StartsWith(TEXT("stu")) || Name.StartsWith(TEXT("storm")))
	{
		Region->SetWeather(EVaelWeather::Storm);
	}
	else if (Name.StartsWith(TEXT("kla")) || Name.StartsWith(TEXT("clear")))
	{
		Region->SetWeather(EVaelWeather::Clear);
	}
	else if (Name.StartsWith(TEXT("d")))
	{
		Region->SetWeather(EVaelWeather::Drought);
	}
	else if (Name.StartsWith(TEXT("w")))
	{
		Region->SetWeather(EVaelWeather::Wind);
	}
	else if (Name.StartsWith(TEXT("sch")) || Name.StartsWith(TEXT("snow")))
	{
		Region->SetWeather(EVaelWeather::Snow);
	}
	else if (Name.StartsWith(TEXT("bl")) || Name.StartsWith(TEXT("sturm2")))
	{
		Region->SetWeather(EVaelWeather::Blizzard);
	}
	else
	{
		UE_LOG(LogVael, Warning, TEXT("VaelWeather: use Klar, Regen, Sturm, Duerre, Wind, Schnee or Blizzard"));
	}
}

void UVaelCheatManager::VaelCorruption(float Corruption)
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	const APawn* PlayerPawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
	if (AVaelRegion* Region = PlayerPawn != nullptr ? AVaelRegion::GetRegionAt(GetWorld(), PlayerPawn->GetActorLocation()) : nullptr)
	{
		Region->SetCorruption(Corruption);
		UE_LOG(LogVael, Log, TEXT("VaelCorruption: '%s' is at %.0f"), *Region->GetRegionName().ToString(), Region->GetCorruption());
	}
}

void UVaelCheatManager::VaelPlayerCorruption(float Corruption)
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	const AVaelCharacter* VaelCharacter = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
	if (VaelCharacter != nullptr)
	{
		VaelCharacter->GetAbilitySystemComponent()->SetNumericAttributeBase(UVaelAttributeSet::GetCorruptionAttribute(), FMath::Clamp(Corruption, 0.0f, 100.0f));
	}
}

void UVaelCheatManager::VaelGear(const FString& Gear)
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	const AVaelCharacter* VaelCharacter = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
	if (VaelCharacter == nullptr)
	{
		return;
	}

	const FString Name = Gear.ToLower();
	const FGameplayTag Tag = Name.Contains(TEXT("ward")) ? VaelTags::Gear_WeatherWard : Name.Contains(TEXT("attun")) ? VaelTags::Gear_WeatherAttunement : FGameplayTag();
	if (!Tag.IsValid())
	{
		UE_LOG(LogVael, Warning, TEXT("VaelGear: use WeatherWard or WeatherAttunement"));
		return;
	}

	// Loose tags stand in for the effect of a worn item
	UAbilitySystemComponent* AbilitySystem = VaelCharacter->GetAbilitySystemComponent();
	const bool bWearing = AbilitySystem->HasMatchingGameplayTag(Tag);
	if (bWearing)
	{
		AbilitySystem->RemoveLooseGameplayTag(Tag);
	}
	else
	{
		AbilitySystem->AddLooseGameplayTag(Tag);
	}

	UE_LOG(LogVael, Log, TEXT("VaelGear: %s %s"), *Tag.ToString(), bWearing ? TEXT("taken off") : TEXT("put on"));
}

void UVaelCheatManager::VaelQuest(const FString& QuestId, int32 Step)
{
	UVaelQuestSubsystem* Quests = UVaelQuestSubsystem::Get(GetWorld());
	if (Quests == nullptr)
	{
		return;
	}

	if (!QuestId.IsEmpty())
	{
		if (Quests->FindQuest(FName(*QuestId)) == nullptr)
		{
			UE_LOG(LogVael, Warning, TEXT("VaelQuest: no quest '%s'"), *QuestId);
		}

		Quests->SetStep(FName(*QuestId), Step - 1);
	}

	UE_LOG(LogVael, Log, TEXT("VaelQuest: %s"), *Quests->Describe());
}

void UVaelCheatManager::VaelAwakenMark()
{
	const UGameInstance* GameInstance = GetWorld() != nullptr ? GetWorld()->GetGameInstance() : nullptr;
	UVaelGrimoireSubsystem* Grimoire = GameInstance != nullptr ? GameInstance->GetSubsystem<UVaelGrimoireSubsystem>() : nullptr;

	if (Grimoire == nullptr)
	{
		UE_LOG(LogVael, Warning, TEXT("VaelAwakenMark: no grimoire"));
		return;
	}

	const bool bAwakened = Grimoire->AwakenMark(NSLOCTEXT("VaelMagic", "MarkAwakensCheat", "Das Nid erwacht in dir."));
	UE_LOG(LogVael, Log, TEXT("VaelAwakenMark: %s"), bAwakened ? TEXT("the Mark awakens") : TEXT("the Mark is awake already"));

	// Seen on screen too, with the button of the fifth element
	UVaelNoticeSubsystem::Post(GetWorld()->GetGameInstance(), bAwakened ? NSLOCTEXT("VaelMagic", "MarkAwakeCheat", "Das Nid ist erwacht") : NSLOCTEXT("VaelMagic", "MarkAwakeAlready", "Das Nid ist schon wach"),
		NSLOCTEXT("VaelMagic", "MarkButtonHint", "Nid-Element: RB / R1 oder Taste 5"), UVaelMagicSettings::Get()->GetElementColor(EVaelElement::Mark), 5.0f);
}

void UVaelCheatManager::VaelSealSource()
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	const APawn* Pawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
	if (Pawn == nullptr)
	{
		return;
	}

	AVaelMarkSource* Closest = nullptr;
	float ClosestDistance = TNumericLimits<float>::Max();

	for (TActorIterator<AVaelMarkSource> It(GetWorld()); It; ++It)
	{
		const float Distance = FVector::Dist2D(It->GetActorLocation(), Pawn->GetActorLocation());
		if (!It->IsSealed() && Distance < ClosestDistance)
		{
			Closest = *It;
			ClosestDistance = Distance;
		}
	}

	if (Closest == nullptr || !Closest->Seal())
	{
		UE_LOG(LogVael, Warning, TEXT("VaelSealSource: no open Mark source in the level"));
	}
}

void UVaelCheatManager::VaelMaterial(const FString& Name, int32 Count)
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	const AVaelCharacter* VaelCharacter = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
	if (VaelCharacter == nullptr)
	{
		return;
	}

	// "alle": every material of the game, to look at all their pictures (Boris 17.11.)
	if (Name.Equals(TEXT("alle"), ESearchCase::IgnoreCase))
	{
		TArray<FAssetData> Found;
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().GetAssetsByPath(FName(TEXT("/Game/Vael/Items/Materials")), Found);
		for (const FAssetData& Each : Found)
		{
			if (const UVaelMaterial* Material = Cast<UVaelMaterial>(Each.GetAsset()))
			{
				VaelCharacter->GetMaterialBag()->AddMaterial(Material, FMath::Max(Count, 1));
			}
		}
		UE_LOG(LogVael, Log, TEXT("VaelMaterial alle: %d"), Found.Num());
		return;
	}

	const FString AssetName = TEXT("DA_Material_") + Name;
	const FSoftObjectPath Path(FString::Printf(TEXT("/Game/Vael/Items/Materials/%s.%s"), *AssetName, *AssetName));

	if (const UVaelMaterial* Material = Cast<UVaelMaterial>(VaelAssets::LoadOptional(Path)))
	{
		VaelCharacter->GetMaterialBag()->AddMaterial(Material, FMath::Max(Count, 1));
	}
	else
	{
		UE_LOG(LogVael, Warning, TEXT("VaelMaterial: no material %s, try Glutdruese, Russfeder, Ordenssiegel, Aeltestenschwinge, Nidkristall or HerzDerGlut"), *AssetName);
	}
}

void UVaelCheatManager::VaelLoot(const FString& Name, int32 Count)
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	const AVaelCharacter* VaelCharacter = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
	if (VaelCharacter == nullptr)
	{
		return;
	}

	TArray<FString> Names;
	if (Name.Equals(TEXT("alle"), ESearchCase::IgnoreCase))
	{
		Names = { TEXT("Nidkristall"), TEXT("GelaeuterterKristall"), TEXT("Nidtrunk"), TEXT("Klartrunk") };
	}
	else
	{
		Names.Add(Name);
	}

	// Drops land in a ring around the source; the player's feet are the source, a little ahead so they don't pick it up at once
	TArray<FVaelLootEntry> Loot;
	for (const FString& Each : Names)
	{
		const FString AssetName = TEXT("DA_Material_") + Each;
		const FString Path = FString::Printf(TEXT("/Game/Vael/Items/Materials/%s.%s"), *AssetName, *AssetName);
		Loot.Add(FVaelLootEntry(*Path, 1.0f, FMath::Max(Count, 1), FMath::Max(Count, 1)));
	}

	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Ahead = VaelCharacter->GetActorLocation() + VaelCharacter->GetActorForwardVector() * 320.0f;
	if (AActor* Source = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Ahead), Parameters))
	{
		Source->SetRootComponent(NewObject<USceneComponent>(Source));
		Source->GetRootComponent()->SetWorldLocation(Ahead);
		UVaelMaterialBag::GiveToGroup(Source, Loot);
		Source->Destroy();
	}
}

void UVaelCheatManager::VaelMondtraufeReif()
{
	int32 Count = 0;
	for (TActorIterator<AVaelMoonEave> It(GetWorld()); It; ++It)
	{
		if (It->bHarvestable)
		{
			It->RipenAll();
			++Count;
		}
	}
	UE_LOG(LogVael, Log, TEXT("VaelMondtraufeReif: %d great Mondtraufen ripened"), Count);
}

void UVaelCheatManager::VaelMondtraufe(int32 Number)
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	APawn* Pawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
	if (Pawn == nullptr)
	{
		return;
	}

	TArray<AVaelMoonEave*> Great;
	for (TActorIterator<AVaelMoonEave> It(GetWorld()); It; ++It)
	{
		if (It->bHarvestable)
		{
			Great.Add(*It);
		}
	}
	Great.Sort([](const AVaelMoonEave& A, const AVaelMoonEave& B) { return A.GetName() < B.GetName(); });
	if (!Great.IsValidIndex(Number - 1))
	{
		UE_LOG(LogVael, Warning, TEXT("VaelMondtraufe: there are %d great Mondtraufen here"), Great.Num());
		return;
	}

	// The plant's own place is its picking spot on the bank; where roots crowd it, the nearest free spot around it
	FVector Spot = Great[Number - 1]->GetActorLocation() + FVector(0.0f, 0.0f, Pawn->GetSimpleCollisionHalfHeight() + 5.0f);
	FRotator Facing = Pawn->GetActorRotation();
	for (int32 Ring = 0; Ring <= 6; ++Ring)
	{
		for (int32 Step = 0; Step < (Ring == 0 ? 1 : 8); ++Step)
		{
			const float Angle = Step * UE_TWO_PI / 8.0f;
			FVector Try = Spot + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Ring * 100.0f;
			if (GetWorld()->FindTeleportSpot(Pawn, Try, Facing) && Pawn->TeleportTo(Try, Facing))
			{
				return;
			}
		}
	}
	UE_LOG(LogVael, Warning, TEXT("VaelMondtraufe: no free spot near great Mondtraufe %d"), Number);
}

void UVaelCheatManager::VaelItem(const FString& Name)
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	AVaelCharacter* VaelCharacter = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
	if (VaelCharacter == nullptr)
	{
		return;
	}

	const FString Lower = Name.ToLower();
	FVaelItem Item;

	if (Lower.StartsWith(TEXT("gew")) || Lower.StartsWith(TEXT("mag")) || Lower.StartsWith(TEXT("sel")))
	{
		const EVaelRarity Rarity = Lower.StartsWith(TEXT("gew")) ? EVaelRarity::Common : Lower.StartsWith(TEXT("mag")) ? EVaelRarity::Magic : EVaelRarity::Rare;
		Item = UVaelItemSettings::Get()->RollItem(Rarity);
	}
	else
	{
		const FString AssetName = TEXT("DA_Item_") + Name;
		const FSoftObjectPath Path(FString::Printf(TEXT("/Game/Vael/Items/Gear/%s.%s"), *AssetName, *AssetName));

		if (const UVaelItemData* ItemData = Cast<UVaelItemData>(VaelAssets::LoadOptional(Path)))
		{
			Item = ItemData->MakeItem();
		}
	}

	if (!Item.IsValid())
	{
		UE_LOG(LogVael, Warning, TEXT("VaelItem: use Gewoehnlich, Magisch, Selten or an item asset like Sturmmantel"));
		return;
	}

	AVaelItemDrop::DropItem(GetWorld(), VaelCharacter->GetActorLocation() + VaelCharacter->GetActorForwardVector() * 150.0f, Item, VaelCharacter);
}

void UVaelCheatManager::VaelInventory()
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	const AVaelCharacter* VaelCharacter = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
	if (VaelCharacter == nullptr || GEngine == nullptr)
	{
		return;
	}

	const UVaelInventory* Inventory = VaelCharacter->GetInventory();

	const auto DescribeItem = [](const FVaelItem& Item)
	{
		FString Text = FString::Printf(TEXT("%s (%s)"), *Item.DisplayName.ToString(), *UEnum::GetDisplayValueAsText(Item.Rarity).ToString());
		for (const FVaelItemStatValue& StatValue : Item.Stats)
		{
			Text += TEXT(", ") + VaelItems::DescribeStat(StatValue).ToString();
		}
		if (const FVaelLegendaryAbility* Ability = Item.GetLegendaryAbility())
		{
			Text += TEXT(" | ") + Ability->Description.ToString();
		}
		return Text;
	};

	// Shown newest on top: totals, backpack, equipment
	TArray<FString> Lines;
	Lines.Add(TEXT("Ausruestung:"));
	for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(EVaelEquipSlot::Count); ++SlotIndex)
	{
		const EVaelEquipSlot EquipSlot = static_cast<EVaelEquipSlot>(SlotIndex);
		const FVaelItem& Item = Inventory->GetEquipped(EquipSlot);
		Lines.Add(FString::Printf(TEXT("  %s: %s"), *UEnum::GetDisplayValueAsText(EquipSlot).ToString(), Item.IsValid() ? *DescribeItem(Item) : TEXT("-")));
	}

	Lines.Add(FString::Printf(TEXT("Rucksack (%d):"), Inventory->GetBackpack().Num()));
	for (int32 Index = 0; Index < Inventory->GetBackpack().Num(); ++Index)
	{
		Lines.Add(FString::Printf(TEXT("  %d. %s"), Index + 1, *DescribeItem(Inventory->GetBackpack()[Index])));
	}

	for (int32 Index = Lines.Num() - 1; Index >= 0; --Index)
	{
		GEngine->AddOnScreenDebugMessage(-1, 20.0f, FColor(234, 220, 196), Lines[Index]);
	}
}

void UVaelCheatManager::VaelEquip(int32 BackpackField)
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	const AVaelCharacter* VaelCharacter = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
	if (VaelCharacter != nullptr && !VaelCharacter->GetInventory()->EquipFromBackpack(BackpackField - 1))
	{
		UE_LOG(LogVael, Warning, TEXT("VaelEquip: field %d of the backpack is empty"), BackpackField);
	}
}

void UVaelCheatManager::VaelStudy()
{
	UVaelCompendiumSubsystem* Compendium = UVaelCompendiumSubsystem::Get(GetWorld());
	if (Compendium != nullptr)
	{
		UE_LOG(LogVael, Log, TEXT("VaelStudy: %d entries researched, %s"), Compendium->StudyAtCamp(), *Compendium->Describe());
	}
}

void UVaelCheatManager::VaelResearch(const FString& Name, int32 Stage)
{
	UVaelCompendiumSubsystem* Compendium = UVaelCompendiumSubsystem::Get(GetWorld());
	if (Compendium == nullptr)
	{
		return;
	}

	const EVaelResearchStage NewStage = static_cast<EVaelResearchStage>(FMath::Clamp(Stage, 0, static_cast<int32>(EVaelResearchStage::Researched)));
	const bool bAll = Name.Equals(TEXT("Alle"), ESearchCase::IgnoreCase) || Name.Equals(TEXT("All"), ESearchCase::IgnoreCase);

	for (const EVaelCompendiumBook Book : UVaelCompendiumSubsystem::GetBooks())
	{
		for (const UVaelCompendiumEntry* Entry : Compendium->GetEntries(Book))
		{
			if (bAll || Entry->SubjectId.ToString().Equals(Name, ESearchCase::IgnoreCase) || Entry->DisplayName.ToString().Equals(Name, ESearchCase::IgnoreCase))
			{
				Compendium->SetStage(Entry->SubjectId, NewStage);
			}
		}
	}

	UE_LOG(LogVael, Log, TEXT("VaelResearch: %s"), *Compendium->Describe());
}

void UVaelCheatManager::VaelNeuesSpiel()
{
	if (UVaelSaveSubsystem* Save = UVaelSaveSubsystem::Get(GetWorld()))
	{
		Save->StartNewGame();
	}
}

void UVaelCheatManager::VaelSpeichern()
{
	if (UVaelSaveSubsystem* Save = UVaelSaveSubsystem::Get(GetWorld()))
	{
		Save->SaveNow();
		UE_LOG(LogVael, Log, TEXT("VaelSpeichern: save game written"));
	}
}

void UVaelCheatManager::VaelSaat(const FString& What)
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
	UVaelGardenSubsystem* Garden = UVaelGardenSubsystem::Get(this);
	if (Player == nullptr || Garden == nullptr)
	{
		return;
	}

	if (What.StartsWith(TEXT("all"), ESearchCase::IgnoreCase))
	{
		for (const FVaelSeedPlant& Plant : UVaelGardenSettings::Get()->Plants)
		{
			Garden->Research(Plant.Id, Player);
		}
		return;
	}

	if (const UVaelMaterial* Seed = UVaelGardenSettings::Get()->SurpriseSeed.LoadSynchronous())
	{
		Player->GetMaterialBag()->AddMaterial(Seed, 5);
	}
}

void UVaelCheatManager::VaelGestalt(const FString& Look)
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	if (AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr)
	{
		const bool bMetaHuman = Look.StartsWith(TEXT("meta"), ESearchCase::IgnoreCase);
		Player->SetMetaHumanByDefault(bMetaHuman);
		Player->SetMetaHumanShown(bMetaHuman);
		if (!bMetaHuman)
		{
			Player->SetBaseLook(Look.StartsWith(TEXT("f"), ESearchCase::IgnoreCase) || Look.StartsWith(TEXT("w"), ESearchCase::IgnoreCase) ? 1 : 0);
		}
	}
}

void UVaelCheatManager::VaelDodge()
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	if (AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr)
	{
		Player->StartDodge(Player->GetActorForwardVector());
	}
}

void UVaelCheatManager::VaelFrameTest(int32 ExtraMetaHumans)
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	if (AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr)
	{
		Player->SpawnMetaHumanCopies(ExtraMetaHumans);
	}

	// Waits 3 s for everything to settle, samples for 5 s and writes the means to the log
	struct FFrameSamples
	{
		float Age = 0.0f;
		int32 Num = 0;
		double Frame = 0.0, Game = 0.0, Render = 0.0, Gpu = 0.0;
	};
	TSharedRef<FFrameSamples> Samples = MakeShared<FFrameSamples>();
	TSharedRef<FTimerHandle> Handle = MakeShared<FTimerHandle>();
	UWorld* World = GetWorld();
	World->GetTimerManager().SetTimer(*Handle, FTimerDelegate::CreateWeakLambda(this, [this, Samples, Handle, World, ExtraMetaHumans]()
	{
		Samples->Age += 0.02f;
		if (Samples->Age < 3.0f)
		{
			return;
		}
		Samples->Frame += FApp::GetDeltaTime() * 1000.0;
		Samples->Game += FPlatformTime::ToMilliseconds(GGameThreadTime);
		Samples->Render += FPlatformTime::ToMilliseconds(GRenderThreadTime);
		Samples->Gpu += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
		++Samples->Num;
		if (Samples->Age >= 8.0f)
		{
			const double Num = Samples->Num;
			UE_LOG(LogVael, Display, TEXT("Frame test (%d extra): frame %.1f ms, game %.1f ms, render %.1f ms, GPU %.1f ms"),
				ExtraMetaHumans, Samples->Frame / Num, Samples->Game / Num, Samples->Render / Num, Samples->Gpu / Num);
			World->GetTimerManager().ClearTimer(*Handle);
		}
	}), 0.02f, true);
}

void UVaelCheatManager::VaelLastTest(float Seconds, int32 Pack)
{
	APlayerController* PlayerController = GetOuterAPlayerController();
	if (PlayerController == nullptr || PlayerController->GetPawn() == nullptr)
	{
		return;
	}
	if (PlayerController->GetPawn()->CanBeDamaged())
	{
		God();
	}
	VaelLearnAll();


	struct FLoad
	{
		float Age = 0.0f;
		float NextCast = 0.0f;
		int32 Cast = 0;
		TArray<float> Frames;
		double Game = 0.0, Render = 0.0, Gpu = 0.0;
		float Worst = 0.0f;
		float WorstAt = 0.0f;
		bool bSpawned = false;
	};
	TSharedRef<FLoad> Load = MakeShared<FLoad>();
	TWeakObjectPtr<UWorld> WeakWorld(GetWorld());
	TWeakObjectPtr<UVaelCheatManager> WeakThis(this);
	static const TCHAR* Formulas[] = { TEXT("F"), TEXT("FF"), TEXT("FFF"), TEXT("W"), TEXT("WL"), TEXT("E"), TEXT("EE"), TEXT("L"), TEXT("LL"), TEXT("FL"),
		TEXT("FE"), TEXT("WE"), TEXT("WW"), TEXT("LLL"), TEXT("FFL"), TEXT("EEE"), TEXT("FW"), TEXT("WWL") };
	// Once per frame (a looping timer would fire several times in a long frame)
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis, Load, WeakWorld, Seconds, Pack](float) -> bool
	{
		UVaelCheatManager* Self = WeakThis.Get();
		UWorld* World = WeakWorld.Get();
		if (Self == nullptr || World == nullptr)
		{
			return false;
		}
		const float Delta = FApp::GetDeltaTime();
		Load->Age += Delta;
		APlayerController* Controller = Self->GetOuterAPlayerController();
		const APawn* Pawn = Controller != nullptr ? Controller->GetPawn() : nullptr;
		if (Pawn == nullptr)
		{
			return false;
		}
		// A mixed pack: walkers, fliers and casters, like a fight in the open (after a moment, so a teleport has landed)
		if (!Load->bSpawned && Load->Age >= 3.0f)
		{
			Load->bSpawned = true;
			static const TCHAR* Kinds[] = { TEXT("Kriecher"), TEXT("Harpyie"), TEXT("Prediger"), TEXT("Kriecher"), TEXT("Falter"), TEXT("Spinne") };
			for (int32 Index = 0; Index < Pack; ++Index)
			{
				Self->VaelSpawn(Kinds[Index % UE_ARRAY_COUNT(Kinds)], 1);
			}
		}
		if (Load->Age < 5.0f)
		{
			return true;
		}
		const float Frame = Delta * 1000.0f;
		Load->Frames.Add(Frame);
		Load->Game += FPlatformTime::ToMilliseconds(GGameThreadTime);
		Load->Render += FPlatformTime::ToMilliseconds(GRenderThreadTime);
		Load->Gpu += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
		if (Frame > Load->Worst)
		{
			Load->Worst = Frame;
			Load->WorstAt = Load->Age;
		}
		if (Frame > 50.0f)
		{
			UE_LOG(LogVael, Display, TEXT("Load test: hitch %.0f ms at %.1f s (game %.1f, render %.1f, GPU %.1f)"), Frame, Load->Age,
				FPlatformTime::ToMilliseconds(GGameThreadTime), FPlatformTime::ToMilliseconds(GRenderThreadTime), FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()));
		}

		// Casts at the nearest living enemy
		if (Load->Age >= Load->NextCast)
		{
			Load->NextCast = Load->Age + 0.5f;
			const AActor* Nearest = nullptr;
			float Best = UE_BIG_NUMBER;
			for (TActorIterator<AVaelCreature> It(World); It; ++It)
			{
				const float Distance = FVector::DistSquared(It->GetActorLocation(), Pawn->GetActorLocation());
				if (!It->IsDead() && Distance < Best)
				{
					Best = Distance;
					Nearest = *It;
				}
			}
			if (Nearest == nullptr)
			{
				Self->VaelSpawn(TEXT("Kriecher"), 3);
				Self->VaelSpawn(TEXT("Harpyie"), 2);
			}
			else
			{
				const float Yaw = (Nearest->GetActorLocation() - Pawn->GetActorLocation()).Rotation().Yaw;
				if (const AVaelCharacter* Hero = Cast<AVaelCharacter>(Pawn))
				{
					Hero->GetAbilitySystemComponent()->SetNumericAttributeBase(UVaelAttributeSet::GetManaAttribute(), Hero->GetMaxMana());
				}
				Self->VaelCast(Formulas[Load->Cast++ % UE_ARRAY_COUNT(Formulas)], Yaw);
			}
		}

		if (Load->Age >= Seconds + 5.0f)
		{
			TArray<float> Sorted = Load->Frames;
			Sorted.Sort();
			const int32 Num = FMath::Max(Sorted.Num(), 1);
			double Sum = 0.0;
			int32 Over33 = 0, Over50 = 0;
			for (const float Each : Sorted)
			{
				Sum += Each;
				Over33 += Each > 33.4f ? 1 : 0;
				Over50 += Each > 50.0f ? 1 : 0;
			}
			int32 Alive = 0;
			for (TActorIterator<AVaelCreature> It(World); It; ++It)
			{
				Alive += It->IsDead() ? 0 : 1;
			}
			UE_LOG(LogVael, Display, TEXT("Load test (%.0f s, %d casts, %d enemies alive): frame mean %.1f ms (%.0f fps), p99 %.1f ms, worst %.0f ms at %.1f s, hitches >33 ms %d, >50 ms %d; game %.1f, render %.1f, GPU %.1f ms"),
				Seconds, Load->Cast, Alive, Sum / Num, 1000.0 / FMath::Max(Sum / Num, 0.1), Sorted.IsEmpty() ? 0.0f : Sorted[FMath::Min(Num - 1, FMath::FloorToInt(Num * 0.99f))],
				Load->Worst, Load->WorstAt, Over33, Over50, Load->Game / Num, Load->Render / Num, Load->Gpu / Num);
			return false;
		}
		return true;
	}));
}

void UVaelCheatManager::VaelTropfen(float X, float Y)
{
	APlayerController* Controller = GetOuterAPlayerController();
	APawn* Player = Controller != nullptr ? Controller->GetPawn() : nullptr;
	if (Player == nullptr)
	{
		return;
	}
	const FVector From = X == 0.0f && Y == 0.0f ? Player->GetActorLocation() : FVector(X * 100.0f, Y * 100.0f, Player->GetActorLocation().Z);
	FVector Best = FVector::ZeroVector;
	float BestFloor = 0.0f;
	float BestDistance = UE_BIG_NUMBER;
	for (TActorIterator<AVaelCaveDrips> It(GetWorld()); It; ++It)
	{
		for (int32 Index = 0; Index < It->Tips.Num(); ++Index)
		{
			const float Distance = FVector::DistSquared2D(It->Tips[Index], From);
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				Best = It->Tips[Index];
				BestFloor = It->FloorHeights.IsValidIndex(Index) ? It->FloorHeights[Index] : 0.0f;
			}
		}
	}
	if (BestDistance >= UE_BIG_NUMBER)
	{
		UE_LOG(LogVael, Warning, TEXT("VaelTropfen: no dripping tip in this level"));
		return;
	}
	const FVector Spot(Best.X + 150.0f, Best.Y - 150.0f, BestFloor + Player->GetSimpleCollisionHalfHeight() + 20.0f);
	Player->TeleportTo(Spot, Player->GetActorRotation());
	UE_LOG(LogVael, Log, TEXT("VaelTropfen: beside the tip at %s"), *Best.ToCompactString());
}

void UVaelCheatManager::VaelZielTest(float Seconds, const FString& Kind, int32 Count)
{
	APlayerController* Controller = GetOuterAPlayerController();
	if (Controller == nullptr || Controller->GetPawn() == nullptr)
	{
		return;
	}
	if (Controller->GetPawn()->CanBeDamaged())
	{
		God();
	}
	VaelLearnAll();
	struct FAim
	{
		float Age = 0.0f;
		float NextCast = 3.0f;
		int32 Casts = 0;
		int32 Hits = 0;
		bool bHitThisCast = true;
		TMap<TWeakObjectPtr<AVaelCreature>, float> Health;
	};
	TSharedRef<FAim> Aim = MakeShared<FAim>();
	TWeakObjectPtr<UVaelCheatManager> WeakThis(this);
	static const TCHAR* Shots[] = { TEXT("W"), TEXT("E") };
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis, Aim, Seconds, Kind, Count](float) -> bool
	{
		UVaelCheatManager* Self = WeakThis.Get();
		APlayerController* Owner = Self != nullptr ? Self->GetOuterAPlayerController() : nullptr;
		APawn* Hero = Owner != nullptr ? Owner->GetPawn() : nullptr;
		if (Hero == nullptr)
		{
			return false;
		}
		Aim->Age += FApp::GetDeltaTime();
		// hits: every drop of health of a test creature after a cast
		int32 Alive = 0;
		for (auto It = Aim->Health.CreateIterator(); It; ++It)
		{
			AVaelCreature* Creature = It.Key().Get();
			if (Creature == nullptr)
			{
				It.RemoveCurrent();
				continue;
			}
			const float Now = Creature->IsDead() ? 0.0f : Creature->GetHealth();
			// one hit per cast at most (a shot that bursts or pierces hurts several)
			if (Now < It.Value() - 0.5f && !Aim->bHitThisCast)
			{
				++Aim->Hits;
				Aim->bHitThisCast = true;
			}
			It.Value() = Now;
			Alive += Creature->IsDead() ? 0 : 1;
		}
		if (Alive < Count && Aim->Age > 1.0f)
		{
			TSet<AVaelCreature*> Before;
			for (TActorIterator<AVaelCreature> It(Hero->GetWorld()); It; ++It)
			{
				Before.Add(*It);
			}
			Self->VaelSpawn(Kind, Count - Alive);
			for (TActorIterator<AVaelCreature> It(Hero->GetWorld()); It; ++It)
			{
				if (!Before.Contains(*It))
				{
					Aim->Health.Add(*It, It->GetHealth());
				}
			}
		}
		if (Aim->Age >= Aim->NextCast)
		{
			Aim->NextCast = Aim->Age + 0.6f;
			const AVaelCreature* Nearest = nullptr;
			float Best = UE_BIG_NUMBER;
			for (const TPair<TWeakObjectPtr<AVaelCreature>, float>& Each : Aim->Health)
			{
				const AVaelCreature* Creature = Each.Key.Get();
				const float Distance = Creature != nullptr && !Creature->IsDead() ? FVector::DistSquared(Creature->GetActorLocation(), Hero->GetActorLocation()) : UE_BIG_NUMBER;
				if (Distance < Best)
				{
					Best = Distance;
					Nearest = Creature;
				}
			}
			if (Nearest != nullptr)
			{
				if (const AVaelCharacter* Mage = Cast<AVaelCharacter>(Hero))
				{
					Mage->GetAbilitySystemComponent()->SetNumericAttributeBase(UVaelAttributeSet::GetManaAttribute(), Mage->GetMaxMana());
				}
				Self->VaelCast(Shots[Aim->Casts % UE_ARRAY_COUNT(Shots)], (Nearest->GetActorLocation() - Hero->GetActorLocation()).Rotation().Yaw);
				++Aim->Casts;
				Aim->bHitThisCast = false;
			}
		}
		if (Aim->Age >= Seconds + 3.0f)
		{
			const IConsoleVariable* Assist = IConsoleManager::Get().FindConsoleVariable(TEXT("vael.AimAssist"));
			UE_LOG(LogVael, Display, TEXT("Aim test (%s, aim help %s): %d casts, %d hits (%.0f %%)"), *Kind, Assist != nullptr && Assist->GetInt() != 0 ? TEXT("on") : TEXT("off"),
				Aim->Casts, Aim->Hits, Aim->Casts > 0 ? 100.0f * Aim->Hits / Aim->Casts : 0.0f);
			return false;
		}
		return true;
	}));
}

void UVaelCheatManager::VaelTodTest(const FString& Way, const FString& Kind, int32 Count)
{
	APlayerController* Controller = GetOuterAPlayerController();
	APawn* Player = Controller != nullptr ? Controller->GetPawn() : nullptr;
	if (Player == nullptr)
	{
		return;
	}
	// after a moment, so a teleport just before has landed
	TWeakObjectPtr<UVaelCheatManager> WeakThis(this);
	FTimerHandle Later;
	GetWorld()->GetTimerManager().SetTimer(Later, FTimerDelegate::CreateWeakLambda(this, [this, Way, Kind, Count]()
	{
	APlayerController* Controller = GetOuterAPlayerController();
	APawn* Player = Controller != nullptr ? Controller->GetPawn() : nullptr;
	if (Player == nullptr)
	{
		return;
	}
	TSet<AVaelCreature*> Before;
	for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
	{
		Before.Add(*It);
	}
	VaelSpawn(Kind, Count);
	TArray<TWeakObjectPtr<AVaelCreature>> Fresh;
	for (TActorIterator<AVaelCreature> It(GetWorld()); It; ++It)
	{
		if (!Before.Contains(*It))
		{
			// close beside the player, in the picture of the camera
			const float Angle = Fresh.Num() * 50.0f - 50.0f;
			const FVector Side = FVector(1.0f, 0.0f, 0.0f).RotateAngleAxis(Angle, FVector::UpVector);
			It->SetActorLocation(Player->GetActorLocation() + Side * 380.0f + FVector(0.0f, 0.0f, It->GetSimpleCollisionHalfHeight() - Player->GetSimpleCollisionHalfHeight() + 10.0f),
				false, nullptr, ETeleportType::TeleportPhysics);
			Fresh.Add(*It);
		}
	}
	const FString Lower = Way.ToLower();
	TWeakObjectPtr<APawn> WeakPlayer(Player);
	FTimerHandle Handle;
	GetWorld()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [Fresh, Lower, WeakPlayer]()
	{
		APawn* Hero = WeakPlayer.Get();
		for (const TWeakObjectPtr<AVaelCreature>& Weak : Fresh)
		{
			AVaelCreature* Creature = Weak.Get();
			if (Creature == nullptr || Creature->IsDead() || Hero == nullptr)
			{
				continue;
			}
			const float Life = Creature->GetHealth();
			const FVector Away = (Creature->GetActorLocation() - Hero->GetActorLocation()).GetSafeNormal2D();
			EVaelElement Element = EVaelElement::Earth;
			float Damage = Life + 1.0f;
			float Push = 0.0f;
			if (Lower.StartsWith(TEXT("f")))
			{
				Element = EVaelElement::Fire;
			}
			else if (Lower.StartsWith(TEXT("ei")))
			{
				UVaelCombatStatics::ApplyStatus(Hero, Creature, EVaelStatus::Frozen, 3.0f);
				Element = EVaelElement::Water;
			}
			else if (Lower.StartsWith(TEXT("w")))
			{
				Element = EVaelElement::Air;
				Damage = Life * 4.0f;
				Push = 1100.0f;
			}
			else if (Lower.StartsWith(TEXT("b")))
			{
				Element = EVaelElement::Air;
			}
			else if (Lower.StartsWith(TEXT("er")))
			{
				Element = EVaelElement::Earth;
				Damage = Life * 4.0f;
			}
			else if (Lower.StartsWith(TEXT("n")))
			{
				Element = EVaelElement::Mark;
			}
			UVaelCombatStatics::DealDamage(Hero, Creature, Damage, Element);
			if (Push > 0.0f)
			{
				Creature->ApplyKnockback(Away, Push);
			}
			const UVaelDeathMotion* Motion = Creature->FindComponentByClass<UVaelDeathMotion>();
			UE_LOG(LogVael, Log, TEXT("VaelTodTest: '%s' %s"), *Creature->GetName(), Motion != nullptr ? TEXT("dies with a motion") : TEXT("no motion"));
		}
	}), 1.2f, false);
	}), 1.5f, false);
}

void UVaelCheatManager::VaelKlasse(const FString& Class, const FString& Look)
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	const AVaelPlayerController* VaelController = Cast<AVaelPlayerController>(PlayerController);
	AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
	if (Player == nullptr)
	{
		return;
	}

	FVaelHeroChoice Choice;
	Choice.Class = Class.StartsWith(TEXT("f"), ESearchCase::IgnoreCase) ? EVaelHeroClass::Floromant
			: Class.StartsWith(TEXT("i"), ESearchCase::IgnoreCase) || Class.StartsWith(TEXT("e"), ESearchCase::IgnoreCase) ? EVaelHeroClass::Engineer : EVaelHeroClass::Mage;
	Choice.bFemale = Look.StartsWith(TEXT("f"), ESearchCase::IgnoreCase) || Look.StartsWith(TEXT("w"), ESearchCase::IgnoreCase);

	// Remembered for the slot like a choice on the page "Gestalt"
	if (UVaelHeroSubsystem* Heroes = VaelController != nullptr ? UVaelHeroSubsystem::Get(Player) : nullptr)
	{
		Heroes->SetChoice(VaelController->GetPlayerSlot(), Choice);
	}
	Player->SetHero(Choice.Class, Choice.bFemale);
}

void UVaelCheatManager::VaelTurm(const FString& Hand)
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	const AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
	if (Player != nullptr && Player->IsFloromant())
	{
		Player->GetFloromant()->RootTower(!Hand.StartsWith(TEXT("r"), ESearchCase::IgnoreCase));
	}
}


void UVaelCheatManager::VaelZielmarke(int32 Hands, float Seconds)
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	const AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
	if (Player != nullptr && Player->IsFloromant())
	{
		UVaelFloromantComponent* Floromant = Player->GetFloromant();
		Floromant->ShowTowerAim(Hands, Floromant->GetTowerAimPoint(Seconds));
	}
}

void UVaelCheatManager::VaelPaarsaat(float Seconds)
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	const AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
	if (Player != nullptr && Player->IsFloromant())
	{
		UVaelFloromantComponent* Floromant = Player->GetFloromant();
		Floromant->SowSymbiosis(Floromant->GetTowerAimPoint(Seconds));
	}
}

void UVaelCheatManager::VaelDrachenerbe(const FString& State)
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	const AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
	if (Player == nullptr || !Player->IsFloromant())
	{
		return;
	}
	const bool bAwake = !State.StartsWith(TEXT("aus"), ESearchCase::IgnoreCase);
	for (int32 Kind = 1; Kind < 256; ++Kind)
	{
		if (VaelSymbiosis::HasHeritage(static_cast<EVaelSymbiosis>(Kind)))
		{
			Player->GetFloromant()->SetHeritageAwake(static_cast<EVaelSymbiosis>(Kind), bAwake, true);
		}
	}
}

void UVaelCheatManager::VaelKreuzen(float Seconds)
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
	if (Player == nullptr || !Player->IsFloromant())
	{
		return;
	}
	// Pressed now, let go after the seconds; the short charge runs out by itself after an early release
	UVaelFloromantComponent* Floromant = Player->GetFloromant();
	Floromant->StartCross();
	if (Seconds <= 0.0f)
	{
		Floromant->ReleaseCross();
		return;
	}
	TWeakObjectPtr<UVaelFloromantComponent> WeakFloromant = Floromant;
	FTimerHandle Timer;
	Player->GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([WeakFloromant]()
	{
		if (UVaelFloromantComponent* Held = WeakFloromant.Get())
		{
			Held->ReleaseCross();
		}
	}), Seconds, false);
}

void UVaelCheatManager::VaelWurzeln()
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	const AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
	if (Player != nullptr && Player->IsFloromant())
	{
		Player->GetFloromant()->ToggleRoot();
	}
}

void UVaelCheatManager::VaelTauschen()
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	const AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
	if (Player != nullptr && Player->IsFloromant())
	{
		Player->GetFloromant()->SwapHands();
	}
}

void UVaelCheatManager::VaelTurmruf()
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	const AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
	if (Player != nullptr && Player->IsFloromant())
	{
		Player->GetFloromant()->Overgrow();
	}
}

void UVaelCheatManager::VaelSteigen()
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	const AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
	if (Player != nullptr && Player->IsFloromant())
	{
		Player->GetFloromant()->ToggleRise();
	}
}

void UVaelCheatManager::VaelHudKlasse(const FString& ClassName)
{
	if (const AVaelPlayerController* VaelController = Cast<AVaelPlayerController>(GetOuterAPlayerController()))
	{
		AVaelHUD::SetPanelPreview(VaelController, VaelController->GetPlayerSlot(), ClassName);
	}
}

void UVaelCheatManager::VaelWeg(const FString& Way)
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	if (AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr)
	{
		Player->SetPath(Way.StartsWith(TEXT("l"), ESearchCase::IgnoreCase) ? EVaelPath::Light : Way.StartsWith(TEXT("s"), ESearchCase::IgnoreCase) ? EVaelPath::Shadow
			: Way.StartsWith(TEXT("z"), ESearchCase::IgnoreCase) ? EVaelPath::Twilight : EVaelPath::None);
	}
}

void UVaelCheatManager::VaelErfahrung(float Amount)
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	if (AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr)
	{
		Player->AddExperience(Amount);
	}
}

void UVaelCheatManager::VaelRucksack(int32 Count)
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	if (AVaelCharacter* VaelCharacter = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr)
	{
		const EVaelRarity Rarities[] = { EVaelRarity::Common, EVaelRarity::Magic, EVaelRarity::Rare };
		for (int32 Index = 0; Index < Count; ++Index)
		{
			VaelCharacter->GetInventory()->AddItem(UVaelItemSettings::Get()->RollItem(Rarities[Index % UE_ARRAY_COUNT(Rarities)]));
		}
	}
}

void UVaelCheatManager::VaelBuch(int32 Mode)
{
	AVaelPlayerController* VaelController = Cast<AVaelPlayerController>(GetOuterAPlayerController());
	const AVaelCharacter* VaelCharacter = VaelController != nullptr ? VaelController->GetPawn<AVaelCharacter>() : nullptr;
	if (VaelCharacter == nullptr)
	{
		return;
	}

	if (Mode == 2)
	{
		VaelController->OpenGrimoire(EVaelMenuPage::Inventory);
		return;
	}
	VaelCharacter->GetFloatingGrimoire()->SetForced(Mode != 0);
}

void UVaelCheatManager::VaelInventarSchritt(int32 Right, int32 Down)
{
	if (AVaelPlayerController* VaelController = Cast<AVaelPlayerController>(GetOuterAPlayerController()))
	{
		VaelController->NavigateInventory(Right, Down);
	}
}

void UVaelCheatManager::VaelBeutel()
{
	if (AVaelPlayerController* VaelController = Cast<AVaelPlayerController>(GetOuterAPlayerController()))
	{
		VaelController->OnMenuBag();
	}
}

void UVaelCheatManager::VaelMenue(const FString& Page)
{
	if (AVaelPlayerController* VaelController = Cast<AVaelPlayerController>(GetOuterAPlayerController()))
	{
		// "Ruestung" or "Ruestung<n>": the engineer's toolbox open on its armour tab, piece n picked (Boris 17.11.)
		const bool bArmor = Page.StartsWith(TEXT("r"), ESearchCase::IgnoreCase);
		const bool bBag = Page.StartsWith(TEXT("b"), ESearchCase::IgnoreCase);
		const EVaelMenuPage Open = Page.StartsWith(TEXT("i"), ESearchCase::IgnoreCase) || bArmor || bBag ? EVaelMenuPage::Inventory : Page.StartsWith(TEXT("k"), ESearchCase::IgnoreCase) ? EVaelMenuPage::Compendium
			: Page.StartsWith(TEXT("a"), ESearchCase::IgnoreCase) ? EVaelMenuPage::Quests : Page.StartsWith(TEXT("g"), ESearchCase::IgnoreCase) ? EVaelMenuPage::Hero : EVaelMenuPage::Formulas;
		VaelController->CloseGrimoire();
		VaelController->OpenGrimoire(Open);
		if (bBag)
		{
			// "Beutel<n>": the tab n of the bag (Boris 17.11., pictures of the materials)
			VaelController->ShowBagTab(FCString::Atoi(*Page.RightChop(6)));
		}
		if (bArmor)
		{
			int32 Digits = Page.Len();
			while (Digits > 0 && FChar::IsDigit(Page[Digits - 1]))
			{
				--Digits;
			}
			VaelController->ShowInventoryRow(FCString::Atoi(*Page.Mid(Digits)));
		}
	}
}

void UVaelCheatManager::VaelMitspieler(const FString& Class, const FString& Look)
{
	UGameInstance* GameInstance = GetWorld() != nullptr ? GetWorld()->GetGameInstance() : nullptr;
	if (GameInstance == nullptr || GameInstance->GetNumLocalPlayers() >= AVaelGameMode::MaxLocalPlayers)
	{
		return;
	}

	// A class named here is kept for the slot the new player will most likely get: the next one
	UVaelHeroSubsystem* Heroes = UVaelHeroSubsystem::Get(GetWorld());
	if (!Class.IsEmpty() && Heroes != nullptr)
	{
		FVaelHeroChoice Choice;
		Choice.Class = Class.StartsWith(TEXT("f"), ESearchCase::IgnoreCase) ? EVaelHeroClass::Floromant
			: Class.StartsWith(TEXT("i"), ESearchCase::IgnoreCase) || Class.StartsWith(TEXT("e"), ESearchCase::IgnoreCase) ? EVaelHeroClass::Engineer : EVaelHeroClass::Mage;
		Choice.bFemale = Look.StartsWith(TEXT("f"), ESearchCase::IgnoreCase) || Look.StartsWith(TEXT("w"), ESearchCase::IgnoreCase);
		Heroes->SetChoice(GameInstance->GetNumLocalPlayers(), Choice);
	}

	// The next free controller number, like a controller that pressed Start
	FString Error;
	if (GameInstance->CreateLocalPlayer(GameInstance->GetNumLocalPlayers(), Error, true) == nullptr)
	{
		UE_LOG(LogVael, Warning, TEXT("VaelMitspieler: %s"), *Error);
	}
}

void UVaelCheatManager::VaelSamen(const FString& Seed, float Metres)
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
	if (Player == nullptr || !Player->IsFloromant())
	{
		return;
	}

	UVaelFloromantComponent* Floromant = Player->GetFloromant();
	for (int32 Index = 0; Index < Floromant->GetBeltSize(); ++Index)
	{
		if (!Seed.IsEmpty() && Floromant->GetBeltName(Index).ToString().StartsWith(Seed, ESearchCase::IgnoreCase))
		{
			Floromant->SelectBelt(Index);
		}
	}

	// Saft for tests: the cheat never fails for lack of it
	Player->GetAbilitySystemComponent()->SetNumericAttributeBase(UVaelAttributeSet::GetManaAttribute(), Player->GetMaxMana());
	Floromant->Sow(Player->GetActorLocation() + Player->GetActorForwardVector() * FMath::Max(Metres, 1.2f) * 100.0f);
}

void UVaelCheatManager::VaelErz(const FString& Name, float Metres)
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	const APawn* Player = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
	if (Player == nullptr)
	{
		return;
	}

	FVector Spot = Player->GetActorLocation() + Player->GetActorForwardVector() * Metres * 100.0f;
	FHitResult GroundHit;
	if (VaelGround::TraceGround(GetWorld(), Spot + FVector(0.0f, 0.0f, 300.0f), Spot - FVector(0.0f, 0.0f, 1000.0f), GroundHit))
	{
		Spot = GroundHit.Location;
	}

	// What can be gathered: the real thing, with its data
	if (UVaelHarvestableData* Data = LoadObject<UVaelHarvestableData>(nullptr, *FString::Printf(TEXT("/Game/Vael/Nature/DA_Harvest_%s.DA_Harvest_%s"), *Name, *Name)))
	{
		AVaelHarvestable* Ore = GetWorld()->SpawnActorDeferred<AVaelHarvestable>(AVaelHarvestable::StaticClass(), FTransform(Spot), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Ore != nullptr)
		{
			Ore->SetData(Data);
			Ore->FinishSpawning(FTransform(Spot));
			UE_LOG(LogVael, Log, TEXT("VaelErz: %s"), *Name);
		}
		return;
	}

	// A model that is not in the game's data yet: only to look at, with the glint planned for it
	for (const TCHAR* Region : { TEXT("Aschenmark"), TEXT("Wurzelforst") })
	{
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Vael/Requisiten/%s/%s/SM_%s.SM_%s"), Region, *Name, *Name, *Name), nullptr, LOAD_NoWarn);
		if (Mesh == nullptr)
		{
			continue;
		}
		AStaticMeshActor* Prop = GetWorld()->SpawnActor<AStaticMeshActor>(Spot, FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f));
		Prop->SetMobility(EComponentMobility::Movable);
		Prop->GetStaticMeshComponent()->SetStaticMesh(Mesh);
		Prop->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// The pearl plants are plants, not ores: they sway in the wind and drip (Boris 2026-11-12), no glint.
		// Saftperle (light): clear red sap; Eiterperle (shadow): thick yellow-green pus, slower and heavier
		if (Name.Contains(TEXT("perle")))
		{
			const bool bPus = Name.Contains(TEXT("Eiter"));
			FVaelPlantLife Life;
			Life.Sway = bPus ? 1.6f : 2.2f;
			Life.SwayPeriod = bPus ? 6.0f : 4.6f;
			Life.DripRate = bPus ? 0.9f : 1.3f;
			Life.DripColor = bPus ? FLinearColor(0.3f, 0.36f, 0.05f) : FLinearColor(0.36f, 0.04f, 0.03f);
			Life.DripGlow = bPus ? 0.3f : 0.25f;
			Life.DripSize = bPus ? 4.4f : 3.6f;
			UVaelPlantLifeComponent* PlantLife = NewObject<UVaelPlantLifeComponent>(Prop);
			PlantLife->SetupAttachment(Prop->GetRootComponent());
			PlantLife->RegisterComponent();
			PlantLife->Show(Prop->GetStaticMeshComponent(), Life);
			UE_LOG(LogVael, Log, TEXT("VaelErz: plant model %s (no plant data yet)"), *Name);
			return;
		}

		// The Niddruse away from its source (its harvest data only lets it grow at an open one): violet with a red heart
		FVaelSparkle Sparkle;
		Sparkle.Rate = 9.0f;
		Sparkle.TopShare = 0.8f;
		if (Name.Contains(TEXT("druse")))
		{
			Sparkle.Color = FLinearColor(0.62f, 0.22f, 1.0f);
			Sparkle.SecondColor = FLinearColor(1.0f, 0.15f, 0.2f);
			Sparkle.SecondShare = 0.3f;
			Sparkle.Size = 24.0f;
			Sparkle.Brightness = 22.0f;
			Sparkle.Pulse = 0.6f;
			Sparkle.PulsePeriod = 2.8f;
		}
		UVaelSparkleComponent* Sparkles = NewObject<UVaelSparkleComponent>(Prop);
		Sparkles->SetupAttachment(Prop->GetRootComponent());
		Sparkles->RegisterComponent();
		UMaterialInstanceDynamic* Material = Prop->GetStaticMeshComponent()->CreateAndSetMaterialInstanceDynamic(0);
		Sparkles->SetGlow(Material, 2.0f);
		Sparkles->Show(Prop->GetStaticMeshComponent(), Sparkle);
		UE_LOG(LogVael, Log, TEXT("VaelErz: model %s (no harvest data yet)"), *Name);
		return;
	}

	UE_LOG(LogVael, Warning, TEXT("VaelErz: neither DA_Harvest_%s nor a model SM_%s"), *Name, *Name);
}

void UVaelCheatManager::VaelPflanze(const FString& PlantId, float Metres, const FString& Form)
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
	UVaelPlantData* Data = UVaelFloromantComponent::FindPlantData(FName(*PlantId));
	if (Player == nullptr || Data == nullptr)
	{
		return;
	}

	FVector Ground = Player->GetActorLocation() + Player->GetActorForwardVector() * FMath::Max(Metres, 1.0f) * 100.0f;
	AVaelCreature::FindGround(GetWorld(), Ground, Ground);
	Player->GetFloromant()->PlantSeed(Data, Form.StartsWith(TEXT("s"), ESearchCase::IgnoreCase), Ground, Player->GetActorRotation().Yaw);
}

void UVaelCheatManager::VaelFaehigkeit(const FString& Ability, float Metres)
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
	if (Player == nullptr || !Player->IsFloromant())
	{
		return;
	}

	Player->GetAbilitySystemComponent()->SetNumericAttributeBase(UVaelAttributeSet::GetManaAttribute(), Player->GetMaxMana());
	UVaelFloromantComponent* Floromant = Player->GetFloromant();
	const FVector Aim = Player->GetActorLocation() + Player->GetActorForwardVector() * FMath::Max(Metres, 1.2f) * 100.0f;
	if (Ability.StartsWith(TEXT("r"), ESearchCase::IgnoreCase))
	{
		Floromant->CallGarden();
	}
	else if (Ability.StartsWith(TEXT("g"), ESearchCase::IgnoreCase))
	{
		Floromant->RootGrip(Aim);
	}
	else if (Ability.StartsWith(TEXT("s"), ESearchCase::IgnoreCase))
	{
		Floromant->RootLeap(Aim);
	}
	else if (Ability.StartsWith(TEXT("w"), ESearchCase::IgnoreCase))
	{
		Floromant->Overgrow();
	}
	else
	{
		Floromant->ThornWhip(Aim);
	}
}

void UVaelCheatManager::VaelHand(const FString& Hands, const FString& Plant)
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
	if (Player == nullptr || !Player->IsFloromant())
	{
		return;
	}

	UVaelFloromantComponent* Floromant = Player->GetFloromant();
	// Any plant of her repertoire: one not on the wheel goes into the picked place
	for (int32 Index = 0; !Plant.IsEmpty() && Index < Floromant->GetCatalogSize(); ++Index)
	{
		const UVaelPlantArmData* Data = Floromant->GetCatalogPlant(Index);
		if (Data != nullptr && Data->GetDisplayName(false).ToString().StartsWith(Plant, ESearchCase::IgnoreCase))
		{
			Floromant->PickPlant(Data);
			break;
		}
	}
	const bool bBoth = Hands.IsEmpty() || Hands.Equals(TEXT("beide"), ESearchCase::IgnoreCase);
	if (bBoth || Hands.Equals(TEXT("links"), ESearchCase::IgnoreCase))
	{
		Floromant->PutOnHand(true);
	}
	if (bBoth || Hands.Equals(TEXT("rechts"), ESearchCase::IgnoreCase))
	{
		Floromant->PutOnHand(false);
	}
}

void UVaelCheatManager::VaelFeuern(const FString& Hands, float Seconds)
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
	if (Player == nullptr || !Player->IsFloromant())
	{
		return;
	}

	UVaelFloromantComponent* Floromant = Player->GetFloromant();
	const bool bOff = Hands.Equals(TEXT("aus"), ESearchCase::IgnoreCase);
	const bool bBoth = Hands.IsEmpty() || Hands.Equals(TEXT("beide"), ESearchCase::IgnoreCase);
	Floromant->SetHandFiring(true, !bOff && (bBoth || Hands.Equals(TEXT("links"), ESearchCase::IgnoreCase)));
	Floromant->SetHandFiring(false, !bOff && (bBoth || Hands.Equals(TEXT("rechts"), ESearchCase::IgnoreCase)));

	// A tap: let go by itself after the given seconds
	if (Seconds > 0.0f && !bOff && GetWorld() != nullptr)
	{
		TWeakObjectPtr<UVaelFloromantComponent> Weak(Floromant);
		FTimerHandle Handle;
		GetWorld()->GetTimerManager().SetTimer(Handle, [Weak]()
		{
			if (UVaelFloromantComponent* Held = Weak.Get())
			{
				Held->SetHandFiring(true, false);
				Held->SetHandFiring(false, false);
			}
		}, Seconds, false);
	}
}

void UVaelCheatManager::VaelNachladen(float Percent)
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
	if (Player == nullptr || !Player->IsFloromant())
	{
		return;
	}

	UVaelFloromantComponent* Floromant = Player->GetFloromant();
	if (Percent >= 0.0f)
	{
		Floromant->AddKeimpelz(Percent / 100.0f * UVaelFloromantSettings::Get()->KeimpelzMax - Floromant->GetKeimpelz());
	}
	const int32 Shots = Floromant->ReloadHands();
	UE_LOG(LogVael, Display, TEXT("VaelNachladen: %d shots reloaded, Keimpelz %.0f left"), Shots, Floromant->GetKeimpelz());
}

void UVaelCheatManager::VaelBrand(float Radius, float Metres, const FString& Where)
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	const APawn* Player = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
	if (Player == nullptr)
	{
		return;
	}

	FVaelFireShape Shape;
	Shape.Radius = FMath::Max(Radius, 5.0f);
	Shape.Height = Shape.Radius * 2.3f;
	Shape.bScorch = true;
	// Towards the camera, so the fire is in the picture whichever way the figure faces; "wand" looks for a wall ahead
	const bool bWall = Where.Equals(TEXT("wand"), ESearchCase::IgnoreCase);
	// "live" simulates the fire while the game runs (test of NiagaraFluids)
	Shape.bLive = Where.Equals(TEXT("live"), ESearchCase::IgnoreCase);
	const FVector ToCamera = PlayerController->PlayerCameraManager != nullptr ? (PlayerController->PlayerCameraManager->GetCameraLocation() - Player->GetActorLocation()).GetSafeNormal2D() : -Player->GetActorForwardVector();
	const FVector Forward = bWall ? Player->GetActorForwardVector() : ToCamera;
	FVector Location = Player->GetActorLocation() + Forward * Metres * 100.0f;

	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(VaelBrand), false, Player);
	if (bWall)
	{
		// The first wall ahead
		if (GetWorld()->LineTraceSingleByChannel(Hit, Player->GetActorLocation(), Player->GetActorLocation() + Forward * 3000.0f, ECC_Visibility, Query))
		{
			Location = Hit.ImpactPoint;
			Shape.WallNormal = Hit.ImpactNormal;
		}
	}
	else if (GetWorld()->LineTraceSingleByChannel(Hit, Location + FVector(0.0f, 0.0f, 300.0f), Location - FVector(0.0f, 0.0f, 600.0f), ECC_Visibility, Query))
	{
		Location = Hit.ImpactPoint;
	}
	if (Where.Equals(TEXT("stoss"), ESearchCase::IgnoreCase))
	{
		// The blast of a Feuerball, a metre above the ground
		AVaelFireSpot::SpawnBurst(GetWorld(), Location + FVector(0.0f, 0.0f, 100.0f), Shape.Radius);
		return;
	}
	// Burns for a while like the fire of a spell; a second call on the same spot feeds it, so it grows as a Brandherd
	AVaelFireSpot::SpawnOrJoin(GetWorld(), Location, Shape, 25.0f, false);
	UE_LOG(LogVael, Display, TEXT("VaelBrand: fire of %.0f cm at %s, player at %s"), Shape.Radius, *Location.ToCompactString(), *Player->GetActorLocation().ToCompactString());
}

void UVaelCheatManager::VaelFalter()
{
	const APawn* Player = GetOuterAPlayerController()->GetPawn();
	if (Player == nullptr)
	{
		return;
	}

	const FVector Beside = Player->GetActorRightVector() * (UVaelWorldSettings::Get()->OrbPickupRadius + 60.0f);
	AVaelPickupOrb::DropOrb(GetWorld(), Player->GetActorLocation() + Beside, EVaelOrbKind::Health);
	AVaelPickupOrb::DropOrb(GetWorld(), Player->GetActorLocation() - Beside, EVaelOrbKind::Mana);
}

void UVaelCheatManager::VaelAufladen(int32 Mana)
{
	AVaelRechargeGlow::Play(GetOuterAPlayerController()->GetPawn<AVaelCharacter>(), Mana != 0 ? EVaelOrbKind::Mana : EVaelOrbKind::Health);
}

void UVaelCheatManager::VaelNatterBrut()
{
	for (TActorIterator<AVaelPfuhlnatterPopulation> It(GetWorld()); It; ++It)
	{
		It->BreedNow();
	}
}

void UVaelCheatManager::VaelMedaillon()
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	const AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
	if (Player != nullptr && Player->IsFloromant())
	{
		Player->GetFloromant()->FlipWheel();
	}
}

void UVaelCheatManager::VaelKeimpelz(float Share)
{
	const float Max = FMath::Max(UVaelFloromantSettings::Get()->KeimpelzMax, 1.0f);
	for (TActorIterator<AVaelCharacter> It(GetWorld()); It; ++It)
	{
		if (UVaelFloromantComponent* Floromant = It->IsFloromant() ? It->GetFloromant() : nullptr)
		{
			Floromant->AddKeimpelz(FMath::Clamp(Share, 0.0f, 1.0f) * Max - Floromant->GetKeimpelz());
		}
	}
}

void UVaelCheatManager::VaelNid(float Percent, int32 ShadowHands, const FString& Way)
{
	for (TActorIterator<AVaelCharacter> It(GetWorld()); It; ++It)
	{
		UVaelFloromantComponent* Floromant = It->IsFloromant() ? It->GetFloromant() : nullptr;
		if (Floromant == nullptr)
		{
			continue;
		}
		It->GetAbilitySystemComponent()->SetNumericAttributeBase(UVaelAttributeSet::GetCorruptionAttribute(), FMath::Clamp(Percent, 0.0f, 100.0f));
		if (!Way.IsEmpty())
		{
			It->SetPath(Way.StartsWith(TEXT("l"), ESearchCase::IgnoreCase) ? EVaelPath::Light : Way.StartsWith(TEXT("s"), ESearchCase::IgnoreCase) ? EVaelPath::Shadow
				: Way.StartsWith(TEXT("z"), ESearchCase::IgnoreCase) ? EVaelPath::Twilight : EVaelPath::None);
		}
		for (const bool bLeft : { true, false })
		{
			if (UVaelPlantArmComponent* Hand = Floromant->FindHand(bLeft))
			{
				Hand->SetShadowForm(ShadowHands >= (bLeft ? 1 : 2));
			}
		}
	}
}

void UVaelCheatManager::VaelKeim(int32 Right, int32 Down, int32 Press)
{
	if (AVaelPlayerController* VaelController = Cast<AVaelPlayerController>(GetOuterAPlayerController()))
	{
		if (Right != 0 || Down != 0)
		{
			VaelController->NavigateKeimgeflecht(Right, Down);
		}
		if (Press != 0)
		{
			VaelController->ConfirmKeimgeflecht();
		}
	}
}

void UVaelCheatManager::VaelRefugium(int32 Stage, const FString& Name)
{
	AVaelRefugiumClutch::SetReachedStage(GetOuterAPlayerController(), FName(*Name), Stage);
}

void UVaelCheatManager::VaelSchluepfen()
{
	AVaelRefugiumClutch::HatchAllAwake(GetOuterAPlayerController());
}

void UVaelCheatManager::VaelBrauen(const FString& What)
{
	AVaelPlayerController* Controller = Cast<AVaelPlayerController>(GetOuterAPlayerController());
	AVaelCharacter* Player = Controller != nullptr ? Controller->GetPawn<AVaelCharacter>() : nullptr;
	if (Player == nullptr)
	{
		return;
	}
	if (What != TEXT("nur"))
	{
		TSet<const UVaelMaterial*> Given;
		for (const FVaelRecipe& Recipe : FVaelBrewing::GetRecipes())
		{
			for (const TSoftObjectPtr<UVaelMaterial>& Ingredient : Recipe.Ingredients)
			{
				const UVaelMaterial* Loaded = Ingredient.LoadSynchronous();
				if (Loaded != nullptr && !Given.Contains(Loaded))
				{
					Player->GetMaterialBag()->AddMaterial(Loaded, 3);
					Given.Add(Loaded);
				}
			}
		}
		Player->GetMaterialBag()->AddMaterial(LoadObject<UVaelMaterial>(nullptr, TEXT("/Game/Vael/Items/Materials/DA_Material_Ordenssiegel.DA_Material_Ordenssiegel")), 1);
	}
	// a recipe id brews it right away and reports what the bag holds afterwards
	for (const FVaelRecipe& Recipe : FVaelBrewing::GetRecipes())
	{
		if (Recipe.Id == FName(*What))
		{
			const bool bBrewed = FVaelBrewing::Brew(Player, Recipe);
			UE_LOG(LogVael, Log, TEXT("VaelBrauen: %s brewed %s, the bag now holds %d"), *Recipe.Id.ToString(), bBrewed ? TEXT("yes") : TEXT("no"),
				Player->GetMaterialBag()->GetCount(Recipe.Result.LoadSynchronous()));
			return;
		}
	}
	Controller->OpenBrewing();
}

void UVaelCheatManager::VaelBeet(const FString& What)
{
	AVaelPlayerController* Controller = Cast<AVaelPlayerController>(GetOuterAPlayerController());
	const AVaelCharacter* Player = Controller != nullptr ? Controller->GetPawn<AVaelCharacter>() : nullptr;
	if (Player == nullptr)
	{
		return;
	}
	AVaelRefugiumClutch* Nearest = nullptr;
	float Best = TNumericLimits<float>::Max();
	for (TActorIterator<AVaelRefugiumClutch> It(GetWorld()); It; ++It)
	{
		const float Distance = FVector::Dist2D(It->GetActorLocation(), Player->GetActorLocation());
		if (It->GetState() == EVaelClutchState::Bed && !It->HasPlant() && Distance < Best)
		{
			Nearest = *It;
			Best = Distance;
		}
	}
	const TArray<FName> Choices = Controller->GetSowChoices();
	// "drehen": a shadow form into the bed, the player steps beside it and pours a Klartrunk over it (Die Waage)
	if (What == TEXT("drehen") && Nearest != nullptr && !Choices.IsEmpty())
	{
		AVaelCharacter* Pourer = Controller->GetPawn<AVaelCharacter>();
		Nearest->SowFrom(Pourer, Choices[0], true);
		Pourer->SetActorLocation(Nearest->GetActorLocation() + FVector(120.0f, 0.0f, 100.0f), false, nullptr, ETeleportType::TeleportPhysics);
		const UVaelMaterial* Klartrunk = LoadObject<UVaelMaterial>(nullptr, TEXT("/Game/Vael/Items/Materials/DA_Material_Klartrunk.DA_Material_Klartrunk"));
		Pourer->GetMaterialBag()->AddMaterial(Klartrunk, 1);
		const bool bTurned = Pourer->GetMaterialBag()->UseMaterial(Klartrunk);
		int32 Light = 0;
		int32 Shadow = 0;
		UVaelGardenSubsystem::Get(Pourer)->CountSown(FName(TEXT("L_Glutkammer")), Light, Shadow);
		UE_LOG(LogVael, Log, TEXT("VaelBeet drehen: poured %s, beds now light %d shadow %d"), bTurned ? TEXT("yes") : TEXT("no"), Light, Shadow);
		return;
	}
	if ((What == TEXT("sofort") || What == TEXT("schatten")) && Nearest != nullptr && !Choices.IsEmpty())
	{
		const bool bSown = Nearest->SowFrom(Controller->GetPawn<AVaelCharacter>(), Choices[0], What == TEXT("schatten"));
		UE_LOG(LogVael, Log, TEXT("VaelBeet: %s sown into %s: %s"), *Choices[0].ToString(), *Nearest->GetName(), bSown ? TEXT("yes") : TEXT("no"));
		return;
	}
	Controller->OpenSowing(Nearest);
}

void UVaelCheatManager::VaelTrinken(const FString& PotionId)
{
	AVaelPlayerController* Controller = Cast<AVaelPlayerController>(GetOuterAPlayerController());
	AVaelCharacter* Player = Controller != nullptr ? Controller->GetPawn<AVaelCharacter>() : nullptr;
	if (Player == nullptr)
	{
		return;
	}
	VaelBrauen(PotionId);
	for (const FVaelRecipe& Recipe : FVaelBrewing::GetRecipes())
	{
		if (Recipe.Id != FName(*PotionId))
		{
			continue;
		}
		UAbilitySystemComponent* AbilitySystem = Player->GetAbilitySystemComponent();
		const float ManaBefore = AbilitySystem->GetNumericAttribute(UVaelAttributeSet::GetManaAttribute());
		AbilitySystem->SetNumericAttributeBase(UVaelAttributeSet::GetManaAttribute(), 0.0f);
		const bool bUsed = Player->GetMaterialBag()->UseMaterial(Recipe.Result.LoadSynchronous());
		const UVaelPotionEffects* Effects = UVaelPotionEffects::Find(Player);
		FString Active;
		for (const TPair<FName, float>& Pair : Effects != nullptr ? Effects->GetActive() : TArray<TPair<FName, float>>())
		{
			Active += FString::Printf(TEXT("%s %.0fs "), *Pair.Key.ToString(), Pair.Value);
		}
		FGameplayTagContainer Fire;
		Fire.AddTag(VaelTags::Element_Fire);
		UE_LOG(LogVael, Log, TEXT("VaelTrinken: %s used %s; active: %s; speed x%.2f, fire damage x%.2f, notice x%.2f, mana %.0f (was %.0f before emptying)"),
			*PotionId, bUsed ? TEXT("yes") : TEXT("no"), *Active, Effects != nullptr ? Effects->GetSpeedMultiplier() : 1.0f,
			Effects != nullptr ? Effects->GetIncomingMultiplier(Fire) : 1.0f, Effects != nullptr ? Effects->GetNoticeMultiplier() : 1.0f,
			AbilitySystem->GetNumericAttribute(UVaelAttributeSet::GetManaAttribute()), ManaBefore);
		return;
	}
}

void UVaelCheatManager::VaelTrankTest(const FString& What)
{
	AVaelPlayerController* Controller = Cast<AVaelPlayerController>(GetOuterAPlayerController());
	AVaelCharacter* Player = Controller != nullptr ? Controller->GetPawn<AVaelCharacter>() : nullptr;
	if (Player == nullptr)
	{
		return;
	}
	// "bogen": the dummy stands far, for the engineer's bow
	const FVector Spot = Player->GetActorLocation() + Player->GetActorForwardVector() * (What == TEXT("bogen") ? 1500.0f : 400.0f);
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AVaelCharacterBase* Dummy = GetWorld()->SpawnActor<AVaelTrainingDummy>(AVaelTrainingDummy::StaticClass(), Spot, FRotator::ZeroRotator, Parameters);
	if (Dummy == nullptr)
	{
		UE_LOG(LogVael, Warning, TEXT("VaelTrankTest: no dummy"));
		return;
	}
	const float Before = Dummy->GetHealth();
	// "puls": the power potions that strike by themselves (Hainruf, Würgherrschaft, Feenleib) instead of the vials
	const TArray<const TCHAR*> Potions = What == TEXT("puls") ? TArray<const TCHAR*>{ TEXT("Hainruf"), TEXT("Wuergherrschaft"), TEXT("Feenleib") }
		: TArray<const TCHAR*>{ TEXT("Giftphiole"), TEXT("Schwarzbrand"), TEXT("Ermattungsfluch"), TEXT("Saeurewolke") };
	for (const TCHAR* Potion : Potions)
	{
		VaelTrinken(Potion);
		// a vial is an oil on the weapon (one strike carries it to the dummy); the engineer fires it with the sling instead
		if (What != TEXT("puls") && !UVaelPotionEffects::ThrowsVials(Player))
		{
			UVaelCombatStatics::DealDamage(Player, Dummy, 4.0f, EVaelElement::Earth);
		}
	}
	TWeakObjectPtr<AVaelCharacterBase> Weak = Dummy;
	FTimerHandle Handle;
	GetWorld()->GetTimerManager().SetTimer(Handle, [Weak, Before]()
	{
		if (const AVaelCharacterBase* Target = Weak.Get())
		{
			UE_LOG(LogVael, Log, TEXT("VaelTrankTest: dummy health %.0f -> %.0f, frozen %d, burning %d, slowed %d, confused %d, marked %d"), Before, Target->GetHealth(),
				UVaelCombatStatics::HasStatus(Target, EVaelStatus::Frozen), UVaelCombatStatics::HasStatus(Target, EVaelStatus::Burning), UVaelCombatStatics::HasStatus(Target, EVaelStatus::Slowed),
				UVaelCombatStatics::HasStatus(Target, EVaelStatus::Confused), UVaelCombatStatics::HasStatus(Target, EVaelStatus::Marked));
		}
	}, 4.0f, false);
}

namespace
{
	/** Test: only this kind of shot in the pockets of the Werkrolle, filled from the backpack */
	void PocketOnly(UVaelEngineerComponent* Engineer, FName Kind)
	{
		if (Engineer == nullptr)
		{
			return;
		}
		const TArray<FName> Kinds = Engineer->GetPocketKinds();
		for (const FName Other : Kinds)
		{
			if (Other != Kind)
			{
				Engineer->EmptyPocket(Other);
			}
		}
		Engineer->PutInPocket(Kind);
	}
}

void UVaelCheatManager::VaelIngenieur(const FString& What, const FString& Id)
{
	AVaelPlayerController* Controller = Cast<AVaelPlayerController>(GetOuterAPlayerController());
	AVaelCharacter* Player = Controller != nullptr ? Controller->GetPawn<AVaelCharacter>() : nullptr;
	UVaelEngineerComponent* Engineer = Player != nullptr ? Player->GetEngineer() : nullptr;
	if (Engineer == nullptr)
	{
		return;
	}
	const auto FillMaterials = [Player]()
	{
		for (const FVaelGear& Gear : VaelEngineerGear::GetAll())
		{
			for (const FVaelGearCost& Cost : Gear.Costs)
			{
				if (const UVaelMaterial* Material = VaelEngineerGear::LoadMaterial(Cost.Material))
				{
					if (Player->GetMaterialBag()->GetCount(Material) < 20)
					{
						Player->GetMaterialBag()->AddMaterial(Material, 20);
					}
				}
				else
				{
					UE_LOG(LogVael, Warning, TEXT("VaelIngenieur: material %s is missing"), *Cost.Material.ToString());
				}
			}
		}
	};
	// Where the Glutpanzer sits against the body that is seen (goal 16.11.): "panzerlage" logs bounds of the helmet, of the face
	// and of the body of the MetaHuman, and where its head bone is
	if (What == TEXT("panzerlage"))
	{
		USkeletalMeshComponent* Shown = Player->GetMetaHumanBody();
		const auto Report = [](const TCHAR* Label, const FBoxSphereBounds& B)
		{
			UE_LOG(LogVael, Display, TEXT("[Panzerlage] %s z %.1f..%.1f  x %.1f..%.1f  y %.1f..%.1f"), Label, B.Origin.Z - B.BoxExtent.Z, B.Origin.Z + B.BoxExtent.Z,
				B.Origin.X - B.BoxExtent.X, B.Origin.X + B.BoxExtent.X, B.Origin.Y - B.BoxExtent.Y, B.Origin.Y + B.BoxExtent.Y);
		};
		if (Shown != nullptr)
		{
			Report(TEXT("MH body"), Shown->Bounds);
			UE_LOG(LogVael, Display, TEXT("[Panzerlage] MH head bone %s, actor %s, MH scale %s"), *Shown->GetSocketLocation(TEXT("head")).ToString(),
				*Player->GetActorLocation().ToString(), *Shown->GetComponentScale().ToString());
			TInlineComponentArray<USkeletalMeshComponent*> Parts(Shown->GetOwner());
			for (USkeletalMeshComponent* Part : Parts)
			{
				Report(*Part->GetName(), Part->Bounds);
			}
		}
		TInlineComponentArray<UStaticMeshComponent*> Pieces(Player);
		for (UStaticMeshComponent* Piece : Pieces)
		{
			if (Piece->GetName().StartsWith(TEXT("EngineerGlutpanzer")) && Piece->IsVisible())
			{
				Report(*FString::Printf(TEXT("%s %s"), *Piece->GetName(), *GetNameSafe(Piece->GetStaticMesh())), Piece->Bounds);
			}
		}
		return;
	}
	// The bones of the body in the reference pose, in actor space, as AttachPiece reckons them (goal 16.11.: fitting the Glutpanzer
	// of Tripo in Blender): "knochen" writes Saved/Ingenieur_Knochen.json
	if (What == TEXT("knochen"))
	{
		Player->SetHero(EVaelHeroClass::Engineer, false);
		// the visible body (the MetaHuman) if there is one, its bones in its own component space; else the base look in actor space
		USkeletalMeshComponent* Shown = Player->GetMetaHumanBody();
		USkeletalMeshComponent* Body = Shown != nullptr ? Shown : Player->GetMesh();
		const FTransform Space = Shown != nullptr ? FTransform::Identity : Body->GetRelativeTransform();
		const FReferenceSkeleton& Skeleton = Body->GetSkeletalMeshAsset()->GetRefSkeleton();
		FString Json = FString::Printf(TEXT("{\"mesh\": \"%s\", \"component\": {\"loc\": [%f, %f, %f], \"yaw\": %f}, \"bones\": {"),
			*Body->GetSkeletalMeshAsset()->GetPathName(), Body->GetRelativeLocation().X, Body->GetRelativeLocation().Y, Body->GetRelativeLocation().Z, Body->GetRelativeRotation().Yaw);
		bool bFirst = true;
		for (int32 BoneIndex = 0; BoneIndex < Skeleton.GetNum(); ++BoneIndex)
		{
			FTransform InComponent = FTransform::Identity;
			for (int32 Index = BoneIndex; Index != INDEX_NONE; Index = Skeleton.GetParentIndex(Index))
			{
				InComponent = InComponent * Skeleton.GetRefBonePose()[Index];
			}
			const FTransform InActor = InComponent * Space;
			const FVector L = InActor.GetLocation();
			const FQuat Q = InActor.GetRotation();
			const int32 Parent = Skeleton.GetParentIndex(BoneIndex);
			Json += FString::Printf(TEXT("%s\"%s\": {\"loc\": [%.3f, %.3f, %.3f], \"quat\": [%.5f, %.5f, %.5f, %.5f], \"parent\": \"%s\"}"), bFirst ? TEXT("") : TEXT(", "),
				*Skeleton.GetBoneName(BoneIndex).ToString(), L.X, L.Y, L.Z, Q.X, Q.Y, Q.Z, Q.W, Parent != INDEX_NONE ? *Skeleton.GetBoneName(Parent).ToString() : TEXT(""));
			bFirst = false;
		}
		Json += TEXT("}}");
		FFileHelper::SaveStringToFile(Json, *(FPaths::ProjectSavedDir() / TEXT("Ingenieur_Knochen.json")));
		UE_LOG(LogVael, Display, TEXT("[Knochen] %d bones of %s written"), Skeleton.GetNum(), *Body->GetSkeletalMeshAsset()->GetName());
		return;
	}
	// "brille": builds the Zielhilfebrille (shown as the Quarzbrille on the face) and takes the helmet off, for pictures
	if (What == TEXT("brille"))
	{
		Player->SetHero(EVaelHeroClass::Engineer, false);
		FillMaterials();
		Engineer->LearnAllPlans();
		Engineer->TakeOff(EVaelArmorPlace::Head);
		const bool bBuilt = Engineer->Build(VaelEngineerGear::Goggles, true) || Engineer->HasBuilt(VaelEngineerGear::Goggles);
		UE_LOG(LogVael, Display, TEXT("[Brille] built %d"), bBuilt);
		// "brille Waermeglas|Rauchglas|Aderglas": that glass set into the frame too (26.11.); logs the fire damage scale
		if (!Id.IsEmpty())
		{
			FillMaterials();
			const bool bGlass = Engineer->Build(FName(*Id), true) || Engineer->HasBuilt(FName(*Id));
			UE_LOG(LogVael, Display, TEXT("[Brille] glass %s built %d, in the frame %s, fire 100 -> %.0f"), *Id, bGlass, *Engineer->GetGlass().ToString(), Engineer->ScaleIncoming(100.0f, nullptr, true));
		}
		return;
	}
	// The face of the MetaHuman (25.11., fitting the Quarzbrille): "gesicht" logs the skeletal parts of the MetaHuman and, in the
	// face, the eye and head bones in its component space (reference pose)
	if (What == TEXT("gesicht"))
	{
		Player->SetHero(EVaelHeroClass::Engineer, false);
		const USkeletalMeshComponent* Body = Player->GetMetaHumanBody();
		AActor* Human = Body != nullptr ? Body->GetOwner() : nullptr;
		if (Human == nullptr)
		{
			UE_LOG(LogVael, Display, TEXT("[Gesicht] no MetaHuman"));
			return;
		}
		TInlineComponentArray<USkeletalMeshComponent*> Parts(Human);
		for (USkeletalMeshComponent* Part : Parts)
		{
			const USkeletalMesh* Mesh = Part->GetSkeletalMeshAsset();
			UE_LOG(LogVael, Display, TEXT("[Gesicht] part %s mesh %s parent %s"), *Part->GetName(), *GetNameSafe(Mesh), *GetNameSafe(Part->GetAttachParent()));
			if (Mesh == nullptr)
			{
				continue;
			}
			const FReferenceSkeleton& Skeleton = Mesh->GetRefSkeleton();
			for (int32 BoneIndex = 0; BoneIndex < Skeleton.GetNum(); ++BoneIndex)
			{
				const FString Name = Skeleton.GetBoneName(BoneIndex).ToString();
				if (Name == TEXT("head") || Name.Contains(TEXT("_Eye")) && !Name.Contains(TEXT("lid")) && !Name.Contains(TEXT("Lid")) || Name.Contains(TEXT("Forehead")) || Name == TEXT("FACIAL_C_Nose"))
				{
					FTransform InComponent = FTransform::Identity;
					for (int32 Index = BoneIndex; Index != INDEX_NONE; Index = Skeleton.GetParentIndex(Index))
					{
						InComponent = InComponent * Skeleton.GetRefBonePose()[Index];
					}
					UE_LOG(LogVael, Display, TEXT("[Gesicht]   %s: loc %s rot %s"), *Name, *InComponent.GetLocation().ToString(), *InComponent.Rotator().ToString());
				}
			}
			UE_LOG(LogVael, Display, TEXT("[Gesicht]   bounds %s / %s, comp rel %s"), *Part->Bounds.Origin.ToString(), *Part->Bounds.BoxExtent.ToString(), *Part->GetRelativeTransform().ToString());
		}
		return;
	}
	// "nid": the engineer with three Nid charges, to see them in the HUD
	if (What == TEXT("nid"))
	{
		Player->SetHero(EVaelHeroClass::Engineer, false);
		Engineer->AddNidCharge(3);
		return;
	}
	// Nid-Verlockungen aus dem Orden (25.11.): "orden" builds the Siegelkapsel, Gebrochenes Siegel, Bannkreis, Saugkreis and
	// Überdruck, holds a doll in the Bannkreis, drains one in the Saugkreis for a Nid charge, spends it on an overcharged
	// Funkenladung, and checks the Sprengmeister's loot; every step logs [OrdenTest]
	if (What == TEXT("orden"))
	{
		Player->SetHero(EVaelHeroClass::Engineer, false);
		FillMaterials();
		Engineer->LearnAllPlans();
		for (const TCHAR* Piece : { TEXT("Siegelkapsel"), TEXT("GebrochenesSiegel"), TEXT("Bannkreis"), TEXT("Saugkreis"), TEXT("Ueberdruck"), TEXT("Funkenladung") })
		{
			Engineer->Build(Piece, true);
			UE_LOG(LogVael, Display, TEXT("[OrdenTest] build %s: %s"), Piece, Engineer->HasBuilt(Piece) || Engineer->GetCarried(Piece) > 0 ? TEXT("PASS") : TEXT("FAIL"));
		}
		UE_LOG(LogVael, Display, TEXT("[OrdenTest] Sprengmeister drops ore, stoppers, wire: %s (%d entries)"),
			GetMutableDefault<UVaelKoboldSprengmeisterData>()->Loot.Num() >= 3 ? TEXT("PASS") : TEXT("FAIL"), GetMutableDefault<UVaelKoboldSprengmeisterData>()->Loot.Num());
		const auto Ground = [this](const FVector& Spot)
		{
			FHitResult Hit;
			return GetWorld()->LineTraceSingleByChannel(Hit, Spot + FVector(0.0f, 0.0f, 500.0f), Spot - FVector(0.0f, 0.0f, 1000.0f), ECC_Visibility) ? Hit.ImpactPoint : Spot;
		};
		if (AVaelSafeZone::IsSafe(GetWorld(), Player->GetActorLocation(), 1800.0f))
		{
			for (int32 Turn = 0; Turn < 8; ++Turn)
			{
				const FVector Spot = Ground(Player->GetActorLocation() + FRotator(0.0f, 45.0f * Turn, 0.0f).Vector() * 3000.0f);
				if (!AVaelSafeZone::IsSafe(GetWorld(), Spot, 1800.0f) && Player->TeleportTo(Spot + FVector(0.0f, 0.0f, Player->GetSimpleCollisionHalfHeight() + 20.0f), Player->GetActorRotation()))
				{
					break;
				}
			}
		}
		const FVector Home = Player->GetActorLocation();
		const FVector Ahead = Player->GetActorForwardVector().GetSafeNormal2D();
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Ahead);
		const auto Doll = [this, Ground](const FVector& Spot)
		{
			return AVaelCreature::SpawnCreature(GetWorld(), GetMutableDefault<UVaelSickleDollData>(), Ground(Spot), FRotator::ZeroRotator);
		};
		const auto Lay = [Engineer](FName Id, const FVector& Spot)
		{
			for (int32 Step = 0; Step < 30 && Engineer->GetDevice() != Id; ++Step)
			{
				Engineer->CycleDevice(1);
			}
			return Engineer->GetDevice() == Id && Engineer->PlaceDevice(Spot);
		};
		const bool bWard = Lay(TEXT("Bannkreis"), Home + Ahead * 400.0f);
		Doll(Home + Ahead * 420.0f);
		const bool bDrain = Lay(TEXT("Saugkreis"), Home - Ahead * 400.0f);
		Doll(Home - Ahead * 420.0f);
		const float Before = UVaelBalanceSubsystem::GetPlayer(Player);
		UE_LOG(LogVael, Display, TEXT("[OrdenTest] laid: ward %d, drain %d"), bWard, bDrain);

		FTimerHandle Check;
		TWeakObjectPtr<AVaelCharacter> WeakPlayer = Player;
		GetWorld()->GetTimerManager().SetTimer(Check, FTimerDelegate::CreateLambda([this, WeakPlayer, Lay, Home, Side, Before]()
		{
			AVaelCharacter* P = WeakPlayer.Get();
			UVaelEngineerComponent* Eng = P != nullptr ? P->GetEngineer() : nullptr;
			if (Eng == nullptr)
			{
				return;
			}
			int32 Held = 0;
			for (TActorIterator<AVaelEngineerDevice> It(GetWorld()); It; ++It)
			{
				Held += It->GetDeviceType() == EVaelDeviceType::Ward ? It->GetWork() : 0;
			}
			UE_LOG(LogVael, Display, TEXT("[OrdenTest] Bannkreis holds a Nid doll: %s (%d)"), Held > 0 ? TEXT("PASS") : TEXT("FAIL"), Held);
			const float After = UVaelBalanceSubsystem::GetPlayer(P);
			UE_LOG(LogVael, Display, TEXT("[OrdenTest] Saugkreis gives Nid charges: %s (%d), pulls to shadow: %s (%.2f -> %.2f)"), Eng->GetNidCharges() > 0 ? TEXT("PASS") : TEXT("FAIL"),
				Eng->GetNidCharges(), After > Before ? TEXT("PASS") : TEXT("FAIL"), Before, After);
			const int32 Charges = Eng->GetNidCharges();
			Lay(TEXT("Ueberdruck"), Home);
			UE_LOG(LogVael, Display, TEXT("[OrdenTest] Überdruck spends a charge: %s (%d -> %d, armed %d)"), Eng->IsOvercharged() && Eng->GetNidCharges() == Charges - 1 ? TEXT("PASS") : TEXT("FAIL"),
				Charges, Eng->GetNidCharges(), Eng->IsOvercharged());
			Lay(TEXT("Funkenladung"), Home + Side * 450.0f);
			float Over = 1.0f;
			for (TActorIterator<AVaelEngineerDevice> It(GetWorld()); It; ++It)
			{
				Over = It->GetDeviceType() == EVaelDeviceType::OreCharge ? It->GetOvercharge() : Over;
			}
			UE_LOG(LogVael, Display, TEXT("[OrdenTest] the next charge is overcharged: %s (x%.1f), valve free again: %s"), Over > 1.5f ? TEXT("PASS") : TEXT("FAIL"), Over,
				!Eng->IsOvercharged() ? TEXT("PASS") : TEXT("FAIL"));
		}), 5.0f, false);
		return;
	}
	// Verlangen of the shadow sets (Todsünden 25.11.): "verlangen" wakes the Hunger with much Nid in him, lets it grow in a fight,
	// starves it until it takes him over and lashes out, then feeds it with a kill; every step logs [VerlangenTest]
	if (What == TEXT("verlangen"))
	{
		Player->SetHero(EVaelHeroClass::Engineer, false);
		UVaelCravingComponent* Craving = Player->GetCraving();
		if (Craving == nullptr)
		{
			return;
		}
		const auto Ground = [this](const FVector& Spot)
		{
			FHitResult Hit;
			return GetWorld()->LineTraceSingleByChannel(Hit, Spot + FVector(0.0f, 0.0f, 500.0f), Spot - FVector(0.0f, 0.0f, 1000.0f), ECC_Visibility) ? Hit.ImpactPoint : Spot;
		};
		if (AVaelSafeZone::IsSafe(GetWorld(), Player->GetActorLocation(), 1800.0f))
		{
			for (int32 Turn = 0; Turn < 8; ++Turn)
			{
				const FVector Spot = Ground(Player->GetActorLocation() + FRotator(0.0f, 45.0f * Turn, 0.0f).Vector() * 3000.0f);
				if (!AVaelSafeZone::IsSafe(GetWorld(), Spot, 1800.0f) && Player->TeleportTo(Spot + FVector(0.0f, 0.0f, Player->GetSimpleCollisionHalfHeight() + 20.0f), Player->GetActorRotation()))
				{
					break;
				}
			}
		}
		UVaelBalanceSubsystem::ChangePlayer(Player, 90.0f - UVaelBalanceSubsystem::GetPlayer(Player), FText::FromString(TEXT("Test")));
		Craving->SetCraving(VaelCravings::Hunger());
		const FVector Home = Player->GetActorLocation();
		AVaelCreature* Near = AVaelCreature::SpawnCreature(GetWorld(), GetMutableDefault<UVaelSickleDollData>(), Ground(Home + Player->GetActorForwardVector() * 220.0f), FRotator::ZeroRotator);
		AVaelCreature::SpawnCreature(GetWorld(), GetMutableDefault<UVaelSickleDollData>(), Ground(Home - Player->GetActorForwardVector() * 900.0f), FRotator::ZeroRotator);
		TWeakObjectPtr<AVaelCharacter> WeakPlayer = Player;
		TWeakObjectPtr<AVaelCreature> WeakNear = Near;
		FTimerHandle Grow;
		GetWorld()->GetTimerManager().SetTimer(Grow, FTimerDelegate::CreateLambda([this, WeakPlayer, WeakNear]()
		{
			AVaelCharacter* P = WeakPlayer.Get();
			UVaelCravingComponent* C = P != nullptr ? P->GetCraving() : nullptr;
			if (C == nullptr)
			{
				return;
			}
			UE_LOG(LogVael, Display, TEXT("[VerlangenTest] grows in the fight: %s (meter %.2f)"), C->GetMeter() > 0.05f ? TEXT("PASS") : TEXT("FAIL"), C->GetMeter());
			UE_LOG(LogVael, Display, TEXT("[VerlangenTest] fed it hits harder: %s (x%.2f)"), C->GetDamageScale() > 1.0f ? TEXT("PASS") : TEXT("FAIL"), C->GetDamageScale());
			const float Before = UVaelBalanceSubsystem::GetPlayer(P);
			C->SetMeter(1.0f);
			FTimerHandle Starve;
			GetWorld()->GetTimerManager().SetTimer(Starve, FTimerDelegate::CreateLambda([this, WeakPlayer, WeakNear, Before]()
			{
				AVaelCharacter* P2 = WeakPlayer.Get();
				UVaelCravingComponent* C2 = P2 != nullptr ? P2->GetCraving() : nullptr;
				if (C2 == nullptr)
				{
					return;
				}
				const float After = UVaelBalanceSubsystem::GetPlayer(P2);
				UE_LOG(LogVael, Display, TEXT("[VerlangenTest] starving raises corruption: %s (%.2f -> %.2f), life %.0f"), After > Before ? TEXT("PASS") : TEXT("FAIL"), Before, After, P2->GetHealth());
				UE_LOG(LogVael, Display, TEXT("[VerlangenTest] takes him over: %s, won't come off: %s, lashes %d: %s"), C2->IsPossessed() ? TEXT("PASS") : TEXT("FAIL"),
					!C2->CanTakeOff() ? TEXT("PASS") : TEXT("FAIL"), C2->GetLashes(), C2->GetLashes() > 0 ? TEXT("PASS") : TEXT("FAIL"));
				// a kill close by feeds it and it lets go
				if (AVaelCreature* Victim = WeakNear.Get())
				{
					FVaelSpellHit Kill;
					Kill.Damage = 99999.0f;
					UVaelCombatStatics::ApplySpellHit(P2, Victim, Kill, FVector::ForwardVector);
				}
				FTimerHandle Fed;
				GetWorld()->GetTimerManager().SetTimer(Fed, FTimerDelegate::CreateLambda([WeakPlayer]()
				{
					AVaelCharacter* P3 = WeakPlayer.Get();
					UVaelCravingComponent* C3 = P3 != nullptr ? P3->GetCraving() : nullptr;
					if (C3 != nullptr)
					{
						UE_LOG(LogVael, Display, TEXT("[VerlangenTest] a kill feeds it: %s (feedings %d, meter %.2f), lets go: %s"), C3->GetFeedings() > 0 ? TEXT("PASS") : TEXT("FAIL"),
							C3->GetFeedings(), C3->GetMeter(), !C3->IsPossessed() ? TEXT("PASS") : TEXT("FAIL"));
						C3->ClearCraving();
					}
				}), 1.0f, false);
			}), 3.0f, false);
		}), 3.0f, false);
		return;
	}
	// Rammschild, Fernglas and the Sprengmeister craft (25.11.): "sprengmeister" lays the Rammschild with a roller rolling at it,
	// a Stolperdraht a doll walks across, the three Kristallladungen each by a doll, and raises the Fernglas on a doll at its
	// far focus; every step logs [SprengTest]
	if (What == TEXT("rauchschritt") || What == TEXT("glaeser"))
	{
		// 26.11.: "rauchschritt" = the whole Aschenjaeger dodges: smoke, a doll beside him staggers, the first hit is critical, the
		// Rauchglas makes hits on staggering enemies critical. "glaeser" = Waermeglas beside a burrowed Glutkriecher, Aderglas
		// beside a hidden stretch of a Nid vein (if the level has one); logs PASS/FAIL
		Player->SetHero(EVaelHeroClass::Engineer, false);
		FillMaterials();
		Engineer->LearnAllPlans();
		Engineer->Build(VaelEngineerGear::Goggles, true);
		Player->GetCharacterMovement()->StopMovementImmediately();
		Player->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		const FVector Home = Player->GetActorLocation();
		const FVector Ahead = Player->GetActorForwardVector().GetSafeNormal2D();
		if (What == TEXT("rauchschritt"))
		{
			for (int32 Place = 0; Place < 4; ++Place)
			{
				FillMaterials();
				const FName Piece(*FString::Printf(TEXT("Aschenjaeger_%d"), Place));
				Engineer->Build(Piece, true);
				Engineer->Wear(Piece);
			}
			AVaelCreature* Doll = AVaelCreature::SpawnCreature(GetWorld(), GetMutableDefault<UVaelSickleDollData>(), Home + Ahead * 200.0f - FVector(0.0f, 0.0f, 60.0f), FRotator::ZeroRotator);
			if (Doll == nullptr)
			{
				UE_LOG(LogVael, Display, TEXT("[RauchTest] doll: FAIL"));
				return;
			}
			Doll->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
			Engineer->OnDodged();
			UE_LOG(LogVael, Display, TEXT("[RauchTest] Rauchschritt %d, doll staggers: %s, next hit critical: %s"), Engineer->GetSmokeSteps(),
				UVaelCombatStatics::HasStatus(Doll, EVaelStatus::Confused) ? TEXT("PASS") : TEXT("FAIL"), Engineer->HasSmokeStrike() ? TEXT("PASS") : TEXT("FAIL"));
			Engineer->ApplyHit(Doll, Doll->GetActorLocation(), 10.0f, VaelEngineerGear::Standard, Home);
			UE_LOG(LogVael, Display, TEXT("[RauchTest] first hit out of the smoke critical: %s, used up: %s"), Engineer->WasLastCrit() ? TEXT("PASS") : TEXT("FAIL"),
				!Engineer->HasSmokeStrike() ? TEXT("PASS") : TEXT("FAIL"));
			FillMaterials();
			Engineer->Build(VaelEngineerGear::GlassSmoke, true);
			int32 Crits = 0;
			for (int32 Hit = 0; Hit < 5; ++Hit)
			{
				UVaelCombatStatics::ApplyStatus(Player, Doll, EVaelStatus::Confused, 2.0f);
				Engineer->ApplyHit(Doll, Doll->GetActorLocation(), 1.0f, VaelEngineerGear::Standard, Home);
				Crits += Engineer->WasLastCrit() ? 1 : 0;
			}
			UE_LOG(LogVael, Display, TEXT("[RauchTest] Rauchglas in the frame: %s, staggering doll hit critically %d/5: %s"), *Engineer->GetGlass().ToString(), Crits, Crits == 5 ? TEXT("PASS") : TEXT("FAIL"));
			return;
		}
		FillMaterials();
		Engineer->Build(VaelEngineerGear::GlassHeat, true);
		AVaelCreature* Crawler = AVaelCreature::SpawnCreature(GetWorld(), GetMutableDefault<UVaelEmberCrawlerData>(), Home + Ahead * 500.0f - FVector(0.0f, 0.0f, 60.0f), FRotator::ZeroRotator);
		const AVaelEmberCrawler* Ember = Cast<AVaelEmberCrawler>(Crawler);
		UE_LOG(LogVael, Display, TEXT("[GlasTest] Waermeglas in the frame: %s, crawler burrowed: %d"), *Engineer->GetGlass().ToString(),
			Ember != nullptr && Ember->GetCrawlerState() == EVaelCrawlerState::Burrowed);
		TWeakObjectPtr<UVaelEngineerComponent> WeakEng(Engineer);
		TWeakObjectPtr<AVaelCharacter> WeakPlayer(Player);
		FTimerHandle Look;
		GetWorld()->GetTimerManager().SetTimer(Look, FTimerDelegate::CreateWeakLambda(this, [this, WeakEng, WeakPlayer, FillMaterials]()
		{
			UVaelEngineerComponent* Eng = WeakEng.Get();
			AVaelCharacter* Hero = WeakPlayer.Get();
			if (Eng == nullptr || Hero == nullptr)
			{
				return;
			}
			UE_LOG(LogVael, Display, TEXT("[GlasTest] Waermeglas shows the burrowed crawler (%d glows): %s"), Eng->GetGlassGlows(), Eng->GetGlassGlows() > 0 ? TEXT("PASS") : TEXT("FAIL"));
			// the Aderglas beside the nearest hidden stretch of a Nid vein
			const FVaelNidVeinPoint* Hidden = nullptr;
			for (TActorIterator<AVaelNidVeins> It(GetWorld()); It && Hidden == nullptr; ++It)
			{
				Hidden = It->Points.FindByPredicate([](const FVaelNidVeinPoint& Point) { return !Point.bOpen; });
			}
			if (Hidden == nullptr)
			{
				// this level has none: a short vein beside him, under the ground for its first ten metres
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				if (AVaelNidVeins* Veins = GetWorld()->SpawnActor<AVaelNidVeins>(AVaelNidVeins::StaticClass(), Hero->GetActorLocation(), FRotator::ZeroRotator, Params))
				{
					for (int32 Step = 0; Step < 10; ++Step)
					{
						FVaelNidVeinPoint Point;
						Point.Location = Hero->GetActorLocation() + FVector(300.0f + Step * 100.0f, 200.0f, -90.0f);
						Point.Along = Step * 100.0f;
						Point.Width = 40.0f;
						Point.bOpen = false;
						Veins->Points.Add(Point);
					}
					Hidden = &Veins->Points[0];
					UE_LOG(LogVael, Display, TEXT("[GlasTest] no Nid vein in this level: a test vein laid"));
				}
			}
			if (Hidden == nullptr)
			{
				UE_LOG(LogVael, Display, TEXT("[GlasTest] Aderglas not checked: FAIL"));
				return;
			}
			FillMaterials();
			Eng->Build(VaelEngineerGear::GlassVein, true);
			Hero->TeleportTo(Hidden->Location + FVector(0.0f, 0.0f, 150.0f), Hero->GetActorRotation());
			FTimerHandle Again;
			GetWorld()->GetTimerManager().SetTimer(Again, FTimerDelegate::CreateWeakLambda(this, [WeakEng]()
			{
				if (const UVaelEngineerComponent* Later = WeakEng.Get())
				{
					UE_LOG(LogVael, Display, TEXT("[GlasTest] Aderglas (%s) shows the hidden vein (%d glows): %s"), *Later->GetGlass().ToString(), Later->GetGlassGlows(),
						Later->GetGlassGlows() > 0 ? TEXT("PASS") : TEXT("FAIL"));
				}
			}), 1.5f, false);
		}), 1.5f, false);
		return;
	}
	if (What == TEXT("stich"))
	{
		// 26.11.: one brood chamber full: the Glutkrone's sting strikes into the ground (for pictures)
		if (Engineer->GetSetStage(TEXT("Schwarzglut")) < 2)
		{
			Player->SetHero(EVaelHeroClass::Engineer, false);
			Engineer->LearnAllPlans();
			for (const TCHAR* Format : { TEXT("Schwarzglut_%d"), TEXT("Schwarzglut2_%d") })
			{
				for (int32 Place = 0; Place < 4; ++Place)
				{
					FillMaterials();
					Engineer->Build(FName(FString(Format).Replace(TEXT("%d"), *FString::FromInt(Place))), true);
				}
			}
			Engineer->Wear(TEXT("Schwarzglut2_1"));
			return;
		}
		Engineer->AddBroodHeat(60.0f);
		for (TActorIterator<AVaelGlutEgg> It(GetWorld()); It; ++It)
		{
			UE_LOG(LogVael, Display, TEXT("[Stich] egg at %s (player %s), under %d, hidden %d"), *It->GetActorLocation().ToCompactString(), *Player->GetActorLocation().ToCompactString(), It->IsUnderground(), It->IsHidden());
		}
		UE_LOG(LogVael, Display, TEXT("[Stich] sting %d, strikes so far %d, set %d stage %d, worn %s %s %s %s, engineer %d"), Engineer->HasSting(), Engineer->GetStingStrikes(),
			Engineer->CountSet(TEXT("Schwarzglut")), Engineer->GetSetStage(TEXT("Schwarzglut")), *Engineer->GetWorn(EVaelArmorPlace::Head).ToString(), *Engineer->GetWorn(EVaelArmorPlace::Chest).ToString(),
			*Engineer->GetWorn(EVaelArmorPlace::Arms).ToString(), *Engineer->GetWorn(EVaelArmorPlace::Legs).ToString(), Player->IsEngineer());
		return;
	}
	if (What == TEXT("wechsel"))
	{
		// 26.11.: everything lies in the toolbox as if unlocked; each set (and stage) is put on in turn from there. [Wechsel]
		Player->SetHero(EVaelHeroClass::Engineer, false);
		Engineer->ResetToStart();
		Engineer->PutTestArmorInToolbox();
		bool bAll = true;
		for (const TCHAR* Piece : { TEXT("Schwarzglut_1"), TEXT("Aschenjaeger_1"), TEXT("Schwarzglut2_1"), TEXT("Aschenjaeger2_1"), TEXT("Schwarzglut_2") })
		{
			const FVaelGear* Gear = VaelEngineerGear::Find(Piece);
			const bool bKnown = Gear != nullptr && Engineer->KnowsRecipe(*Gear) && Engineer->HasBuilt(Piece);
			const bool bOn = Engineer->Wear(Piece);
			const int32 Stage = Gear != nullptr ? Engineer->GetSetStage(Gear->Set) : 0;
			const bool bOk = bKnown && bOn && Gear != nullptr && Engineer->CountSet(Gear->Set) == 4 && Stage == Gear->SetStage;
			bAll &= bOk;
			UE_LOG(LogVael, Display, TEXT("[Wechsel] %s: known %d, put on %d, set %d of 4, stage %d: %s"), Piece, bKnown, bOn, Gear != nullptr ? Engineer->CountSet(Gear->Set) : 0, Stage,
				bOk ? TEXT("PASS") : TEXT("FAIL"));
		}
		UE_LOG(LogVael, Display, TEXT("[Wechsel] pieces in the toolbox %d: %s"), AVaelPlayerController::GetToolboxList(Engineer).Num(), bAll ? TEXT("PASS") : TEXT("FAIL"));
		return;
	}
	if (What == TEXT("glutkrone") || What == TEXT("drehtanz"))
	{
		// Stage II (26.11.): "glutkrone" = Brutbuechse (one pellet, then the clutch), a Glutei that seeks the doll, the Glutbad;
		// "drehtanz" = Aschenjaeger II, dodge held: turns, tornado, a Klingenteufel, the Aschenteufel on letting go. [Stufe2]
		const bool bGlut = What == TEXT("glutkrone");
		const TCHAR* Set = bGlut ? TEXT("Schwarzglut") : TEXT("Aschenjaeger");
		Player->SetHero(EVaelHeroClass::Engineer, false);
		Engineer->LearnAllPlans();
		for (int32 Stage = 1; Stage <= 2; ++Stage)
		{
			for (int32 Place = 0; Place < 4; ++Place)
			{
				FillMaterials();
				Engineer->Build(FName(Stage == 1 ? *FString::Printf(TEXT("%s_%d"), Set, Place) : *FString::Printf(TEXT("%s2_%d"), Set, Place)), true);
			}
		}
		Engineer->Wear(FName(*FString::Printf(TEXT("%s2_1"), Set)));
		UE_LOG(LogVael, Display, TEXT("[Stufe2] %s stage %d worn: %s"), Set, Engineer->GetSetStage(Set), Engineer->GetSetStage(Set) == 2 ? TEXT("PASS") : TEXT("FAIL"));
		Player->GetCharacterMovement()->StopMovementImmediately();
		Player->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		const FVector Home = Player->GetActorLocation();
		const FVector Ahead = Player->GetActorForwardVector().GetSafeNormal2D();
		AVaelCreature* Doll = AVaelCreature::SpawnCreature(GetWorld(), GetMutableDefault<UVaelSickleDollData>(), Home + Ahead * (bGlut ? 500.0f : 200.0f) - FVector(0.0f, 0.0f, 60.0f), FRotator::ZeroRotator);
		if (Doll == nullptr)
		{
			UE_LOG(LogVael, Display, TEXT("[Stufe2] doll: FAIL"));
			return;
		}
		Doll->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		const float Start = Doll->GetHealth();
		TWeakObjectPtr<UVaelEngineerComponent> WeakEng(Engineer);
		TWeakObjectPtr<AVaelCreature> WeakDoll(Doll);
		const auto Later = [this](float Seconds, TFunction<void()> Work)
		{
			FTimerHandle Handle;
			GetWorld()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [Work]() { Work(); }), Seconds, false);
		};
		if (bGlut)
		{
			const bool bGun = Engineer->UsesBroodGun();
			Engineer->Fire(Doll->GetActorLocation());
			UE_LOG(LogVael, Display, TEXT("[Stufe2] Brutbuechse in use %d, one pellet fired (left %d): %s"), bGun, Engineer->GetBroodPellets(), bGun && Engineer->GetBroodPellets() == 5 ? TEXT("PASS") : TEXT("FAIL"));
			Later(1.0f, [WeakEng, WeakDoll, Start]()
			{
				UVaelEngineerComponent* Eng = WeakEng.Get();
				if (Eng == nullptr || !WeakDoll.IsValid())
				{
					return;
				}
				UE_LOG(LogVael, Display, TEXT("[Stufe2] pellet hit the doll (%.0f): %s"), Start - WeakDoll->GetHealth(), WeakDoll->GetHealth() < Start ? TEXT("PASS") : TEXT("FAIL"));
				Eng->StartDraw();
			});
			Later(1.6f, [WeakEng, WeakDoll]()
			{
				if (UVaelEngineerComponent* Eng = WeakEng.Get())
				{
					const int32 Before = Eng->GetBroodPellets();
					Eng->ReleaseDraw(WeakDoll.IsValid() ? WeakDoll->GetActorLocation() : Eng->GetOwner()->GetActorLocation());
					UE_LOG(LogVael, Display, TEXT("[Stufe2] held: the clutch of %d pellets, sack empty: %s"), Before, Eng->GetBroodPellets() == 0 && Before > 1 ? TEXT("PASS") : TEXT("FAIL"));
					// a fresh doll for the egg (the clutch may have finished the first)
					const FVector Ahead2 = Eng->GetOwner()->GetActorForwardVector().GetSafeNormal2D();
					if (AVaelCreature* Second = AVaelCreature::SpawnCreature(Eng->GetWorld(), GetMutableDefault<UVaelSickleDollData>(), Eng->GetOwner()->GetActorLocation() + Ahead2 * 450.0f - FVector(0.0f, 0.0f, 60.0f), FRotator::ZeroRotator))
					{
						Second->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
					}
					Eng->AddBroodHeat(60.0f);
					UE_LOG(LogVael, Display, TEXT("[Stufe2] 60 heat in the chambers: sting there %d, egg laid or strike coming: %s"), Eng->HasSting(), Eng->GetEggsLaid() == 1 || Eng->HasSting() ? TEXT("PASS") : TEXT("FAIL"));
				}
			});
			Later(2.0f, [WeakDoll, Start]() { if (WeakDoll.IsValid()) { UE_LOG(LogVael, Display, TEXT("[Stufe2] doll before the egg: %.0f lost"), Start - WeakDoll->GetHealth()); } });
			Later(3.0f, [this, WeakDoll]()
			{
				for (TActorIterator<AVaelGlutEgg> It(GetWorld()); It; ++It)
				{
					UE_LOG(LogVael, Display, TEXT("[Stufe2] egg at %s, prey %s, would seek %s, doll at %s"), *It->GetActorLocation().ToCompactString(), It->GetPrey() ? *It->GetPrey()->GetName() : TEXT("-"),
						It->FindPrey() ? *It->FindPrey()->GetName() : TEXT("-"), WeakDoll.IsValid() ? *WeakDoll->GetActorLocation().ToCompactString() : TEXT("-"));
				}
			});
			Later(4.5f, [this, WeakEng, WeakDoll]()
			{
				UVaelEngineerComponent* Eng = WeakEng.Get();
				if (Eng == nullptr)
				{
					return;
				}
				UE_LOG(LogVael, Display, TEXT("[Stufe2] sting strikes %d, eggs laid %d: %s"), Eng->GetStingStrikes(), Eng->GetEggsLaid(), Eng->GetEggsLaid() >= 1 ? TEXT("PASS") : TEXT("FAIL"));
				UE_LOG(LogVael, Display, TEXT("[Stufe2] the Glutei crawled to the doll and burst (eggs left %d, doll burning %d): %s"), Eng->GetEggsAlive(),
					WeakDoll.IsValid() && UVaelCombatStatics::HasStatus(WeakDoll.Get(), EVaelStatus::Burning),
					(WeakDoll.IsValid() && UVaelCombatStatics::HasStatus(WeakDoll.Get(), EVaelStatus::Burning)) || Eng->GetEggsAlive() == 0 ? TEXT("PASS") : TEXT("FAIL"));
				AVaelGroundArea::SpawnArea(GetWorld(), Eng->GetOwner()->GetActorLocation() - FVector(0.0f, 0.0f, 90.0f), EVaelElement::Fire, 200.0f, 5.0f, 0.0f, nullptr);
				const float Taken = Eng->ScaleIncoming(100.0f, nullptr, true);
				UE_LOG(LogVael, Display, TEXT("[Stufe2] Glutbad: fire damage in a fire %.0f: %s"), Taken, Taken <= 0.0f ? TEXT("PASS") : TEXT("FAIL"));
			});
			return;
		}
		// the Drehtanz: dodge pressed and held
		Engineer->OnDodged();
		Engineer->SetRollHeld(true);
		Later(1.0f, [WeakEng]()
		{
			if (const UVaelEngineerComponent* Eng = WeakEng.Get())
			{
				UE_LOG(LogVael, Display, TEXT("[Stufe2] dodge held: Drehtanz %d, turns %d, speed %.2f: %s"), Eng->IsWhirling(), Eng->GetWhirlStacks(), Eng->GetMoveSpeedScale(),
					Eng->IsWhirling() ? TEXT("PASS") : TEXT("FAIL"));
			}
		});
		Later(2.0f, [WeakEng, WeakDoll]()
		{
			if (UVaelEngineerComponent* Eng = WeakEng.Get())
			{
				// away from the doll, so the devil does not carry it off before the blades reach it
				Eng->Fire(Eng->GetOwner()->GetActorLocation() - Eng->GetOwner()->GetActorForwardVector() * 600.0f);
				UE_LOG(LogVael, Display, TEXT("[Stufe2] attack in the dance lets go a Klingenteufel: %s"), Eng->GetDevilsReleased() == 1 ? TEXT("PASS") : TEXT("FAIL"));
			}
		});
		// the doll wanders off on its own: set it back beside him once the tornado stands
		Later(4.5f, [WeakEng, WeakDoll]()
		{
			if (WeakEng.IsValid() && WeakDoll.IsValid())
			{
				const AActor* Owner = WeakEng->GetOwner();
				WeakDoll->SetActorLocation(Owner->GetActorLocation() + Owner->GetActorForwardVector() * 180.0f, false, nullptr, ETeleportType::TeleportPhysics);
			}
		});
		Later(6.0f, [WeakEng, WeakDoll, Start]()
		{
			if (UVaelEngineerComponent* Eng = WeakEng.Get())
			{
				if (WeakDoll.IsValid())
				{
					UE_LOG(LogVael, Display, TEXT("[Stufe2] doll %.0f cm away (z %.0f)"), FVector::Dist2D(WeakDoll->GetActorLocation(), Eng->GetOwner()->GetActorLocation()), WeakDoll->GetActorLocation().Z - Eng->GetOwner()->GetActorLocation().Z);
				}
				UE_LOG(LogVael, Display, TEXT("[Stufe2] after 5 s: turns %d, tornado %d, cuts %d, doll lost %.0f: %s"), Eng->GetWhirlStacks(), Eng->HasTornado(), Eng->GetWhirlCuts(),
					WeakDoll.IsValid() ? Start - WeakDoll->GetHealth() : Start, Eng->HasTornado() && Eng->GetWhirlCuts() > 0 ? TEXT("PASS") : TEXT("FAIL"));
				Eng->SetRollHeld(false);
			}
		});
		Later(6.5f, [WeakEng]()
		{
			if (const UVaelEngineerComponent* Eng = WeakEng.Get())
			{
				UE_LOG(LogVael, Display, TEXT("[Stufe2] let go after the tornado: Aschenteufel (%d devils), dance over: %s"), Eng->GetDevilsReleased(), Eng->GetDevilsReleased() == 2 && !Eng->IsWhirling() ? TEXT("PASS") : TEXT("FAIL"));
			}
		});
		return;
	}
	if (What == TEXT("sichelumwelt"))
	{
		// The Doppelsichel built out (26.11.): five throws one after the other, each logged [SichelUmwelt] PASS/FAIL:
		// a Giftladung brushed on, through a fire, through smoke, through a Säurefass pool, over a Knallmine
		Player->SetHero(EVaelHeroClass::Engineer, false);
		Engineer->LearnAllPlans();
		for (int32 Place = 0; Place < 4; ++Place)
		{
			FillMaterials();
			const FName Piece(*FString::Printf(TEXT("Aschenjaeger_%d"), Place));
			Engineer->Build(Piece, true);
			Engineer->Wear(Piece);
		}
		FillMaterials();
		{
			FText Hint;
			FName Formula;
			Engineer->Mix({ TEXT("Schlacke"), TEXT("Glutstein") }, Hint, Formula);
		}
		Player->GetCharacterMovement()->StopMovementImmediately();
		Player->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		const FVector Home = Player->GetActorLocation();
		const FVector Ahead = Player->GetActorForwardVector().GetSafeNormal2D();
		const FVector Ground = Home - FVector(0.0f, 0.0f, Player->GetSimpleCollisionHalfHeight());
		const FVector Aim = Home + Ahead * 900.0f;
		const FVector Middle = Ground + Ahead * 450.0f;
		TWeakObjectPtr<UVaelEngineerComponent> WeakEng(Engineer);
		TWeakObjectPtr<AVaelCharacter> WeakPlayer(Player);
		TSharedRef<int32> Stage = MakeShared<int32>(0);
		TSharedRef<FName> Seen = MakeShared<FName>();
		TSharedRef<int32> Struck = MakeShared<int32>(0);
		TSharedRef<int32> GiftBefore = MakeShared<int32>(Engineer->GetCarried(TEXT("Brand")));
		TSharedRef<TWeakObjectPtr<AVaelEngineerDevice>> Mine = MakeShared<TWeakObjectPtr<AVaelEngineerDevice>>();
		TSharedRef<FTimerHandle> Watch = MakeShared<FTimerHandle>();
		TSharedRef<float> Waited = MakeShared<float>(0.0f);
		const auto Setup = [this, WeakEng, WeakPlayer, Middle, Mine](int32 Step)
		{
			UVaelEngineerComponent* Eng = WeakEng.Get();
			switch (Step)
			{
			case 0: Eng->PickAmmo(TEXT("Brand")); break;
			case 1:
				Eng->PickAmmo(VaelEngineerGear::Standard);
				AVaelGroundArea::SpawnArea(GetWorld(), Middle, EVaelElement::Fire, 150.0f, 3.0f, 0.0f, nullptr);
				break;
			case 2:
				for (TActorIterator<AVaelGroundArea> It(GetWorld()); It; ++It)
				{
					It->Destroy();
				}
				Eng->AddSmokeSpot(Middle, 200.0f, 4.0f);
				break;
			case 3:
				if (const FVaelGear* Barrel = VaelEngineerGear::Find(TEXT("Saeurefass")))
				{
					if (AVaelEngineerDevice* Placed = AVaelEngineerDevice::Lay(Eng, *Barrel, Middle + FVector(0.0f, 0.0f, 5.0f)))
					{
						Placed->BurstAcid();
					}
				}
				break;
			case 4:
				if (const FVaelGear* Knall = VaelEngineerGear::Find(TEXT("Knallmine")))
				{
					// on the ground under the sickle's path, raised to its height so the blades brush it
					*Mine = AVaelEngineerDevice::Lay(Eng, *Knall, WeakPlayer->GetActorLocation() + WeakPlayer->GetActorForwardVector().GetSafeNormal2D() * 300.0f);
				}
				break;
			default: break;
			}
		};
		Setup(0);
		// he reaches into the pouch for the charge first (0.35 s)
		FTimerHandle FirstThrow;
		GetWorld()->GetTimerManager().SetTimer(FirstThrow, FTimerDelegate::CreateWeakLambda(this, [WeakEng, Aim]()
		{
			if (UVaelEngineerComponent* Eng = WeakEng.Get())
			{
				UE_LOG(LogVael, Display, TEXT("[SichelUmwelt] in hand: %s"), *Eng->GetAmmo().ToString());
				Eng->Fire(Aim);
			}
		}), 0.6f, false);
		GetWorld()->GetTimerManager().SetTimer(*Watch, FTimerDelegate::CreateWeakLambda(this, [this, WeakEng, Stage, Seen, Struck, GiftBefore, Mine, Watch, Waited, Aim, Setup]()
		{
			UVaelEngineerComponent* Eng = WeakEng.Get();
			*Waited += 0.02f;
			if (Eng == nullptr || *Waited > 10.0f)
			{
				UE_LOG(LogVael, Display, TEXT("[SichelUmwelt] stage %d timed out: FAIL"), *Stage);
				GetWorld()->GetTimerManager().ClearTimer(*Watch);
				return;
			}
			if (const AVaelDoubleSickle* Flying = Eng->GetFlyingSickle())
			{
				*Seen = Flying->GetCoat().IsNone() ? *Seen : Flying->GetCoat();
				*Struck = FMath::Max(*Struck, Flying->GetDevicesStruck());
				return;
			}
			if (*Waited < (*Stage == 0 ? 0.8f : 0.2f))
			{
				return;
			}
			static const TCHAR* const Names[] = { TEXT("Glutladung brushed on"), TEXT("through fire"), TEXT("through smoke"), TEXT("through acid"), TEXT("over a Knallmine") };
			static const TCHAR* const Wanted[] = { TEXT("Brand"), TEXT("Brand"), TEXT("Rauch"), TEXT("Saeure"), TEXT("") };
			bool bPass = *Seen == FName(Wanted[*Stage]) || (*Stage == 4 && Seen->IsNone());
			if (*Stage == 0)
			{
				bPass = bPass && Eng->GetCarried(TEXT("Brand")) == *GiftBefore - 1;
			}
			if (*Stage == 4)
			{
				bPass = *Struck > 0 && (!Mine->IsValid() || (*Mine)->HasDetonated());
			}
			UE_LOG(LogVael, Display, TEXT("[SichelUmwelt] %s: blades carried %s, devices struck %d: %s"), Names[*Stage], *Seen->ToString(), *Struck, bPass ? TEXT("PASS") : TEXT("FAIL"));
			if (++*Stage > 4)
			{
				GetWorld()->GetTimerManager().ClearTimer(*Watch);
				return;
			}
			*Seen = NAME_None;
			*Struck = 0;
			*Waited = 0.0f;
			Setup(*Stage);
			Eng->Fire(Aim);
		}), 0.02f, true);
		return;
	}
	if (What == TEXT("sichel"))
	{
		// The Doppelsichel of the Aschenjaeger (26.11.): the whole set worn, a straight throw at a doll ahead, caught in time as it
		// comes back, then the faster held throw round a second doll; logs PASS/FAIL per step
		Player->SetHero(EVaelHeroClass::Engineer, false);
		Engineer->LearnAllPlans();
		for (int32 Place = 0; Place < 4; ++Place)
		{
			FillMaterials();
			const FName Piece(*FString::Printf(TEXT("Aschenjaeger_%d"), Place));
			Engineer->Build(Piece, true);
			Engineer->Wear(Piece);
		}
		UE_LOG(LogVael, Display, TEXT("[SichelTest] set worn, sickle in use: %s"), Engineer->UsesDoubleSickle() ? TEXT("PASS") : TEXT("FAIL"));
		const auto Ground = [this](const FVector& Spot)
		{
			FHitResult Hit;
			return GetWorld()->LineTraceSingleByChannel(Hit, Spot + FVector(0.0f, 0.0f, 500.0f), Spot - FVector(0.0f, 0.0f, 1000.0f), ECC_Visibility) ? Hit.ImpactPoint : Spot;
		};
		if (AVaelSafeZone::IsSafe(GetWorld(), Player->GetActorLocation(), 1800.0f))
		{
			for (int32 Turn = 0; Turn < 8; ++Turn)
			{
				const FVector Spot = Ground(Player->GetActorLocation() + FRotator(0.0f, 45.0f * Turn, 0.0f).Vector() * 3000.0f);
				if (!AVaelSafeZone::IsSafe(GetWorld(), Spot, 1800.0f) && Player->TeleportTo(Spot + FVector(0.0f, 0.0f, Player->GetSimpleCollisionHalfHeight() + 20.0f), Player->GetActorRotation()))
				{
					break;
				}
			}
		}
		// headless the ground may not hold him after the teleport: he stays where he is for the test
		Player->GetCharacterMovement()->StopMovementImmediately();
		Player->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		const FVector Home = Player->GetActorLocation();
		const FVector Ahead = Player->GetActorForwardVector().GetSafeNormal2D();
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Ahead);
		AVaelCreature* First = AVaelCreature::SpawnCreature(GetWorld(), GetMutableDefault<UVaelSickleDollData>(), Ground(Home + Ahead * 650.0f), FRotator::ZeroRotator);
		AVaelCreature* Second = AVaelCreature::SpawnCreature(GetWorld(), GetMutableDefault<UVaelSickleDollData>(), Ground(Home + Ahead * 700.0f + Side * 500.0f), FRotator::ZeroRotator);
		if (First == nullptr || Second == nullptr)
		{
			UE_LOG(LogVael, Display, TEXT("[SichelTest] dolls: FAIL"));
			return;
		}
		for (AVaelCreature* Doll : { First, Second })
		{
			// headless the ground does not hold them either
			Doll->GetCharacterMovement()->StopMovementImmediately();
			Doll->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		}
		const float FirstHealth = First->GetHealth();
		const float SecondHealth = Second->GetHealth();
		const bool bThrown = Engineer->Fire(First->GetActorLocation());
		// two at once (26.11.): the second goes after the first, a third is refused while both are out
		const bool bSecond = Engineer->Fire(Second->GetActorLocation());
		UE_LOG(LogVael, Display, TEXT("[SichelTest] straight throw: %s, second at once: %s, third refused: %s"), bThrown ? TEXT("PASS") : TEXT("FAIL"),
			bSecond && Engineer->GetSicklesOut() == 2 ? TEXT("PASS") : TEXT("FAIL"), !Engineer->Fire(First->GetActorLocation()) ? TEXT("PASS") : TEXT("FAIL"));
		TWeakObjectPtr<UVaelEngineerComponent> WeakEng(Engineer);
		TWeakObjectPtr<AVaelCreature> WeakFirst(First), WeakSecond(Second);
		TSharedRef<FTimerHandle> Watch = MakeShared<FTimerHandle>();
		TSharedRef<int32> Stage = MakeShared<int32>(0);
		TSharedRef<float> Waited = MakeShared<float>(0.0f);
		GetWorld()->GetTimerManager().SetTimer(*Watch, FTimerDelegate::CreateWeakLambda(this, [this, WeakEng, WeakFirst, WeakSecond, Watch, Stage, Waited, FirstHealth, SecondHealth]()
		{
			UVaelEngineerComponent* Eng = WeakEng.Get();
			*Waited += 0.02f;
			if (Eng == nullptr || *Waited > 12.0f)
			{
				const AVaelDoubleSickle* Late = Eng != nullptr ? Eng->GetFlyingSickle() : nullptr;
				UE_LOG(LogVael, Display, TEXT("[SichelTest] timed out at stage %d (sickle %s, returning %d, to hand %.1f s, at %s, thrower at %s): FAIL"), *Stage, Late != nullptr ? TEXT("flying") : TEXT("none"),
					Late != nullptr && Late->IsReturning(), Late != nullptr ? Late->GetSecondsToHand() : 0.0f, Late != nullptr ? *Late->GetActorLocation().ToCompactString() : TEXT("-"), Eng != nullptr ? *Eng->GetOwner()->GetActorLocation().ToCompactString() : TEXT("-"));
				GetWorld()->GetTimerManager().ClearTimer(*Watch);
				return;
			}
			const AVaelDoubleSickle* Sickle = Eng->GetFlyingSickle();
			if (*Stage == 0 && Sickle != nullptr && Sickle->IsReturning() && Sickle->GetSecondsToHand() < 0.15f)
			{
				// the button just before it arrives: caught
				Eng->StartDraw();
				*Stage = 1;
			}
			else if (*Stage <= 1 && Sickle == nullptr && *Waited > 0.2f)
			{
				const float Lost = WeakFirst.IsValid() ? FirstHealth - WeakFirst->GetHealth() : FirstHealth;
				UE_LOG(LogVael, Display, TEXT("[SichelTest] straight throw cut the doll (%.0f damage, out and back): %s, came home: PASS, caught: %s"),
					Lost, Lost > 0.0f ? TEXT("PASS") : TEXT("FAIL"), Eng->HasSickleCatchBonus() ? TEXT("PASS") : TEXT("FAIL"));
				const FVector Aim = WeakSecond.IsValid() ? WeakSecond->GetActorLocation() : Eng->GetOwner()->GetActorLocation() + Eng->GetOwner()->GetActorForwardVector() * 700.0f;
				Eng->StartDraw();
				*Stage = 2;
				*Waited = 0.0f;
				// held past the curve hold, then let go
				FTimerHandle Release;
				GetWorld()->GetTimerManager().SetTimer(Release, FTimerDelegate::CreateWeakLambda(this, [WeakEng, Aim, Stage]()
				{
					if (UVaelEngineerComponent* Held = WeakEng.Get())
					{
						const bool bCurve = Held->ReleaseDraw(Aim);
						UE_LOG(LogVael, Display, TEXT("[SichelTest] held throw round the second doll: %s, catch bonus used: %s"), bCurve ? TEXT("PASS") : TEXT("FAIL"),
							!Held->HasSickleCatchBonus() ? TEXT("PASS") : TEXT("FAIL"));
						*Stage = 3;
					}
				}), 0.45f, false);
			}
			else if (*Stage == 3 && Sickle == nullptr)
			{
				const float Lost = WeakSecond.IsValid() ? SecondHealth - WeakSecond->GetHealth() : SecondHealth;
				UE_LOG(LogVael, Display, TEXT("[SichelTest] curve cut the second doll (%.0f damage, doll at %s, thrower at %s): %s, home again: PASS"), Lost,
					WeakSecond.IsValid() ? *WeakSecond->GetActorLocation().ToCompactString() : TEXT("-"), *Eng->GetOwner()->GetActorLocation().ToCompactString(), Lost > 0.0f ? TEXT("PASS") : TEXT("FAIL"));
				GetWorld()->GetTimerManager().ClearTimer(*Watch);
			}
		}), 0.02f, true);
		return;
	}
	if (What == TEXT("sprengmeister"))
	{
		Player->SetHero(EVaelHeroClass::Engineer, false);
		FillMaterials();
		Engineer->LearnAllPlans();
		for (const TCHAR* Piece : { TEXT("Rammschild"), TEXT("Stolperdraht"), TEXT("Funkenladung"), TEXT("Salzladung"), TEXT("Gruenspanladung"), TEXT("Fernglas") })
		{
			Engineer->Build(Piece, true);
		}
		UE_LOG(LogVael, Display, TEXT("[SprengTest] Fernglas built: %s"), Engineer->HasBuilt(VaelEngineerGear::Spyglass) ? TEXT("PASS") : TEXT("FAIL"));
		const auto Ground = [this](const FVector& Spot)
		{
			FHitResult Hit;
			return GetWorld()->LineTraceSingleByChannel(Hit, Spot + FVector(0.0f, 0.0f, 500.0f), Spot - FVector(0.0f, 0.0f, 1000.0f), ECC_Visibility) ? Hit.ImpactPoint : Spot;
		};
		if (AVaelSafeZone::IsSafe(GetWorld(), Player->GetActorLocation(), 1800.0f))
		{
			for (int32 Turn = 0; Turn < 8; ++Turn)
			{
				const FVector Spot = Ground(Player->GetActorLocation() + FRotator(0.0f, 45.0f * Turn, 0.0f).Vector() * 3000.0f);
				if (!AVaelSafeZone::IsSafe(GetWorld(), Spot, 1800.0f) && Player->TeleportTo(Spot + FVector(0.0f, 0.0f, Player->GetSimpleCollisionHalfHeight() + 20.0f), Player->GetActorRotation()))
				{
					break;
				}
			}
		}
		const FVector Home = Player->GetActorLocation();
		const FVector Ahead = Player->GetActorForwardVector().GetSafeNormal2D();
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Ahead);
		const auto Doll = [this, Ground](const FVector& Spot)
		{
			return AVaelCreature::SpawnCreature(GetWorld(), GetMutableDefault<UVaelSickleDollData>(), Ground(Spot), FRotator::ZeroRotator);
		};
		const auto Lay = [Engineer](FName Id, const FVector& Spot)
		{
			for (int32 Step = 0; Step < 24 && Engineer->GetDevice() != Id; ++Step)
			{
				Engineer->CycleDevice(1);
			}
			return Engineer->GetDevice() == Id && Engineer->PlaceDevice(Spot);
		};
		const bool bRam = Lay(TEXT("Rammschild"), Home + Ahead * 350.0f);
		if (AVaelAshRoller* Roller = Cast<AVaelAshRoller>(AVaelCreature::SpawnCreature(GetWorld(), GetMutableDefault<UVaelAshRollerData>(), Ground(Home + Ahead * 1100.0f), FRotator::ZeroRotator)))
		{
			Roller->StartRoll(-Ahead);
		}
		const bool bTrip = Lay(TEXT("Stolperdraht"), Home + Side * 450.0f);
		if (AVaelCreature* Walker = Doll(Home + Side * 750.0f))
		{
			Walker->LureTo(Player, Home + Side * 150.0f, 8.0f, 1.0f);
		}
		const bool bFire = Lay(TEXT("Funkenladung"), Home - Side * 450.0f);
		Doll(Home - Side * 470.0f);
		const bool bSalt = Lay(TEXT("Salzladung"), Home - Ahead * 450.0f);
		Doll(Home - Ahead * 470.0f);
		const bool bGreen = Lay(TEXT("Gruenspanladung"), Home + (Ahead - Side).GetSafeNormal() * 500.0f);
		Doll(Home + (Ahead - Side).GetSafeNormal() * 520.0f);
		UE_LOG(LogVael, Display, TEXT("[SprengTest] laid: ram %d, wire %d, fire %d, salt %d, verdigris %d"), bRam, bTrip, bFire, bSalt, bGreen);

		FTimerHandle Check;
		TWeakObjectPtr<AVaelCharacter> WeakPlayer = Player;
		GetWorld()->GetTimerManager().SetTimer(Check, FTimerDelegate::CreateLambda([this, WeakPlayer, Doll]()
		{
			TMap<FName, int32> Work;
			for (TActorIterator<AVaelEngineerDevice> It(GetWorld()); It; ++It)
			{
				Work.FindOrAdd(It->GetDeviceType() == EVaelDeviceType::Ram ? FName(TEXT("Ram")) : It->GetDeviceType() == EVaelDeviceType::Tripwire ? FName(TEXT("Wire")) : FName(TEXT("Charge"))) += It->GetWork();
			}
			UE_LOG(LogVael, Display, TEXT("[SprengTest] Rammschild bounces the roller: %s (%d)"), Work.FindRef(TEXT("Ram")) > 0 ? TEXT("PASS") : TEXT("FAIL"), Work.FindRef(TEXT("Ram")));
			UE_LOG(LogVael, Display, TEXT("[SprengTest] Stolperdraht / charges still lying: wire %d, charges %d (the log lines \"crosses the Stolperdraht\" and \"bursts\" tell)"),
				Work.FindRef(TEXT("Wire")), Work.FindRef(TEXT("Charge")));
			// the Fernglas: raised, its focus reaches twice as far, and a doll there is marked
			AVaelCharacter* P = WeakPlayer.Get();
			if (P == nullptr)
			{
				return;
			}
			P->SetObserving(true);
			const FVector Feet = P->GetActorLocation() - FVector(0.0f, 0.0f, P->GetSimpleCollisionHalfHeight());
			const float Reach = FVector::Dist2D(P->GetFocusLocation(), Feet);
			UE_LOG(LogVael, Display, TEXT("[SprengTest] Fernglas reach %.0f cm (plain %.0f): %s"), Reach, UVaelCompendiumSettings::Get()->SpyglassReach,
				Reach > UVaelCompendiumSettings::Get()->SpyglassReach * 1.5f ? TEXT("PASS") : TEXT("FAIL"));
			Doll(P->GetFocusLocation());
			FTimerHandle Look;
			GetWorld()->GetTimerManager().SetTimer(Look, FTimerDelegate::CreateLambda([WeakPlayer]()
			{
				if (AVaelCharacter* Watcher = WeakPlayer.Get())
				{
					const int32 Marked = Watcher->GetEngineer() != nullptr ? Watcher->GetEngineer()->GetSpyglassMarked() : 0;
					UE_LOG(LogVael, Display, TEXT("[SprengTest] Fernglas marks: %s (%d)"), Marked > 0 ? TEXT("PASS") : TEXT("FAIL"), Marked);
					Watcher->SetObserving(false);
				}
			}), 1.5f, false);
		}), 4.0f, false);
		return;
	}
	// Five more devices (25.11.): "geraete" lays the Sichelkreisel, Würgnetz, Blendlaterne, Sturmwarner and Rollwall each by
	// Sichelpuppen and reports what each did after a while; every step logs [GeraeteTest]
	if (What == TEXT("geraete"))
	{
		Player->SetHero(EVaelHeroClass::Engineer, false);
		FillMaterials();
		Engineer->LearnAllPlans();
		const TCHAR* Pieces[] = { TEXT("Sichelkreisel"), TEXT("Wuergnetz"), TEXT("Blendlaterne"), TEXT("Sturmwarner"), TEXT("Rollwall") };
		for (const TCHAR* Piece : Pieces)
		{
			Engineer->Build(Piece, true);
		}
		const auto Ground = [this](const FVector& Spot)
		{
			FHitResult Hit;
			return GetWorld()->LineTraceSingleByChannel(Hit, Spot + FVector(0.0f, 0.0f, 500.0f), Spot - FVector(0.0f, 0.0f, 1000.0f), ECC_Visibility) ? Hit.ImpactPoint : Spot;
		};
		if (AVaelSafeZone::IsSafe(GetWorld(), Player->GetActorLocation(), 1800.0f))
		{
			for (int32 Turn = 0; Turn < 8; ++Turn)
			{
				const FVector Spot = Ground(Player->GetActorLocation() + FRotator(0.0f, 45.0f * Turn, 0.0f).Vector() * 3000.0f);
				if (!AVaelSafeZone::IsSafe(GetWorld(), Spot, 1800.0f) && Player->TeleportTo(Spot + FVector(0.0f, 0.0f, Player->GetSimpleCollisionHalfHeight() + 20.0f), Player->GetActorRotation()))
				{
					break;
				}
			}
		}
		const FVector Home = Player->GetActorLocation();
		const FVector Ahead = Player->GetActorForwardVector().GetSafeNormal2D();
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Ahead);
		const auto Doll = [this, Ground](const FVector& Spot)
		{
			return AVaelCreature::SpawnCreature(GetWorld(), GetMutableDefault<UVaelSickleDollData>(), Ground(Spot), FRotator::ZeroRotator);
		};
		const auto Lay = [Engineer](FName Id, const FVector& Spot)
		{
			for (int32 Step = 0; Step < 20 && Engineer->GetDevice() != Id; ++Step)
			{
				Engineer->CycleDevice(1);
			}
			return Engineer->GetDevice() == Id && Engineer->PlaceDevice(Spot);
		};
		// each device on its own side, its dolls placed so they come to it
		const bool bWhirl = Lay(TEXT("Sichelkreisel"), Home + Ahead * 350.0f);
		Doll(Home + Ahead * 420.0f);
		const bool bNet = Lay(TEXT("Wuergnetz"), Home + Side * 450.0f);
		const bool bLantern = Lay(TEXT("Blendlaterne"), Home - Side * 450.0f);
		Doll(Home - Side * 750.0f);
		const bool bWarner = Lay(TEXT("Sturmwarner"), Home - Ahead * 300.0f);
		const bool bRoll = Lay(TEXT("Rollwall"), Home + (Ahead + Side).GetSafeNormal() * 900.0f);
		Doll(Home + (Ahead + Side).GetSafeNormal() * 450.0f);
		UE_LOG(LogVael, Display, TEXT("[GeraeteTest] laid: whirl %d, net %d, lantern %d, warner %d, rollwall %d"), bWhirl, bNet, bLantern, bWarner, bRoll);
		// the net is sprung by a doll put down beside it once it is armed
		FTimerHandle NetDoll;
		GetWorld()->GetTimerManager().SetTimer(NetDoll, FTimerDelegate::CreateLambda([Doll, Home, Side]()
		{
			Doll(Home + Side * 520.0f);
			Doll(Home + Side * 380.0f);
		}), 1.0f, false);

		FTimerHandle Check;
		TWeakObjectPtr<AVaelPlayerController> WeakController = Controller;
		GetWorld()->GetTimerManager().SetTimer(Check, FTimerDelegate::CreateLambda([this, WeakController]()
		{
			TMap<EVaelDeviceType, int32> Work;
			bool bWall = false;
			for (TActorIterator<AVaelEngineerDevice> It(GetWorld()); It; ++It)
			{
				Work.FindOrAdd(It->GetDeviceType()) += It->GetWork();
				bWall |= It->IsWallUp();
			}
			const auto Line = [&Work](const TCHAR* Name, EVaelDeviceType Kind)
			{
				const int32 Done = Work.FindRef(Kind);
				UE_LOG(LogVael, Display, TEXT("[GeraeteTest] %s: %s (%d)"), Name, Done > 0 ? TEXT("PASS") : TEXT("FAIL"), Done);
			};
			Line(TEXT("Sichelkreisel cuts"), EVaelDeviceType::Whirl);
			Line(TEXT("Blendlaterne blinds"), EVaelDeviceType::Lantern);
			Line(TEXT("Sturmwarner marks"), EVaelDeviceType::Warner);
			Line(TEXT("Rollwall knocks aside"), EVaelDeviceType::Rollwall);
			UE_LOG(LogVael, Display, TEXT("[GeraeteTest] Rollwall stands: %s"), bWall ? TEXT("PASS") : TEXT("FAIL"));
			// the net is gone once its hold is over; its log line "Würgnetz holds" tells
			if (WeakController.IsValid())
			{
				WeakController->ConsoleCommand(TEXT("VaelWeather sturm"));
			}
			FTimerHandle Storm;
			GetWorld()->GetTimerManager().SetTimer(Storm, FTimerDelegate::CreateLambda([this]()
			{
				int32 Done = 0;
				for (TActorIterator<AVaelEngineerDevice> It(GetWorld()); It; ++It)
				{
					Done += It->GetDeviceType() == EVaelDeviceType::Warner ? It->GetWork() : 0;
				}
				UE_LOG(LogVael, Display, TEXT("[GeraeteTest] Sturmwarner in the storm (marks + gusts): %d"), Done);
			}), 4.0f, false);
		}), 2.2f, false);
		return;
	}
	// Umwelt (25.11.): "umwelt" tries the devices that read their surroundings: the Glutzapfer refused away from fire and
	// spitting by a Brandherd, a Glutknall by the fire, the Löschpulver putting the Brandherd out, the Säurefass pool, then in
	// rain a Dampfknall and in a storm smoke that drifts; every step logs [UmweltTest]
	if (What == TEXT("umwelt"))
	{
		Player->SetHero(EVaelHeroClass::Engineer, false);
		FillMaterials();
		Engineer->LearnAllPlans();
		for (const TCHAR* Piece : { TEXT("Knallmine"), TEXT("Knallmine"), TEXT("Rauchtopf"), TEXT("Glutzapfer"), TEXT("Saeurefass"), TEXT("Loeschpulver") })
		{
			Engineer->Build(Piece, true);
		}
		const auto Ground = [this](const FVector& Spot)
		{
			FHitResult Hit;
			return GetWorld()->LineTraceSingleByChannel(Hit, Spot + FVector(0.0f, 0.0f, 500.0f), Spot - FVector(0.0f, 0.0f, 1000.0f), ECC_Visibility) ? Hit.ImpactPoint : Spot;
		};
		const auto Pick = [](UVaelEngineerComponent* Eng, FName Id)
		{
			for (int32 Step = 0; Step < 16 && Eng->GetDevice() != Id; ++Step)
			{
				Eng->CycleDevice(1);
			}
			return Eng->GetDevice() == Id;
		};
		if (AVaelSafeZone::IsSafe(GetWorld(), Player->GetActorLocation(), 1800.0f))
		{
			for (int32 Turn = 0; Turn < 8; ++Turn)
			{
				const FVector Spot = Ground(Player->GetActorLocation() + FRotator(0.0f, 45.0f * Turn, 0.0f).Vector() * 3000.0f);
				if (!AVaelSafeZone::IsSafe(GetWorld(), Spot, 1800.0f) && Player->TeleportTo(Spot + FVector(0.0f, 0.0f, Player->GetSimpleCollisionHalfHeight() + 20.0f), Player->GetActorRotation()))
				{
					break;
				}
			}
		}
		const FVector Home = Player->GetActorLocation();
		const FVector Ahead = Player->GetActorForwardVector().GetSafeNormal2D();
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Ahead);

		// 1. No fire here: the Glutzapfer stays with him
		Pick(Engineer, TEXT("Glutzapfer"));
		const bool bFireHere = UVaelEnvironmentStatics::IsElementInEnvironment(Player, EVaelElement::Fire);
		const bool bRefused = !Engineer->PlaceDevice(Home - Ahead * 300.0f) && Engineer->GetCarried(TEXT("Glutzapfer")) == 1;
		UE_LOG(LogVael, Display, TEXT("[UmweltTest] Glutzapfer refused away from fire: %s (fire here %d)"), bRefused || bFireHere ? TEXT("PASS") : TEXT("FAIL"), bFireHere);

		// 2. A Brandherd ahead; the Glutzapfer by it, an Aschenroller to spit at, a mine by the fire
		const FVector FireAt = Ground(Home + Ahead * 450.0f);
		FVaelFireShape Shape;
		Shape.Radius = 90.0f;
		TWeakObjectPtr<AVaelFireSpot> Fire = AVaelFireSpot::Spawn(GetWorld(), FireAt, Shape, 60.0f);
		Pick(Engineer, TEXT("Glutzapfer"));
		const bool bTapped = Engineer->PlaceDevice(FireAt - Ahead * 110.0f);
		AVaelCreature::SpawnCreature(GetWorld(), GetMutableDefault<UVaelAshRollerData>(), Ground(FireAt + Side * 450.0f), (-Side).Rotation());
		Pick(Engineer, TEXT("Knallmine"));
		Engineer->PlaceDevice(FireAt + Ahead * 120.0f);
		UE_LOG(LogVael, Display, TEXT("[UmweltTest] Glutzapfer set by the Brandherd: %s"), bTapped ? TEXT("PASS") : TEXT("FAIL"));

		TWeakObjectPtr<UVaelEngineerComponent> WeakEng = Engineer;
		TWeakObjectPtr<AVaelCharacter> WeakPlayer = Player;
		TWeakObjectPtr<AVaelPlayerController> WeakController = Controller;
		FTimerHandle First;
		GetWorld()->GetTimerManager().SetTimer(First, FTimerDelegate::CreateLambda([this, WeakEng, WeakController, Fire, FireAt, Home, Ahead, Side, Pick, Ground]()
		{
			UVaelEngineerComponent* Eng = WeakEng.Get();
			if (Eng == nullptr)
			{
				return;
			}
			int32 Shots = 0;
			for (TActorIterator<AVaelEngineerDevice> It(GetWorld()); It; ++It)
			{
				Shots = FMath::Max(Shots, It->GetDeviceType() == EVaelDeviceType::Tapper ? It->GetTapperShots() : 0);
			}
			UE_LOG(LogVael, Display, TEXT("[UmweltTest] Glutzapfer spits: %s (%d spits)"), Shots > 0 ? TEXT("PASS") : TEXT("FAIL"), Shots);
			// the mine by the fire (look for "by fire" in the log), then the Löschpulver on the Brandherd
			Eng->IgniteAll();
			Pick(Eng, TEXT("Loeschpulver"));
			Eng->PlaceDevice(FireAt);
			// the Säurefass, with a roller dropped right on it
			Pick(Eng, TEXT("Saeurefass"));
			const FVector Barrel = Ground(Home - Side * 400.0f);
			Eng->PlaceDevice(Barrel);
			AVaelCreature::SpawnCreature(GetWorld(), GetMutableDefault<UVaelAshRollerData>(), Barrel, Ahead.Rotation());

			FTimerHandle Second;
			GetWorld()->GetTimerManager().SetTimer(Second, FTimerDelegate::CreateLambda([this, WeakEng, WeakController, Fire, Home, Ahead, Pick, Ground]()
			{
				UVaelEngineerComponent* Eng = WeakEng.Get();
				if (Eng == nullptr)
				{
					return;
				}
				const bool bOut = !Fire.IsValid() || !Fire->CanGiveFire();
				UE_LOG(LogVael, Display, TEXT("[UmweltTest] Löschpulver puts the Brandherd out: %s"), bOut ? TEXT("PASS") : TEXT("FAIL"));
				int32 Bitten = 0;
				bool bPool = false;
				for (TActorIterator<AVaelEngineerDevice> It(GetWorld()); It; ++It)
				{
					if (It->GetDeviceType() == EVaelDeviceType::Acid)
					{
						bPool |= It->IsPool();
						Bitten = FMath::Max(Bitten, It->GetAcidBitten());
					}
				}
				UE_LOG(LogVael, Display, TEXT("[UmweltTest] Säurefass pool bites: %s (pool %d, bites %d)"), bPool && Bitten > 0 ? TEXT("PASS") : TEXT("FAIL"), bPool, Bitten);

				// rain: the second mine bursts as steam (look for "on wet ground")
				if (WeakController.IsValid())
				{
					WeakController->ConsoleCommand(TEXT("VaelWeather regen"));
				}
				Pick(Eng, TEXT("Knallmine"));
				Eng->PlaceDevice(Home - Ahead * 400.0f);
				FTimerHandle Third;
				GetWorld()->GetTimerManager().SetTimer(Third, FTimerDelegate::CreateLambda([this, WeakEng, WeakController, Home, Ahead, Pick]()
				{
					UVaelEngineerComponent* Eng = WeakEng.Get();
					if (Eng == nullptr)
					{
						return;
					}
					Eng->IgniteAll();
					// storm: the smoke drifts
					if (WeakController.IsValid())
					{
						WeakController->ConsoleCommand(TEXT("VaelWeather sturm"));
					}
					Pick(Eng, TEXT("Rauchtopf"));
					Eng->PlaceDevice(Home + Ahead * 200.0f);
					FTimerHandle Fourth;
					GetWorld()->GetTimerManager().SetTimer(Fourth, FTimerDelegate::CreateLambda([this]()
					{
						float Drifted = 0.0f;
						for (TActorIterator<AVaelEngineerDevice> It(GetWorld()); It; ++It)
						{
							Drifted = FMath::Max(Drifted, It->GetDeviceType() == EVaelDeviceType::Smoke ? It->GetSmokeDrifted() : 0.0f);
						}
						UE_LOG(LogVael, Display, TEXT("[UmweltTest] smoke drifts in the storm: %s (%.0f cm)"), Drifted > 100.0f ? TEXT("PASS") : TEXT("FAIL"), Drifted);
					}), 4.0f, false);
				}), 2.0f, false);
			}), 2.5f, false);
		}), 4.0f, false);
		return;
	}
	// Sets only whole (Boris 25.11.): "satz" builds the Schwarzglut piece by piece; nothing goes on until the fourth, then all
	// four at once; taking off one place takes off the whole set, the shield stays its own
	if (What == TEXT("satz"))
	{
		Player->SetHero(EVaelHeroClass::Engineer, false);
		Engineer->ResetToStart();
		FillMaterials();
		Engineer->LearnAllPlans();
		const auto Worn = [Engineer]()
		{
			int32 Count = 0;
			for (int32 Place = 0; Place < 4; ++Place)
			{
				Count += Engineer->GetWorn(static_cast<EVaelArmorPlace>(Place)).IsNone() ? 0 : 1;
			}
			return Count;
		};
		bool bOk = true;
		for (int32 Place = 0; Place < 3; ++Place)
		{
			const FName Id(*FString::Printf(TEXT("Schwarzglut_%d"), Place));
			const bool bBuilt = Engineer->Build(Id, true);
			const bool bRefused = !Engineer->Wear(Id);
			bOk &= bBuilt && bRefused && Worn() == 0;
			UE_LOG(LogVael, Display, TEXT("[SatzTest] built %s %d, wear refused %d, worn %d"), *Id.ToString(), bBuilt, bRefused, Worn());
		}
		Engineer->Build(TEXT("Schwarzglut_3"), true);
		const int32 AllOn = Worn();
		Engineer->Build(TEXT("Schwarzglut_4"), true);
		const bool bShield = Engineer->HasShield();
		Engineer->TakeOff(EVaelArmorPlace::Arms);
		const int32 AfterOff = Worn();
		const bool bShieldStays = Engineer->HasShield();
		Engineer->Wear(TEXT("Schwarzglut_1"));
		const int32 Again = Worn();
		UE_LOG(LogVael, Display, TEXT("[SatzTest] %s: fourth piece puts on %d, shield %d, one off takes %d left, shield stays %d, wear one puts on %d"),
			bOk && AllOn == 4 && bShield && AfterOff == 0 && bShieldStays && Again == 4 ? TEXT("PASS") : TEXT("FAIL"), AllOn, bShield, AfterOff, bShieldStays, Again);

		// 26.11.: the Aschenjaeger too, its pieces found one by one at its creatures, its bonus only with the whole set
		Engineer->ResetToStart();
		FillMaterials();
		const FVaelGear* Hood = VaelEngineerGear::Find(TEXT("Aschenjaeger_0"));
		const bool bLocked = Hood != nullptr && !Engineer->KnowsRecipe(*Hood);
		Engineer->LearnAllPlans();
		bool bPartsOff = true;
		for (int32 Place = 0; Place < 3; ++Place)
		{
			FillMaterials();
			const FName Id(*FString::Printf(TEXT("Aschenjaeger_%d"), Place));
			Engineer->Build(Id, true);
			bPartsOff &= !Engineer->Wear(Id) && Worn() == 0;
		}
		const float SpeedPart = Engineer->GetMoveSpeedScale();
		FillMaterials();
		Engineer->Build(TEXT("Aschenjaeger_3"), true);
		Engineer->Wear(TEXT("Aschenjaeger_3"));
		const float SpeedWhole = Engineer->GetMoveSpeedScale();
		UE_LOG(LogVael, Display, TEXT("[SatzTest] Aschenjaeger %s: hood locked before its idea %d, three pieces not worn %d (speed %.2f), whole set worn %d (speed %.2f)"),
			bLocked && bPartsOff && FMath::IsNearlyEqual(SpeedPart, 1.0f) && Worn() == 4 && FMath::IsNearlyEqual(SpeedWhole, 1.1f) ? TEXT("PASS") : TEXT("FAIL"),
			bLocked, bPartsOff, SpeedPart, Worn(), SpeedWhole);
		return;
	}
	// Fallen-Paket (25.11.): "fallen" builds Grabmine, Rauchtopf, Lockpfeife and Wurfhaken, lays a Grabmine, whistles three
	// Sichelpuppen onto it, smokes the rest, then hooks a light and a heavy creature; every step logs [FallenTest]
	if (What == TEXT("fallen"))
	{
		Player->SetHero(EVaelHeroClass::Engineer, false);
		FillMaterials();
		Engineer->LearnAllPlans();
		Engineer->Build(TEXT("Knallmine"), true);
		const TCHAR* Pieces[] = { TEXT("Grabmine"), TEXT("Rauchtopf"), TEXT("Lockpfeife"), TEXT("Wurfhaken") };
		for (const TCHAR* Piece : Pieces)
		{
			const bool bBuilt = Engineer->Build(Piece, true);
			UE_LOG(LogVael, Display, TEXT("[FallenTest] build %s: %s (carried %d)"), Piece, bBuilt ? TEXT("PASS") : TEXT("FAIL"), Engineer->GetCarried(Piece));
		}
		const bool bAgain = Engineer->Build(TEXT("Lockpfeife"), true);
		UE_LOG(LogVael, Display, TEXT("[FallenTest] second Lockpfeife refused: %s"), !bAgain ? TEXT("PASS") : TEXT("FAIL"));

		TWeakObjectPtr<UVaelEngineerComponent> WeakEng = Engineer;
		TWeakObjectPtr<AVaelCharacter> WeakPlayer = Player;
		const auto Pick = [](UVaelEngineerComponent* Eng, FName Id)
		{
			for (int32 Step = 0; Step < 12 && Eng->GetDevice() != Id; ++Step)
			{
				Eng->CycleDevice(1);
			}
			return Eng->GetDevice() == Id;
		};
		const auto Ground = [this](const FVector& Spot)
		{
			FHitResult Hit;
			return GetWorld()->LineTraceSingleByChannel(Hit, Spot + FVector(0.0f, 0.0f, 500.0f), Spot - FVector(0.0f, 0.0f, 1000.0f), ECC_Visibility) ? Hit.ImpactPoint : Spot;
		};
		// Creatures keep out of the camps: out of the safe zone first, on open ground with room round it
		if (AVaelSafeZone::IsSafe(GetWorld(), Player->GetActorLocation(), 1800.0f))
		{
			bool bMoved = false;
			for (float Distance = 3000.0f; Distance <= 9000.0f && !bMoved; Distance += 1500.0f)
			{
				for (int32 Turn = 0; Turn < 8 && !bMoved; ++Turn)
				{
					const FVector Spot = Ground(Player->GetActorLocation() + FRotator(0.0f, 45.0f * Turn, 0.0f).Vector() * Distance);
					if (!AVaelSafeZone::IsSafe(GetWorld(), Spot, 1800.0f))
					{
						bMoved = Player->TeleportTo(Spot + FVector(0.0f, 0.0f, Player->GetSimpleCollisionHalfHeight() + 20.0f), Player->GetActorRotation());
					}
				}
			}
			UE_LOG(LogVael, Display, TEXT("[FallenTest] out of the safe zone: %s (%s)"), bMoved ? TEXT("PASS") : TEXT("FAIL"), *Player->GetActorLocation().ToString());
		}
		const FVector Ahead = Player->GetActorForwardVector().GetSafeNormal2D();
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Ahead);
		const FVector Home = Player->GetActorLocation();
		const FVector MineSpot = Home + Ahead * 500.0f;

		// 1. The Grabmine, and three Sichelpuppen 13 m off to the side, whistled onto it
		Pick(Engineer, TEXT("Grabmine"));
		Engineer->PlaceDevice(MineSpot);
		TArray<TWeakObjectPtr<AVaelCreature>> Dolls;
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const FVector Spot = Ground(Home + Ahead * 600.0f + Side * (500.0f + 150.0f * Index));
			Dolls.Add(AVaelCreature::SpawnCreature(GetWorld(), GetMutableDefault<UVaelSickleDollData>(), Spot, (-Side).Rotation()));
		}
		FTimerHandle Call;
		GetWorld()->GetTimerManager().SetTimer(Call, FTimerDelegate::CreateLambda([WeakEng, MineSpot, Pick]()
		{
			if (UVaelEngineerComponent* Eng = WeakEng.Get())
			{
				Pick(Eng, TEXT("Lockpfeife"));
				Eng->PlaceDevice(MineSpot);
				const bool bWaits = !Eng->PlaceDevice(MineSpot) && Eng->GetDeviceReadyIn(TEXT("Lockpfeife")) > 10.0f;
				UE_LOG(LogVael, Display, TEXT("[FallenTest] whistle cooldown holds: %s (%.1f s)"), bWaits ? TEXT("PASS") : TEXT("FAIL"), Eng->GetDeviceReadyIn(TEXT("Lockpfeife")));
			}
		}), 1.0f, false);

		FTimerHandle Check;
		GetWorld()->GetTimerManager().SetTimer(Check, FTimerDelegate::CreateLambda([this, WeakEng, WeakPlayer, Dolls, MineSpot, Pick, Ground, Ahead, Side, Home]()
		{
			UVaelEngineerComponent* Eng = WeakEng.Get();
			AVaelCharacter* Hero = WeakPlayer.Get();
			if (Eng == nullptr || Hero == nullptr)
			{
				return;
			}
			bool bMineLeft = false;
			for (TActorIterator<AVaelEngineerDevice> It(GetWorld()); It; ++It)
			{
				bMineLeft |= It->GetDeviceType() == EVaelDeviceType::BuriedMine && !It->HasDetonated();
			}
			int32 Hurt = 0;
			AVaelCreature* Living = nullptr;
			for (const TWeakObjectPtr<AVaelCreature>& Doll : Dolls)
			{
				Hurt += Doll.IsValid() ? 0 : 1;
				if (Doll.IsValid())
				{
					UE_LOG(LogVael, Display, TEXT("[FallenTest]   %s %.0f cm from the mine, health %.0f/%.0f, lured %d"), *Doll->GetName(), FVector::Dist2D(Doll->GetActorLocation(), MineSpot), Doll->GetHealth(), Doll->GetMaxHealth(), UVaelCombatStatics::HasStatus(Doll.Get(), EVaelStatus::Lured));
					Hurt += Doll->GetHealth() < Doll->GetMaxHealth() || Doll->IsDead() ? 1 : 0;
					Living = !Doll->IsDead() ? Doll.Get() : Living;
				}
			}
			UE_LOG(LogVael, Display, TEXT("[FallenTest] lured onto the Grabmine: %s (mine went off %d, %d of %d dolls hurt)"),
				!bMineLeft && Hurt > 0 ? TEXT("PASS") : TEXT("FAIL"), !bMineLeft, Hurt, Dolls.Num());

			// 2. The Rauchtopf on a doll that still stands (or a fresh one)
			if (Living == nullptr)
			{
				Living = AVaelCreature::SpawnCreature(GetWorld(), GetMutableDefault<UVaelSickleDollData>(), Ground(Home + Ahead * 450.0f), (-Ahead).Rotation());
			}
			Pick(Eng, TEXT("Rauchtopf"));
			const bool bLaid = Living != nullptr && Eng->PlaceDevice(Living->GetActorLocation());
			TWeakObjectPtr<AVaelCreature> WeakLiving = Living;
			FTimerHandle SmokeCheck;
			GetWorld()->GetTimerManager().SetTimer(SmokeCheck, FTimerDelegate::CreateLambda([this, WeakEng, WeakPlayer, bLaid, WeakLiving, Pick, Ground, Ahead, Side, Home]()
			{
				int32 Held = 0;
				for (TActorIterator<AVaelEngineerDevice> It(GetWorld()); It; ++It)
				{
					Held = FMath::Max(Held, It->GetDeviceType() == EVaelDeviceType::Smoke ? It->GetSmokeHeld() : 0);
				}
				const bool bConfused = WeakLiving.IsValid() && UVaelCombatStatics::HasStatus(WeakLiving.Get(), EVaelStatus::Confused);
				UE_LOG(LogVael, Display, TEXT("[FallenTest] smoke holds: %s (laid %d, held %d, doll confused %d)"), bLaid && Held > 0 ? TEXT("PASS") : TEXT("FAIL"), bLaid, Held, bConfused);

				// 3. The Wurfhaken: a light doll 9 m off is yanked in, then a heavy roller pulls him
				UVaelEngineerComponent* Eng = WeakEng.Get();
				AVaelCharacter* Hero = WeakPlayer.Get();
				if (Eng == nullptr || Hero == nullptr)
				{
					return;
				}
				AVaelCreature* Light = AVaelCreature::SpawnCreature(GetWorld(), GetMutableDefault<UVaelSickleDollData>(), Ground(Hero->GetActorLocation() + Side * 900.0f), (-Side).Rotation());
				const float Before = Light != nullptr ? FVector::Dist2D(Light->GetActorLocation(), Hero->GetActorLocation()) : 0.0f;
				Pick(Eng, TEXT("Wurfhaken"));
				Hero->SetActorRotation(Side.Rotation());
				const bool bThrown = Light != nullptr && Eng->PlaceDevice(Light->GetActorLocation());
				TWeakObjectPtr<AVaelCreature> WeakLight = Light;
				FTimerHandle HookCheck;
				GetWorld()->GetTimerManager().SetTimer(HookCheck, FTimerDelegate::CreateLambda([this, WeakEng, WeakPlayer, WeakLight, Before, bThrown, Ground, Ahead]()
				{
					UVaelEngineerComponent* Eng = WeakEng.Get();
					AVaelCharacter* Hero = WeakPlayer.Get();
					if (Eng == nullptr || Hero == nullptr)
					{
						return;
					}
					const float After = WeakLight.IsValid() ? FVector::Dist2D(WeakLight->GetActorLocation(), Hero->GetActorLocation()) : -1.0f;
					UE_LOG(LogVael, Display, TEXT("[FallenTest] hook yanks a light one: %s (thrown %d, %.0f -> %.0f cm)"),
						bThrown && After >= 0.0f && After < Before - 300.0f ? TEXT("PASS") : TEXT("FAIL"), bThrown, Before, After);

					AVaelCreature* Heavy = AVaelCreature::SpawnCreature(GetWorld(), GetMutableDefault<UVaelEmberQueenData>(), Ground(Hero->GetActorLocation() + Ahead * 1000.0f), (-Ahead).Rotation());
					const bool bHeavy = Heavy != nullptr && Heavy->IsHeavy();
					const FVector HeroBefore = Hero->GetActorLocation();
					const float HeavyBefore = Heavy != nullptr ? FVector::Dist2D(Heavy->GetActorLocation(), HeroBefore) : 0.0f;
					// the hook is still cooling down: wait it out
					FTimerHandle Second;
					TWeakObjectPtr<AVaelCreature> WeakHeavy = Heavy;
					GetWorld()->GetTimerManager().SetTimer(Second, FTimerDelegate::CreateLambda([this, WeakEng, WeakPlayer, WeakHeavy, bHeavy, HeavyBefore]()
					{
						UVaelEngineerComponent* Eng = WeakEng.Get();
						AVaelCharacter* Hero = WeakPlayer.Get();
						if (Eng == nullptr || Hero == nullptr || !WeakHeavy.IsValid())
						{
							UE_LOG(LogVael, Display, TEXT("[FallenTest] hook pulls him to a heavy one: FAIL (no roller)"));
							return;
						}
						const FVector HeroBefore = Hero->GetActorLocation();
						const bool bThrown = Eng->PlaceDevice(WeakHeavy->GetActorLocation());
						FTimerHandle Third;
						GetWorld()->GetTimerManager().SetTimer(Third, FTimerDelegate::CreateLambda([WeakPlayer, WeakHeavy, bHeavy, bThrown, HeroBefore, HeavyBefore]()
						{
							if (!WeakPlayer.IsValid() || !WeakHeavy.IsValid())
							{
								return;
							}
							const float Moved = FVector::Dist2D(WeakPlayer->GetActorLocation(), HeroBefore);
							const float Gap = FVector::Dist2D(WeakPlayer->GetActorLocation(), WeakHeavy->GetActorLocation());
							UE_LOG(LogVael, Display, TEXT("[FallenTest] hook on a heavy one: %s (heavy %d, thrown %d, he moved %.0f cm, gap %.0f of %.0f cm)"),
								bThrown && (!bHeavy || Moved > 300.0f) ? TEXT("PASS") : TEXT("FAIL"), bHeavy, bThrown, Moved, Gap, HeavyBefore);
						}), 1.5f, false);
					}), 6.2f, false);
				}), 1.5f, false);
			}), 2.0f, false);
		}), 8.0f, false);
		return;
	}
	// The first camp (goal 16.11.): "lager" opens the store chest of the level as he would, then mixes his first shot
	// (Schlacke + Glutstein = Glutladung) and reports what he has
	if (What == TEXT("lager"))
	{
		Player->SetHero(EVaelHeroClass::Engineer, false);
		AVaelStorageChest* Chest = nullptr;
		for (TActorIterator<AVaelStorageChest> It(GetWorld()); It; ++It)
		{
			Chest = *It;
		}
		const bool bForge = TActorIterator<AVaelForge>(GetWorld()) ? true : false;
		const bool bBench = TActorIterator<AVaelWorkbench>(GetWorld()) ? true : false;
		if (Chest != nullptr)
		{
			Chest->Interact(Player);
		}
		FText Hint;
		FName Formula;
		const EVaelMixResult Result = Engineer->Mix({ TEXT("Schlacke"), TEXT("Glutstein") }, Hint, Formula);
		UE_LOG(LogVael, Display, TEXT("[LagerTest] %s: chest %d, forge %d, bench %d, mix %d (%s), Glutladung carried %d, Schlacke left %d"),
			Chest != nullptr && bForge && bBench && Result == EVaelMixResult::Learned ? TEXT("PASS") : TEXT("FAIL"), Chest != nullptr, bForge, bBench,
			static_cast<int32>(Result), *Formula.ToString(), Engineer->GetCarried(TEXT("Brand")),
			Player->GetMaterialBag()->GetCount(VaelEngineerGear::LoadMaterial(TEXT("Schlacke"))));
		return;
	}
	// Showcase of the sling (goal 16.11.): "zwille [Ladung]" draws for 1.2 s and lets go every 2.6 s, straight ahead
	if (What == TEXT("zwille"))
	{
		Player->SetHero(EVaelHeroClass::Engineer, false);
		FillMaterials();
		Engineer->LearnAllPlans();
		Engineer->LearnAllFormulas();
		if (!Id.IsEmpty())
		{
			Engineer->Build(FName(*Id), true);
			Engineer->SetAmmo(FName(*Id));
		}
		TWeakObjectPtr<UVaelEngineerComponent> WeakEng = Engineer;
		TWeakObjectPtr<AVaelCharacter> WeakPlayer = Player;
		const FName Shot = Id.IsEmpty() ? VaelEngineerGear::Standard : FName(*Id);
		FTimerHandle Loop;
		GetWorld()->GetTimerManager().SetTimer(Loop, FTimerDelegate::CreateLambda([WeakEng, WeakPlayer, Shot]()
		{
			if (!WeakEng.IsValid() || !WeakPlayer.IsValid())
			{
				return;
			}
			if (Shot != VaelEngineerGear::Standard && WeakEng->GetCarried(Shot) <= 0)
			{
				WeakEng->Build(Shot, true);
			}
			WeakEng->SetAmmo(Shot);
			WeakEng->StartDraw();
			FTimerHandle Release;
			WeakPlayer->GetWorldTimerManager().SetTimer(Release, FTimerDelegate::CreateLambda([WeakEng, WeakPlayer]()
			{
				if (WeakEng.IsValid() && WeakPlayer.IsValid())
				{
					WeakEng->ReleaseDraw(WeakPlayer->GetActorLocation() + WeakPlayer->GetActorForwardVector() * 1500.0f);
				}
			}), 1.2f, false);
		}), 2.6f, true, 0.3f);
		return;
	}
	// Ideas (goal 16.11.): "ideen" lists them, "ideen alle" gives every one, "ideen test" checks that meeting a creature gives
	// its idea and unlocks its recipe, "ideen <Id>" shows the card of one
	if (What == TEXT("ideen"))
	{
		if (Id == TEXT("alle"))
		{
			Engineer->LearnAllIdeas();
		}
		else if (Id == TEXT("test"))
		{
			Engineer->ResetToStart();
			FillMaterials();
			const FVaelGear* Branch = VaelEngineerGear::Find(TEXT("Glutschleuder"));
			const bool bLockedBefore = Branch != nullptr && Engineer->GetBuildBlock(*Branch, true) == UVaelEngineerComponent::EBuildBlock::NeedsIdea;
			UVaelCompendiumSubsystem* Compendium = UVaelCompendiumSubsystem::Get(Player);
			if (Compendium != nullptr)
			{
				Compendium->SetStage(TEXT("Glutkriecher"), EVaelResearchStage::Sighted);
			}
			Engineer->CheckIdeas(true);
			const bool bIdea = Engineer->HasIdea(TEXT("Krustenglut")) && Engineer->GetLastIdea() == FName(TEXT("Krustenglut"));
			const bool bOpenAfter = Branch != nullptr && Engineer->GetBuildBlock(*Branch, true) == UVaelEngineerComponent::EBuildBlock::Ok;
			const bool bArmorStillLocked = !Engineer->HasIdea(TEXT("Schwarzglut"));
			const bool bPass = bLockedBefore && bIdea && bOpenAfter && bArmorStillLocked;
			UE_LOG(LogVael, Display, TEXT("[IdeenTest] %s: locked before %d, idea %d, open after %d, armour still locked %d, %d ideas in all"), bPass ? TEXT("PASS") : TEXT("FAIL"),
				bLockedBefore, bIdea, bOpenAfter, bArmorStillLocked, VaelEngineerIdeas::GetAll().Num());
			return;
		}
		else if (!Id.IsEmpty())
		{
			Engineer->LearnIdea(FName(*Id), true);
		}
		for (const FVaelEngineerIdea& Idea : VaelEngineerIdeas::GetAll())
		{
			UE_LOG(LogVael, Display, TEXT("[Ideen] %s %s (%s, stage %d): %d recipes"), Engineer->HasIdea(Idea.Id) ? TEXT("+") : TEXT("-"), *Idea.Id.ToString(),
				*Idea.Subject.ToString(), static_cast<int32>(Idea.Stage), Idea.Gear.Num());
		}
		return;
	}

	if (What == TEXT("material"))
	{
		FillMaterials();
		return;
	}
	if (What == TEXT("formeln"))
	{
		// Goal 14.11.: mixing. From a fresh start: every formula found by mixing its parts, the refining recipes, mixes that
		// must fail (with their hints), a mix tried twice, and the pockets (4 kinds, the fifth goes into the backpack)
		Player->SetHero(EVaelHeroClass::Engineer, false);
		Engineer->ResetToStart();
		FillMaterials();
		Engineer->LearnAllPlans();
		int32 Refined = 0;
		for (const FVaelGear& Gear : VaelEngineerGear::GetAll())
		{
			if (Gear.Kind == EVaelGearKind::Refine)
			{
				const bool bOk = Engineer->Build(Gear.Id, false);
				Refined += bOk ? 1 : 0;
				UE_LOG(LogVael, Log, TEXT("VaelIngenieur formeln: refine %s -> %d"), *Gear.Id.ToString(), bOk);
			}
		}
		int32 Found = 0, Total = 0, Unbuildable = 0;
		for (const FVaelGear& Gear : VaelEngineerGear::GetAll())
		{
			if (!Gear.IsFormula())
			{
				continue;
			}
			++Total;
			Unbuildable += Engineer->GetBuildBlock(Gear, true) == UVaelEngineerComponent::EBuildBlock::Unknown ? 1 : 0;
			TArray<FName> Parts = { Gear.Boosters.Num() > 0 ? Gear.Boosters.Last() : Gear.Core, Gear.Hull };
			Parts.Append(Gear.Boosters.Num() > 0 ? TArray<FName>{ Gear.Core } : TArray<FName>{});
			if (Gear.Boosters.Num() > 1)
			{
				Parts.Add(Gear.Boosters[0]);
			}
			FText Hint;
			FName Formula;
			const EVaelMixResult Result = Engineer->Mix(Parts, Hint, Formula);
			Found += Result == EVaelMixResult::Learned ? 1 : 0;
			UE_LOG(LogVael, Log, TEXT("VaelIngenieur formeln: %s from [%s] -> %d (%s) carried %d stock %d"), *Gear.Id.ToString(),
				*VaelEngineerGear::GetMixKey(Parts), static_cast<int32>(Result), *Formula.ToString(), Engineer->GetCarried(Gear.Id), Engineer->GetStock(Gear.Id));
		}
		const TArray<TArray<FName>> Wrong = {
			{ TEXT("Latex"), TEXT("Glutstein") }, { TEXT("Latex"), TEXT("Verdauungssaeure") }, { TEXT("Blasenhaut"), TEXT("Glutstein") },
			{ TEXT("Schlacke"), TEXT("Blatttau") }, { TEXT("Glutstein"), TEXT("Knistersamen") }, { TEXT("Schlacke"), TEXT("Latex"), TEXT("Glutstein") },
			{ TEXT("Schlacke"), TEXT("Distelflaum") }, { TEXT("Schlacke"), TEXT("Glutstein"), TEXT("Pelzwasser") } };
		for (const TArray<FName>& Parts : Wrong)
		{
			FText Hint;
			FName Formula;
			const EVaelMixResult Result = Engineer->Mix(Parts, Hint, Formula);
			UE_LOG(LogVael, Log, TEXT("VaelIngenieur formeln: wrong [%s] -> %d: %s"), *VaelEngineerGear::GetMixKey(Parts), static_cast<int32>(Result), *Hint.ToString());
		}
		{
			FText Hint;
			FName Formula;
			const EVaelMixResult Again = Engineer->Mix(Wrong[0], Hint, Formula);
			UE_LOG(LogVael, Log, TEXT("VaelIngenieur formeln: same wrong mix again -> %d (3 = tried)"), static_cast<int32>(Again));
		}
		UE_LOG(LogVael, Log, TEXT("VaelIngenieur formeln: %d of %d learned (%d were locked before), %d refined, pockets %d of %d: %s, choices %d"),
			Found, Total, Unbuildable, Refined, Engineer->GetPocketKinds().Num(), Engineer->GetPocketCount(),
			*FString::JoinBy(Engineer->GetPocketKinds(), TEXT(","), [](FName Kind) { return Kind.ToString(); }), Engineer->GetAmmoChoices().Num());
		// a kind out of its pocket and another one in
		const FName First = Engineer->GetPocketKinds().Num() > 0 ? Engineer->GetPocketKinds()[0] : NAME_None;
		const int32 Back = Engineer->EmptyPocket(First);
		const bool bIn = Engineer->PutInPocket(TEXT("Nid"));
		UE_LOG(LogVael, Log, TEXT("VaelIngenieur formeln: %s back into the backpack (%d), Nid into a pocket %d, carried %d"),
			*First.ToString(), Back, bIn, Engineer->GetCarried(TEXT("Nid")));
		return;
	}
	if (What == TEXT("bauen"))
	{
		const bool bBuilt = Engineer->Build(FName(*Id), true);
		UE_LOG(LogVael, Log, TEXT("VaelIngenieur: build %s -> %d"), *Id, bBuilt);
		return;
	}
	if (What == TEXT("alles"))
	{
		FillMaterials();
		Engineer->LearnAllPlans();
		int32 Count = 0;
		for (const FVaelGear& Gear : VaelEngineerGear::GetAll())
		{
			Count += Engineer->Build(Gear.Id, true) ? 1 : 0;
		}
		UE_LOG(LogVael, Log, TEXT("VaelIngenieur: built %d pieces, armour %.0f, max health %.0f"), Count, Engineer->GetArmorTotal(), Player->GetMaxHealth());
		return;
	}
	if (What == TEXT("werkbank") || What == TEXT("feld"))
	{
		// The page of building, as at the workbench or the field box; "werkbank 3" opens a tab, Id a row
		Player->SetHero(EVaelHeroClass::Engineer, false);
		FillMaterials();
		Controller->OpenCrafting(What == TEXT("werkbank"));
		for (int32 Step = 0; Step < FCString::Atoi(*Id); ++Step)
		{
			Controller->PressMenuPageForTest(1);
		}
		return;
	}
	if (What == TEXT("gestalt"))
	{
		// The page Gestalt with the figure turned: height Id steps, build two steps broader
		Controller->OpenHeroPageForTest();
		for (int32 Step = 0; Step < 2; ++Step)
		{
			Controller->StepFigure(1, 1);
		}
		for (int32 Step = 0; Step < FCString::Atoi(*Id); ++Step)
		{
			Controller->StepFigure(0, 1);
		}
		UE_LOG(LogVael, Log, TEXT("VaelIngenieur: figure height %.2f build %.2f, mesh scale %s"), Player->GetFigureHeight(), Player->GetFigureBuild(), *Player->GetMesh()->GetRelativeScale3D().ToString());
		return;
	}
	if (What == TEXT("flugbild"))
	{
		// Goal 9.11., step 6: the nine new shots side by side in slow flight, 2 m in front of him, to see each one's look
		Player->SetHero(EVaelHeroClass::Engineer, false);
		const TCHAR* Kinds[] = { TEXT("Schwer"), TEXT("Klebe"), TEXT("Rauch"), TEXT("Knister"), TEXT("Schlaf"), TEXT("Leucht"), TEXT("Dorn"), TEXT("Tau"), TEXT("Nid") };
		const FVector Fwd = Player->GetActorForwardVector();
		const FVector Right = Player->GetActorRightVector();
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Kinds); ++Index)
		{
			const FVaelGear* Gear = VaelEngineerGear::Find(Kinds[Index]);
			// in a row behind him, flying slowly across from his left to his right
			const FVector From = Player->GetActorLocation() - Right * 420.0f + Fwd * (Index * 95.0f - 640.0f) + FVector(0.0f, 0.0f, 40.0f);
			const AVaelEngineerShot* Shot = AVaelEngineerShot::Launch(Engineer, From, From + Right * 3000.0f, 101.0f, 0.0f, 0.0f, Kinds[Index], false,
				Gear != nullptr ? Gear->Color : FLinearColor::White, 9.0f, true);
			UE_LOG(LogVael, Log, TEXT("VaelIngenieur flugbild: %s %s at %s"), Kinds[Index], Shot != nullptr ? TEXT("flies") : TEXT("failed"), *From.ToCompactString());
		}
		return;
	}
	if (What == TEXT("munition"))
	{
		// Goal 8.11. evening, step 9: the new shots one after another at the nearest enemy (Id: only this one), each hit logged
		// with the target's health and statuses
		Player->SetHero(EVaelHeroClass::Engineer, false);
		FillMaterials();
		Engineer->LearnAllPlans();
		Engineer->LearnAllFormulas();
		// goal 14.11.: every formula of shots, in the order of the workbench
		TArray<FName> Kinds;
		for (const FVaelGear& Gear : VaelEngineerGear::GetAll())
		{
			if (Gear.IsFormula())
			{
				Kinds.Add(Gear.Id);
			}
		}
		if (!Id.IsEmpty())
		{
			Kinds = { FName(*Id) };
		}
		for (const FName Kind : Kinds)
		{
			Engineer->Build(Kind, true);
		}
		TWeakObjectPtr<UVaelEngineerComponent> WeakEng = Engineer;
		TWeakObjectPtr<AVaelCharacter> WeakPlayer = Player;
		TSharedRef<FTimerHandle> Loop = MakeShared<FTimerHandle>();
		TSharedRef<int32> Next = MakeShared<int32>(0);
		GetWorld()->GetTimerManager().SetTimer(*Loop, FTimerDelegate::CreateLambda([WeakEng, WeakPlayer, Loop, Next, Kinds]()
		{
			if (!WeakEng.IsValid() || !WeakPlayer.IsValid())
			{
				return;
			}
			AVaelCharacterBase* Target = nullptr;
			float Best = 2500.0f;
			for (TActorIterator<AVaelCharacterBase> It(WeakPlayer->GetWorld()); It; ++It)
			{
				const float Dist = FVector::Dist(It->GetActorLocation(), WeakPlayer->GetActorLocation());
				if (!It->IsA<AVaelCharacter>() && It->GetHealth() > 0.0f && Dist < Best)
				{
					Best = Dist;
					Target = *It;
				}
			}
			// Without enemies (Id "frei" or none near): shots into the open, 16 m ahead, to see them fly
			if (Target == nullptr && *Next < Kinds.Num())
			{
				const FName Kind = Kinds[*Next];
				PocketOnly(WeakEng.Get(), Kind);
				WeakEng->SetAmmo(Kind);
				WeakEng->Fire(WeakPlayer->GetActorLocation() + WeakPlayer->GetActorForwardVector() * 1600.0f - FVector(0.0f, 0.0f, 90.0f));
				UE_LOG(LogVael, Log, TEXT("VaelIngenieur munition %s: into the open"), *Kind.ToString());
				++*Next;
				return;
			}
			if (Target == nullptr || *Next >= Kinds.Num())
			{
				UE_LOG(LogVael, Log, TEXT("VaelIngenieur munition: done after %d shots, balance %.1f"), *Next,
					UVaelBalanceSubsystem::GetPlayer(WeakPlayer.Get()));
				WeakPlayer->GetWorld()->GetTimerManager().ClearTimer(*Loop);
				return;
			}
			const FName Kind = Kinds[*Next];
			WeakPlayer->SetActorRotation((Target->GetActorLocation() - WeakPlayer->GetActorLocation()).GetSafeNormal2D().Rotation());
			PocketOnly(WeakEng.Get(), Kind);
			WeakEng->SetAmmo(Kind);
			const float Before = Target->GetHealth();
			const bool bFired = WeakEng->Fire(Target->GetActorLocation());
			UE_LOG(LogVael, Log, TEXT("VaelIngenieur munition %s: fired %d at %.0f cm, %d left"), *Kind.ToString(), bFired, Best, WeakEng->GetCarried(Kind));
			++*Next;
			TWeakObjectPtr<AVaelCharacterBase> WeakTarget = Target;
			FTimerHandle Report;
			WeakPlayer->GetWorld()->GetTimerManager().SetTimer(Report, FTimerDelegate::CreateLambda([WeakTarget, Kind, Before]()
			{
				if (!WeakTarget.IsValid())
				{
					UE_LOG(LogVael, Log, TEXT("VaelIngenieur munition %s: target gone"), *Kind.ToString());
					return;
				}
				FString Statuses;
				for (EVaelStatus Status : { EVaelStatus::Wet, EVaelStatus::Burning, EVaelStatus::Frozen, EVaelStatus::Slowed, EVaelStatus::Marked, EVaelStatus::Confused })
				{
					if (UVaelCombatStatics::HasStatus(WeakTarget.Get(), Status))
					{
						Statuses += UEnum::GetValueAsString(Status) + TEXT(" ");
					}
				}
				UE_LOG(LogVael, Log, TEXT("VaelIngenieur munition %s: %s %.0f -> %.0f [%s]"), *Kind.ToString(), *WeakTarget->GetName(), Before,
					WeakTarget->GetHealth(), *Statuses);
			}), 0.9f, false);
		}), 2.0f, true, 1.0f);
		return;
	}
	if (What == TEXT("bruch"))
	{
		// A dummy 6 m ahead facing the player: stones at its head until it breaks
		Player->SetHero(EVaelHeroClass::Engineer, false);
		FActorSpawnParameters BreakSpawn;
		BreakSpawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		AVaelTrainingDummy* Target = GetWorld()->SpawnActor<AVaelTrainingDummy>(AVaelTrainingDummy::StaticClass(),
			Player->GetActorLocation() + Player->GetActorForwardVector() * 600.0f, FRotator::ZeroRotator, BreakSpawn);
		if (Target == nullptr)
		{
			return;
		}
		Target->SetActorRotation((Player->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D().Rotation());
		TWeakObjectPtr<UVaelEngineerComponent> WeakEng = Engineer;
		TWeakObjectPtr<AVaelTrainingDummy> WeakTarget = Target;
		TSharedRef<FTimerHandle> Loop = MakeShared<FTimerHandle>();
		TSharedRef<int32> Shots = MakeShared<int32>(0);
		GetWorld()->GetTimerManager().SetTimer(*Loop, FTimerDelegate::CreateLambda([WeakEng, WeakTarget, Loop, Shots]()
		{
			if (!WeakEng.IsValid() || !WeakTarget.IsValid())
			{
				return;
			}
			if (WeakEng->BrokenParts > 0 || *Shots > 40)
			{
				UE_LOG(LogVael, Log, TEXT("VaelIngenieur: %d parts broken after %d shots"), WeakEng->BrokenParts, *Shots);
				WeakTarget->GetWorld()->GetTimerManager().ClearTimer(*Loop);
				return;
			}
			if (WeakEng->Fire(WeakTarget->GetActorLocation()))
			{
				++*Shots;
			}
		}), 0.1f, true, 1.0f);
		return;
	}
	if (What == TEXT("griff"))
	{
		// Everything built, the pouches emptied into the backpack: one reach from the pouch, one from the backpack (logged)
		VaelIngenieur(TEXT("alles"));
		Engineer->CycleAmmo(1);
		UE_LOG(LogVael, Log, TEXT("VaelIngenieur: reach for %s from %s, pouch tier %d"), *Engineer->GetGrabAmmo().ToString(), Engineer->IsGrabFromPack() ? TEXT("backpack") : TEXT("pouch"), Engineer->GetPouchTier());
		// keep turning the wheel for the screenshot
		TWeakObjectPtr<UVaelEngineerComponent> WeakEng = Engineer;
		FTimerHandle Turn;
		GetWorld()->GetTimerManager().SetTimer(Turn, FTimerDelegate::CreateLambda([WeakEng]()
		{
			if (WeakEng.IsValid() && !WeakEng->IsGrabbing())
			{
				WeakEng->CycleAmmo(1);
			}
		}), 0.7f, true);
		return;
	}
	if (What == TEXT("ablegen"))
	{
		// Takes the piece off a place (0 helmet, 1 chest, 2 arms, 3 legs, 4 shield); it goes back into the toolbox
		const int32 Place = FMath::Clamp(FCString::Atoi(*Id), 0, 4);
		UE_LOG(LogVael, Log, TEXT("VaelIngenieur ablegen %d: %d"), Place, Engineer->TakeOff(static_cast<EVaelArmorPlace>(Place)));
		return;
	}
	if (What == TEXT("ruestung"))
	{
		// Everything built, the set named in Id worn on all four places (since 17.11. only Schwarzglut is in the game)
		VaelIngenieur(TEXT("alles"));
		const FString Set = Id.IsEmpty() ? FString(TEXT("Schwarzglut")) : Id;
		// a stage II set (Schwarzglut2, Aschenjaeger2) is built on the pieces of stage I: those first, each with fresh materials
		// ("alles" fills the bag once, and the last pieces of a long list may go without)
		if (Set.EndsWith(TEXT("2")))
		{
			for (int32 Place = 0; Place < 4; ++Place)
			{
				FillMaterials();
				Engineer->Build(FName(*FString::Printf(TEXT("%s_%d"), *Set.LeftChop(1), Place)), true);
			}
		}
		for (int32 Place = 0; Place < 4; ++Place)
		{
			FillMaterials();
			const FName Piece(*FString::Printf(TEXT("%s_%d"), *Set, Place));
			const bool bBuilt = Engineer->Build(Piece, true);
			const bool bOn = Engineer->Wear(Piece);
			UE_LOG(LogVael, Log, TEXT("VaelIngenieur ruestung: %s built now %d, has %d, put on %d, chest %s"), *Piece.ToString(), bBuilt, Engineer->HasBuilt(Piece), bOn, *Engineer->GetWorn(EVaelArmorPlace::Chest).ToString());
		}
		UE_LOG(LogVael, Log, TEXT("VaelIngenieur: wears %s, chest %s, armour %.0f"), *Set, *Engineer->GetWorn(EVaelArmorPlace::Chest).ToString(), Engineer->GetArmorTotal());
		return;
	}
	if (What == TEXT("panzerschild"))
	{
		// The Glutpanzer with its back shell as shield (16.11.): the whole Schwarzglut set and the Klappschild worn the first
		// time; with a number the shell holds still that far between back (0) and front (1), for pictures of the roll
		if (Engineer->GetWorn(EVaelArmorPlace::Shield).IsNone())
		{
			Player->SetHero(EVaelHeroClass::Engineer, false);
			VaelIngenieur(TEXT("alles"));
			for (int32 Place = 0; Place < 5; ++Place)
			{
				FillMaterials();
				const FName Piece(*FString::Printf(TEXT("Schwarzglut_%d"), Place));
				Engineer->Build(Piece, true);
				Engineer->Wear(Piece);
			}
		}
		const float Open = Id.IsEmpty() ? -1.0f : FCString::Atof(*Id);
		Engineer->SetShieldPreview(Open);
		UE_LOG(LogVael, Log, TEXT("VaelIngenieur panzerschild: set %d, shield %d, preview %.2f"), Engineer->CountSet(TEXT("Schwarzglut")), Engineer->HasShield(), Open);
		return;
	}
	if (What == TEXT("armschleuder"))
	{
		// The Panzerschleuder (16.11.): the whole Schwarzglut set and the sling; with a number the aim holds still (0 = fork
		// back on the forearm, 1 = slid forward over the fist), for pictures
		VaelIngenieur(TEXT("panzerschild"));
		Engineer->ForceBranch(VaelEngineerGear::Sling, Engineer->GetBranch(VaelEngineerGear::Sling));
		Engineer->SetSlingAimPreview(Id.IsEmpty() ? -1.0f : FCString::Atof(*Id));
		for (UActorComponent* Each : Player->GetComponents())
		{
			if (const UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Each); Mesh != nullptr && Mesh->GetName().StartsWith(TEXT("EngineerArmSling")))
			{
				UE_LOG(LogVael, Log, TEXT("VaelIngenieur armschleuder: %s visible %d mesh %s at %s (actor space)"), *Mesh->GetName(), Mesh->IsVisible(), *GetNameSafe(Mesh->GetStaticMesh()),
					*Player->GetActorTransform().InverseTransformPosition(Mesh->GetComponentLocation()).ToCompactString());
			}
		}
		return;
	}
	if (What == TEXT("kugel") || What == TEXT("kugelstehen") || What == TEXT("kugelwand"))
	{
		// The Panzerkugel (17.11.): the whole Schwarzglut set and a dodge straight ahead, the button held for the seconds
		// given (none: a plain tap); "kugelstehen" shows the ball standing still, for pictures
		VaelIngenieur(TEXT("panzerschild"));
		Engineer->SetShieldPreview(-1.0f);
		Engineer->SetBallPreview(What == TEXT("kugelstehen"));
		if (What != TEXT("kugelstehen"))
		{
			const float Hold = Id.IsEmpty() ? 0.0f : FCString::Atof(*Id);
			Engineer->SetRollHeld(Hold > 0.0f);
			// Rolls the way with the most room, away from tables and fences
			FVector Away = Player->GetActorForwardVector();
			float Room = -1.0f;
			for (int32 Step = 0; Step < 16; ++Step)
			{
				const FVector Way = FVector::ForwardVector.RotateAngleAxis(Step * 22.5f, FVector::UpVector);
				FHitResult Block;
				const FVector From = Player->GetActorLocation() - FVector(0.0f, 0.0f, 40.0f);
				const float Free = GetWorld()->SweepSingleByChannel(Block, From, From + Way * 2500.0f, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(30.0f), FCollisionQueryParams(NAME_None, false, Player))
					? Block.Distance : 2500.0f;
				// "kugelwand": the nearest wall at least a few metres off instead, to roll into it
				if (What == TEXT("kugelwand") ? Free > 350.0f && (Room < 0.0f || Free < Room) : Free > Room)
				{
					Room = Free;
					Away = Way;
				}
			}
			const bool bStarted = Player->StartDodge(Away);
			if (Hold > 0.0f)
			{
				FTimerHandle Release;
				TWeakObjectPtr<UVaelEngineerComponent> Weak(Engineer);
				Player->GetWorldTimerManager().SetTimer(Release, [Weak]() { if (Weak.IsValid()) { Weak->SetRollHeld(false); } }, Hold, false);
			}
			UE_LOG(LogVael, Log, TEXT("VaelIngenieur kugel: dodge %d, held %.1f s, set %d, speed scale %.2f"), bStarted, Hold, Engineer->CountSet(TEXT("Schwarzglut")), Engineer->GetMoveSpeedScale());
		}
		return;
	}
	if (What == TEXT("panzerschildallein"))
	{
		// The same, but everything of the hero hidden except the shell, to see the shield alone
		VaelIngenieur(TEXT("panzerschild"), Id);
		TArray<AActor*> Owners = { Player };
		if (USkeletalMeshComponent* Human = Player->GetMetaHumanBody())
		{
			Owners.AddUnique(Human->GetOwner());
		}
		for (AActor* Each : Owners)
		{
			TInlineComponentArray<UPrimitiveComponent*> Parts(Each);
			for (UPrimitiveComponent* Part : Parts)
			{
				const bool bShell = Part->GetName() == TEXT("EngineerGlutpanzerSchild");
				Part->SetHiddenInGame(!bShell);
				if (bShell)
				{
					UE_LOG(LogVael, Log, TEXT("VaelIngenieur panzerschildallein: shell at %s, bounds %s"), *Player->GetActorTransform().InverseTransformPosition(Part->GetComponentLocation()).ToCompactString(),
						*Part->Bounds.BoxExtent.ToCompactString());
				}
			}
		}
		return;
	}
	if (What == TEXT("schild"))
	{
		// Goal 14.11.: the Schwarzglut-Rüstung with its folding shield. Damage from a dummy ahead and one behind: bare, fire,
		// the perfect block right after raising, the block once open, from behind, curled after a dodge; firing is held while
		// the shield is up; the tower shield slides open and folds again
		Player->SetHero(EVaelHeroClass::Engineer, false);
		VaelIngenieur(TEXT("alles"));
		for (int32 Place = 0; Place < 5; ++Place)
		{
			FillMaterials();
			const FName Piece(*FString::Printf(TEXT("Schwarzglut_%d"), Place));
			const FVaelGear* Gear = VaelEngineerGear::Find(Piece);
			const int32 Block = Gear != nullptr ? static_cast<int32>(Engineer->GetBuildBlock(*Gear, true)) : -1;
			const bool bBuilt = Engineer->Build(Piece, true);
			const bool bWorn = Engineer->Wear(Piece);
			UE_LOG(LogVael, Log, TEXT("VaelIngenieur schild: %s block %d, built %d, worn %d"), *Piece.ToString(), Block, bBuilt, bWorn);
		}
		Engineer->SetShieldRaised(false);
		UE_LOG(LogVael, Log, TEXT("VaelIngenieur schild: set %d, armour %.0f, max health %.0f, shield %d"), Engineer->CountSet(TEXT("Schwarzglut")),
			Engineer->GetArmorTotal(), Player->GetMaxHealth(), Engineer->HasShield());
		FActorSpawnParameters Parameters;
		Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		const FVector Ahead = Player->GetActorForwardVector();
		AVaelTrainingDummy* Front = GetWorld()->SpawnActor<AVaelTrainingDummy>(AVaelTrainingDummy::StaticClass(), Player->GetActorLocation() + Ahead * 300.0f, FRotator::ZeroRotator, Parameters);
		AVaelTrainingDummy* Back = GetWorld()->SpawnActor<AVaelTrainingDummy>(AVaelTrainingDummy::StaticClass(), Player->GetActorLocation() - Ahead * 300.0f, FRotator::ZeroRotator, Parameters);
		if (Front == nullptr || Back == nullptr)
		{
			return;
		}
		const float Bare = Engineer->ScaleIncoming(100.0f, Front, false);
		const float Fire = Engineer->ScaleIncoming(100.0f, Front, true);
		Engineer->SetShieldRaised(true);
		const float Perfect = Engineer->ScaleIncoming(100.0f, Front, false);
		const bool bShot = Engineer->Fire(Front->GetActorLocation());
		UE_LOG(LogVael, Log, TEXT("VaelIngenieur schild: bare %.1f, fire %.1f, perfect block %.1f (front burning %d), fired while raised %d"), Bare, Fire, Perfect,
			UVaelCombatStatics::HasStatus(Front, EVaelStatus::Burning), bShot);
		TWeakObjectPtr<UVaelEngineerComponent> WeakEng = Engineer;
		TWeakObjectPtr<AVaelTrainingDummy> WeakFront = Front;
		TWeakObjectPtr<AVaelTrainingDummy> WeakBack = Back;
		TWeakObjectPtr<AVaelCharacter> WeakPlayer = Player;
		FTimerHandle Later;
		GetWorld()->GetTimerManager().SetTimer(Later, FTimerDelegate::CreateLambda([WeakEng, WeakFront, WeakBack, WeakPlayer]()
		{
			if (!WeakEng.IsValid() || !WeakFront.IsValid() || !WeakBack.IsValid() || !WeakPlayer.IsValid())
			{
				return;
			}
			UVaelEngineerComponent* Eng = WeakEng.Get();
			const float Held = Eng->ScaleIncoming(100.0f, WeakFront.Get(), false);
			const float Behind = Eng->ScaleIncoming(100.0f, WeakBack.Get(), false);
			int32 Bands = 0;
			FVector HubAt = FVector::ZeroVector;
			for (UActorComponent* Each : WeakPlayer->GetComponents())
			{
				const UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Each);
				Bands += Mesh != nullptr && Mesh->GetName().StartsWith(TEXT("EngineerShieldBand")) && Mesh->GetStaticMesh() != nullptr ? 1 : 0;
				if (Mesh != nullptr && Mesh->GetName() == TEXT("EngineerShieldHub"))
				{
					HubAt = WeakPlayer->GetActorTransform().InverseTransformPosition(Mesh->GetComponentLocation());
				}
			}
			UE_LOG(LogVael, Log, TEXT("VaelIngenieur schild: open %.2f, held block %.1f, from behind %.1f, bands %d, hub at %s (actor space)"),
				Eng->GetShieldOpen(), Held, Behind, Bands, *HubAt.ToCompactString());
			Eng->SetShieldRaised(false);
			Eng->OnDodged();
			const float Curled = Eng->ScaleIncoming(100.0f, WeakBack.Get(), false);
			UE_LOG(LogVael, Log, TEXT("VaelIngenieur schild: curled %.1f (behind burning %d)"), Curled, UVaelCombatStatics::HasStatus(WeakBack.Get(), EVaelStatus::Burning));
		}), 0.6f, false);
		FTimerHandle Folded;
		GetWorld()->GetTimerManager().SetTimer(Folded, FTimerDelegate::CreateLambda([WeakEng]()
		{
			if (WeakEng.IsValid())
			{
				UE_LOG(LogVael, Log, TEXT("VaelIngenieur schild: folded again, open %.2f, raised %d"), WeakEng->GetShieldOpen(), WeakEng->IsShieldRaised());
			}
		}), 1.4f, false);
		return;
	}
	if (What == TEXT("zweig"))
	{
		// Everything built (the last branch of each tree wins), then the branch named in Id built again so it is the one
		VaelIngenieur(TEXT("alles"));
		if (!Id.IsEmpty())
		{
			FillMaterials();
			Engineer->LearnAllPlans();
			const FVaelGear* Gear = VaelEngineerGear::Find(FName(*Id));
			if (Gear != nullptr)
			{
				// built again over the other branch
				Engineer->ForceBranch(Gear->UpgradeOf, Gear->Id);
			}
		}
		UE_LOG(LogVael, Log, TEXT("VaelIngenieur: weapon %s, branch %s, tier %d, interval %.2f"), *Engineer->GetWeapon()->Id.ToString(),
			*Engineer->GetBranch(Engineer->GetWeapon()->Id).ToString(), Engineer->GetWeaponTier(Engineer->GetWeapon()->Id), Engineer->GetInterval());
		return;
	}
	if (What == TEXT("beute"))
	{
		VaelEngineerGear::LoadMaterial(TEXT("Schlacke"));
		const bool bTaken = UVaelEngineerComponent::Dismantle(Player, 2, 10);
		UE_LOG(LogVael, Log, TEXT("VaelIngenieur: found gear taken apart -> %d"), bTaken);
		return;
	}
	if (What != TEXT("test"))
	{
		return;
	}

	// The whole round: the engineer with everything built, three dummies in a row ahead
	FVaelHeroChoice Choice;
	Choice.Class = EVaelHeroClass::Engineer;
	Player->SetHero(Choice.Class, false);
	FillMaterials();
	for (const FVaelGear& Gear : VaelEngineerGear::GetAll())
	{
		Engineer->Build(Gear.Id, true);
	}
	const FVector Forward = Player->GetActorForwardVector();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	TArray<TWeakObjectPtr<AVaelCharacterBase>> Dummies;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FVector Spot = Player->GetActorLocation() + Forward * 600.0f + Right * (Index - 1) * 450.0f;
		Dummies.Add(GetWorld()->SpawnActor<AVaelTrainingDummy>(AVaelTrainingDummy::StaticClass(), Spot, FRotator::ZeroRotator, Parameters));
	}
	UE_LOG(LogVael, Log, TEXT("VaelIngenieur: weapon %s, armour %.0f, max health %.0f"), *Engineer->GetWeapon()->Id.ToString(), Engineer->GetArmorTotal(), Player->GetMaxHealth());

	// The sling with a stone at the middle dummy, the trap under the left one, a mine by the right one set off with a fire shot
	TWeakObjectPtr<UVaelEngineerComponent> WeakEngineer = Engineer;
	while (Engineer->GetWeapon()->Id != VaelEngineerGear::Sling)
	{
		Engineer->CycleWeapon();
	}
	TWeakObjectPtr<AVaelCharacter> WeakPlayer = Player;
	FTimerHandle Handle;
	GetWorld()->GetTimerManager().SetTimer(Handle, [WeakEngineer, WeakPlayer, Dummies]()
	{
		UVaelEngineerComponent* Eng = WeakEngineer.Get();
		if (Eng == nullptr || Dummies.Num() < 3 || !Dummies[0].IsValid() || !Dummies[1].IsValid() || !Dummies[2].IsValid())
		{
			return;
		}
		Eng->Fire(Dummies[1]->GetActorLocation());
		while (Eng->GetDevice() != FName(TEXT("Fangeisen")))
		{
			Eng->CycleDevice(1);
		}
		Eng->PlaceDevice(Dummies[0]->GetActorLocation());
		while (Eng->GetDevice() != FName(TEXT("Knallmine")))
		{
			Eng->CycleDevice(1);
		}
		Eng->PlaceDevice(Dummies[2]->GetActorLocation() - (Dummies[2]->GetActorLocation() - WeakPlayer->GetActorLocation()).GetSafeNormal2D() * 120.0f);
		while (Eng->GetDevice() != FName(TEXT("Schleuderturm")))
		{
			Eng->CycleDevice(1);
		}
		Eng->PlaceDevice(WeakPlayer->GetActorLocation() + WeakPlayer->GetActorForwardVector() * 200.0f);
	}, 1.0f, false);

	FTimerHandle Second;
	GetWorld()->GetTimerManager().SetTimer(Second, [WeakEngineer, WeakPlayer, Dummies]()
	{
		UVaelEngineerComponent* Eng = WeakEngineer.Get();
		if (Eng == nullptr || !Dummies[2].IsValid())
		{
			return;
		}
		// A fire shot into the mine by the right dummy
		while (Eng->GetAmmo() != FName(TEXT("Brand")))
		{
			Eng->CycleAmmo(1);
		}
		Eng->Fire(Dummies[2]->GetActorLocation() - (Dummies[2]->GetActorLocation() - WeakPlayer->GetActorLocation()).GetSafeNormal2D() * 120.0f);
		while (Eng->GetWeapon()->Id != VaelEngineerGear::Bow)
		{
			Eng->CycleWeapon();
		}
	}, 2.0f, false);

	FTimerHandle Third;
	GetWorld()->GetTimerManager().SetTimer(Third, [WeakEngineer, Dummies]()
	{
		UVaelEngineerComponent* Eng = WeakEngineer.Get();
		if (Eng == nullptr || !Dummies[1].IsValid())
		{
			return;
		}
		// The bow (taken up a moment before) with a poison arrow at the middle one
		while (Eng->GetAmmo() != FName(TEXT("Gift")))
		{
			Eng->CycleAmmo(1);
		}
		Eng->Fire(Dummies[1]->GetActorLocation());
	}, 3.2f, false);

	FTimerHandle Report;
	GetWorld()->GetTimerManager().SetTimer(Report, [Dummies]()
	{
		for (int32 Index = 0; Index < Dummies.Num(); ++Index)
		{
			if (const AVaelCharacterBase* Dummy = Dummies[Index].Get())
			{
				UE_LOG(LogVael, Log, TEXT("VaelIngenieur: dummy %d health %.0f, frozen %d, burning %d, confused %d"), Index, Dummy->GetHealth(),
					UVaelCombatStatics::HasStatus(Dummy, EVaelStatus::Frozen), UVaelCombatStatics::HasStatus(Dummy, EVaelStatus::Burning), UVaelCombatStatics::HasStatus(Dummy, EVaelStatus::Confused));
			}
		}
	}, 6.0f, false);
}

void UVaelCheatManager::VaelDpsTest(const FString& Klasse)
{
	AVaelPlayerController* Controller = Cast<AVaelPlayerController>(GetOuterAPlayerController());
	AVaelCharacter* Player = Controller != nullptr ? Controller->GetPawn<AVaelCharacter>() : nullptr;
	if (Player == nullptr)
	{
		return;
	}
	const bool bSustained = Klasse.StartsWith(TEXT("d"));
	const bool bEngineer = Klasse.StartsWith(TEXT("i")) || Klasse.StartsWith(TEXT("v")) || bSustained;
	const bool bPrepared = Klasse.StartsWith(TEXT("v")) || bSustained;
	const bool bFloromant = Klasse.StartsWith(TEXT("f"));
	Player->SetHero(bEngineer ? EVaelHeroClass::Engineer : bFloromant ? EVaelHeroClass::Floromant : EVaelHeroClass::Mage, bFloromant);
	if (bPrepared)
	{
		// the comparison at the start of the game: bow and goggles, for the prepared run also shots and devices; no tiers, sets, lenses
		VaelIngenieur(TEXT("material"));
		UVaelEngineerComponent* Eng = Player->GetEngineer();
		Eng->LearnAllPlans();
		for (const TCHAR* Piece : { TEXT("Bogen"), TEXT("Brille") })
		{
			Eng->Build(FName(Piece), true);
		}
		if (!bSustained)
		{
			for (const TCHAR* Piece : { TEXT("Splitter"), TEXT("Brand"), TEXT("Knallmine"), TEXT("Knallmine"), TEXT("Fangeisen"), TEXT("Schleuderturm") })
			{
				Eng->Build(FName(Piece), true);
			}
		}
	}

	// A dummy that can't fall, 7 m ahead
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AVaelTrainingDummy* Dummy = GetWorld()->SpawnActor<AVaelTrainingDummy>(AVaelTrainingDummy::StaticClass(),
		Player->GetActorLocation() + Player->GetActorForwardVector() * (bFloromant ? 450.0f : 700.0f), FRotator::ZeroRotator, Parameters);
	if (Dummy == nullptr)
	{
		return;
	}
	if (UAbilitySystemComponent* DummyAbilities = Dummy->GetAbilitySystemComponent())
	{
		DummyAbilities->SetNumericAttributeBase(UVaelAttributeSet::GetMaxHealthAttribute(), 1000000.0f);
		DummyAbilities->SetNumericAttributeBase(UVaelAttributeSet::GetHealthAttribute(), 1000000.0f);
	}
	Player->SetActorRotation((Dummy->GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D().Rotation());
	// The dummy faces the player, like a creature that fights him (its front is its weak side)
	Dummy->SetActorRotation((Player->GetActorLocation() - Dummy->GetActorLocation()).GetSafeNormal2D().Rotation());

	struct FRun
	{
		TWeakObjectPtr<AVaelCharacter> Player;
		TWeakObjectPtr<AVaelTrainingDummy> Dummy;
		FString Klasse;
		float Start = 0.0f;
		int32 Step = 0;
		bool bLaid = false;
		bool bFloromant = false;
		bool bEngineer = false;
		bool bPrepared = false;
		bool bSustained = false;
	};
	TSharedRef<FRun> Run = MakeShared<FRun>();
	Run->Player = Player;
	Run->Dummy = Dummy;
	Run->Klasse = Klasse;
	Run->bFloromant = bFloromant;
	Run->bEngineer = bEngineer;
	Run->bPrepared = bPrepared;
	Run->bSustained = bSustained;
	Run->Start = GetWorld()->GetTimeSeconds() + 1.0f;
	AVaelTrainingDummy::ResetMeter();
	UVaelGrimoireSubsystem* Grimoire = GetWorld()->GetGameInstance()->GetSubsystem<UVaelGrimoireSubsystem>();

	TSharedRef<FTimerHandle> Handle = MakeShared<FTimerHandle>();
	GetWorld()->GetTimerManager().SetTimer(*Handle, FTimerDelegate::CreateLambda([this, Run, Handle, Grimoire]()
	{
		AVaelCharacter* Who = Run->Player.Get();
		AVaelTrainingDummy* Target = Run->Dummy.Get();
		UWorld* World = Who != nullptr ? Who->GetWorld() : nullptr;
		if (Who == nullptr || Target == nullptr || World == nullptr)
		{
			return;
		}
		const float Now = World->GetTimeSeconds();
		if (Now < Run->Start)
		{
			AVaelTrainingDummy::ResetMeter();
			return;
		}
		Who->SetActorRotation((Target->GetActorLocation() - Who->GetActorLocation()).GetSafeNormal2D().Rotation());
		const float Elapsed = Now - Run->Start;
		if (Elapsed >= 30.0f)
		{
			World->GetTimerManager().ClearTimer(*Handle);
			if (Run->bFloromant)
			{
				Who->GetFloromant()->SetHandFiring(true, false);
				Who->GetFloromant()->SetHandFiring(false, false);
			}
			UE_LOG(LogVael, Log, TEXT("VaelDpsTest: %s %.1f DPS (%.0f in %.0f s)"), Run->bSustained ? TEXT("dauer") : Run->bPrepared ? TEXT("vorbereitet") : Run->bEngineer ? TEXT("ingenieur") : Run->bFloromant ? TEXT("floromant") : TEXT("magier"), AVaelTrainingDummy::TotalDamage / Elapsed, AVaelTrainingDummy::TotalDamage, Elapsed);
			return;
		}
		if (Run->bFloromant)
		{
			// both hands with plants of the belt fire for the whole time (the towers and crossing come on top for a good player)
			if (!Run->bLaid)
			{
				Run->bLaid = true;
				Who->GetFloromant()->SelectBelt(0);
				Who->GetFloromant()->PutOnHand(true);
				Who->GetFloromant()->SelectBelt(1);
				Who->GetFloromant()->PutOnHand(false);
			}
			if (FMath::Fmod(Elapsed, 6.0f) < 0.1f)
			{
				Who->GetFloromant()->ReloadHands();
			}
			Who->GetFloromant()->SetHandFiring(true, true);
			Who->GetFloromant()->SetHandFiring(false, true);
			return;
		}
		if (Run->bEngineer)
		{
			UVaelEngineerComponent* Eng = Who->GetEngineer();
			if (Run->bSustained && !Run->bLaid)
			{
				// sustained: the bow, endless arrows, no devices
				Run->bLaid = true;
				int32 Guard = 0;
				while (Eng->GetWeapon()->Id != VaelEngineerGear::Bow && Guard++ < 5)
				{
					Eng->CycleWeapon();
				}
			}
			if (Run->bPrepared && !Run->bSustained && !Run->bLaid)
			{
				// prepared: three mines and two traps by the dummy, the bow with shards
				Run->bLaid = true;
				for (const TCHAR* Kind : { TEXT("Knallmine"), TEXT("Knallmine"), TEXT("Fangeisen"), TEXT("Fangeisen"), TEXT("Schleuderturm") })
				{
					int32 Guard = 0;
					while (Eng->GetDevice() != FName(Kind) && Guard++ < 10)
					{
						Eng->CycleDevice(1);
					}
					const FVector Offset = FVector(FMath::FRandRange(-120.0f, 120.0f), FMath::FRandRange(-120.0f, 120.0f), 0.0f);
					Eng->PlaceDevice(Kind == FString(TEXT("Schleuderturm")) ? Who->GetActorLocation() + Who->GetActorForwardVector() * 150.0f : Target->GetActorLocation() + Offset);
				}
				int32 Guard = 0;
				while (Eng->GetWeapon()->Id != VaelEngineerGear::Bow && Guard++ < 5)
				{
					Eng->CycleWeapon();
				}
				Guard = 0;
				while (Eng->GetAmmo() != FName(TEXT("Brand")) && Guard++ < 10)
				{
					Eng->CycleAmmo(1);
				}
			}
			if (Eng->UsesDraw())
			{
				// the bow drawn in full each time
				if (!Eng->IsDrawing())
				{
					Eng->StartDraw();
				}
				else if (Eng->GetDrawSeconds() >= 1.2f)
				{
					Eng->ReleaseDraw(Target->GetActorLocation());
				}
			}
			else
			{
				Eng->Fire(Target->GetActorLocation());
			}
			if (Run->bPrepared && !Run->bSustained && Eng->GetAmmo() == VaelEngineerGear::Standard)
			{
				int32 Guard = 0;
				while (Eng->GetAmmo() != FName(TEXT("Splitter")) && Guard++ < 10)
				{
					Eng->CycleAmmo(1);
				}
			}
			return;
		}
		// the mage: the known formulas that hurt, one after the other, each queued and cast
		UVaelElementComponent* Elements = Who->GetElementComponent();
		if (Elements->GetQueue().IsEmpty() && Grimoire != nullptr && !Grimoire->GetKnownFormulas().IsEmpty())
		{
			const TArray<TObjectPtr<UVaelFormula>>& Known = Grimoire->GetKnownFormulas();
			const UVaelFormula* Formula = Known[Run->Step++ % Known.Num()];
			if (Formula != nullptr && !Formula->Elements.Contains(EVaelElement::Mark))
			{
				for (const EVaelElement Element : Formula->Elements)
				{
					Elements->AddElement(Element);
				}
			}
		}
		if (!Elements->GetQueue().IsEmpty())
		{
			Elements->CastQueue();
		}
	}), 0.1f, true);
}

void UVaelCheatManager::VaelMenuOk()
{
	AVaelPlayerController* Controller = Cast<AVaelPlayerController>(GetOuterAPlayerController());
	const AVaelCharacter* Player = Controller != nullptr ? Controller->GetPawn<AVaelCharacter>() : nullptr;
	if (Player == nullptr)
	{
		return;
	}
	const TArray<const FVaelRecipe*> Recipes = FVaelBrewing::GetRecipes(static_cast<EVaelBrewTier>(Controller->GetBrewTier()));
	const FVaelRecipe* Selected = Recipes.IsValidIndex(Controller->GetBrewSelection()) ? Recipes[Controller->GetBrewSelection()] : nullptr;
	const int32 Before = Selected != nullptr ? Player->GetMaterialBag()->GetCount(Selected->Result.LoadSynchronous()) : 0;
	Controller->PressMenuConfirm();
	UE_LOG(LogVael, Log, TEXT("VaelMenuOk: page %d, recipe %s, potions in the bag %d -> %d"), static_cast<int32>(Controller->GetMenuPage()),
		Selected != nullptr ? *Selected->Id.ToString() : TEXT("-"), Before, Selected != nullptr ? Player->GetMaterialBag()->GetCount(Selected->Result.LoadSynchronous()) : 0);
}

void UVaelCheatManager::VaelInventar()
{
	if (AVaelPlayerController* Controller = Cast<AVaelPlayerController>(GetOuterAPlayerController()))
	{
		Controller->OpenGrimoire(EVaelMenuPage::Inventory);
		Controller->PressMenuBag();
	}
}

void UVaelCheatManager::VaelTafel()
{
	if (AVaelPlayerController* Controller = Cast<AVaelPlayerController>(GetOuterAPlayerController()))
	{
		Controller->OpenBoard();
	}
}

void UVaelCheatManager::VaelGuertel(const FString& PotionId)
{
	AVaelPlayerController* Controller = Cast<AVaelPlayerController>(GetOuterAPlayerController());
	AVaelCharacter* Player = Controller != nullptr ? Controller->GetPawn<AVaelCharacter>() : nullptr;
	if (Player == nullptr)
	{
		return;
	}
	for (const FVaelRecipe& Recipe : FVaelBrewing::GetRecipes())
	{
		if (Recipe.Id == FName(*PotionId))
		{
			const UVaelMaterial* Potion = Recipe.Result.LoadSynchronous();
			if (Player->GetMaterialBag()->GetCount(Potion) <= 0)
			{
				VaelBrauen(PotionId);
			}
			const int32 Place = Player->GetMaterialBag()->ToggleBelt(Potion);
			UE_LOG(LogVael, Log, TEXT("VaelGuertel: %s on belt place %d"), *PotionId, Place);
			return;
		}
	}
}

void UVaelCheatManager::VaelLaeutern(const FString& What)
{
	AVaelPlayerController* Controller = Cast<AVaelPlayerController>(GetOuterAPlayerController());
	AVaelCharacter* Player = Controller != nullptr ? Controller->GetPawn<AVaelCharacter>() : nullptr;
	if (Player == nullptr)
	{
		return;
	}
	for (const TCHAR* Name : { TEXT("Klartrunk"), TEXT("Lichttropfen"), TEXT("Nidkristall") })
	{
		Player->GetMaterialBag()->AddMaterial(LoadObject<UVaelMaterial>(nullptr, *FString::Printf(TEXT("/Game/Vael/Items/Materials/DA_Material_%s.DA_Material_%s"), Name, Name)), 1);
	}
	const float Before = UVaelBalanceSubsystem::GetPlayer(Player);
	if (What == TEXT("kristall"))
	{
		const bool bDone = UVaelBalanceSubsystem::PurifyCrystal(Player);
		UE_LOG(LogVael, Log, TEXT("VaelLaeutern: crystal %s, balance %.1f -> %.1f"), bDone ? TEXT("purified") : TEXT("missing"), Before, UVaelBalanceSubsystem::GetPlayer(Player));
	}
	else
	{
		UVaelBalanceSubsystem::RestGroup(GetWorld(), 8.0f, false);
		UE_LOG(LogVael, Log, TEXT("VaelLaeutern: the group rests, balance before %.1f"), Before);
	}
}

namespace
{
	/** Strain by a German name of the cheats; false for "Zufall" or anything unknown */
	bool ParseSeuchenStamm(const FString& Name, EVaelSeuchenStamm& OutStrain)
	{
		const FString Lower = Name.ToLower();
		if (Lower.StartsWith(TEXT("gr")) || Lower.StartsWith(TEXT("ver")))
		{
			OutStrain = EVaelSeuchenStamm::Verwesung;
		}
		else if (Lower.StartsWith(TEXT("hon")) || Lower.StartsWith(TEXT("gold")) || Lower.StartsWith(TEXT("bern")))
		{
			OutStrain = EVaelSeuchenStamm::Honig;
		}
		else if (Lower.StartsWith(TEXT("k")) || Lower.StartsWith(TEXT("eis")) || Lower.StartsWith(TEXT("blau")))
		{
			OutStrain = EVaelSeuchenStamm::Kaelte;
		}
		else if (Lower.StartsWith(TEXT("nid")) || Lower.StartsWith(TEXT("viol")))
		{
			OutStrain = EVaelSeuchenStamm::Nid;
		}
		else if (Lower.StartsWith(TEXT("rot")) || Lower.StartsWith(TEXT("rau")))
		{
			OutStrain = EVaelSeuchenStamm::Rausch;
		}
		else
		{
			return false;
		}
		return true;
	}
}

void UVaelCheatManager::VaelSchleimgaenger(const FString& Stamm, int32 Count, const FString& Koerper)
{
	APlayerController* PlayerController = GetOuterAPlayerController();
	const APawn* PlayerPawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
	if (PlayerPawn == nullptr)
	{
		return;
	}
	EVaelSeuchenStamm Strain = EVaelSeuchenStamm::Verwesung;
	const bool bChosen = ParseSeuchenStamm(Stamm, Strain);
	const FVector Center = PlayerPawn->GetActorLocation() + PlayerPawn->GetActorForwardVector().GetSafeNormal2D() * 600.0f;
	int32 Spawned = 0;
	for (int32 Index = 0; Index < FMath::Max(Count, 1); ++Index)
	{
		const FVector Spot = Center + FVector(FMath::FRandRange(-180.0f, 180.0f), FMath::FRandRange(-180.0f, 180.0f), 300.0f);
		FVector Ground;
		if (!AVaelCreature::FindGround(GetWorld(), Spot, Ground))
		{
			continue;
		}
		const FRotator Facing = (PlayerPawn->GetActorLocation() - Ground).GetSafeNormal2D().Rotation();
		const FString Body = Koerper.ToLower();
		UVaelSchleimgaengerData* Data = Body.StartsWith(TEXT("kr")) ? GetMutableDefault<UVaelKoboldSchleimkratzerData>()
			: Body.StartsWith(TEXT("w")) || Body.StartsWith(TEXT("hort")) ? GetMutableDefault<UVaelKoboldHortwaechterData>()
			: Body.StartsWith(TEXT("ae")) || Body.StartsWith(TEXT("äl")) || Body.StartsWith(TEXT("kon")) ? GetMutableDefault<UVaelKoboldKontoraeltesterData>()
			: GetMutableDefault<UVaelSchleimgaengerData>();
		if (AVaelCreature::SpawnCreature(GetWorld(), Data, Ground, Facing, nullptr,
			[bChosen, Strain](AVaelCreature* Creature)
			{
				AVaelSchleimgaenger* Host = Cast<AVaelSchleimgaenger>(Creature);
				if (Host != nullptr && bChosen)
				{
					Host->SetStrain(Strain);
				}
			}) != nullptr)
		{
			++Spawned;
		}
	}
	UE_LOG(LogVael, Log, TEXT("VaelSchleimgaenger: %d x %s"), Spawned, bChosen ? *UVaelTropfseucheSettings::Get()->GetStrain(Strain).Name.ToString() : TEXT("Zufall"));
}

void UVaelCheatManager::VaelSeuche(const FString& Stamm)
{
	APlayerController* PlayerController = GetOuterAPlayerController();
	AVaelCharacter* Player = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
	if (Player == nullptr)
	{
		return;
	}
	EVaelSeuchenStamm Strain = EVaelSeuchenStamm::Verwesung;
	if (!ParseSeuchenStamm(Stamm, Strain))
	{
		UE_LOG(LogVael, Log, TEXT("VaelSeuche: washed off (%s)"), UVaelTropfseuche::Cure(Player) ? TEXT("was infested") : TEXT("was clean"));
		return;
	}
	UVaelTropfseuche::Infest(nullptr, Player, Strain);
}

void UVaelCheatManager::VaelKobold(const FString& Rolle, int32 Count, bool bBeute)
{
	APlayerController* PlayerController = GetOuterAPlayerController();
	const APawn* PlayerPawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
	if (PlayerPawn == nullptr)
	{
		return;
	}
	if (bBeute)
	{
		VaelMaterial(TEXT("alle"), 2);
	}
	const FString Name = Rolle.ToLower();
	const EVaelKoboldRolle Kind = Name.StartsWith(TEXT("w")) || Name.StartsWith(TEXT("h")) ? EVaelKoboldRolle::Waechter
		: Name.StartsWith(TEXT("z")) || Name.StartsWith(TEXT("k")) ? EVaelKoboldRolle::Zauberer
		: Name.StartsWith(TEXT("t")) || Name.StartsWith(TEXT("sk")) ? EVaelKoboldRolle::Treiber
		: Name.StartsWith(TEXT("sp")) ? EVaelKoboldRolle::Sprengmeister
		: Name.StartsWith(TEXT("l")) ? EVaelKoboldRolle::Laternentraeger : EVaelKoboldRolle::Dieb;
	const FVector Center = PlayerPawn->GetActorLocation() + PlayerPawn->GetActorForwardVector().GetSafeNormal2D() * 600.0f;
	int32 Spawned = 0;
	for (int32 Index = 0; Index < FMath::Max(Count, 1); ++Index)
	{
		FVector Ground;
		if (AVaelCreature::FindGround(GetWorld(), Center + FVector(FMath::FRandRange(-180.0f, 180.0f), FMath::FRandRange(-180.0f, 180.0f), 300.0f), Ground)
			&& AVaelCreature::SpawnCreature(GetWorld(), UVaelKobolde::GetData(Kind), Ground, (PlayerPawn->GetActorLocation() - Ground).GetSafeNormal2D().Rotation()) != nullptr)
		{
			++Spawned;
		}
	}
	UE_LOG(LogVael, Log, TEXT("VaelKobold: %d x %s"), Spawned, *Rolle);
}

void UVaelCheatManager::VaelAmeise(const FString& Erz, int32 Count, bool bAhnenpanzer, bool bVersklavt)
{
	APlayerController* PlayerController = GetOuterAPlayerController();
	const APawn* PlayerPawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
	if (PlayerPawn == nullptr)
	{
		return;
	}
	const FVector Center = PlayerPawn->GetActorLocation() + PlayerPawn->GetActorForwardVector().GetSafeNormal2D() * 600.0f;
	int32 Spawned = 0;
	for (int32 Index = 0; Index < FMath::Max(Count, 1); ++Index)
	{
		FVector Ground;
		if (!AVaelCreature::FindGround(GetWorld(), Center + FVector(FMath::FRandRange(-250.0f, 250.0f), FMath::FRandRange(-250.0f, 250.0f), 300.0f), Ground))
		{
			continue;
		}
		AVaelSchuerfameise* Ant = Cast<AVaelSchuerfameise>(AVaelCreature::SpawnCreature(GetWorld(), GetMutableDefault<UVaelSchuerfameiseData>(), Ground,
			(PlayerPawn->GetActorLocation() - Ground).GetSafeNormal2D().Rotation()));
		if (Ant != nullptr)
		{
			Ant->SetupAnt(FName(*Erz), bAhnenpanzer, bVersklavt);
			++Spawned;
		}
	}
	UE_LOG(LogVael, Log, TEXT("VaelAmeise: %d x %s%s%s"), Spawned, *Erz, bAhnenpanzer ? TEXT(" Ahnenpanzer") : TEXT(""), bVersklavt ? TEXT(" versklavt") : TEXT(""));
}

void UVaelCheatManager::VaelErztochter(int32 Paar, int32 Count, bool bVersklavt)
{
	APlayerController* PlayerController = GetOuterAPlayerController();
	const APawn* PlayerPawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
	if (PlayerPawn == nullptr)
	{
		return;
	}
	const FVector Center = PlayerPawn->GetActorLocation() + PlayerPawn->GetActorForwardVector().GetSafeNormal2D() * 700.0f;
	int32 Spawned = 0;
	for (int32 Index = 0; Index < FMath::Max(Count, 1); ++Index)
	{
		FVector Ground;
		if (!AVaelCreature::FindGround(GetWorld(), Center + FVector(FMath::FRandRange(-300.0f, 300.0f), FMath::FRandRange(-300.0f, 300.0f), 300.0f), Ground))
		{
			continue;
		}
		AVaelErztochter* Daughter = Cast<AVaelErztochter>(AVaelCreature::SpawnCreature(GetWorld(), GetMutableDefault<UVaelErztochterData>(), Ground,
			(PlayerPawn->GetActorLocation() - Ground).GetSafeNormal2D().Rotation()));
		if (Daughter != nullptr)
		{
			Daughter->SetupDaughter(Paar, bVersklavt);
			++Spawned;
		}
	}
	FName Lead, Companion;
	AVaelErztochter::GetPairOres(Paar, Lead, Companion);
	UE_LOG(LogVael, Log, TEXT("VaelErztochter: %d x %s + %s%s"), Spawned, *Lead.ToString(), *Companion.ToString(), bVersklavt ? TEXT(" versklavt") : TEXT(""));
}
