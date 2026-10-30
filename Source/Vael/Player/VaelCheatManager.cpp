// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/VaelCheatManager.h"
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
#include "Items/VaelItemData.h"
#include "Items/VaelItemDrop.h"
#include "Items/VaelItemSettings.h"
#include "Items/VaelMaterial.h"
#include "Items/VaelMaterialBag.h"
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
#include "Player/VaelHeroSubsystem.h"
#include "UI/VaelHUD.h"
#include "Player/VaelPlayerController.h"
#include "Floromant/VaelFloromantComponent.h"
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

	// Down from high above onto whatever ground is there
	const FVector Above(X * 100.0f, Y * 100.0f, 50000.0f);
	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VaelTeleport), false, Pawn);
	const float GroundZ = GetWorld()->LineTraceSingleByChannel(Hit, Above, Above - FVector(0.0f, 0.0f, 100000.0f), ECC_Visibility, QueryParams) ? Hit.ImpactPoint.Z : 0.0f;

	Pawn->TeleportTo(FVector(Above.X, Above.Y, GroundZ + Pawn->GetSimpleCollisionHalfHeight() + 5.0f), Pawn->GetActorRotation());
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

	const bool bAwakened = Grimoire->AwakenMark(NSLOCTEXT("VaelMagic", "MarkAwakensCheat", "Das Mark erwacht in dir."));
	UE_LOG(LogVael, Log, TEXT("VaelAwakenMark: %s"), bAwakened ? TEXT("the Mark awakens") : TEXT("the Mark is awake already"));

	// Seen on screen too, with the button of the fifth element
	UVaelNoticeSubsystem::Post(GetWorld()->GetGameInstance(), bAwakened ? NSLOCTEXT("VaelMagic", "MarkAwakeCheat", "Das Mark ist erwacht") : NSLOCTEXT("VaelMagic", "MarkAwakeAlready", "Das Mark ist schon wach"),
		NSLOCTEXT("VaelMagic", "MarkButtonHint", "Mark-Element: RB / R1 oder Taste 5"), UVaelMagicSettings::Get()->GetElementColor(EVaelElement::Mark), 5.0f);
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
		UE_LOG(LogVael, Warning, TEXT("VaelMaterial: no material %s, try Glutdruese, Russfeder, Ordenssiegel, Aeltestenschwinge, Markkristall or HerzDerGlut"), *AssetName);
	}
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
	Choice.Class = Class.StartsWith(TEXT("f"), ESearchCase::IgnoreCase) ? EVaelHeroClass::Floromant : EVaelHeroClass::Mage;
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
		Choice.Class = Class.StartsWith(TEXT("f"), ESearchCase::IgnoreCase) ? EVaelHeroClass::Floromant : EVaelHeroClass::Mage;
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
	for (int32 Index = 0; !Plant.IsEmpty() && Index < Floromant->GetHandBeltSize(); ++Index)
	{
		const UVaelPlantArmData* Data = Floromant->GetHandBeltPlant(Floromant->GetHandBeltSelection());
		if (Data != nullptr && Data->GetDisplayName(false).ToString().StartsWith(Plant, ESearchCase::IgnoreCase))
		{
			break;
		}
		Floromant->PageHandBelt(1);
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

void UVaelCheatManager::VaelFeuern(const FString& Hands)
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

void UVaelCheatManager::VaelNatterBrut()
{
	for (TActorIterator<AVaelPfuhlnatterPopulation> It(GetWorld()); It; ++It)
	{
		It->BreedNow();
	}
}
