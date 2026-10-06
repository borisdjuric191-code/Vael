// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/VaelCheatManager.h"
#include "Combat/VaelCharacterBase.h"
#include "Combat/VaelCombatStatics.h"
#include "Creatures/VaelCreature.h"
#include "Creatures/VaelCreatureData.h"
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
#include "Magic/VaelElementComponent.h"
#include "Magic/VaelFormula.h"
#include "Magic/VaelFormulaScroll.h"
#include "Magic/VaelGrimoireSubsystem.h"
#include "Player/VaelCharacter.h"
#include "Vael.h"
#include "VaelAssets.h"
#include "VaelGameMode.h"
#include "UI/VaelUISettings.h"
#include "AbilitySystemComponent.h"
#include "Combat/VaelAttributeSet.h"
#include "World/VaelMarkSource.h"
#include "World/VaelRegion.h"
#include "Magic/VaelGameplayTags.h"

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
	else if (Name.StartsWith(TEXT("wae")) || Name.StartsWith(TEXT("wäch")) || Name.StartsWith(TEXT("quell")) || Name.StartsWith(TEXT("guard")))
	{
		Data = GetMutableDefault<UVaelSourceGuardianData>();
	}

	if (Data == nullptr)
	{
		UE_LOG(LogVael, Warning, TEXT("VaelSpawn: unknown creature '%s'. Use Glutkriecher, Aschharpyie, Aelteste, Prediger, Koenigin or Waechter."), *Kind);
		return;
	}

	const FVector Center = PlayerPawn->GetActorLocation() + PlayerPawn->GetActorForwardVector().GetSafeNormal2D() * 800.0f;
	const int32 NumSpawned = GameMode->SpawnCreatureGroup(Data, Center, FMath::Max(Count, 1), 150.0f + Data->CollisionRadius);

	UE_LOG(LogVael, Log, TEXT("VaelSpawn: %d x %s"), NumSpawned, *Data->DisplayName.ToString());
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
	else if (Name.StartsWith(TEXT("d")))
	{
		Region->SetWeather(EVaelWeather::Drought);
	}
	else
	{
		UE_LOG(LogVael, Warning, TEXT("VaelWeather: use Klar, Regen, Sturm or Duerre"));
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

void UVaelCheatManager::VaelAwakenMark()
{
	const UGameInstance* GameInstance = GetWorld() != nullptr ? GetWorld()->GetGameInstance() : nullptr;
	UVaelGrimoireSubsystem* Grimoire = GameInstance != nullptr ? GameInstance->GetSubsystem<UVaelGrimoireSubsystem>() : nullptr;

	if (Grimoire != nullptr && !Grimoire->AwakenMark(NSLOCTEXT("VaelMagic", "MarkAwakensCheat", "Das Mark erwacht in dir.")))
	{
		UE_LOG(LogVael, Log, TEXT("VaelAwakenMark: the Mark is awake already"));
	}
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
