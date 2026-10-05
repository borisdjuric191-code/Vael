// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/VaelCheatManager.h"
#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelCombatStatics.h"
#include "Creatures/VaelCreature.h"
#include "Creatures/VaelCreatureData.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Magic/VaelElementComponent.h"
#include "Magic/VaelFormula.h"
#include "Magic/VaelGrimoireSubsystem.h"
#include "Player/VaelCharacter.h"
#include "Vael.h"
#include "VaelGameMode.h"
#include "UI/VaelUISettings.h"
#include "AbilitySystemComponent.h"
#include "Combat/VaelAttributeSet.h"
#include "World/VaelRegion.h"

void UVaelCheatManager::VaelCast(const FString& Elements, float AimYaw)
{
	APlayerController* PlayerController = GetOuterAPlayerController();
	const AVaelCharacter* VaelCharacter = PlayerController != nullptr ? PlayerController->GetPawn<AVaelCharacter>() : nullptr;
	if (VaelCharacter == nullptr)
	{
		return;
	}

	UVaelElementComponent* ElementComponent = VaelCharacter->GetElementComponent();
	ElementComponent->ClearQueue();

	for (const TCHAR Letter : Elements.ToUpper())
	{
		switch (Letter)
		{
		case TEXT('F'): ElementComponent->AddElement(EVaelElement::Fire); break;
		case TEXT('W'): ElementComponent->AddElement(EVaelElement::Water); break;
		case TEXT('E'): ElementComponent->AddElement(EVaelElement::Earth); break;
		case TEXT('L'): ElementComponent->AddElement(EVaelElement::Air); break;
		case TEXT('M'): ElementComponent->AddElement(EVaelElement::Mark); break;
		default: break;
		}
	}

	PlayerController->SetControlRotation(FRotator(0.f, AimYaw, 0.f));

	const EVaelCastResult Result = ElementComponent->CastQueue();

	UE_LOG(LogVael, Log, TEXT("VaelCast %s: %s, mana now %.1f"), *Elements, *UEnum::GetValueAsString(Result), VaelCharacter->GetMana());
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

	if (Name.StartsWith(TEXT("glut")) || Name.StartsWith(TEXT("krie")) || Name.StartsWith(TEXT("crawl")))
	{
		Data = GetMutableDefault<UVaelEmberCrawlerData>();
	}
	else if (Name.StartsWith(TEXT("asch")) || Name.StartsWith(TEXT("harp")))
	{
		Data = GetMutableDefault<UVaelAshHarpyData>();
	}
	else if (Name.StartsWith(TEXT("ael")) || Name.StartsWith(TEXT("alt")) || Name.StartsWith(TEXT("elder")))
	{
		Data = GetMutableDefault<UVaelHarpyElderData>();
	}
	else if (Name.StartsWith(TEXT("pred")) || Name.StartsWith(TEXT("preach")))
	{
		Data = GetMutableDefault<UVaelPreacherData>();
	}
	else if (Name.StartsWith(TEXT("koe")) || Name.StartsWith(TEXT("kon")) || Name.StartsWith(TEXT("queen")))
	{
		Data = GetMutableDefault<UVaelEmberQueenData>();
	}

	if (Data == nullptr)
	{
		UE_LOG(LogVael, Warning, TEXT("VaelSpawn: unknown creature '%s'. Use Glutkriecher, Aschharpyie, Aelteste, Prediger or Koenigin."), *Kind);
		return;
	}

	const FVector Center = PlayerPawn->GetActorLocation() + PlayerPawn->GetActorForwardVector().GetSafeNormal2D() * 800.0f;
	const int32 NumSpawned = GameMode->SpawnCreatureGroup(Data, Center, FMath::Max(Count, 1), 150.0f + Data->CollisionRadius);

	UE_LOG(LogVael, Log, TEXT("VaelSpawn: %d x %s"), NumSpawned, *Data->DisplayName.ToString());
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
	else
	{
		UE_LOG(LogVael, Warning, TEXT("VaelWeather: use Klar, Regen or Sturm"));
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
