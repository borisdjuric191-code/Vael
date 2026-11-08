// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/VaelCheatManager.h"
#include "UI/VaelWorldMap.h"
#include "World/VaelProgressSubsystem.h"
#include "World/VaelTravel.h"
#include "World/VaelBalance.h"
#include "World/VaelRefugiumClutch.h"
#include "Items/VaelBrewing.h"
#include "Items/VaelPotionEffects.h"
#include "Creatures/VaelTrainingDummy.h"
#include "Combat/VaelAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Compendium/VaelCompendiumSubsystem.h"
#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelCombatStatics.h"
#include "Creatures/VaelCreature.h"
#include "Creatures/VaelCreatureData.h"
#include "Creatures/VaelEncounterDirector.h"
#include "Creatures/VaelKelchreisser.h"
#include "Creatures/VaelRankenluchs.h"
#include "Creatures/VaelPfuhlnatter.h"
#include "Creatures/VaelPfuhlnatterPopulation.h"
#include "Creatures/VaelThistleSpider.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Items/VaelInventory.h"
#include "Engineer/VaelEngineerComponent.h"
#include "Engineer/VaelEngineerGear.h"
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

void UVaelCheatManager::VaelTeleport(float X, float Y)
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	APawn* Pawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
	if (Pawn == nullptr)
	{
		return;
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

	if (Data == nullptr)
	{
		UE_LOG(LogVael, Warning, TEXT("VaelSpawn: unknown creature '%s'. Use Glutkriecher, Aschharpyie, Aelteste, Prediger, Markprediger, Koenigin, Waechter, Spannhornkaefer, Rankenluchs, Kelchreisser, Pfuhlnatter, Distelspinne, Distelsaemling, Aschenroller, Sichelpuppe or Schleieraffe."), *Kind);
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
	else
	{
		UE_LOG(LogVael, Warning, TEXT("VaelWeather: use Klar, Regen, Sturm, Duerre or Wind"));
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
		const EVaelMenuPage Open = Page.StartsWith(TEXT("i"), ESearchCase::IgnoreCase) ? EVaelMenuPage::Inventory : Page.StartsWith(TEXT("k"), ESearchCase::IgnoreCase) ? EVaelMenuPage::Compendium
			: Page.StartsWith(TEXT("a"), ESearchCase::IgnoreCase) ? EVaelMenuPage::Quests : Page.StartsWith(TEXT("g"), ESearchCase::IgnoreCase) ? EVaelMenuPage::Hero : EVaelMenuPage::Formulas;
		VaelController->CloseGrimoire();
		VaelController->OpenGrimoire(Open);
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

	if (What == TEXT("material"))
	{
		FillMaterials();
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
	if (What == TEXT("ruestung"))
	{
		// Everything built, the set named in Id worn on all four places (Rollpanzer, Sichelklinge, Russfeder, Blutschuppen)
		VaelIngenieur(TEXT("alles"));
		const FString Set = Id.IsEmpty() ? FString(TEXT("Sichelklinge")) : Id;
		for (int32 Place = 0; Place < 4; ++Place)
		{
			Engineer->Wear(FName(*FString::Printf(TEXT("%s_%d"), *Set, Place)));
		}
		UE_LOG(LogVael, Log, TEXT("VaelIngenieur: wears %s, set count %d, armour %.0f"), *Set, Engineer->CountSet(FName(*Set)), Engineer->GetArmorTotal());
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
